#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Data/OBMinimapConfigAsset.h"
#include "Data/OBNavigationTypes.h"
#include "Widget/OBMapMarkerWidget.h"
#include "OBMinimapWidget.generated.h"

class APawn;
class UImage;
class UMaterialInstanceDynamic;
class UOBMapOverlayWidget;
class UOBMinimapConfigAsset;
class UOBNavigationSubsystem;

/**
 * Base map widget that displays a player-centered minimap and manages the shared map-marker
 * projection pipeline.
 *
 * This class owns the full rendering stack — from the dynamic material instance that scrolls
 * the map texture, through marker projection, to the vector overlay paint layer. Subclasses
 * (e.g. UOBTacticalMapWidget) override the virtual "policy" functions to change zoom ranges,
 * rotation behavior, and view-center logic without duplicating the core tick path.
 *
 * Lifecycle
 * ---------
 * 1. Create the widget from a UMG Blueprint whose hierarchy contains the four BindWidget slots
 *    (MapImage, MinimapMarkerCanvas, CompassRingImage, and optionally others).
 * 2. Call InitializeAndStartTracking() with a valid UOBMinimapConfigAsset. The widget creates
 *    a MID from the config's material, subscribes to UOBNavigationSubsystem layer-change
 *    events, and starts ticking.
 * 3. On every visible tick, the widget:
 *    a) Resolves the view-center UV from the tracked pawn,
 *    b) Builds an FOBNavigationMapViewContext and pushes it to the MID,
 *    c) Projects every visible marker into canvas space via OBNavigation::MapView::ProjectUVToCanvas,
 *    d) Feeds overlay elements to the vector overlay child widget.
 * 4. NativeDestruct unbinds the layer-change delegate and tears down child widgets.
 *
 * =========================================================================
 * Visual ASCII Wireframe:
 *
 *  +------------------[Root/Overlay]------------------+
 *  |                                                  |
 *  |  +---------------[MapImage]----------------+     |
 *  |  |                                         |     |
 *  |  |     (Dynamic Material Instance)         |     |
 *  |  |     UV offset / zoom / rotation         |     |
 *  |  |                                         |     |
 *  |  +-----------------------------------------+     |
 *  |                                                  |
 *  |  +---------[MinimapMarkerCanvas]-----------+     |
 *  |  |   (A)           (Player)          (B)   |     |
 *  |  |          [OBMapMarkerWidget ...]        |     |
 *  |  +-----------------------------------------+     |
 *  |                                                  |
 *  |  +----------[CompassRingImage]-------------+     |
 *  |  |              N                          |     |
 *  |  |           W     E                       |     |
 *  |  |              S                          |     |
 *  |  +-----------------------------------------+     |
 *  |                                                  |
 *  |  +----------[OverlayWidget]----------------+     |
 *  |  |   (OBMapOverlayWidget — NativePaint)    |     |
 *  |  +-----------------------------------------+     |
 *  +--------------------------------------------------+
 *
 *  Note: All layers are stacked on top of each other via an
 *  Overlay or CanvasPanel. The MapImage sits at the bottom,
 *  markers and compass ring above, overlay paint on top.
 * =========================================================================
 */
