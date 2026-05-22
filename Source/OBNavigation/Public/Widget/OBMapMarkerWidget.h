// Copyright OBExtraction. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBMapMarkerWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

/**
 * Base C++ class for a map marker widget containing a static identifier icon
 * and an optional rotating directional indicator.
 *
 * =========================================================================
 * Visual ASCII Wireframe:
 * 
 *  +-----------------[Root Layout]------------------+
 *  |                                                |
 *  |      +-----[DirectionalIndicator]-----+        |
 *  |      |               /\               |        |
 *  |      |              /  \              |        |
 *  |      |             /____\             |        |
 *  |      +--------------------------------+        |
 *  |                                                |
 *  |      +--------[IdentifierIcon]--------+        |
 *  |      |               O                |        |
 *  |      |              -|-               |        |
 *  |      |              / \               |        |
 *  |      +--------------------------------+        |
 *  |                                                |
 *  |      +---------[DistanceText]---------+        |
 *  |      |             120m               |        |
 *  |      +--------------------------------+        |
 *  +------------------------------------------------+
 * 
 *  Note: Indicator and Icon overlap in the center. 
 *  The Indicator rotates based on the target's rotation.
 * =========================================================================
 */
UCLASS(Abstract)
class OBNAVIGATION_API UOBMapMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	
	/**
	 * Sets up the static visual properties of the marker. Call once after widget creation.
	 *
	 * @param IdentifierTexture The texture for the non-rotating identifier icon.
	 * @param IndicatorMaterial The material for the rotating directional indicator. Can be null.
	 */
	UFUNCTION(BlueprintCallable, Category="Map Marker")
	void InitializeMarker(UTexture2D* IdentifierTexture, UMaterialInterface* IndicatorMaterial);

	/**
	 * Updates the directional indicator rotation. Call once per frame while visible.
	 *
	 * @param IndicatorAngle The new rotation angle (in degrees) for the directional indicator.
	 */
	UFUNCTION(BlueprintCallable, Category="Map Marker")
	void UpdateRotation(float IndicatorAngle);

	/**
	 * Updates rotation and directional material parameters.
	 *
	 * @param IndicatorAngle The new rotation angle (in degrees).
	 * @param InViewAngle The FOV angle for the cone material.
	 * @param InViewDistance The normalized view distance for the cone.
	 */
	UFUNCTION(BlueprintCallable, Category="Map Marker")
	void UpdateVisuals(float IndicatorAngle, float InViewAngle, float InViewDistance);

	/**
	 * Updates the optional distance label.
	 *
	 * @param DistanceMeters Distance to the tracked pawn in meters.
	 * @param bIsClampedToEdge True when the marker is edge-clamped on the map.
	 */
	UFUNCTION(BlueprintCallable, Category="Map Marker")
	void UpdateDistance(float DistanceMeters, bool bIsClampedToEdge);


protected:
	/** Applies design-time defaults before the widget is fully constructed. */
	virtual void NativePreConstruct() override;
	
	/** Static icon that identifies the marker. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> IdentifierIcon;
	
	/** Dynamic icon that indicates orientation or view direction. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> DirectionalIndicator;

	/** Optional text label for distance to the tracked pawn. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DistanceText;

	/** Dynamic material instance used for field-of-view cone parameters. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FOVMaterialInstance;
};
