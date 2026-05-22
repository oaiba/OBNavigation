# Hướng Dẫn Thiết Lập Tiled Minimap & Tactical Map

> Mục đích: checklist thiết lập end-to-end để dùng output Panoramic Tile Set + LOD với minimap và tactical map của OBNavigation.

Tài liệu này giả định project đang dùng:

- `OBPanoramicMinimapGenerator` để capture/export `UMinimapDefinitionDataAsset`.
- Project settings của `ExtractionCoreGame` để đưa các Panoramic map layer vào `UOBNavigationSubsystem`.
- `UOBMinimapWidget` và `UOBTacticalMapWidget` làm UI runtime.

---

## 1. Capture Asset Bản Đồ Từ Panoramic

Dùng Panoramic minimap generator để export runtime definition asset.

Output bắt buộc cho tiled map:

- `UMinimapDefinitionDataAsset`
- `UMinimapTileSetDataAsset`
- các asset tile `UTexture2D` trong các folder generated `Tiles/LOD_<LOD>/`

Definition asset nên có:

- `WorldBounds`
- `OutputSize`
- `MapRotationDegrees`
- `bClampQueriesToBounds`
- `TileSet`
- `BaseMapTexture` tùy chọn

Thiết lập capture khuyến nghị:

- Bật export Tile Set + LOD trong Panoramic.
- Giữ `BaseMapTexture` để có fallback mượt trong lúc tiled assets đang stream.
- Dùng capture bounds ổn định cho level/floor mục tiêu.
- Nếu map có nhiều tầng/layer dọc trùng nhau, capture mỗi tầng/layer thành một Panoramic definition riêng.

Hành vi runtime:

- Nếu có `TileSet`, OBNavigation dùng tiled streaming.
- Nếu thiếu `TileSet` nhưng có `BaseMapTexture`, OBNavigation dùng single-texture material path.
- Nếu có cả hai, OBNavigation hiển thị `BaseMapTexture` trước, rồi chuyển sang tiles khi các active tile texture đã sẵn sàng.

---

## 2. Đăng Ký Map Layer

Với OBExtraction, cấu hình layer tại:

```text
Project Settings -> Extraction Navigation -> Panoramic Map Layers
```

Với mỗi layer:

| Trường | Cách thiết lập |
|---|---|
| `MinimapDefinition` | Gán `UMinimapDefinitionDataAsset` đã export. |
| `LayerName` | Dùng tên ổn định như `GroundFloor`, `Basement`, hoặc `MainMap`. |
| `Priority` | Priority cao hơn thắng khi bounds của các layer overlap. |
| `bClampQueriesToBounds` | Thường nên bật cho UI minimap/tactical map. |
| `bEnabled` | Bật để layer được dùng ở runtime. |

Bridge sẽ build `FOBNavigationMapLayerSpec` từ Panoramic definition. Không cần tạo tile asset thủ công trong OBNavigation.

---

## 3. Cấu Hình Minimap Data Asset

Mở hoặc tạo một `UOBMinimapConfigAsset`.

Các field bắt buộc:

| Trường | Cách thiết lập |
|---|---|
| `MinimapBackgroundMaterial` | Gán UI material từ `Minimap_Material_Setup.md`. Dùng cho single-texture fallback và legacy maps. |
| `Zoom`, `MinZoom`, `MaxZoom` | Tinh chỉnh scale của minimap centered theo người chơi. |
| `bShouldRotateMap` | Bật nếu minimap cần xoay theo yaw của pawn/control. |
| `MapAlignment` | Khớp hướng ảnh map đã capture với trục world. |
| `MinimapShape` | Chọn `Circle` hoặc `Square`. |

Các field cho tiled map:

| Trường | Cách thiết lập |
|---|---|
| `TiledMapTileBudget` | Mặc định `25`. Số tile texture tối đa cache cho minimap. |
| `MinimapMaxLODTileLimit` | Mặc định `12`. Minimap request `MaxLOD` khi số tile full-detail nhìn thấy vẫn nằm trong giới hạn này. |
| `TiledMapTileMaterial` | UI material tùy chọn cho mask minimap tròn dạng tiled. Nên có nếu muốn minimap tròn hoàn chỉnh. |

Ghi chú về LOD:

