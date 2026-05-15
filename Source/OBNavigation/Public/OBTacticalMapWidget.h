#pragma once

#include "CoreMinimal.h"
#include "OBMinimapWidget.h"
#include "OBTacticalMapWidget.generated.h"

/**
 * Full-map variant of the minimap widget. V1 exposes runtime zoom/pan state for Blueprint input
 * while reusing the same marker projection and pooling path as the minimap.
 */
UCLASS()
class OBNAVIGATION_API UOBTacticalMapWidget : public UOBMinimapWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Tactical Map")
	void AddZoomInput(float ZoomDelta);

	UFUNCTION(BlueprintCallable, Category = "Tactical Map")
	void SetPanInput(FVector2D InPanInput);

	UFUNCTION(BlueprintPure, Category = "Tactical Map")
	FVector2D GetPanInput() const { return PanInput; }

protected:
	virtual EOBNavigationSurface GetNavigationSurface() const override { return EOBNavigationSurface::FullMap; }

private:
	UPROPERTY(Transient)
	FVector2D PanInput = FVector2D::ZeroVector;
};
