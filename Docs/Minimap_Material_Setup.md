# Hướng Dẫn Thiết Lập Material Cho Minimap

> **Mục đích**: Tài liệu này hướng dẫn tạo **custom material** cho minimap của OBNavigation, khớp với các tham số C++ được truyền trong `UOBMapWidgetBase::UpdateMapMaterial`.

Để xem toàn bộ quy trình thiết lập tiled minimap/tactical map, đọc thêm `Tiled_Minimap_Setup.md`.

---

## 1. Tổng Quan Các Tham Số Bắt Buộc

| Tham số C++ | Input trong Material | Kiểu | Mô tả |
|---|---|---|---|
| `ViewCenterUV` | **PlayerPosUV** (Custom Node) | `float2` | Tọa độ UV của người chơi trên texture bản đồ, là tâm của vùng nhìn. |
| `PlayerYawRad` | **PlayerYawRad** | `float` | Góc xoay động từ người chơi, theo control rotation hoặc actor rotation. |
| `ZoomAmount` | **ZoomAmount** | `float` | Hệ số zoom; lớn hơn `1` là zoom vào, nhỏ hơn `1` là zoom ra. |
| `MapRotationOffsetRad` | **MapRotationOffsetRad** | `float` | Góc xoay tĩnh của toàn bản đồ, gồm alignment và offset tùy chỉnh. |
| `Map` | **Map** (Texture2D) | `Texture2D` | Texture minimap thực tế, thường là render target hoặc static map atlas. |
| `MapSampler` | **MapSampler** (Sampler) | `SamplerState` | Sampler dùng để lấy mẫu map texture. |
| `UV` (screen) | **UV** (float2) | `float2` | UV mặc định của widget, từ `0-1` trên toàn widget. |

> **Lưu ý**: Material phải dùng đúng tên tham số ở trên. Code C++ set các tham số bằng `TEXT("...")`. Nếu tên không khớp, material sẽ bỏ qua giá trị và minimap có thể đứng yên.

---

## 2. Tạo Material

1. Trong **Content Browser**, nhấn chuột phải -> **Material** -> đặt tên, ví dụ `M_MinimapBackground`.
2. Mở material editor và đặt **Material Domain** thành **User Interface**.
3. Đặt **Blend Mode** thành **Transparent** hoặc **Masked** nếu chỉ cần bản đồ opaque.
4. Bật **Two-Sided** nếu có khả năng dùng bản đồ bị mirror.

---

## 3. Thêm Custom Expression Node

1. Kéo một node **Custom** vào graph.
2. Trong panel **Details** của node:
   - **Code**: dán đoạn HLSL bên dưới.
   - **Inputs**: thêm các input sau. Thứ tự không quan trọng, nhưng tên phải khớp chính xác:
     ```text
     UV                   (float2)
     Map                  (Texture2D)
     MapSampler           (SamplerState)
     PlayerPosUV          (float2)
     PlayerYawRad         (float)
     MapRotationOffsetRad (float)
     ZoomAmount           (float)
     ```
   - **Outputs**: đặt một output tên `Result`, kiểu **float4**.
3. Nối output **Result** vào **Base Color** của material node.

### HLSL Cho Custom Node

```hlsl
// Tham số đầu vào
// float2 UV: Tọa độ texture mặc định của UI widget (0-1).
// Texture2D Map: Texture bản đồ cần sample.
// SamplerState MapSampler: Sampler của texture.
// float2 PlayerPosUV: Vị trí người chơi trên map texture theo không gian U,V.
// float PlayerYawRad: Góc xoay động theo người chơi (0 nếu map không xoay động).
// float MapRotationOffsetRad: Góc xoay tĩnh của toàn bộ map view.
// float ZoomAmount: Mức zoom vào.

// Tâm của màn hình/widget. Mọi phép xoay diễn ra quanh điểm này.
float2 Center = float2(0.5, 0.5);

// 1. Bắt đầu từ UV hiện tại của pixel trên màn hình.
float2 ScreenPixelUV = UV;

// 2. Dịch offset để người chơi nằm ở tâm.
float2 OffsetFromCenter = ScreenPixelUV - Center;

// 3. Áp dụng zoom. ZoomAmount lớn hơn làm offset nhỏ hơn, tức zoom vào.
OffsetFromCenter /= ZoomAmount;

// 4. Xoay ngược theo yaw động của người chơi.
float PlayerCos = cos(-PlayerYawRad);
float PlayerSin = sin(-PlayerYawRad);
float2x2 PlayerRotationMatrix = float2x2(PlayerCos, -PlayerSin, PlayerSin, PlayerCos);
OffsetFromCenter = mul(OffsetFromCenter, PlayerRotationMatrix);

// 5. Xoay ngược theo góc tĩnh của bản đồ.
float MapCos = cos(-MapRotationOffsetRad);
float MapSin = sin(-MapRotationOffsetRad);
float2x2 MapRotationMatrix = float2x2(MapCos, -MapSin, MapSin, MapCos);
OffsetFromCenter = mul(OffsetFromCenter, MapRotationMatrix);

// 6. Tọa độ cuối cùng để sample texture.
float2 FinalUVsToSample = PlayerPosUV + OffsetFromCenter;

// 7. Sample map texture.
return Map.Sample(MapSampler, FinalUVsToSample);
```

---

## 4. Gắn Material Vào Widget

