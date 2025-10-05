// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "OBMinimapWidget.generated.h"

class UImage;
class UOBNavigationSubsystem;
class UOBMapLayerAsset;
class UMaterialInstanceDynamic;

/**
 * @enum EMinimapRotationSource
 * @brief Defines the source of rotation for the dynamic map orientation.
 */
UENUM(BlueprintType)
enum class EMinimapRotationSource : uint8
{
	// Use the Pawn's Control Rotation (camera direction). Ideal for First-Person / Third-Person games.
	ControlRotation UMETA(DisplayName = "Control Rotation (Camera)"),

	// Use the Pawn's Actor Rotation (forward direction of the mesh). Ideal for Top-Down / Twin-Stick games.
	ActorRotation UMETA(DisplayName = "Actor Rotation (Character Forward)")
};

/**
 * @enum EMapAlignment
 * @brief Defines which world axis should be treated as "Up" on the minimap.
 */
UENUM(BlueprintType)
enum class EMapAlignment : uint8
{
	// World Forward (+X) is Up on the map. (Default)
	Forward_PlusX   UMETA(DisplayName = "Forward (+X) is Up"),

	// World Right (+Y) is Up on the map.
	Right_PlusY     UMETA(DisplayName = "Right (+Y) is Up"),

	// World Backward (-X) is Up on the map.
	Backward_MinusX UMETA(DisplayName = "Backward (-X) is Up"),
	
	// World Left (-Y) is Up on the map.
	Left_MinusY     UMETA(DisplayName = "Left (-Y) is Up")
};

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
	 * @brief Sets a permanent rotation offset for the map texture.
	 * Useful for Top-Down/Isometric games where the camera has a fixed world rotation.
	 * @param NewOffsetYaw The new rotation offset in degrees.
	 */
	UFUNCTION(BlueprintCallable, Category = "Minimap Settings")
	void SetMapRotationOffset(float NewOffsetYaw);

protected:
	// UUserWidget overrides
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Called when the subsystem detects a map layer change
	UFUNCTION()
	void OnMinimapLayerChanged(UOBMapLayerAsset* NewLayer);

	// --- MINIMAP WIDGETS ---
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> MapImage;

	// The UImage widget for the player's icon, always centered.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> PlayerIcon;

	// --- COMPASS WIDGETS ---
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> CompassRingImage;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> CompassMarkerCanvas;

	// --- MINIMAP SETTINGS ---
	// The desired zoom level for the minimap.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap Settings")
	float Zoom = 5.0f;

	// Determines which rotation to use for the map's orientation WHEN bShouldRotateMap is TRUE.
	// This setting has NO effect on the player icon's rotation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap Settings")
	EMinimapRotationSource RotationSource = EMinimapRotationSource::ActorRotation;

	// If true, the map texture itself will rotate dynamically. If false, the map remains static.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap Settings")
	bool bShouldRotateMap = false;

	// A fixed rotation offset (in degrees) applied to the map texture WHEN bShouldRotateMap is FALSE.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap Settings", meta = (EditCondition = "!bShouldRotateMap"))
	float MapRotationOffset = 0.0f;

	// Determines which world axis is considered "Up" for the entire minimap, aligning both the map and icons.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap Settings")
	EMapAlignment MapAlignment = EMapAlignment::Forward_PlusX;

	// --- COMPASS SETTINGS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compass Settings")
	bool bIsCompassEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compass Settings", meta = (EditCondition = "bIsCompassEnabled"))
	float CompassMarkerRadius = 200.0f;

private:

	// Helper function to get the base rotation angle from the alignment enum.
	float GetAlignmentAngle() const;

	// Helper function to update the compass and its markers.
	void UpdateCompass(const APawn* TrackedPawn, float InTotalStaticRotation);
	
	// Cached pointer to our subsystem for quick access
	UPROPERTY(Transient)
	TObjectPtr<UOBNavigationSubsystem> NavSubsystem;

	// The dynamic material instance we will control
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MinimapMaterialInstance;

	// A map to keep track of active compass marker widgets to avoid recreating them.
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<UImage>> ActiveCompassMarkerWidgets;
};