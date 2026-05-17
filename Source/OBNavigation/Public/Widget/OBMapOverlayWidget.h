#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBNavigationTypes.h"
#include "OBMapOverlayWidget.generated.h"

/**
 * @class UOBMapOverlayWidget
 * @brief Handles custom painting of overlay elements on top of the map.
 * 
 * =========================================================================
 * Visual ASCII Wireframe:
 * 
 *  +-----------------[Root Layout]------------------+
 *  |                                                |
 *  |  +-------[Custom Painted Widget]------------+  |
 *  |  |                                          |  |
 *  |  |   /--------\     <-- Painted Path        |  |
 *  |  |  /          \                            |  |
 *  |  | |   [Zone]   |   <-- Painted Area        |  |
 *  |  |  \          /                            |  |
 *  |  |   \--------/                             |  |
 *  |  |                                          |  |
 *  |  +------------------------------------------+  |
 *  |                                                |
 *  +------------------------------------------------+
 * =========================================================================
 */
UCLASS()
class OBNAVIGATION_API UOBMapOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOverlayContext(const FOBNavigationMapLayerSpec& InLayerSpec,
	                       const TArray<FOBNavigationOverlayElement>& InOverlayElements,
	                       const FVector& InTrackedWorldLocation,
	                       const float InCurrentZoom,
	                       const float InTotalStaticRotation,
	                       const float InDynamicMapYaw,
	                       const bool bInShouldRotateMap);

	void ClearOverlayContext();

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	                          const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	                          int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	bool ProjectWorldToCanvas(const FVector& WorldLocation, const FVector2D& CanvasSize,
	                          FVector2D& OutCanvasPosition) const;

	UPROPERTY(Transient)
	FOBNavigationMapLayerSpec LayerSpec;

	UPROPERTY(Transient)
	TArray<FOBNavigationOverlayElement> OverlayElements;

	FVector TrackedWorldLocation = FVector::ZeroVector;
	float CurrentZoom = 1.0f;
	float TotalStaticRotation = 0.0f;
	float DynamicMapYaw = 0.0f;
	bool bShouldRotateMap = false;
	bool bHasOverlayContext = false;
};
