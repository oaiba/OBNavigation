# Hướng Dẫn Tạo Tiled Map Tile Material Cho Minimap Tròn

> Mục đích: hướng dẫn tạo `TiledMapTileMaterial` để tiled minimap có mask tròn đúng khi `UOBMinimapConfigAsset::MinimapShape = Circle`.

Tài liệu này chỉ áp dụng cho tiled minimap. Single-texture minimap dùng material background trong `Minimap_Material_Setup.md`.

---

## 1. Khi Nào Cần Material Này

Cần tạo và gán `TiledMapTileMaterial` khi dùng đồng thời:

- Panoramic map layer có `TileSet`.
- `UOBMinimapWidget` render minimap dạng tiled.
- `UOBMinimapConfigAsset::MinimapShape = Circle`.

Nếu không gán material này, OBNavigation vẫn render tile bằng direct texture brush, nhưng tile layer sẽ không được cắt theo mask tròn. Runtime sẽ log warning một lần khi debug message được bật:

```text
Circle minimap tiled layer is using direct textures because TiledMapTileMaterial is not configured.
```

Minimap dạng `Square` và tactical map không bắt buộc dùng material này.

---

## 2. Layout Contract Của Widget

Để mask tròn chính xác, widget minimap nên dùng vùng bản đồ vuông:

- `MapImage` và `MapMarkerCanvas` có cùng anchors, alignment và size.
- Vùng hiển thị minimap là hình vuông.
- Circle mask là hình tròn nội tiếp hình vuông đó.

Runtime truyền `TileScreenMin` và `TileScreenMax` theo không gian chuẩn hóa của canvas chứa tile. Vì vậy nếu canvas ngoài là hình chữ nhật nhưng bạn vẫn muốn mask tròn tuyệt đối theo pixel, hãy đặt `MapImage` và `MapMarkerCanvas` vào một container vuông ở UMG. Đây là setup khuyến nghị cho minimap tròn.

Trong material preview, đặt default `TileScreenMin = (0,0,0,0)` và `TileScreenMax = (1,1,0,0)` để thấy được vòng tròn đầy đủ. Runtime sẽ tự overwrite hai giá trị này cho từng tile.

---

## 3. Parameter Contract

Tạo một UI material, ví dụ `M_MinimapTiledTile_CircleMask`, với các parameter đúng tên sau:

| Parameter | Kiểu | Runtime set từ đâu | Mô tả |
|---|---|---|---|
| `TileTexture` | Texture2D | `UOBMapWidgetBase` | Texture của tile hiện tại. |
| `TileScreenMin` | Vector | `UOBMapWidgetBase` | Góc top-left của tile trong canvas normalized space. |
| `TileScreenMax` | Vector | `UOBMapWidgetBase` | Góc bottom-right của tile trong canvas normalized space. |
| `ShapeAlpha` | Scalar | `UOBMapWidgetBase` | `1.0` bật circle mask, `0.0` giữ nguyên tile vuông. |

Không đổi tên các parameter này. Nếu đổi tên trong material, C++ sẽ không set được value runtime.

Quan trọng với UI material: alpha nằm trong `Final Color` không tự trở thành opacity. Phải nối alpha mask vào pin **Opacity** của material.

---

## 4. Tạo Material Trong Unreal

1. Trong **Content Browser**, tạo một **Material** mới, ví dụ:
   ```text
   M_MinimapTiledTile_CircleMask
   ```
2. Mở material và đặt:
   - **Material Domain**: `User Interface`
   - **Blend Mode**: `Translucent`
   - **Shading Model**: `Unlit`
3. Tạo các parameter:
   - `TextureObjectParameter` tên `TileTexture`
   - `VectorParameter` tên `TileScreenMin`, preview default `(0, 0, 0, 0)`
   - `VectorParameter` tên `TileScreenMax`, preview default `(1, 1, 0, 0)`
   - `ScalarParameter` tên `ShapeAlpha`, default `1.0`
4. Chọn một trong hai cách triển khai:
   - Dùng **Custom node** ở mục 6. Cách này tự sample `TileTexture` trong HLSL.
   - Dùng **Material nodes** ở mục 7. Cách này dùng `TextureSample` graph truyền thống.
5. Với cả hai cách, output cuối phải được nối như sau:
   - `RGB` -> **Final Color**
   - `A` hoặc `FinalAlpha` -> **Opacity**

---

## 5. Công Thức Mask

Với mỗi pixel của tile:

1. Lấy local UV của tile trong khoảng `0..1`.
2. Convert local UV sang canvas normalized UV bằng `TileScreenMin/Max`.
3. Đổi canvas UV về không gian tâm `(-1..1)`.
4. Dùng `length()` để tạo circle mask.
5. Nhân alpha của tile với mask.

Pseudo logic:

```hlsl
float2 TileLocalUV = UV;
float2 CanvasUV = lerp(TileScreenMin.xy, TileScreenMax.xy, TileLocalUV);
float2 Centered = CanvasUV * 2.0 - 1.0;

float Radius = length(Centered);
float CircleMask = 1.0 - smoothstep(0.985, 1.0, Radius);
float FinalMask = lerp(1.0, CircleMask, saturate(ShapeAlpha));

float4 TileColor = TileTexture.Sample(TileTextureSampler, TileLocalUV);
return float4(TileColor.rgb, TileColor.a * FinalMask);
```

`smoothstep(0.985, 1.0, Radius)` tạo mép mềm nhẹ để hạn chế răng cưa. Có thể đổi `0.985` thành `0.98` nếu muốn feather rộng hơn.

---

## 6. Cách Làm Bằng Custom Node

