#pragma once

#include "CoreMinimal.h"
#include "Widget/OBMinimapWidget.h"
#include "OBTacticalMapWidget.generated.h"

class UOBTacticalMapConfigAsset;

/**
 * Full-screen tactical map widget with independent zoom, pan, and view center state.
 */
UCLASS()
class OBNAVIGATION_API UOBTacticalMapWidget : public UOBMinimapWidget
{
	GENERATED_BODY()

public:
	/**
	 * Initializes the tactical map using minimap visual assets and tactical-map view settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void InitializeTacticalMapAndStartTracking(UOBMinimapConfigAsset* InMinimapConfigAsset,
	                                           UOBTacticalMapConfigAsset* InTacticalConfigAsset);

	/**
	 * Applies a zoom step. Positive values zoom in, negative values zoom out.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void AddZoomInput(float ZoomDelta);

	/**
	 * Applies a screen-space pan delta in Slate units.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void AddPanInput(FVector2D PanDelta);

	/**
	 * Sets normalized continuous pan input applied every tick.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void SetPanInput(FVector2D InPanInput);

	/**
	 * Returns the current continuous pan input.
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|TacticalMap")
	FVector2D GetPanInput() const { return PanInput; }

	/**
	 * Sets the tactical map center from a world location on the active map layer.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void SetViewCenterWorldLocation(FVector WorldLocation);

	/**
	 * Sets the tactical map center as normalized UV coordinates.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void SetViewCenterUV(FVector2D MapUV);

	/**
	 * Recenters the tactical map on the tracked player and resumes follow mode.
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
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|TacticalMap")
	float GetTacticalMapZoom() const { return GetMinimapZoom(); }

	/**
	 * Returns true while the tactical map view center follows the tracked player.
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|TacticalMap")
	bool IsFollowingTrackedPlayer() const { return bIsFollowingTrackedPlayer; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual EOBNavigationSurface GetNavigationSurface() const override { return EOBNavigationSurface::FullMap; }
	virtual float GetInitialZoom() const override;
	virtual float GetMinimumZoom() const override;
	virtual float GetMaximumZoom() const override;
	virtual bool ShouldRotateMap() const override;
	virtual bool ShouldCenterPlayerMarker() const override;
	virtual bool ShouldShowPlayerMarker() const override;
	virtual float GetMarkerScale() const override;
	virtual bool ResolveViewCenterUV(const FOBNavigationMapLayerSpec& CurrentLayer, const APawn* TrackedPawn,
	                                 FVector2D& OutViewCenterUV) const override;
	virtual void OnViewContextUpdated(const FOBNavigationMapViewContext& ViewContext,
	                                  const FOBNavigationMapLayerSpec& CurrentLayer, const APawn* TrackedPawn) override;

private:
	void ApplyContinuousPanInput(float DeltaTime);
	void SetViewCenterUVInternal(FVector2D MapUV, bool bFollowTrackedPlayer);

	UPROPERTY(Transient)
	TObjectPtr<UOBTacticalMapConfigAsset> TacticalConfigAsset;

	FVector2D ViewCenterUV = FVector2D(0.5f, 0.5f);
	FVector2D PanInput = FVector2D::ZeroVector;
	bool bIsFollowingTrackedPlayer = true;
};
