#pragma once

#include "CoreMinimal.h"
#include "Widget/OBMinimapWidget.h"
#include "OBTacticalMapWidget.generated.h"

class UOBTacticalMapConfigAsset;

/**
 * Full-screen tactical map widget with independent zoom, pan, and view-center state.
 *
 * Extends UOBMinimapWidget to provide a pannable/zoomable full-screen map experience.
 * Unlike the minimap — which always centers on the tracked pawn — the tactical map
 * maintains its own ViewCenterUV that can be freely moved via mouse drag (AddPanInput),
 * gamepad stick (SetPanInput + continuous apply), or Blueprint API (SetViewCenterUV /
 * SetViewCenterWorldLocation). A "follow mode" toggle (bIsFollowingTrackedPlayer) lets
 * the view snap back to the player when RecenterOnTrackedPlayer() is called.
 *
 * Override Summary
 * ----------------
 * | Base virtual              | Tactical override                                       |
 * |---------------------------|---------------------------------------------------------|
 * | GetNavigationSurface()    | Returns EOBNavigationSurface::FullMap                   |
 * | GetInitialZoom()          | Reads from UOBTacticalMapConfigAsset::InitialZoom       |
 * | GetMinimumZoom()          | Reads from UOBTacticalMapConfigAsset::MinZoom           |
 * | GetMaximumZoom()          | Reads from UOBTacticalMapConfigAsset::MaxZoom           |
 * | ShouldRotateMap()         | Reads from UOBTacticalMapConfigAsset::bRotateWithPlayer |
 * | ShouldCenterPlayerMarker()| Returns false — player floats freely on the canvas      |
 * | ShouldShowPlayerMarker()  | Reads from UOBTacticalMapConfigAsset::bShowPlayerMarker |
 * | GetMarkerScale()          | Reads from UOBTacticalMapConfigAsset::MarkerScale       |
 * | ResolveViewCenterUV()     | Uses player UV in follow mode, else cached ViewCenterUV |
 * | OnViewContextUpdated()    | Caches resolved UV back into ViewCenterUV when following|
 *
 * Initialization
 * ---------------
 * Call InitializeTacticalMapAndStartTracking() instead of the base InitializeAndStartTracking().
 * This sets up the tactical-specific config, resets pan/zoom state, calls through to the base
 * initialization, and optionally hides the compass ring.
 *
 * =========================================================================
 * Visual ASCII Wireframe:
 *
 *  (Inherits all bound widgets from UOBMinimapWidget)
 *
 *  +------------------[Root/Overlay]------------------+
 *  |                                                  |
 *  |  +---------------[MapImage]----------------+     |
 *  |  |                                         |     |
 *  |  |     (Pannable & Zoomable)               |     |
 *  |  |     ViewCenterUV drives scroll          |     |
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
 *  Note: Inherits the full minimap layout. Adds runtime
 *  pan/zoom state (ViewCenterUV, PanInput) and a follow-mode
 *  toggle. Typically occupies the full screen.
 * =========================================================================
 */
UCLASS()
class OBNAVIGATION_API UOBTacticalMapWidget : public UOBMinimapWidget
{
	GENERATED_BODY()

public:

