// Copyright OBExtraction. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MinimapDefinitionDataAsset.h"
#include "OBNavigationDeveloperSettings.generated.h"

class UOBNavigationMapRegistryAsset;

/** Designer-owned panoramic map layer configuration loaded by OBNavigation. */
USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBNavigationPanoramicMapLayer
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OBNavigation|Map Layers")
	TSoftObjectPtr<UMinimapDefinitionDataAsset> MinimapDefinition;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OBNavigation|Map Layers")
	FName LayerName = NAME_None;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OBNavigation|Map Layers")
	int32 Priority = 0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OBNavigation|Map Layers")
	bool bClampQueriesToBounds = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "OBNavigation|Map Layers")
	bool bEnabled = true;
};

/**
 * Project-wide configuration for the OBNavigation plugin.
 *
 * Extends UDeveloperSettings so that entries appear automatically in
 * Project Settings → Game → OB Navigation. Values are serialized into
 * DefaultGame.ini (Config = Game) and shared across all team members
 * via source control.
 *
 * @note This class is loaded once at engine startup. Changing values at
 *       runtime has no effect unless the consuming code explicitly re-reads them.
 *
 * @see UOBNavigationSubsystem   – reads DefaultMapRegistry during Initialize().
 * @see UOBNavigationMapRegistryAsset – the asset type referenced here.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "OB Navigation"))
class OBNAVIGATION_API UOBNavigationDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UOBNavigationDeveloperSettings()
	{
		CategoryName = TEXT("OB Config");
	}

	/**
	 * The primary map-registry data asset that the navigation subsystem loads
	 * on initialization. It contains all map-layer definitions and marker
	 * configuration entries for the project.
	 *
	 * Stored as a TSoftObjectPtr so the asset is not force-loaded into memory
	 * at CDO construction time; the subsystem resolves it on demand.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "OBNavigation")
	TSoftObjectPtr<UOBNavigationMapRegistryAsset> DefaultMapRegistry;

	/** Panoramic map layers are loaded asynchronously when navigation starts. */
	UPROPERTY(Config, EditAnywhere, Category = "OBNavigation|Map Layers")
	TArray<FOBNavigationPanoramicMapLayer> PanoramicMapLayers;

	/**
	 * When enabled, the navigation subsystem will create and display debug-only
	 * markers (e.g., axis gizmos, bounding volume outlines) on all surfaces.
	 * Typically used during level design to verify marker placement and
	 * world-to-UV projection accuracy. Should be disabled in shipping builds.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "OBNavigation")
	bool bShowDebugMarkers = false;

	/**
	 * Draws the world-space XY bounds used by minimap layer selection. The
	 * outline is rendered around the tracked pawn height so it remains visible
	 * even when map capture bounds have a low Z range.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "OBNavigation|Debug")
	bool bDrawDebugMapLayerBounds = false;

	/** Draw the currently selected/active minimap layer bound. */
	UPROPERTY(Config, EditAnywhere, Category = "OBNavigation|Debug", meta = (EditCondition = "bDrawDebugMapLayerBounds"))
	bool bDrawDebugActiveMapLayerBounds = true;

	/** Draw every available layer bound in addition to the active layer bound. */
	UPROPERTY(Config, EditAnywhere, Category = "OBNavigation|Debug", meta = (EditCondition = "bDrawDebugMapLayerBounds"))
	bool bDrawDebugAllMapLayerBounds = false;

	/** Vertical offset added to the tracked pawn Z when drawing minimap bounds. */
	UPROPERTY(Config, EditAnywhere, Category = "OBNavigation|Debug", meta = (EditCondition = "bDrawDebugMapLayerBounds"))
	float DebugMapLayerBoundsZOffset = 10.0f;
};
