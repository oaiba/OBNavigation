#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Data/OBMapTileManager.h"
#include "Data/OBMinimapConfigAsset.h"
#include "Data/OBNavigationTypes.h"
#include "Widget/OBMapMarkerWidget.h"
#include "OBMapWidgetBase.generated.h"

class APawn;
class UMinimapDefinitionDataAsset;
class UImage;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UOBMapMarker;
class UOBMapOverlayWidget;
class UOBMinimapConfigAsset;
class UOBNavigationSubsystem;
class UTextBlock;
class UTexture2D;

/**
 * Abstract base class that owns the shared map rendering pipeline: dynamic material
 * scrolling, map marker projection/pooling, and vector overlay painting.
 *
 * Subclasses (UOBMinimapWidget, UOBTacticalMapWidget) override virtual "policy"
 * functions to control zoom ranges, rotation behavior, view-center logic, and
 * marker/overlay filtering without modifying the core tick path.
 *
 * =========================================================================
 * Inheritance tree:
 *
 *   UUserWidget
 *     └─ UOBMapWidgetBase          (this class — abstract)
 *          ├─ UOBMinimapWidget      (player-centered, rotating, shape-masked)
 *          └─ UOBTacticalMapWidget  (free-pan, north-up, filterable)
 *
 * =========================================================================
 * Visual ASCII Wireframe  (shared BindWidget layout):
 *
 *  +──────────────────[Root / Overlay]──────────────────+
 *  │                                                    │
 *  │  ┌──────────────[MapImage]──────────────┐          │
 *  │  │                                      │          │
 *  │  │   Dynamic Material Instance (MID)    │          │
 *  │  │   · ViewCenterUV → UV scroll         │          │
 *  │  │   · Zoom         → UV scale          │          │
 *  │  │   · PlayerYaw    → rotation          │          │
 *  │  │   · MapTexture   → layer texture     │          │
 *  │  │                                      │          │
 *  │  └──────────────────────────────────────┘          │
 *  │                                                    │
 *  │  ┌──────────[MapMarkerCanvas]───────────┐          │
 *  │  │                                      │          │
 *  │  │   ┌──[OverlayWidget]──────────┐      │          │
 *  │  │   │  (OBMapOverlayWidget)     │ Z:0  │          │
 *  │  │   │  zones / paths / freehand │      │          │
 *  │  │   └───────────────────────────┘      │          │
 *  │  │                                      │          │
 *  │  │   (A)  [MarkerWidget]   Z:1          │          │
 *  │  │   (B)  [MarkerWidget]   Z:1          │          │
 *  │  │   (☆)  [PlayerMarker]   Z:10         │          │
 *  │  │                                      │          │
 *  │  └──────────────────────────────────────┘          │
 *  │                                                    │
 *  │  (Subclasses may add additional BindWidget slots   │
 *  │   such as CompassRingImage in UOBMinimapWidget)    │
 *  │                                                    │
 *  +────────────────────────────────────────────────────+
 *
 * =========================================================================
 * Per-frame tick pipeline  (NativeTick):
 *
 *  1. ResolveActiveLayer()       → FOBNavigationMapLayerSpec
 *  2. ResolveViewCenterUV()      → FVector2D (center UV)
 *  3. BuildViewContext()         → FOBNavigationMapViewContext
 *  4. OnViewContextUpdated()     → subclass hook
 *  5. UpdateMapMaterial()        → push MID parameters
 *  6. UpdateMapMarkers()         → project + pool marker widgets
 *  7. UpdateMapOverlays()        → feed overlay elements
 *  8. Prune stale markers        → remove widgets not in visible set
 *
 * =========================================================================
 */
UCLASS(Abstract)
class OBNAVIGATION_API UOBMapWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Tiles")
	FOBMapTileRuntimeStats GetTileRuntimeStats() const;

