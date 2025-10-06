// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBMapMarkerWidget.generated.h"

class UImage;
class UTexture2D;
/**
 * @class UOBMapMarkerWidget
 * @brief Base C++ class for a map marker widget, containing a static identifier icon
 * and a dynamic directional indicator.
 */
UCLASS(Abstract)
class OBNAVIGATION_API UOBMapMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Updates the visual representation of the marker.
	 * This function is designed to be called every frame from the main map widget.
	 * @param IdentifierTexture The texture for the non-rotating identifier icon.
	 * @param IndicatorTexture The texture for the rotating directional indicator. Can be null.
	 * @param IndicatorAngle The rotation angle (in degrees) for the directional indicator.
	 */
	UFUNCTION(BlueprintCallable, Category="Map Marker")
	void UpdateMarkerVisuals(UTexture2D* IdentifierTexture, UTexture2D* IndicatorTexture, float IndicatorAngle);

protected:
	// This function is called when the widget is constructed in the game.
	// We can use it for initial setup.
	virtual void NativePreConstruct() override;
	
	/**
	 * The static icon that identifies the object (e.g., a quest icon, a player number).
	 * This part will NOT rotate. It must be named "IdentifierIcon" in the child Blueprint.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> IdentifierIcon;
	
	/**
	 * The dynamic icon that indicates direction (e.g., an arrow, a cone).
	 * This part WILL rotate. It must be named "DirectionalIndicator" in the child Blueprint.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> DirectionalIndicator;
};