	/**
	 * Initializes the tactical map using minimap visual assets and tactical-map view settings.
	 *
	 * Stores the tactical config, resets ViewCenterUV / PanInput / follow-mode, calls the
	 * base InitializeAndStartTracking(), toggles the compass ring visibility based on the
	 * tactical config, and re-centers on the tracked player.
	 *
	 * @param InMinimapConfigAsset   Shared visual config (material, compass texture, etc.).
	 * @param InTacticalConfigAsset  Tactical-specific settings (zoom range, pan speed, etc.).
	 *                               Must be valid; passing nullptr aborts initialization.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void InitializeTacticalMapAndStartTracking(UOBMinimapConfigAsset* InMinimapConfigAsset,
	                                           UOBTacticalMapConfigAsset* InTacticalConfigAsset);

	/**
	 * Applies a discrete zoom step. Positive values zoom in, negative values zoom out.
	 *
	 * The delta is multiplied by UOBTacticalMapConfigAsset::ZoomStep before being added
	 * to the current zoom level.
	 *
	 * @param ZoomDelta  Signed zoom delta (typically +1 / -1 from scroll wheel).
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void AddZoomInput(float ZoomDelta);

	/**
	 * Applies a one-shot screen-space pan delta in Slate units (pixels).
	 *
	 * The delta is un-rotated by the current map rotation, converted to UV space using
	 * the canvas size and current zoom, and subtracted from ViewCenterUV. Automatically
	 * disables follow mode.
	 *
	 * @param PanDelta  Screen-space offset in pixels (e.g. mouse drag delta).
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void AddPanInput(FVector2D PanDelta);

	/**
	 * Sets normalized continuous pan input applied every tick via ApplyContinuousPanInput().
	 *
	 * Use this for gamepad stick or keyboard-driven panning. The input vector is multiplied
	 * by UOBTacticalMapConfigAsset::PanSpeed and DeltaTime each frame.
	 *
	 * @param InPanInput  Normalized direction vector (typically in [-1, 1] per axis).
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void SetPanInput(FVector2D InPanInput);

	/**
	 * Returns the current continuous pan input vector.
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|TacticalMap")
	FVector2D GetPanInput() const { return PanInput; }

	/**
	 * Sets the tactical map center from a world location on the active map layer.
	 *
	 * Projects the world location to UV space via UOBNavigationSubsystem::WorldToMapUVChecked()
	 * and disables follow mode. No-op if the subsystem is unavailable or the projection fails.
	 *
	 * @param WorldLocation  The 3D world position to center on.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void SetViewCenterWorldLocation(FVector WorldLocation);

	/**
	 * Sets the tactical map center as normalized UV coordinates.
	 *
	 * Directly sets ViewCenterUV and disables follow mode. The UV is optionally clamped
	 * to [0, 1] based on UOBTacticalMapConfigAsset::bClampViewToMapBounds.
	 *
	 * @param MapUV  Normalized map coordinates, where (0,0) is the top-left and (1,1) is
	 *               the bottom-right of the map texture.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void SetViewCenterUV(FVector2D MapUV);

	/**
	 * Recenters the tactical map on the tracked player and resumes follow mode.
	 *
	 * Projects the tracked pawn's world location to UV space and stores it as the new
	 * ViewCenterUV. If the pawn or subsystem is unavailable, follow mode is still re-enabled
	 * with the current ViewCenterUV so that the view will snap once the pawn becomes available.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void RecenterOnTrackedPlayer();

	/**
	 * Returns the current tactical map center in normalized UV coordinates.
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|TacticalMap")
	FVector2D GetViewCenterUV() const { return ViewCenterUV; }

	/**
	 * Returns the current tactical map zoom multiplier.
	 * Convenience wrapper around the base GetMinimapZoom().
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|TacticalMap")
	float GetTacticalMapZoom() const { return GetMinimapZoom(); }

	/**
	 * Returns true while the tactical map view center follows the tracked player.
	 * Follow mode is enabled by RecenterOnTrackedPlayer() and disabled by any
	 * explicit pan or SetViewCenter call.
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|TacticalMap")
	bool IsFollowingTrackedPlayer() const { return bIsFollowingTrackedPlayer; }

protected:

	/** Applies continuous pan input before delegating to the base NativeTick which
	 *  handles material updates, marker projection, and overlay painting. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ------------------------------------------------------------------
	//  Policy overrides — customize base minimap behavior for full-screen use
	// ------------------------------------------------------------------

	/** Reports this widget as FullMap so that UOBNavigationSubsystem returns the
	 *  correct marker visibility set for full-screen display. */
	virtual EOBNavigationSurface GetNavigationSurface() const override { return EOBNavigationSurface::FullMap; }

	/** @return UOBTacticalMapConfigAsset::InitialZoom, falling back to the base if unavailable. */
	virtual float GetInitialZoom() const override;

	/** @return UOBTacticalMapConfigAsset::MinZoom, falling back to the base if unavailable. */
	virtual float GetMinimumZoom() const override;

	/** @return UOBTacticalMapConfigAsset::MaxZoom, falling back to the base if unavailable. */
	virtual float GetMaximumZoom() const override;