protected:
	/**
	 * Initializes shared map rendering and starts tracking navigation data.
	 *
	 * @param InVisualConfigAsset Visual map configuration that provides the background material and map alignment.
	 */
	void InitializeMapWidget(UOBMinimapConfigAsset* InVisualConfigAsset);

	/**
	 * Sets the current map zoom multiplier, clamped by GetMinimumZoom() and GetMaximumZoom().
	 * 
	 * @param NewZoom The desired zoom multiplier.
	 */
	void SetMapZoom(float NewZoom);

	/**
	 * Returns the current map zoom multiplier.
	 * 
	 * @return The current zoom multiplier applied to the map.
	 */
	float GetMapZoom() const { return CurrentZoom; }

	/**
	 * Returns the visual map configuration assigned at initialization.
	 * 
	 * @return The visual configuration asset used by this map widget.
	 */
	UOBMinimapConfigAsset* GetVisualConfig() const { return VisualConfigAsset; }

	/**
	 * Returns the canvas used for markers and vector overlays.
	 * 
	 * @return The canvas panel holding map markers and overlays.
	 */
	UCanvasPanel* GetMarkerCanvas() const;

	/**
	 * Main per-frame update for material state, markers, and overlays.
	 * 
	 * @param MyGeometry The geometry of the widget.
	 * @param InDeltaTime The time passed since the last tick.
	 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Unbinds navigation delegates and removes pooled child widgets. */
	virtual void NativeDestruct() override;

	/**
	 * Applies a changed map layer texture to the dynamic material.
	 * 
	 * @param NewLayerSpec The new layer specification to apply.
	 */
	UFUNCTION()
	void OnNavigationMapLayerSpecChanged(FOBNavigationMapLayerSpec NewLayerSpec);

	/**
	 * Returns which navigation surface this widget renders.
	 * 
	 * @return The navigation surface identifier for filtering markers.
	 */
	virtual EOBNavigationSurface GetNavigationSurface() const PURE_VIRTUAL(UOBMapWidgetBase::GetNavigationSurface, return EOBNavigationSurface::Minimap;);

	/**
	 * Resolves the active layer for this widget.
	 * 
	 * @param OutLayerSpec The populated layer specification if successful.
	 * @return True if a valid active layer was resolved, false otherwise.
	 */
	virtual bool ResolveActiveLayer(FOBNavigationMapLayerSpec& OutLayerSpec) const;

	/**
	 * Returns the initial zoom level for this widget.
	 * 
	 * @return The default zoom multiplier to use upon initialization.
	 */
	virtual float GetInitialZoom() const;

	/**
	 * Returns the minimum allowed zoom multiplier.
	 * 
	 * @return The minimum zoom boundary.
	 */
	virtual float GetMinimumZoom() const;

	/**
	 * Returns the maximum allowed zoom multiplier.
	 * 
	 * @return The maximum zoom boundary.
	 */
	virtual float GetMaximumZoom() const;

	/**
	 * Returns true when the widget should update this frame.
	 * 
	 * @return True if the map rendering logic should tick, otherwise false.
	 */
	virtual bool ShouldUpdateMapThisFrame() const;

	/**
	 * Returns true when the map texture should rotate dynamically with the tracked pawn.
	 * 
	 * @return True if dynamic rotation is enabled.
	 */
	virtual bool ShouldRotateMap() const;

	/**
	 * Returns true when the player's own marker should be pinned to the canvas center.
	 * 
	 * @return True if the player marker should stay at the center of the widget canvas.
	 */
	virtual bool ShouldCenterPlayerMarker() const;

	/**
	 * Returns true when the player's own marker should be drawn.
	 * 
	 * @return True if the player marker should be visible.
	 */
	virtual bool ShouldShowPlayerMarker() const;

	/**
	 * Returns true when a visible marker passes widget-specific filters.
	 * 
	 * @param Marker The map marker to evaluate.
	 * @return True if the marker should be drawn, false if filtered out.
	 */
	virtual bool ShouldShowMarker(const UOBMapMarker* Marker) const;

	/**
	 * Returns the marker size multiplier for this widget surface.
	 * 
	 * @return The uniform scale applied to marker icons.
	 */
	virtual float GetMarkerScale() const;

	/**
	 * Returns the overlay category filter for this widget.
	 * 
	 * @return The category name to filter by, or NAME_None to ignore the filter.
	 */
	virtual FName GetOverlayCategoryFilter() const;

	/**
	 * Returns the overlay tag filter for this widget.
	 * 
	 * @return The tag name to filter by, or NAME_None to ignore the filter.
	 */
	virtual FName GetOverlayTagFilter() const;

	/**
	 * Resolves the normalized UV coordinate that should sit at the center of the map view.
	 * 
	 * @param CurrentLayer The active navigation map layer.
	 * @param TrackedPawn The tracked player pawn.
	 * @param OutViewCenterUV The resulting UV coordinate (in [0, 1] range).
	 * @return True if a valid UV was resolved.
	 */
	virtual bool ResolveViewCenterUV(const FOBNavigationMapLayerSpec& CurrentLayer, const APawn* TrackedPawn,
	                                 FVector2D& OutViewCenterUV) const;

	/**
	 * Builds projection state from the resolved layer, center, and tracked pawn.
	 * 
	 * @param CurrentLayer The active navigation map layer.
	 * @param TrackedPawn The tracked player pawn.
	 * @param ViewCenterUV The target view center in UV space.
	 * @return A constructed view context utilized for projecting coordinates onto the canvas.
	 */
	virtual FOBNavigationMapViewContext BuildViewContext(const FOBNavigationMapLayerSpec& CurrentLayer,
	                                                     const APawn* TrackedPawn,
	                                                     const FVector2D& ViewCenterUV) const;

	/**
	 * Called after the view context is built and before marker/overlay updates.
	 * 
	 * @param ViewContext The newly created map view context.
	 * @param CurrentLayer The active navigation map layer.
	 * @param TrackedPawn The tracked player pawn.
	 */
	virtual void OnViewContextUpdated(const FOBNavigationMapViewContext& ViewContext,
	                                  const FOBNavigationMapLayerSpec& CurrentLayer, const APawn* TrackedPawn);

	/**
	 * Returns the static alignment angle derived from the visual config.
	 * 
	 * @return The static alignment angle in degrees.
	 */
	float GetAlignmentAngle() const;

	/**
	 * Returns the dynamic yaw from the tracked pawn.
	 * 
	 * @param TrackedPawn The pawn to extract yaw from.
	 * @return The dynamic rotation yaw in degrees.
	 */
	virtual float GetDynamicMapYaw(const APawn* TrackedPawn) const;

	/**
	 * Returns the static rotation applied to the map texture.
	 * 
	 * @return The total static rotation in degrees.
	 */
	virtual float GetTotalStaticRotation() const;

	/**
	 * Pushes the resolved view context into the dynamic material instance.
	 * 
	 * @param ViewContext The context providing the UV, zoom, and yaw properties.
	 */
	void UpdateMapMaterial(const FOBNavigationMapViewContext& ViewContext);

	/**
	 * Updates tiled map images when the active layer is backed by a Panoramic tile set.
	 *
	 * @param CurrentLayer The active tiled layer.
	 * @param ViewContext The resolved view state for this frame.
	 */
	void UpdateMapTiles(const FOBNavigationMapLayerSpec& CurrentLayer,
	                    const FOBNavigationMapViewContext& ViewContext);

	virtual int32 GetTileBudget() const;
	virtual int32 GetMinimapMaxLODTileLimit() const;
	virtual UMaterialInterface* GetTiledMapTileMaterial() const;
	virtual bool ShouldMaskTiledMapTiles() const;

	/**
	 * Projects visible markers into canvas space and updates marker widget instances.
	 * 
	 * @param TrackedPawn The tracked player pawn.
	 * @param CurrentLayer The active map layer context.
	 * @param ViewContext The map view context for projections.
	 * @param OutHandledMarkerIDs Tracking structure for pooling and discarding out-of-view widgets.
	 */
	void UpdateMapMarkers(const APawn* TrackedPawn, const FOBNavigationMapLayerSpec& CurrentLayer,
	                      const FOBNavigationMapViewContext& ViewContext, TSet<FGuid>& OutHandledMarkerIDs);

	/**
	 * Feeds filtered overlay elements to the overlay paint widget.
	 * 
	 * @param CurrentLayer The active map layer context containing the overlays.
	 * @param ViewContext The map view context to inform how the overlays scale/project.
	 */
	void UpdateMapOverlays(const FOBNavigationMapLayerSpec& CurrentLayer,
	                       const FOBNavigationMapViewContext& ViewContext);

	/** Lazily creates the overlay paint widget as a child of the marker canvas. */
	void EnsureOverlayWidget();

	/** Removes all marker widget children and clears the active marker pool. */
	void ClearMarkerWidgets();

	/** Removes all tile image widgets and releases streamed tile state. */
	void ClearTileWidgets();

	/** Ensures a clipped canvas exists below overlays and markers for tiled rendering. */
	UCanvasPanel* EnsureTileLayerCanvas();

	/**
	 * Applies the layer texture when the layer meaningfully changes.
	 * 
	 * @param NewLayerSpec The new navigation map layer to apply visually.
	 */
	void ApplyMapLayer(const FOBNavigationMapLayerSpec& NewLayerSpec);

	/** The UImage that displays the scrollable, zoomable map material. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> MapImage;

	/** Canvas panel for marker widgets and vector overlays. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> MapMarkerCanvas;

	/** Widget class instantiated for each visible map marker. */
	UPROPERTY(EditAnywhere, Category = "Config")
	TSubclassOf<UOBMapMarkerWidget> MarkerWidgetClass;

	/** Cached navigation subsystem reference. */
	UPROPERTY(Transient)
	TObjectPtr<UOBNavigationSubsystem> NavSubsystem;

	/** Dynamic material instance created from the visual config background material. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MapMaterialInstance;

	/** Active marker widget pool keyed by marker ID. */
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UOBMapMarkerWidget>> ActiveMapMarkerWidgets;

	/** Vector overlay child widget used to paint zones, paths, and freehand elements. */
	UPROPERTY(Transient)
	TObjectPtr<UOBMapOverlayWidget> OverlayWidget = nullptr;

	/** Runtime tile streaming and active-tile query helper for Panoramic tile-set layers. */
	UPROPERTY(Transient)
	TObjectPtr<UOBMapTileManager> TileManager = nullptr;

	/** Active tile image widgets keyed by LOD/X/Y. */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UImage>> ActiveTileImages;

	/** Per-tile dynamic material instances used when tiled masking is configured. */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> ActiveTileMaterialInstances;

	/** Debug coordinate labels keyed by LOD/X/Y. Created only when debug messages are enabled. */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTextBlock>> ActiveTileCoordinateLabels;

	/** Clipped canvas that contains tiled map images below overlays and markers. */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> TileLayerCanvas = nullptr;

	/** Visual configuration shared by minimap and tactical map widgets. */
	UPROPERTY(Transient)
	TObjectPtr<UOBMinimapConfigAsset> VisualConfigAsset;

	/** True after InitializeMapWidget() succeeds. */
	bool bIsInitializedAndTracking = false;

	/** Current zoom multiplier. Higher values zoom in. */
	float CurrentZoom = 1.0f;

	/** GUID of the tracked player's own map marker. */
	FGuid PlayerMarkerID;

	/** Last layer name applied to the map material. */
	FName AppliedLayerName = NAME_None;

	/** Last texture applied to the map material. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> AppliedMapTexture = nullptr;

	/** Last Panoramic definition applied to either render path. */
	UPROPERTY(Transient)
	TSoftObjectPtr<UMinimapDefinitionDataAsset> AppliedPanoramicDefinition;

	/** True when the current layer is rendered through tile widgets. */
	bool bAppliedTiledLayer = false;

	/** Prevents repeated circle-mask material warnings. */
	bool bWarnedMissingTiledMapTileMaterial = false;

	/** Last tile manager state logged for this layer. */
	EOBMapTileManagerState LastLoggedTileManagerState = EOBMapTileManagerState::Uninitialized;

	/** Last active LOD whose tactical tile placements were logged. */
	int32 LastLoggedTacticalTileLOD = INDEX_NONE;
};
