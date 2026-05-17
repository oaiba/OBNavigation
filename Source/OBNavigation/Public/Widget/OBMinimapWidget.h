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
 * Displays the player-centered minimap and owns the shared map marker projection path.
 */
UCLASS()
class OBNAVIGATION_API UOBMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Initializes the widget and starts tracking the active navigation layer.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Minimap")
	virtual void InitializeAndStartTracking(UOBMinimapConfigAsset* InConfigAsset);

	/**
	 * Sets the static map rotation offset in degrees.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Minimap")
	void SetMapRotationOffset(float NewOffsetYaw);

	/**
	 * Sets the minimap clipping shape.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Minimap")
	void SetMinimapShape(EMinimapShape NewShape);

	/**
	 * Sets the current map zoom multiplier, clamped by the active widget configuration.
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
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Config")
	UOBMinimapConfigAsset* GetConfig() const { return ConfigAsset; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void OnNavigationMapLayerSpecChanged(FOBNavigationMapLayerSpec NewLayerSpec);

	virtual EOBNavigationSurface GetNavigationSurface() const { return EOBNavigationSurface::Minimap; }
	virtual float GetInitialZoom() const;
	virtual float GetMinimumZoom() const;
	virtual float GetMaximumZoom() const;
	virtual bool ShouldUpdateMapThisFrame() const;
	virtual bool ShouldRotateMap() const;
	virtual bool ShouldCenterPlayerMarker() const;
	virtual bool ShouldShowPlayerMarker() const;
	virtual float GetMarkerScale() const;
	virtual bool ResolveViewCenterUV(const FOBNavigationMapLayerSpec& CurrentLayer, const APawn* TrackedPawn,
	                                 FVector2D& OutViewCenterUV) const;
	virtual void OnViewContextUpdated(const FOBNavigationMapViewContext& ViewContext,
	                                  const FOBNavigationMapLayerSpec& CurrentLayer, const APawn* TrackedPawn);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> MapImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> MinimapMarkerCanvas;

	UPROPERTY(EditAnywhere, Category = "Config")
	TSubclassOf<UOBMapMarkerWidget> MarkerWidgetClass;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> CompassRingImage;

	float GetAlignmentAngle() const;
	virtual float GetDynamicMapYaw(const APawn* TrackedPawn) const;
	void UpdateMapMaterial(const FOBNavigationMapViewContext& ViewContext);
	void UpdateMapMarkers(const APawn* TrackedPawn, const FOBNavigationMapLayerSpec& CurrentLayer,
	                      const FOBNavigationMapViewContext& ViewContext, TSet<FGuid>& OutHandledMarkerIDs);
	void UpdateMapOverlays(const FOBNavigationMapLayerSpec& CurrentLayer,
	                       const FOBNavigationMapViewContext& ViewContext);
	void EnsureOverlayWidget();
	void ClearMarkerWidgets();

	UPROPERTY(Transient)
	TObjectPtr<UOBNavigationSubsystem> NavSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MinimapMaterialInstance;

	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UOBMapMarkerWidget>> ActiveMinimapMarkerWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UOBMapOverlayWidget> OverlayWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UOBMinimapConfigAsset> ConfigAsset;

	bool bIsInitializedAndTracking = false;
	float CurrentMapRotationOffset = 0.0f;
	EMinimapShape CurrentMinimapShape = EMinimapShape::Square;
	float CurrentZoom = 1.0f;
	FGuid PlayerMarkerID;
};