- Minimap thử Panoramic `MaxLOD` trước.
- Nếu số tile full-detail nhìn thấy vượt `MinimapMaxLODTileLimit`, nó fallback sang LOD selection theo world-units-per-pixel của Panoramic.
- Đặt `MinimapMaxLODTileLimit` bằng `0` nếu muốn luôn request `MaxLOD`.

---

## 4. Cấu Hình Tactical Map Data Asset

Mở hoặc tạo một `UOBTacticalMapConfigAsset`.

Các field chính:

| Trường | Cách thiết lập |
|---|---|
| `InitialZoom`, `MinZoom`, `MaxZoom` | Tinh chỉnh zoom range của full-map. |
| `ZoomStep` | Bước zoom cho mouse wheel/button. |
| `PanSpeed` | Tốc độ pan liên tục khi dùng button/stick input. |
| `bClampViewToMapBounds` | Thường nên bật. |
| `bAllowLayerSwitching` | Bật nếu tactical map có thể đổi tầng/layer. |
| `TacticalTileBudget` | Mặc định `64`. Số tile texture tối đa cache cho tactical map. |

LOD của tactical map luôn được chọn từ tile pyramid của Panoramic bằng world-units-per-screen-pixel. Zoom ra nên chọn overview/intermediate LOD; zoom vào nên chọn LOD chi tiết hơn.

---

## 5. Tạo Widget Blueprint

### Widget Minimap

Tạo hoặc cập nhật widget blueprint dùng `UOBMinimapWidget` làm parent class.

Bind widget bắt buộc:

| Tên | Kiểu | Ghi chú |
|---|---|---|
| `MapImage` | `Image` | Dùng cho single-texture maps và fallback trong lúc tiled maps stream. |
| `MapMarkerCanvas` | `Canvas Panel` | Chứa tiled map canvas, overlays và marker widgets. Nên phủ cùng vùng với `MapImage`. |

Bind widget tùy chọn:

| Tên | Kiểu | Ghi chú |
|---|---|---|
| `CompassRingImage` | `Image` | Dùng bởi `UOBMinimapWidget` nếu có compass ring art. |

Khởi tạo runtime:

```cpp
MinimapWidget->InitializeAndStartTracking(MinimapConfigAsset);
```

### Widget Tactical Map

Tạo hoặc cập nhật widget blueprint dùng `UOBTacticalMapWidget` làm parent class.

Bind widget bắt buộc:

| Tên | Kiểu | Ghi chú |
|---|---|---|
| `MapImage` | `Image` | Đường render fallback single-texture. |
| `MapMarkerCanvas` | `Canvas Panel` | Chứa tiled map canvas, overlays và marker widgets. |

Bind widget tùy chọn:

- `MapInputArea`
- `ZoomInButton`, `ZoomOutButton`, `ZoomSlider`, `ZoomValueText`
- `RecenterButton`, `FollowPlayerCheckBox`, `FollowStateText`
- `LayerComboBox`, `ClearLayerOverrideButton`, `ActiveLayerText`
- Các control filter marker/overlay được liệt kê trong `UOBTacticalMapWidget`

Khởi tạo runtime:

```cpp
TacticalMapWidget->InitializeTacticalMapAndStartTracking(MinimapConfigAsset, TacticalMapConfigAsset);
```

### Yêu Cầu Chung

- `MapImage` và `MapMarkerCanvas` nên có cùng size và anchors.
- `MapMarkerCanvas` phải đủ lớn để phủ toàn bộ vùng bản đồ nhìn thấy.
- Không tự thêm tile canvas trong Blueprint. OBNavigation tự tạo runtime tile canvas có clipping bên dưới overlays và markers.

---

## 6. Material

### Material Single-Texture / Fallback

`MapImage` vẫn dùng minimap background material tiêu chuẩn. Làm theo:

```text
OBNavigation/Docs/Minimap_Material_Setup.md
```

Các tham số runtime bắt buộc:

- `MapTexture`
- `ViewCenterUV`
- `PlayerYawRad`
- `ZoomAmount`
- `MapRotationOffsetRad`
- `ShapeAlpha`

### Material Tile Tùy Chọn Cho Tiled Map

Với minimap tròn dạng tiled, gán `TiledMapTileMaterial` trong `UOBMinimapConfigAsset`.

Material phải có domain **User Interface** và hỗ trợ:

