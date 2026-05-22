#pragma once

#include "CoreMinimal.h"
#include "Widget/OBMapWidgetBase.h"
#include "OBMinimapWidget.generated.h"

class APawn;
class UImage;
class UOBMinimapConfigAsset;

/**
 * Player-centered minimap widget with optional player-facing rotation,
 * compass ring, and shape mask (circle / square).
 *
 * Extends UOBMapWidgetBase with minimap-specific behavior: the view center
 * always tracks the player pawn, the map texture can rotate to match the
 * pawn's facing, and a material-driven shape mask clips the image to a
 * circle or square.
 *
 * =========================================================================
 * Visual ASCII Wireframe:
 *
 *  +──────────────[Root / Overlay]──────────────+
 *  │                                            │
 *  │  ┌────────────[MapImage]──────────┐        │
 *  │  │    ╭ ─ ─ ─ ─ ─ ─ ─ ─ ╮         │        │
 *  │  │   ╱  Dynamic Material  ╲       │        │
 *  │  │  │   UV scroll + zoom   │      │        │
 *  │  │  │   ShapeAlpha mask    │      │        │
 *  │  │  │   PlayerYaw rotate   │      │        │
 *  │  │   ╲                    ╱       │        │ 
 *  │  │    ╰ ─ ─ ─ ─ ─ ─ ─ ─ ╯         │        │
 *  │  │   (circle or square clip)      │        │
 *  │  └────────────────────────────────┘        │
 *  │                                            │
 *  │  ┌──────[MapMarkerCanvas]─────────┐        │
 *  │  │  [OverlayWidget]         Z:0   │        │
 *  │  │  (A) [Marker]            Z:1   │        │
 *  │  │  (☆) [Player — centered] Z:10  │        │
 *  │  └────────────────────────────────┘        │
 *  │                                            │
 *  │  ┌──────[CompassRingImage]────────┐        │
 *  │  │            N                   │        │
 *  │  │         W     E                │        │
 *  │  │            S                   │        │
 *  │  │  (counter-rotated to stay      │        │
 *  │  │   world-aligned)               │        │
 *  │  └────────────────────────────────┘        │
 *  │                                            │
 *  +────────────────────────────────────────────+
 *
 * =========================================================================
 */
UCLASS()
class OBNAVIGATION_API UOBMinimapWidget : public UOBMapWidgetBase
{
	GENERATED_BODY()

public:
	/**
	 * Initializes the minimap and starts tracking the active navigation layer.
	 *
	 * @param InConfigAsset Minimap visual and behavior configuration.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Minimap")
	virtual void InitializeAndStartTracking(UOBMinimapConfigAsset* InConfigAsset);

	/**
	 * Sets the static map rotation offset in degrees.
	 *
	 * @param NewOffsetYaw Rotation offset in degrees.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Minimap")
	void SetMapRotationOffset(float NewOffsetYaw);

	/**
	 * Sets the minimap clipping shape.
	 *
	 * @param NewShape Circle or square shape mask.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Minimap")
	void SetMinimapShape(EMinimapShape NewShape);

	/**
	 * Sets the current minimap zoom multiplier.
	 *
	 * @param NewZoom Desired zoom multiplier.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Minimap")
	void SetMinimapZoom(float NewZoom);

	/** Returns the current minimap zoom multiplier. */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Minimap")
	float GetMinimapZoom() const { return GetMapZoom(); }

	/** Returns the minimap configuration assigned at initialization. */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Config")
	UOBMinimapConfigAsset* GetConfig() const { return GetVisualConfig(); }

protected:
	/**
	 * Returns the minimap navigation surface.
	 * 
	 * @return The navigation surface identifier for the minimap.
	 */
	virtual EOBNavigationSurface GetNavigationSurface() const override { return EOBNavigationSurface::Minimap; }

	/**
	 * Returns true when the minimap should rotate with the tracked pawn.
	 * 
	 * @return True if dynamic rotation is enabled.
	 */
	virtual bool ShouldRotateMap() const override;

	/** Returns the minimap shape used by clamped edge indicators. */
	virtual EOBMapViewportClampShape GetViewportClampShape() const override;

	/** Returns true when tiled map images should use the minimap shape mask. */
	virtual bool ShouldMaskTiledMapTiles() const override;

	/**
	 * Returns true so the player's own marker stays pinned to the canvas center.
	 * 
	 * @return Always true for the minimap.
	 */
	virtual bool ShouldCenterPlayerMarker() const override;

	/**
	 * Returns true so the player's own marker is drawn when visible on the minimap surface.
	 * 
	 * @return Always true for the minimap.
	 */
	virtual bool ShouldShowPlayerMarker() const override;

	/**
	 * Returns dynamic pawn yaw using the configured minimap rotation source.
	 * 
	 * @param TrackedPawn The pawn tracking the rotation.
	 * @return The dynamic yaw in degrees.
	 */
	virtual float GetDynamicMapYaw(const APawn* TrackedPawn) const override;

	/**
	 * Returns static minimap rotation offset plus map alignment correction.
	 * 
	 * @return The combined static rotation and alignment offset in degrees.
	 */
	virtual float GetTotalStaticRotation() const override;

	/** The UImage that shows the compass ring. Optional for Blueprint compatibility. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> CompassRingImage;

private:
	/** Current static rotation offset in degrees. */
	float CurrentMapRotationOffset = 0.0f;

	/** Current shape mask pushed to the map material. */
	EMinimapShape CurrentMinimapShape = EMinimapShape::Square;
};