	/** Returns UOBTacticalMapConfigAsset::bRotateWithPlayer.
	 *  When true, the map texture rotates with the tracked pawn's yaw.
	 * @return True if the map should rotate, false otherwise.
	 */
	virtual bool ShouldRotateMap() const override;

	/** Always returns false — the player marker floats freely on the pannable canvas
	 *  rather than being pinned to the center as on the minimap.
	 * @return True if the player marker should be centered, false otherwise.
	 */
	virtual bool ShouldCenterPlayerMarker() const override;

	/** Returns UOBTacticalMapConfigAsset::bShowPlayerMarker.
	 *  Designers can toggle player marker visibility independently of the minimap.
	 * @return True if the player marker should be shown, false otherwise.
	 */
	virtual bool ShouldShowPlayerMarker() const override;

	/** Returns UOBTacticalMapConfigAsset::MarkerScale (clamped >= 0.01).
	 *  Allows designers to adjust marker icon density on the full-screen map.
	 * @return The marker scale.
	 */
	virtual float GetMarkerScale() const override;

	/**
	 * In follow mode, projects the tracked pawn's world position to UV and outputs it.
	 * Otherwise, outputs the cached ViewCenterUV directly (always returns true).
	 *
	 * @param CurrentLayer     The active map layer specification.
	 * @param TrackedPawn      The locally-controlled pawn. May be nullptr.
	 * @param OutViewCenterUV  [out] The computed view center in [0,1] UV space.
	 * @return Always true — the tactical map can display at any ViewCenterUV.
	 */
	virtual bool ResolveViewCenterUV(const FOBNavigationMapLayerSpec& CurrentLayer, const APawn* TrackedPawn,
	                                 FVector2D& OutViewCenterUV) const override;

	/**
	 * Caches the resolved ViewCenterUV back into this widget's state when following
	 * the tracked player, so that switching to manual pan starts from the player's
	 * last known position.
	 * @param ViewContext The current view context.
	 * @param CurrentLayer The current map layer.
	 * @param TrackedPawn The tracked pawn.
	 */
	virtual void OnViewContextUpdated(const FOBNavigationMapViewContext& ViewContext,
	                                  const FOBNavigationMapLayerSpec& CurrentLayer, const APawn* TrackedPawn) override;

private:
	// ------------------------------------------------------------------
	//  Internal helpers
	// ------------------------------------------------------------------

	/** Applies continuous pan input (PanInput * PanSpeed * DeltaTime) each frame.
	 *  Early-outs if the widget is not initialized, not visible, or PanInput is near zero.
	 * @param DeltaTime The delta time.
	 */
	void ApplyContinuousPanInput(float DeltaTime);

	/** Internal setter for ViewCenterUV. Optionally clamps to [0,1] based on
	 *  UOBTacticalMapConfigAsset::bClampViewToMapBounds and updates the follow flag.
	 * @param MapUV The map UV.
	 * @param bFollowTrackedPlayer True if the view should follow the tracked player, false otherwise.
	 */
	void SetViewCenterUVInternal(FVector2D MapUV, bool bFollowTrackedPlayer);

	// ------------------------------------------------------------------
	//  State — not serialized, rebuilt at runtime
	// ------------------------------------------------------------------

	/** Configuration asset governing zoom range, pan speed, follow behavior, and visibility
	 *  toggles for the tactical map. Set during InitializeTacticalMapAndStartTracking(). */
	UPROPERTY(Transient)
	TObjectPtr<UOBTacticalMapConfigAsset> TacticalConfigAsset;

	/** Current map view center in normalized UV space [0,1].
	 *  Updated every frame in follow mode, or on explicit pan/set calls. */
	FVector2D ViewCenterUV = FVector2D(0.5f, 0.5f);

	/** Continuous pan direction input (typically from gamepad stick).
	 *  Multiplied by PanSpeed and DeltaTime in ApplyContinuousPanInput(). */
	FVector2D PanInput = FVector2D::ZeroVector;

	/** When true, ViewCenterUV tracks the player pawn's projected UV each frame.
	 *  Disabled by any explicit pan or SetViewCenter call; re-enabled by RecenterOnTrackedPlayer(). */
	bool bIsFollowingTrackedPlayer = true;
};