Trong material editor:

1. Thêm node `TextureCoordinate`.
2. Thêm node `Custom`.
3. Thêm các input cho Custom node:

| Input | Kiểu |
|---|---|
| `UV` | `float2` |
| `TileTexture` | `Texture2D` |
| `TileScreenMin` | `float4` |
| `TileScreenMax` | `float4` |
| `ShapeAlpha` | `float` |

Unreal tự tạo sampler tên `TileTextureSampler` cho texture input `TileTexture`, nên không cần tạo input sampler riêng.

4. Đặt output type của Custom node là `CMOT Float4`.
5. Dán HLSL:

```hlsl
float2 TileLocalUV = UV;
float2 CanvasUV = lerp(TileScreenMin.xy, TileScreenMax.xy, TileLocalUV);
float2 Centered = CanvasUV * 2.0 - 1.0;

float Radius = length(Centered);
float CircleMask = 1.0 - smoothstep(0.985, 1.0, Radius);
float FinalMask = lerp(1.0, CircleMask, saturate(ShapeAlpha));

float4 TileColor = TileTexture.Sample(TileTextureSampler, TileLocalUV);
return float4(TileColor.rgb, TileColor.a * FinalMask);
```

6. Tách output của Custom node:
   - `ComponentMask RGB` -> **Final Color**
   - `ComponentMask A` -> **Opacity**

Không nối nguyên output `float4` trực tiếp vào **Final Color** rồi để **Opacity = 1.0**. Khi làm như vậy, UI material sẽ bỏ qua alpha trong `float4`, khiến tile vẫn render thành hình vuông.

Nếu version Unreal của project không expose `Final Color`, nối `RGB` vào color input chính của material UI theo graph hiện tại của project, còn `A` vẫn phải nối vào **Opacity**.

---

## 7. Cách Làm Bằng Material Nodes

Nếu không muốn dùng Custom node, dựng graph tương đương:

1. `TextureCoordinate` -> local tile UV.
2. `LinearInterpolate`
   - `A = TileScreenMin.xy`
   - `B = TileScreenMax.xy`
   - `Alpha = TextureCoordinate`
   - Output là `CanvasUV`.
3. `CanvasUV * 2 - 1` -> `Centered`.
4. `Length(Centered)` -> `Radius`.
5. `SmoothStep`
   - `Min = 0.985`
   - `Max = 1.0`
   - `Value = Radius`
6. `OneMinus` output của SmoothStep -> `CircleMask`.
7. `LinearInterpolate`
   - `A = 1.0`
   - `B = CircleMask`
   - `Alpha = Saturate(ShapeAlpha)`
   - Output là `FinalMask`.
8. `TextureSample(TileTexture, TextureCoordinate)` -> `TileColor`.
9. `TileColor.A * FinalMask` -> alpha cuối.
10. Nối `TileColor.RGB` -> **Final Color**.
11. Nối `FinalAlpha` -> **Opacity**.

---

## 8. Gán Vào Config Asset

Mở `UOBMinimapConfigAsset` đang dùng cho minimap:

| Field | Giá trị |
|---|---|
| `MinimapShape` | `Circle` |
| `TiledMapTileMaterial` | `M_MinimapTiledTile_CircleMask` |

Không cần gán material này cho `UOBTacticalMapConfigAsset`.

---

## 9. Kiểm Tra Runtime

Checklist nhanh:

- [ ] Minimap config dùng `MinimapShape = Circle`.
- [ ] `TiledMapTileMaterial` đã được gán.
- [ ] `MapImage` và `MapMarkerCanvas` nằm trong vùng vuông.
- [ ] Tile layer bị cắt theo vòng tròn, không còn các góc vuông lộ ra.
- [ ] Marker/player marker vẫn nằm trên cùng hệ tọa độ với tile layer.
- [ ] Output Log không còn warning thiếu `TiledMapTileMaterial`.

Nếu cần debug nhanh, tạm đổi HLSL return thành:

```hlsl
return float4(FinalMask.xxx, FinalMask);
```

Sau đó vẫn nối `RGB` vào **Final Color** và `A` vào **Opacity**. Kết quả đúng là vùng trắng hình tròn, ngoài vòng tròn trong suốt.

---

## 10. Xử Lý Sự Cố

| Triệu chứng | Nguyên nhân thường gặp | Cách xử lý |
|---|---|---|
| Material preview hoặc runtime vẫn là hình vuông | Custom node output đang nối thẳng vào `Final Color`, còn `Opacity` vẫn là `1.0`. | Tách `RGB` vào `Final Color`, tách `A` vào `Opacity`. |
| Minimap tròn vẫn thấy tile vuông | Chưa gán `TiledMapTileMaterial` hoặc `MinimapShape` chưa phải `Circle`. | Kiểm tra `UOBMinimapConfigAsset`. |
| Toàn bộ tile biến mất | Output alpha bằng `0` hoặc parameter texture sai tên. | Kiểm tra `TileTexture`, `ShapeAlpha`, HLSL return. |
| Mask bị lệch khỏi tâm | `MapImage` và `MapMarkerCanvas` không cùng vùng hoặc canvas không vuông. | Đặt cả hai vào một container vuông, anchors giống nhau. |
| Mép tròn bị răng cưa | Feather quá hẹp hoặc blend mode không phù hợp. | Dùng `Translucent`, chỉnh smoothstep từ `0.98` đến `1.0`. |
| Tactical map cũng bị mask tròn | Material bị gán vào đường tactical tùy biến ngoài config minimap. | Chỉ gán material này trong `UOBMinimapConfigAsset`. |
