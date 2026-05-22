# Hướng Dẫn Thiết Lập Tactical Full Map

> Mục đích: hướng dẫn setup riêng cho full-screen tactical map dùng `UOBTacticalMapWidget`, bao gồm tiled map từ Panoramic, zoom/pan, layer switching, marker filter và overlay filter.

Tài liệu này tập trung vào tactical full map. Nếu cần setup tổng quát cho cả minimap và tactical map, xem thêm `Tiled_Minimap_Setup.md`.

---

## 1. Điều Kiện Đầu Vào

Tactical full map dùng cùng runtime map layer với minimap. Trước khi setup widget, cần có:

- Panoramic `UMinimapDefinitionDataAsset` đã export.
- Nếu muốn tiled full map, definition phải có `TileSet`.
- `Project Settings -> Extraction Navigation -> Panoramic Map Layers` đã trỏ tới các definition cần dùng.
- `UOBNavigationSubsystem` đã nhận tracked pawn bằng `SetTrackedPlayerPawn`.
- Marker configs đã được đăng ký qua `UOBNavigationMapRegistryAsset`.

Tactical map hỗ trợ các Panoramic definition sau:

- Definition chỉ có `BaseMapTexture` để render single-texture.
- Definition có `TileSet` để stream tiles + LOD.

`FOBNavigationMapLayerSpec.MapTexture` legacy đã bị xóa khỏi runtime layer. Nếu Blueprint/config cũ còn set texture-only layer, hãy đổi sang Panoramic definition.

---

## 2. Cấu Hình Panoramic Map Layers

Vào:

```text
Project Settings -> Extraction Navigation -> Panoramic Map Layers
```

Với mỗi layer/floor:

| Trường | Cách thiết lập |
|---|---|
| `MinimapDefinition` | Gán Panoramic `UMinimapDefinitionDataAsset`. |
| `LayerName` | Đặt tên ổn định để tactical map có thể switch layer, ví dụ `Floor_01`, `Basement`, `Roof`. |
| `Priority` | Layer priority cao hơn sẽ được chọn khi pawn nằm trong nhiều bounds. |
| `bClampQueriesToBounds` | Thường bật để pan/projection không đi quá xa ngoài map. |
| `bEnabled` | Bật layer cần dùng runtime. |

Nếu muốn tactical map có dropdown đổi tầng, mỗi floor nên có `LayerName` rõ ràng và không trùng nhau.

---

## 3. Tạo Tactical Map Config Asset

Tạo asset kiểu `UOBTacticalMapConfigAsset`.

Các field quan trọng:

| Trường | Gợi ý |
|---|---|
| `InitialZoom` | `1.0` cho full map mặc định. |
| `MinZoom` | `0.25` hoặc nhỏ hơn nếu muốn nhìn toàn bản đồ lớn. |
| `MaxZoom` | `8.0` hoặc cao hơn nếu tile capture đủ chi tiết. |
| `ZoomStep` | `0.25` là mức dễ dùng cho mouse wheel/button. |
| `PanSpeed` | `600.0` là mặc định cho input liên tục. |
| `bClampViewToMapBounds` | Nên bật để view center không trôi khỏi map. |
| `bShowPlayerMarker` | Bật nếu muốn thấy marker của local player trên full map. |
| `MarkerScale` | Tăng/giảm kích thước marker riêng cho full map. |
| `bStartFollowingTrackedPlayer` | Bật nếu mở map là center theo player. Tắt nếu muốn mở ở giữa map/free pan. |
| `bAllowLayerSwitching` | Bật nếu UI cho phép đổi floor/layer. |
| `TacticalTileBudget` | Mặc định `64`. Tăng nếu map lớn và pan nhanh bị reload tile nhiều. |

LOD của tiled tactical map được chọn bằng world-units-per-screen-pixel. Khi zoom ra, tactical map sẽ dùng overview/intermediate LOD; khi zoom vào, nó dùng LOD chi tiết hơn.

---

## 4. Dùng Minimap Visual Config Cho Background/Fallback

`InitializeTacticalMapAndStartTracking` cần cả:

- `UOBMinimapConfigAsset`
- `UOBTacticalMapConfigAsset`

`UOBMinimapConfigAsset` cung cấp material/visual chung:

| Trường | Cách dùng trong tactical map |
|---|---|
| `MinimapBackgroundMaterial` | Dùng cho single-texture fallback và legacy map. |
| `MapAlignment` | Dùng để map texture khớp trục world. |
| `MapRotationOffset` | Dùng cho static map rotation. |
| `TiledMapTileMaterial` | Không bắt buộc cho tactical map; tactical có thể nằm trong canvas chữ nhật, nhưng map content luôn preserve aspect nên direct tile brush là đủ. |

Tactical map luôn north-up theo policy hiện tại, không dùng dynamic pawn yaw để xoay map.

---

## 5. Tạo Widget Blueprint Tactical Full Map

Tạo widget blueprint và đặt parent class là:

```text
UOBTacticalMapWidget
```

### Bind Widget Bắt Buộc

