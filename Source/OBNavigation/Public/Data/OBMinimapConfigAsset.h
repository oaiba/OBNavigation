// Copyright OBExtraction. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/OBMapMarker.h"
#include "Engine/DataAsset.h"
#include "OBMinimapConfigAsset.generated.h"

class UMaterialInterface;
class UTexture2D;

/** Defines the source of rotation for the dynamic minimap orientation. */
UENUM(BlueprintType)
enum class EMinimapRotationSource : uint8
{
	/** Use the pawn control rotation. Ideal when aim yaw is decoupled from body yaw. */
	ControlRotation UMETA(DisplayName = "Control Rotation (Aim/Look)"),

	/** Use the pawn actor rotation as the body-facing fallback. */
	ActorRotation UMETA(DisplayName = "Actor Rotation (Character Forward)")
};

/** Defines which world axis should appear as up on the minimap. */
UENUM(BlueprintType)
enum class EMapAlignment : uint8
{
	/** World forward (+X) appears as up on the map. */
	Forward_PlusX UMETA(DisplayName = "Forward (+X) is Up"),

	/** World right (+Y) appears as up on the map. */
	Right_PlusY UMETA(DisplayName = "Right (+Y) is Up"),

	/** World backward (-X) appears as up on the map. */
	Backward_MinusX UMETA(DisplayName = "Backward (-X) is Up"),

	/** World left (-Y) appears as up on the map. */
	Left_MinusY UMETA(DisplayName = "Left (-Y) is Up")
};

/** Defines the clipping shape used by the minimap material. */
UENUM(BlueprintType)
enum class EMinimapShape : uint8
{
	/** Rectangular minimap with no circular alpha mask. */
	Square UMETA(DisplayName = "Square"),

	/** Circular minimap alpha mask. */
	Circle UMETA(DisplayName = "Circle")
};


/** Configuration asset for minimap visuals, rotation, zoom, tiles, and debug output. */
UCLASS(BlueprintType)
class OBNAVIGATION_API UOBMinimapConfigAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Base material for the map background. Widgets create a dynamic instance from this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TObjectPtr<UMaterialInterface> MinimapBackgroundMaterial;

	/** Optional UI material used by tiled minimap images when shape masking is needed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TObjectPtr<UMaterialInterface> TiledMapTileMaterial;

	/** Marker config used for the tracked player's own marker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TObjectPtr<UOBMarkerConfigAsset> PlayerMarkerConfig;

	/** Optional texture for the compass ring that surrounds the minimap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assets")
	TObjectPtr<UTexture2D> CompassRingTexture;

	/** Initial minimap zoom multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings")
	float Zoom = 5.0f;

	/** Minimum minimap zoom multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings", meta = (ClampMin = "0.1"))
	float MinZoom = 1.0f;

	/** Maximum minimap zoom multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings", meta = (ClampMin = "0.1"))
	float MaxZoom = 12.0f;

	/** Rotation source used when bShouldRotateMap is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings")
	EMinimapRotationSource RotationSource = EMinimapRotationSource::ControlRotation;

	/** Whether the minimap background rotates with the tracked pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings")
	bool bShouldRotateMap = false;

	/** Static rotation offset used when the minimap does not rotate dynamically. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings",
		meta = (EditCondition = "!bShouldRotateMap"))
	float MapRotationOffset = 0.0f;

	/** World axis alignment used to determine what appears as up on the map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings")
	EMapAlignment MapAlignment = EMapAlignment::Forward_PlusX;

	/** Shape mask used by the minimap material and tiled minimap material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings")
	EMinimapShape MinimapShape = EMinimapShape::Circle;

	/** Maximum tile textures retained by the minimap tiled render path. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings|Tiles", meta = (ClampMin = "1"))
	int32 TiledMapTileBudget = 25;

	/** Max visible full-detail tiles before minimap LOD downgrades. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minimap Settings|Tiles", meta = (ClampMin = "0"))
	int32 MinimapMaxLODTileLimit = 12;

	/** Enables verbose runtime logs and on-screen diagnostics for map widgets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug Settings")
	bool bShowDebugMessages = true;
};
