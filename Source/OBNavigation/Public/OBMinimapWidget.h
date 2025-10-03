// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBMinimapWidget.generated.h"

class UImage;
class UOBNavigationSubsystem;
class UOBMapLayerAsset;
class UMaterialInstanceDynamic;

/**
 * @class UOBMinimapWidget
 * @brief Displays the minimap. Updates are optimized by driving a dynamic material instance.
 */
UCLASS()
class OBNAVIGATION_API UOBMinimapWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	// UUserWidget overrides
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Called when the subsystem detects a map layer change
	UFUNCTION()
	void OnMinimapLayerChanged(UOBMapLayerAsset* NewLayer);

	// The UImage widget in our UMG designer that will display the map.
	// We must bind this in the Blueprint child class.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> MapImage;

	// The UImage widget for the player's icon, always centered.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> PlayerIcon;

	// The desired zoom level for the minimap
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap Settings")
	float Zoom = 5.0f;

private:
	// Cached pointer to our subsystem for quick access
	UPROPERTY(Transient)
	TObjectPtr<UOBNavigationSubsystem> NavSubsystem;

	// The dynamic material instance we will control
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MinimapMaterialInstance;
};
