// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBMapMarker.h"
#include "Engine/DataAsset.h"
#include "OBMinimapConfigAsset.generated.h"

class UMaterialInterface;
class UTexture2D;

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
	Forward_PlusX UMETA(DisplayName = "Forward (+X) is Up"),

	// World Right (+Y) is Up on the map.
	Right_PlusY UMETA(DisplayName = "Right (+Y) is Up"),

	// World Backward (-X) is Up on the map.
	Backward_MinusX UMETA(DisplayName = "Backward (-X) is Up"),

	// World Left (-Y) is Up on the map.
	Left_MinusY UMETA(DisplayName = "Left (-Y) is Up")
};

/**
 * @enum EMinimapShape
 * @brief Defines the clipping shape for the minimap.
 */
UENUM(BlueprintType)
enum class EMinimapShape : uint8
{
	Square UMETA(DisplayName = "Square"),
	Circle UMETA(DisplayName = "Circle")
};


/**
 * @class UOBMinimapConfigAsset
 * @brief Configuration asset for the visual appearance of the minimap widget.
 */
UCLASS(BlueprintType)
class OBNAVIGATION_API UOBMinimapConfigAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// The base material for the minimap's background. A dynamic instance will be created from this.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TObjectPtr<UMaterialInterface> MinimapBackgroundMaterial;

	// The texture for the player's icon in the center of the minimap.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TObjectPtr<UOBMarkerConfigAsset> PlayerMarkerConfig;

	// The texture for the compass ring that surrounds the minimap.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TObjectPtr<UTexture2D> CompassRingTexture;

	// --- MINIMAP SETTINGS ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings")
	float Zoom = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings")
	EMinimapRotationSource RotationSource = EMinimapRotationSource::ActorRotation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings")
	bool bShouldRotateMap = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings",
		meta = (EditCondition = "!bShouldRotateMap"))
	float MapRotationOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings")
	EMapAlignment MapAlignment = EMapAlignment::Forward_PlusX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings")
	EMinimapShape MinimapShape = EMinimapShape::Circle;

	// --- COMPASS SETTINGS ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compass Settings")
	bool bIsCompassEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compass Settings",
		meta = (EditCondition = "bIsCompassEnabled"))
	float CompassMarkerRadius = 200.0f;

	// --- DEBUG SETTINGS ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug Settings")
	bool bShowDebugMessages = false;
};
