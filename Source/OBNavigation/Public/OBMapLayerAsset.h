// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OBMapLayerAsset.generated.h"

/**
 * @class UOBMapLayerAsset
 * @brief Defines a static map texture and its corresponding world space boundaries.
 * This is the core data for displaying a map layer.
 */
UCLASS()
class OBNAVIGATION_API UOBMapLayerAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// The texture for this map layer (e.g., a top-down view of a dungeon floor)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Layer")
	TObjectPtr<UTexture2D> MapTexture;

	// The real-world boundaries that this map texture covers.
	// This is crucial for converting world coordinates to map coordinates.
	// Z values are ignored for 2D maps.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Layer")
	FBox WorldBounds;

	// Priority for auto-switching on the minimap. Higher values are chosen first.
	// E.g., Dungeon_Floor1 (Priority 100) will be chosen over Overworld (Priority 0)
	// when the player is inside the dungeon's WorldBounds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map Layer")
	int32 Priority = 0;
};