UCLASS()
class OBNAVIGATION_API UOBMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/**
	 * Initializes the widget and starts tracking the active navigation layer.
	 *
	 * Creates a dynamic material instance from the config's background material,
	 * subscribes to UOBNavigationSubsystem::OnNavigationMapLayerSpecChanged,
	 * resolves the player marker ID, and makes the widget visible.
	 * Logs an error and collapses the widget if any critical dependency is missing.
	 *
	 * @param InConfigAsset  The minimap visual configuration. Must be valid; passing nullptr
	 *                       will collapse the widget and abort initialization.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Minimap")
	virtual void InitializeAndStartTracking(UOBMinimapConfigAsset* InConfigAsset);

	/**
	 * Sets the static map rotation offset in degrees.
	 *
	 * This value is baked into the material's "MapRotationOffsetRad" parameter together
	 * with the alignment angle. The compass ring image is counter-rotated so that cardinal
	 * directions remain correct relative to the world.
	 *
	 * @param NewOffsetYaw  Rotation offset to apply (degrees, clockwise positive).
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Minimap")
	void SetMapRotationOffset(float NewOffsetYaw);

	/**
	 * Sets the minimap clipping shape (Circle or Square).
	 *
	 * Drives the material's "ShapeAlpha" scalar parameter:
	 * 1.0 = Circle mask, 0.0 = full Square (no mask).
	 *
	 * @param NewShape  The desired clipping shape.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Minimap")
	void SetMinimapShape(EMinimapShape NewShape);

	/**
	 * Sets the current map zoom multiplier, clamped by the active widget configuration.
	 *
	 * The zoom value is clamped between GetMinimumZoom() and GetMaximumZoom() and then
	 * pushed to the material's "Zoom" parameter. Higher values zoom in (show less area).
	 *
	 * @param NewZoom  Desired zoom multiplier.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Minimap")
	void SetMinimapZoom(float NewZoom);

	/**
	 * Returns the current map zoom multiplier.
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Minimap")
	float GetMinimapZoom() const { return CurrentZoom; }

	/**
	 * Returns the minimap visual configuration assigned at initialization.
	 * May return nullptr if the widget has not been initialized.
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Config")
	UOBMinimapConfigAsset* GetConfig() const { return ConfigAsset; }

protected:

	/** Main per-frame update. Resolves the view center, updates the material,
	 *  projects markers, and feeds overlays. Skipped if the widget is collapsed/hidden
	 *  or initialization has not completed. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Cleanup path: unbinds the layer-change delegate from UOBNavigationSubsystem,
	 *  removes all marker widgets, and destroys the overlay child. */
	virtual void NativeDestruct() override;

	// ------------------------------------------------------------------
	//  Layer-change callback
	// ------------------------------------------------------------------

	/** Called when UOBNavigationSubsystem broadcasts a map-layer change.
	 *  Swaps the map texture on the dynamic material instance and toggles
	 *  MapImage visibility accordingly. */
	UFUNCTION()
	void OnNavigationMapLayerSpecChanged(FOBNavigationMapLayerSpec NewLayerSpec);

	// ------------------------------------------------------------------
	//  Virtual policy functions — override in subclasses to customize behavior
	//  without modifying the core tick pipeline.
	// ------------------------------------------------------------------

	/** Returns which navigation surface this widget represents (Minimap, FullMap, Compass).
	 *  Used by the subsystem to filter which markers are visible on this widget. */
	virtual EOBNavigationSurface GetNavigationSurface() const { return EOBNavigationSurface::Minimap; }

	/** Returns the initial zoom level read from the configuration asset.
	 *  Called once during InitializeAndStartTracking().
	 * @return The initial zoom level.
	 */
	virtual float GetInitialZoom() const;

	/** Returns the minimum allowed zoom (most zoomed-out). Subclasses may read
	 *  from a different config asset (e.g. UOBTacticalMapConfigAsset). 
	 * @return The minimum allowed zoom.
	 */
	virtual float GetMinimumZoom() const;

	/** Returns the maximum allowed zoom (most zoomed-in). 
	 * @return The maximum allowed zoom.
	 */
	virtual float GetMaximumZoom() const;

	/** Per-frame gate that controls whether the map updates this tick.
	 *  Base implementation returns false when the widget is Collapsed or Hidden.
	 * @return True if the map should update this tick, false otherwise.
	 */
	virtual bool ShouldUpdateMapThisFrame() const;

	/** Whether the map texture should rotate dynamically with the tracked pawn.
	 *  When true, the material adds DynamicMapYaw to its rotation; when false,
	 *  only the static offset + alignment angle are applied.
	 * @return True if the map should rotate, false otherwise.
	 */
	virtual bool ShouldRotateMap() const;

	/** Whether the player marker should be pinned to the canvas center.
	 *  Base returns true (standard minimap behavior); the tactical map returns false
	 *  so the player marker floats freely on the pannable canvas.
	 * @return True if the player marker should be centered, false otherwise.
	 */
	virtual bool ShouldCenterPlayerMarker() const;

	/** Whether the player's own marker icon is drawn at all.
	 * @return True if the player marker should be shown, false otherwise.
	 */
	virtual bool ShouldShowPlayerMarker() const;

	/** Scale factor applied to all marker sizes on this widget surface.
	 *  The tactical map uses this to allow designers to adjust marker density.
	 * @return The marker scale.
	 */
	virtual float GetMarkerScale() const;

	/**
	 * Computes the normalized UV coordinate that the map view should be centered on.
	 *
	 * The base implementation projects the tracked pawn's world location into the current
	 * layer's UV space via UOBNavigationSubsystem::WorldToMapUVChecked().
	 *
	 * @param CurrentLayer  The active map layer specification.
	 * @param TrackedPawn   The locally-controlled pawn. May be nullptr.
	 * @param OutViewCenterUV  [out] The computed view center in [0,1] UV space.
	 * @return True if projection succeeded and the view center is valid.
	 */
	virtual bool ResolveViewCenterUV(const FOBNavigationMapLayerSpec& CurrentLayer, const APawn* TrackedPawn,
	                                 FVector2D& OutViewCenterUV) const;

	/**
	 * Hook called after the FOBNavigationMapViewContext has been built for this frame
	 * but before markers and overlays are updated. Subclasses use this to cache the
	 * resolved view center (e.g. for follow-mode tracking in the tactical map).
	 *
	 * @param ViewContext   The fully populated view context for this frame.
	 * @param CurrentLayer  The active map layer specification.
	 * @param TrackedPawn   The locally-controlled pawn. May be nullptr.
	 */
	virtual void OnViewContextUpdated(const FOBNavigationMapViewContext& ViewContext,
	                                  const FOBNavigationMapLayerSpec& CurrentLayer, const APawn* TrackedPawn);

	// ------------------------------------------------------------------
	//  Bound widgets — must exist in the UMG Blueprint hierarchy
	// ------------------------------------------------------------------

	/** The UImage that displays the scrollable/zoomable map via a dynamic material instance. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> MapImage;

	/** Canvas panel that hosts all UOBMapMarkerWidget children and the overlay widget.
	 *  Marker positions are set as absolute CanvasPanelSlot offsets. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> MinimapMarkerCanvas;

	/** The Blueprint widget class instantiated for each visible map marker.
	 *  Must be a subclass of UOBMapMarkerWidget. */
	UPROPERTY(EditAnywhere, Category = "Config")
	TSubclassOf<UOBMapMarkerWidget> MarkerWidgetClass;

	/** The UImage that shows the compass ring (N/E/S/W). Counter-rotated to match
	 *  the static rotation offset so that cardinal labels stay world-aligned. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> CompassRingImage;

	// ------------------------------------------------------------------
	//  Internal helpers
	// ------------------------------------------------------------------

	/** Returns the static alignment angle (in degrees) derived from the config's
	 *  EMapAlignment setting. This corrects which world axis maps to "up" on the texture. */
	float GetAlignmentAngle() const;

	/** Returns the dynamic yaw from the tracked pawn, accounting for the config's
	 *  EMinimapRotationSource (ControlRotation vs. ActorRotation).
	 *  Returns 0.0 if rotation is disabled or the pawn is null. */
	virtual float GetDynamicMapYaw(const APawn* TrackedPawn) const;

	/** Pushes the fully resolved FOBNavigationMapViewContext into the dynamic material instance,
	 *  updating PlayerPositionUV, ViewCenterUV, PlayerYaw, Zoom, and MapRotationOffsetRad. */
	void UpdateMapMaterial(const FOBNavigationMapViewContext& ViewContext);

	/** Iterates all visible markers from UOBNavigationSubsystem, projects them into canvas
	 *  space, creates / updates UOBMapMarkerWidget children, and records handled IDs so that
	 *  stale markers can be removed by the caller. */
	void UpdateMapMarkers(const APawn* TrackedPawn, const FOBNavigationMapLayerSpec& CurrentLayer,
	                      const FOBNavigationMapViewContext& ViewContext, TSet<FGuid>& OutHandledMarkerIDs);

	/** Feeds the current layer's overlay elements and the view context into the child
	 *  UOBMapOverlayWidget so that vector shapes (zones, paths, freehand) are painted. */
	void UpdateMapOverlays(const FOBNavigationMapLayerSpec& CurrentLayer,
	                       const FOBNavigationMapViewContext& ViewContext);

	/** Lazily creates the UOBMapOverlayWidget child and adds it to MinimapMarkerCanvas
	 *  with full-stretch anchors at ZOrder 0 (below markers). */
	void EnsureOverlayWidget();

	/** Removes all marker widget children from the canvas and empties the tracking map. */
	void ClearMarkerWidgets();

	// ------------------------------------------------------------------
	//  Transient state — not serialized, rebuilt at runtime
	// ------------------------------------------------------------------

	/** Cached reference to the game-instance-level navigation subsystem.
	 *  Resolved once during InitializeAndStartTracking(). */
	UPROPERTY(Transient)
	TObjectPtr<UOBNavigationSubsystem> NavSubsystem;

	/** Dynamic material instance created from UOBMinimapConfigAsset::MinimapBackgroundMaterial.
	 *  Scalar and vector parameters are updated every tick to scroll, zoom, and rotate the map. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MinimapMaterialInstance;

	/** Live map of MarkerID → UOBMapMarkerWidget for all markers currently rendered on this
	 *  widget surface. Entries are added on first sight and removed when the marker leaves
	 *  the visible set. */
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UOBMapMarkerWidget>> ActiveMinimapMarkerWidgets;

	/** The lazily-created vector overlay child widget used to paint zones, paths, and freehand
	 *  elements on top of the map. Created by EnsureOverlayWidget(). */
	UPROPERTY(Transient)
	TObjectPtr<UOBMapOverlayWidget> OverlayWidget = nullptr;

	/** The active configuration asset governing zoom limits, rotation source, shape, and debug flags. */
	UPROPERTY(Transient)
	TObjectPtr<UOBMinimapConfigAsset> ConfigAsset;

	/** True after InitializeAndStartTracking() succeeds. Guards the tick path. */
	bool bIsInitializedAndTracking = false;

	/** Current static rotation offset (degrees) applied to the map material and compass ring. */
	float CurrentMapRotationOffset = 0.0f;

	/** Active clipping shape (Circle or Square) pushed to the material's ShapeAlpha parameter. */
	EMinimapShape CurrentMinimapShape = EMinimapShape::Square;

	/** Current zoom multiplier, clamped by GetMinimumZoom()/GetMaximumZoom(). Pushed to the
	 *  material's "Zoom" parameter every tick. Higher values zoom in (show less area). */
	float CurrentZoom = 1.0f;

	/** GUID of the player's own map marker. Used to identify the player marker in the visible
	 *  set so it can be centered (on minimap) or styled differently. */
	FGuid PlayerMarkerID;
};
