// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Data/OBMinimapConfigAsset.h"
#include "Widget/OBMapMarkerWidget.h"
#include "OBMinimapWidget.generated.h"

class UImage;
class UOBNavigationSubsystem;
class UOBMapLayerAsset;
class UMaterialInstanceDynamic;
class UOBMinimapConfigAsset;

/**
 * @class UOBMinimapWidget
 * @brief Displays the minimap. Updates are optimized by driving a dynamic material instance.
 */
UCLASS()
class OBNAVIGATION_API UOBMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/**
	 * @brief Initializes the widget and starts the tracking and ticking process.
	 * This must be called after the widget is added to the viewport.
	 * @param InConfigAsset The configuration asset containing visual resources (materials, textures).
	 */
	UFUNCTION(BlueprintCallable, Category = "Minimap")
	void InitializeAndStartTracking(UOBMinimapConfigAsset* InConfigAsset);
	
	/**
	 * @brief Updates the map rotation offset applied to the minimap texture and related compass elements.
	 * @param NewOffsetYaw The new rotation offset, in degrees, to be applied to the minimap.
	 *                     This value affects the orientation of the map texture and compass ring.
	 */
	UFUNCTION(BlueprintCallable, Category = "Minimap Settings")
	void SetMapRotationOffset(float NewOffsetYaw);

	/**
	 * @brief Sets the shape of the minimap display.
	 * Changes the minimap's shape to either a square or a circle and updates the corresponding material instance.
	 * @param NewShape The desired shape for the minimap, represented by the EMinimapShape enum.
	 */
	UFUNCTION(BlueprintCallable, Category = "Minimap Settings")
	void SetMinimapShape(EMinimapShape NewShape);

	UFUNCTION(BlueprintPure, Category="Config")
	UOBMinimapConfigAsset* GetConfig() const { return ConfigAsset; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Called when the subsystem detects a map layer change
	UFUNCTION()
	void OnMinimapLayerChanged(UOBMapLayerAsset* NewLayer);

	// --- WIDGET COMPONENTS ---
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> MapImage;

	// UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	// TObjectPtr<UImage> PlayerIcon;

	// --- CONFIGURATION ---
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> MinimapMarkerCanvas;
	
	UPROPERTY(EditAnywhere, Category = "Config")
	TSubclassOf<UOBMapMarkerWidget> MarkerWidgetClass;

	// --- COMPASS WIDGETS ---
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> CompassRingImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> CompassMarkerCanvas;

private:
	// Helper function to get the base rotation angle from the alignment enum.
	float GetAlignmentAngle() const;

	// Helper function to update the compass and its markers.
	void UpdateCompassMarkers(const APawn* TrackedPawn, float InTotalStaticRotation);
	void UpdateMinimapMarkers(const APawn* TrackedPawn, float InTotalStaticRotation);

	// --- CACHED POINTERS ---
	// Cached pointer to our subsystem for quick access
	UPROPERTY(Transient)
	TObjectPtr<UOBNavigationSubsystem> NavSubsystem;

	// The dynamic material instance we will control
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MinimapMaterialInstance;

	// Widget Pools
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UOBMapMarkerWidget>> ActiveMinimapMarkerWidgets;
	
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UOBMapMarkerWidget>> ActiveCompassMarkerWidgets;
	
	// --- CONFIGURATION ---
	// Configuration asset for visual resources. Set via InitializeAndStartTracking.
	UPROPERTY(Transient)
	TObjectPtr<UOBMinimapConfigAsset> ConfigAsset;

	// Flag to control whether the widget's Tick logic should run.
	bool bIsInitializedAndTracking = false;

	// These values are now copied from the ConfigAsset on initialization
	// This allows runtime changes (like SetMapRotationOffset) without modifying the asset
	float CurrentMapRotationOffset = 0.0f;
	EMinimapShape CurrentMinimapShape = EMinimapShape::Square;

	FGuid PlayerMarkerID; // Store the player's own marker ID

};