| Tham số | Kiểu | Mô tả |
|---|---|---|
| `TileTexture` | Texture2D | Tile texture được load từ Panoramic. |
| `TileScreenMin` | Vector | Điểm min của tile rect trong widget space chuẩn hóa. |
| `TileScreenMax` | Vector | Điểm max của tile rect trong widget space chuẩn hóa. |
| `ShapeAlpha` | Scalar | `1.0` nghĩa là dùng mask tròn. |

Nếu thiếu material này và `MinimapShape` là `Circle`, map vẫn hoạt động nhưng direct tile images sẽ render không có circular tile mask. Một debug warning sẽ được log một lần.

---

## 7. Thiết Lập Runtime

Khi game start hoặc khi possession:

1. Đảm bảo `UExtractionNavigationMapBridgeSubsystem` đã load các `PanoramicMapLayers` được cấu hình.
2. Set tracked pawn local:

```cpp
NavigationSubsystem->SetTrackedPlayerPawn(PlayerPawn);
```

3. Initialize minimap/tactical widgets bằng config assets tương ứng.
4. Register các marker như player, squad, POI, ping, extraction và loot bằng marker APIs hiện có.

Runtime tile stats có thể lấy từ bất kỳ map widget nào:

```cpp
const FOBMapTileRuntimeStats Stats = MapWidget->GetTileRuntimeStats();
```

Các trường hữu ích:

- `State`
- `FailureReason`
- `CaptureRunId`
- `SourceMapName`
- `MaxLOD`
- `ActiveLOD`
- `ActiveTileCount`
- `LoadedTileCount`
- `CachedTileCount`

---

## 8. Danh Sách Kiểm Tra

### Asset Contract

- [ ] Panoramic definition có `WorldBounds` hợp lệ.
- [ ] Panoramic definition có `OutputSize` hợp lệ.
- [ ] Tiled maps có `TileSet` hợp lệ.
- [ ] `TileSet->GetMaxLOD()` trả về đúng full-detail LOD mong muốn.
- [ ] Tile textures tồn tại trong `Tiles/LOD_<LOD>/`.

### Minimap

- [ ] Marker người chơi nằm ở giữa.
- [ ] Map pan/rotate giống hành vi của single-texture path.
- [ ] Zoom cao dùng `MaxLOD` khi active tile count nằm trong `MinimapMaxLODTileLimit`.
- [ ] Radius rộng hoặc widget nhỏ downgrade LOD thay vì load quá nhiều full-detail tiles.
- [ ] Minimap tròn dùng `TiledMapTileMaterial`, hoặc log warning fallback đúng như mong đợi.

### Tactical Map

- [ ] Zoom ra chọn overview/intermediate LOD.
- [ ] Zoom vào chọn high-detail LOD.
- [ ] Free pan qua ranh giới tile không bị thiếu tile/gap.
- [ ] Đổi layer/floor clear tiles cũ và load layer được chọn.

### Compatibility

- [ ] Các layer `MapTexture` cũ vẫn render.
- [ ] Panoramic definitions chỉ có `BaseMapTexture` vẫn render.
- [ ] Projection marker và overlay vẫn khớp trên cả single-texture và tiled maps.

---

## 9. Xử Lý Sự Cố

| Triệu chứng | Cần kiểm tra |
|---|---|
| Map trắng/blank | Kiểm tra `PanoramicMapLayers`, `WorldBounds`, vị trí active pawn và quá trình initialize widget. |
| Tiled-only map blank trong chốc lát | Đây là hành vi bình thường khi definition, tile set và active textures đầu tiên đang stream. Kiểm tra `GetTileRuntimeStats()`. |
| Base texture không bao giờ chuyển sang tiles | Kiểm tra `Stats.State`, `FailureReason`, `TileSet` và soft paths của tile texture. |
| Minimap tròn nhìn thành vuông | Gán `TiledMapTileMaterial` và kiểm tra material params. |
| Sai LOD | Kiểm tra `MinimapMaxLODTileLimit`, kích thước widget, zoom và Panoramic pyramid settings. |
| Trục Y của tile bị lật | Kiểm tra `UVMin`/`UVMax` của Panoramic và hướng capture bounds. |
| Marker bị lệch | Đảm bảo `MapImage` và `MapMarkerCanvas` có cùng size/anchors. |
