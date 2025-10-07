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
	 * @brief Sets up the static visual properties of the marker.
	 * Call this ONLY ONCE when the widget is created.
	 * @param IdentifierTexture The texture for the non-rotating identifier icon.
	 * @param IndicatorMaterial The material for the rotating directional indicator. Can be null.
	 */
	UFUNCTION(BlueprintCallable, Category="Map Marker")
	void InitializeMarker(UTexture2D* IdentifierTexture, UMaterialInterface* IndicatorMaterial);

	/**
	 * @brief Updates the dynamic properties of the marker, like rotation.
	 * Call this every frame.
	 * @param IndicatorAngle The new rotation angle (in degrees) for the directional indicator.
	 */
	UFUNCTION(BlueprintCallable, Category="Map Marker")
	void UpdateRotation(float IndicatorAngle);

	/**
	 * @brief Updates the dynamic properties of the marker.
	 * @param IndicatorAngle The new rotation angle (in degrees).
	 * @param InViewAngle The FOV angle for the cone material.
	 * @param InViewDistance The normalized view distance for the cone.
	 */
	UFUNCTION(BlueprintCallable, Category="Map Marker")
	void UpdateVisuals(float IndicatorAngle, float InViewAngle, float InViewDistance);


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

	// Dynamic material instance for the FOV cone here
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FOVMaterialInstance;
};