1. Trong **Widget Blueprint**, ví dụ `WBP_Minimap`, chọn **Image** hiển thị background tên `MapImage`.
2. Trong panel **Details**, đặt **Brush -> Material** thành material vừa tạo, ví dụ `M_MinimapBackground`.
3. Code C++ của widget (`UOBMapWidgetBase::InitializeMapWidget`) sẽ tạo **Dynamic Material Instance** từ material này và lưu vào `MapMaterialInstance`.
4. Ở runtime, các tham số sau được set mỗi tick:
   ```cpp
   // UV tâm view, thường là vị trí người chơi trên map
   MapMaterialInstance->SetVectorParameterValue(TEXT("ViewCenterUV"), ViewCenterUVColor);

   // Yaw động, tính bằng radians, dựa trên config
   MapMaterialInstance->SetScalarParameterValue(TEXT("PlayerYawRad"), ...);

   // Mức zoom
   MapMaterialInstance->SetScalarParameterValue(TEXT("ZoomAmount"), CurrentZoom);

   // Offset xoay tĩnh, tính bằng radians
   MapMaterialInstance->SetScalarParameterValue(TEXT("MapRotationOffsetRad"), ...);
   ```

> **Quan trọng**: Tên tham số `ViewCenterUV`, `PlayerYawRad`, `ZoomAmount`, `MapRotationOffsetRad` phải khớp với input trong Custom node. Nếu đổi tên ở material, phải cập nhật cả code C++ tương ứng.

---

## 5. Kiểm Tra Thiết Lập

1. Chạy game trong editor.
2. Mở **Output Log**. Bạn nên thấy log tương tự:
   ```text
   [WBP_Minimap::NativeTick] - Trace #N ... ViewCenterUV= X=0.439 Y=0.510 ...
   ```
3. Background minimap phải trượt dưới marker người chơi khi pawn di chuyển.
4. Xoay camera nếu dùng `ControlRotation`; bản đồ phải xoay mượt.
5. Điều chỉnh **ZoomAmount** trong `UOBMinimapConfigAsset`; bản đồ phải zoom vào/ra ngay.

Nếu bản đồ đứng yên:

- Xác nhận material instance đã được tạo (`MapMaterialInstance` khác null). Có thể thêm `UE_LOG` sau `CreateDynamicMaterialInstance` nếu cần.
- Kiểm tra tên tham số trong material. Hover vào input của Custom node và đảm bảo đúng `ViewCenterUV`, `PlayerYawRad`, `ZoomAmount`, `MapRotationOffsetRad`.
- Kiểm tra **Map texture** đã được gán đúng vào input `Map`. Nên thử bằng một texture đơn giản trước.

---

## 6. Tùy Chọn: Thêm Debug Overlay

Nếu muốn hiển thị vị trí UV của người chơi trên map texture:

1. Thêm **Scalar Parameter** tên `DebugShowMarker`, mặc định `0`.
2. Trong Custom node, sau dòng `float2 FinalUVsToSample = ...;`, có thể thêm:
   ```hlsl
   if (DebugShowMarker > 0.5)
   {
       // Vẽ một chấm đỏ nhỏ tại vị trí người chơi để debug.
       if (distance(FinalUVsToSample, PlayerPosUV) < 0.005)
           return float4(1,0,0,1); // red dot
   }
   ```
3. Expose tham số này từ C++ nếu cần bật/tắt ở runtime.

---

## 7. Tùy Chọn: Material Cho Tile Của Tiled Minimap

Các layer Panoramic dạng tiled có thể render từng tile texture riêng thay vì một atlas texture duy nhất. Minimap vuông và tactical map có thể dùng direct tile brush; tactical map sẽ preserve aspect bằng viewport runtime, không phụ thuộc vào canvas ngoài là vuông hay chữ nhật. Minimap tròn nên gán `TiledMapTileMaterial` trong `UOBMinimapConfigAsset` để từng tile áp dụng cùng mask tròn như single-texture path.

Tạo một UI material có các tham số sau:

| Tham số | Kiểu | Mô tả |
|---|---|---|
| `TileTexture` | Texture2D | Texture tile đã load. |
| `TileScreenMin` | Vector | Điểm min của rect tile trong không gian màn hình chuẩn hóa. |
| `TileScreenMax` | Vector | Điểm max của rect tile trong không gian màn hình chuẩn hóa. |
| `ShapeAlpha` | Scalar | `1.0` để dùng hành vi mask tròn. |

Nếu không gán `TiledMapTileMaterial` và `MinimapShape` là `Circle`, OBNavigation sẽ fallback sang direct tile texture và log một warning trong debug mode.

---

## 8. Danh Sách Tổng Kết

- [ ] Tạo material `M_MinimapBackground` với domain **User Interface** và blend **Transparent**.
- [ ] Thêm node **Custom** với HLSL ở trên.
- [ ] Định nghĩa input: `UV`, `Map`, `MapSampler`, `PlayerPosUV`, `PlayerYawRad`, `MapRotationOffsetRad`, `ZoomAmount`.
- [ ] Nối output vào **Base Color** kiểu `float4`.
- [ ] Gán material vào `MapImage` của widget.
- [ ] Đảm bảo C++ `UpdateMapMaterial` set đúng bốn tham số runtime.
- [ ] Test trong game: marker người chơi nằm giữa, background trượt, rotation và zoom hoạt động.
- [ ] Với minimap tròn dạng tiled, gán `TiledMapTileMaterial` có `TileTexture`, `TileScreenMin`, `TileScreenMax`, `ShapeAlpha`.
