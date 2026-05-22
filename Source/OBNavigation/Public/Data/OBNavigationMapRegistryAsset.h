// Copyright OBExtraction. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "OBNavigationMapRegistryAsset.generated.h"

class UOBMarkerConfigAsset;

/**
 * Maps a single Gameplay Tag to a marker configuration asset.
 *
 * The navigation subsystem uses this mapping at initialization to build
 * a fast lookup table (FGameplayTag → UOBMarkerConfigAsset*). When a
 * new marker is registered with only a MarkerType tag (and no explicit
 * ConfigAsset pointer), the subsystem resolves the visual config through
 * this table.
 *
 * Example tag hierarchy:
 *   OBNavigation.Marker.Player
 *   OBNavigation.Marker.Quest.Objective
 *   OBNavigation.Marker.Ping.Danger
 *
 * @see UOBNavigationMapRegistryAsset – the owning data asset that
 *      holds an array of these entries.
 * @see UOBMarkerConfigAsset – the asset defining icon, indicator material,
 *      color, size, and surface visibility.
 */
USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBNavigationMarkerConfigEntry
{
	GENERATED_BODY()

	/**
	 * Gameplay tag that uniquely identifies a marker category.
	 * Must match the MarkerType tag supplied in FOBNavigationMarkerSpec
	 * when registering markers through the subsystem.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation")
	FGameplayTag MarkerType;

	/**
	 * The data asset that describes this marker type's visual appearance
	 * (icon, material, size, color, per-surface visibility flags).
	 * Must not be null for the entry to be useful at runtime.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation")
	TObjectPtr<UOBMarkerConfigAsset> Config = nullptr;
};

/**
 * Central data asset that acts as the project's marker configuration registry.
 *
 * Designers create one (or more) instances of this asset and reference it from
 * UOBNavigationDeveloperSettings::DefaultMapRegistry. The navigation subsystem
 * loads this asset at startup and iterates its MarkerConfigs array to populate
 * internal lookup tables.
 *
 * Inherits from UPrimaryDataAsset so it can be discovered and loaded by the
 * Asset Manager if needed in cooked builds.
 *
 * @note Adding or removing entries at runtime is not supported. The subsystem
 *       only reads this data once during Initialize().
 *
 * @see UOBNavigationDeveloperSettings – references this asset via TSoftObjectPtr.
 * @see UOBNavigationSubsystem::LoadRegistryData() – consumes these entries.
 */
UCLASS(BlueprintType)
class OBNAVIGATION_API UOBNavigationMapRegistryAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Ordered list of marker-type → config-asset mappings.
	 * Each entry associates a FGameplayTag with a UOBMarkerConfigAsset.
	 * The subsystem iterates this array to build its internal
	 * MarkerConfigsByTag lookup map.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation")
	TArray<FOBNavigationMarkerConfigEntry> MarkerConfigs;
};