| Tên | Kiểu | Ghi chú |
|---|---|---|
| `MapImage` | `Image` | Hiển thị single-texture/fallback `BaseMapTexture` từ Panoramic definition. |
| `MapMarkerCanvas` | `Canvas Panel` | Chứa runtime tile canvas, overlays và marker widgets. |

Yêu cầu layout:

- `MapImage` và `MapMarkerCanvas` phải phủ cùng vùng và nên cùng nằm trong một hệ tọa độ canvas.
- Với full-screen map, cả hai có thể anchor full size trong vùng map chữ nhật; OBNavigation sẽ render map content theo aspect của asset ở giữa vùng đó.
- Nếu muốn map tactical chiếm đúng hình vuông không có khoảng trống hai bên, đặt cả `MapImage` và `MapMarkerCanvas` trong một `SizeBox`/`ScaleBox` vuông ở Blueprint.
- `MapMarkerCanvas` nên nằm phía trên `MapImage`.
- Không tự thêm tile canvas trong Blueprint; OBNavigation tự tạo canvas tile runtime bên trong `MapMarkerCanvas`.

### Bind Widget Tùy Chọn Cho Điều Khiển

Các widget này dùng `BindWidgetOptional`; thiếu cái nào thì core tactical map vẫn chạy.

| Tên | Kiểu | Chức năng |
|---|---|---|
| `MapInputArea` | `Widget` | Vùng nhận mouse drag pan và mouse wheel zoom. Phải hit-test visible. |
| `ZoomInButton` | `Button` | Zoom vào một bước `ZoomStep`. |
| `ZoomOutButton` | `Button` | Zoom ra một bước `ZoomStep`. |
| `ZoomSlider` | `Slider` | Điều khiển zoom theo range config. |
| `ZoomValueText` | `TextBlock` | Hiển thị zoom hiện tại. |
| `PanUpButton` | `Button` | Pan lên. |
| `PanDownButton` | `Button` | Pan xuống. |
| `PanLeftButton` | `Button` | Pan trái. |
| `PanRightButton` | `Button` | Pan phải. |
| `RecenterButton` | `Button` | Center lại theo tracked player. |
| `FollowPlayerCheckBox` | `CheckBox` | Bật/tắt follow tracked player. |
| `FollowStateText` | `TextBlock` | Hiển thị trạng thái follow/free pan. |
| `LayerComboBox` | `ComboBoxString` | Chọn layer/floor. Có option `Auto`. |
| `ClearLayerOverrideButton` | `Button` | Xóa layer override, quay về auto layer. |
| `ActiveLayerText` | `TextBlock` | Hiển thị layer hiện tại. |
| `MarkerLayerComboBox` | `ComboBoxString` | Chọn marker layer để filter. Có option `All`. |
| `MarkerLayerEnabledCheckBox` | `CheckBox` | Bật/tắt marker layer đang chọn. |
| `ActiveMarkerFilterText` | `TextBlock` | Hiển thị marker filter hiện tại. |
| `OverlayCategoryTextBox` | `EditableTextBox` | Filter overlay theo category. |
| `OverlayTagTextBox` | `EditableTextBox` | Filter overlay theo tag. |
| `ClearOverlayFiltersButton` | `Button` | Xóa overlay filters. |
| `ActiveOverlayFilterText` | `TextBlock` | Hiển thị overlay filters hiện tại. |
| `ViewCenterText` | `TextBlock` | Debug view center UV. |
| `PanInputText` | `TextBlock` | Debug pan input. |

---

## 6. Khởi Tạo Runtime

Sau khi tạo widget:

```cpp
TacticalMapWidget->InitializeTacticalMapAndStartTracking(MinimapConfigAsset, TacticalMapConfigAsset);
```

Yêu cầu trước khi gọi:

- `MinimapConfigAsset` không null.
- `TacticalConfigAsset` không null.
- `UOBNavigationSubsystem` đã tồn tại trong `GameInstance`.
- Nếu muốn center theo player, subsystem đã có tracked pawn.

Các API thường dùng:

```cpp
TacticalMapWidget->AddZoomInput(1.0f);
TacticalMapWidget->SetTacticalMapZoom(2.0f);
TacticalMapWidget->AddPanInput(FVector2D(50.0f, 0.0f));
TacticalMapWidget->SetViewCenterUV(FVector2D(0.5f, 0.5f));
TacticalMapWidget->SetViewCenterWorldLocation(WorldLocation);
TacticalMapWidget->RecenterOnTrackedPlayer();
TacticalMapWidget->SetFollowTrackedPlayer(true);
TacticalMapWidget->SetTacticalMapLayerByName(TEXT("Floor_01"));
TacticalMapWidget->ClearTacticalMapLayerOverride();
```

---

## 7. Tiled Full Map Và LOD

Khi active layer có `PanoramicDefinition` với `TileSet`, tactical map dùng tiled render path.

Runtime behavior:

- `MapImage` dùng `BaseMapTexture` fallback nếu có.
- `MapImage`, tiled images, overlays và markers dùng cùng aspect-preserving viewport; capture 8192x8192 sẽ luôn hiển thị thành hình vuông kể cả khi tactical canvas là hình chữ nhật.
- Tile definition/tile set/tile textures được stream async.
- Khi active tile images đã sẵn sàng, `MapImage` được ẩn để tiled map hiển thị.
- Nếu layer không có `TileSet` nhưng có `BaseMapTexture`, tactical map dùng single-texture path.
- Nếu layer thiếu cả `TileSet` và `BaseMapTexture`, map bị ẩn và runtime log lỗi rõ ràng.

LOD behavior:

- Tactical map chọn LOD bằng `ChooseTileLODForWorldUnitsPerPixel`.
- Zoom out chọn LOD thấp hơn để tránh load toàn bộ full-detail tiles.
- Zoom in chọn LOD cao hơn để giữ chi tiết.
- `TacticalTileBudget` giới hạn số tile texture cache lại trong runtime.

Debug stats:

```cpp
const FOBMapTileRuntimeStats Stats = TacticalMapWidget->GetTileRuntimeStats();
```

Các field hữu ích:

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

## 8. Layer, Marker Và Overlay Filters

### Layer/Floor Switching

Nếu `bAllowLayerSwitching` bật:

- `LayerComboBox` sẽ có `Auto` và các `LayerName` từ available map layers.
- Chọn `Auto` để tactical map dùng current layer từ subsystem.
- Chọn một layer cụ thể để override tactical map mà không ảnh hưởng minimap.

Blueprint/C++ API:

```cpp
TacticalMapWidget->SetTacticalMapLayerByName(TEXT("Basement"));
TacticalMapWidget->ClearTacticalMapLayerOverride();
```

### Marker Layer Filter

Marker filter dựa trên `UOBMapMarker::MarkerLayerName`.

```cpp
TacticalMapWidget->SetMarkerLayerFilter(TEXT("Pings"), true);
TacticalMapWidget->ClearMarkerLayerFilter();
```

Nếu không có filter, tactical map hiển thị mọi marker mà marker config cho phép trên `FullMap`.

### Overlay Filter

Overlay filter dựa trên `Category` và `FilterTags` trong overlay elements.

```cpp
TacticalMapWidget->SetOverlayCategoryFilter(TEXT("Extraction"));
TacticalMapWidget->SetOverlayTagFilter(TEXT("Active"));
TacticalMapWidget->ClearOverlayFilters();
```

---

## 9. Checklist Kiểm Tra

### Widget

- [ ] Widget blueprint parent là `UOBTacticalMapWidget`.
- [ ] Có `MapImage` đúng kiểu `Image`.
- [ ] Có `MapMarkerCanvas` đúng kiểu `Canvas Panel`.
- [ ] `MapImage` và `MapMarkerCanvas` phủ cùng vùng.
- [ ] `MapInputArea` hit-test visible nếu dùng mouse drag/wheel.

### Config

- [ ] `UOBTacticalMapConfigAsset` đã được gán khi initialize.
- [ ] `InitialZoom`, `MinZoom`, `MaxZoom` hợp lý với kích thước map.
- [ ] `TacticalTileBudget` đủ lớn cho tốc độ pan mong muốn.
- [ ] `bAllowLayerSwitching` bật nếu cần đổi floor/layer.
- [ ] `bShowPlayerMarker` bật nếu muốn thấy local player trên full map.

### Runtime

- [ ] `SetTrackedPlayerPawn` đã được gọi.
- [ ] `PanoramicMapLayers` đã load.
- [ ] `GetTileRuntimeStats().State` là `Ready` với tiled map.
- [ ] Zoom out đổi sang LOD overview/intermediate.
- [ ] Zoom in đổi sang LOD chi tiết.
- [ ] Pan qua ranh giới tile không có gap hoặc lật trục Y.
- [ ] Layer override không làm đổi layer của minimap.

---

## 10. Xử Lý Sự Cố

| Triệu chứng | Cần kiểm tra |
|---|---|
| Full map trắng/blank | Kiểm tra `MapImage`, `MapMarkerCanvas`, `PanoramicMapLayers`, tracked pawn và active layer. |
| Không pan/zoom bằng chuột | Kiểm tra `MapInputArea` có tồn tại và hit-test visible. |
| Không hiện option layer | Kiểm tra `bAllowLayerSwitching`, `LayerName` và `GetAvailableMapLayerSpecs`. |
| Chỉ thấy base texture, không thấy tiles | Kiểm tra `GetTileRuntimeStats().State`, `FailureReason`, `TileSet` và tile soft paths. |
| Zoom out vẫn load quá nhiều tile chi tiết | Kiểm tra Panoramic pyramid và `TacticalTileBudget`; tactical LOD dựa trên world-units-per-screen-pixel. |
| Marker không hiện trên full map | Kiểm tra marker config có bật `bShowOnFullMap` và filter marker layer. |
| Overlay không hiện | Kiểm tra overlay layer visibility, category/tag filter và active map layer. |
