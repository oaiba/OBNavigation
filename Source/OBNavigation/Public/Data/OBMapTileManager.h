#pragma once

#include "CoreMinimal.h"
#include "Data/OBNavigationTypes.h"
#include "MinimapDefinitionDataAsset.h"
#include "UObject/Object.h"
#include "OBMapTileManager.generated.h"

struct FStreamableHandle;

/** Async loading lifecycle for a Panoramic tiled map layer. */
UENUM(BlueprintType)
enum class EOBMapTileManagerState : uint8
{
	/** Manager has not been initialized with a layer. */
	Uninitialized,

	/** Panoramic definition soft reference is being loaded. */
	LoadingDefinition,

	/** Tile-set data asset soft reference is being loaded. */
	LoadingTileSet,

	/** Definition and tile set are loaded and active-tile queries can run. */
	Ready,

	/** Loading or validation failed; see FailureReason. */
	Failed
};

/** Draw-ready tile returned by UOBMapTileManager for map widgets. */
USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBActiveMapTile
{
	GENERATED_BODY()

	/** Tile LOD and grid coordinate in Panoramic tile-set space. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FMinimapTileCoord Coord;

	/** World-space bounds metadata for diagnostics and future culling. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FBox WorldBounds = FBox(ForceInit);

	/** Minimum map UV covered by this tile. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FVector2D UVMin = FVector2D::ZeroVector;

	/** Maximum map UV covered by this tile. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FVector2D UVMax = FVector2D::ZeroVector;

	/** Soft reference to the tile texture. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	TSoftObjectPtr<UTexture2D> TextureRef;

	/** Loaded tile texture, or null while async streaming is still pending. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	TObjectPtr<UTexture2D> Texture = nullptr;
};

/** Blueprint/debug friendly snapshot of tiled map runtime state. */
USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBMapTileRuntimeStats
{
	GENERATED_BODY()

	/** Current manager state. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	EOBMapTileManagerState State = EOBMapTileManagerState::Uninitialized;

	/** True when State is Ready. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	bool bIsReady = false;

	/** Localized failure reason when State is Failed. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FText FailureReason;

	/** Panoramic capture run identifier from the definition. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FString CaptureRunId;

	/** Source map name captured by Panoramic. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FString SourceMapName;

	/** Asset path of the Panoramic definition. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FString DefinitionPath;

	/** Asset path of the Panoramic tile set. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FString TileSetPath;

	/** Highest available LOD in the tile set. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	int32 MaxLOD = INDEX_NONE;

	/** LOD currently requested for the visible viewport. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	int32 ActiveLOD = INDEX_NONE;

	/** Number of tiles intersecting the current viewport. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	int32 ActiveTileCount = 0;

	/** Number of active tiles whose textures are loaded. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	int32 LoadedTileCount = 0;

	/** Number of tile entries retained in the LRU cache. */
	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	int32 CachedTileCount = 0;
};

/** Internal LRU cache entry for a streamed tile texture. */
USTRUCT()
struct OBNAVIGATION_API FOBMapTileCacheEntry
{
	GENERATED_BODY()

	/** Tile metadata copied from the Panoramic tile set. */
	UPROPERTY()
	FMinimapTileRef TileRef;

	/** Loaded texture retained by the cache. */
	UPROPERTY()
	TObjectPtr<UTexture2D> Texture = nullptr;

	/** Async streaming handle while the texture is loading. */
	TSharedPtr<FStreamableHandle> StreamHandle;

	/** Monotonic touch frame used for LRU eviction. */
	uint64 LastTouchedFrame = 0;

	/** True while this tile is part of the current active-tile set. */
	bool bIsActive = false;
};

/** Streams and caches Panoramic minimap tiles for minimap and tactical widgets. */
UCLASS()
class OBNAVIGATION_API UOBMapTileManager : public UObject
{
	GENERATED_BODY()

public:
	/** Starts async loading for a Panoramic tiled layer. */
	void Initialize(const FOBNavigationMapLayerSpec& InLayerSpec, int32 InTileBudget = 25);

	/** Cancels pending loads and clears active/cache state. */
	void Shutdown();

	/** Returns true after Initialize has been called and before Shutdown resets the state. */
	bool IsInitialized() const { return State != EOBMapTileManagerState::Uninitialized; }

	/** Returns true when the definition and tile set are loaded and valid. */
	bool IsReady() const { return State == EOBMapTileManagerState::Ready; }

	/** Returns true when loading or validation failed. */
	bool HasFailed() const { return State == EOBMapTileManagerState::Failed; }

	/** Returns true when ready and backed by a valid tile set. */
	bool IsTiled() const { return IsReady() && TileSet != nullptr && TileSet->IsValidTileSet(); }

	/** Returns the maximum LOD available in the loaded tile set. */
	int32 GetMaxLOD() const;

	/** Returns the LOD used by the most recent active-tile update. */
	int32 GetActiveLOD() const { return ActiveLOD; }

	/** Returns the number of active tile textures that are loaded. */
	int32 GetLoadedTileCount() const;

	/** Returns the number of entries retained in the cache. */
	int32 GetCachedTileCount() const { return TileCache.Num(); }

	/** Returns the current async loading state. */
	EOBMapTileManagerState GetState() const { return State; }

	/** Returns the localized failure reason when HasFailed is true. */
	const FText& GetFailureReason() const { return FailureReason; }

	/**
	 * Recomputes visible tiles and requests missing tile textures.
	 *
	 * @param ViewContext Current map view state.
	 * @param CanvasSize Aspect-preserved map viewport size in Slate units.
	 * @param Surface Surface-specific LOD policy selector.
	 * @param MinimapMaxLODTileLimit Max full-detail tile count allowed for minimap before LOD downgrade.
	 */
	void UpdateActiveTiles(const FOBNavigationMapViewContext& ViewContext, const FVector2D& CanvasSize,
	                       EOBNavigationSurface Surface, int32 MinimapMaxLODTileLimit = 12);

	/** Returns the active tile list for rendering. */
	const TArray<FOBActiveMapTile>& GetActiveTiles() const { return ActiveTiles; }

	/** Returns the loaded Panoramic definition, or null before ready. */
	const UMinimapDefinitionDataAsset* GetDefinition() const { return Definition; }

	/** Returns the loaded Panoramic tile set, or null before ready. */
	const UMinimapTileSetDataAsset* GetTileSet() const { return TileSet; }

	/** Returns a Blueprint/debug friendly runtime stats snapshot. */
	FOBMapTileRuntimeStats GetRuntimeStats() const;

private:
	/** Builds the stable cache key used for a tile coordinate. */
	static FString MakeTileKey(const FMinimapTileCoord& Coord);

	/** Starts async loading of the Panoramic definition for the current generation. */
	void StartDefinitionLoad(int32 Generation);

	/** Completes definition loading if the generation is still current. */
	void HandleDefinitionLoaded(int32 Generation);

	/** Starts async loading of the tile-set asset referenced by the definition. */
	void StartTileSetLoad(int32 Generation);

	/** Completes tile-set loading if the generation is still current. */
	void HandleTileSetLoaded(int32 Generation);

	/** Marks the manager failed and stores a localized reason. */
	void SetFailed(const FText& Reason);

	/** Computes the visible map UV rectangle for the current view. */
	bool BuildVisibleUVRect(const FOBNavigationMapViewContext& ViewContext, const FVector2D& CanvasSize,
	                        FVector2D& OutUVMin, FVector2D& OutUVMax) const;

	/** Chooses the tile LOD for the current surface and visible UV rectangle. */
	int32 ChooseLOD(const FOBNavigationMapViewContext& ViewContext, const FVector2D& CanvasSize,
	                EOBNavigationSurface Surface, const FVector2D& UVMin, const FVector2D& UVMax,
	                int32 MinimapMaxLODTileLimit) const;

	/** Calculates requested world units per screen pixel for Panoramic LOD selection. */
	float CalculateWorldUnitsPerPixel(const FOBNavigationMapViewContext& ViewContext,
	                                  const FVector2D& CanvasSize) const;

	/** Touches or creates a cache entry for a tile and marks it recently used. */
	FOBMapTileCacheEntry& TouchTile(const FMinimapTileRef& TileRef);

	/** Requests async streaming for a tile texture if it is not already loaded or loading. */
	void RequestTileLoad(const FString& TileKey, FOBMapTileCacheEntry& CacheEntry);

	/** Completes a tile texture load and updates the cache entry if still valid. */
	void HandleTileLoaded(FString TileKey, TSoftObjectPtr<UTexture2D> TextureRef);

	/** Evicts inactive least-recently-used tiles until the cache meets TileBudget. */
	void EvictOldTiles();

	/** Layer spec this manager was initialized with. */
	UPROPERTY()
	FOBNavigationMapLayerSpec LayerSpec;

	/** Loaded Panoramic definition data asset. */
	UPROPERTY()
	TObjectPtr<UMinimapDefinitionDataAsset> Definition = nullptr;

	/** Loaded Panoramic tile set data asset. */
	UPROPERTY()
	TObjectPtr<UMinimapTileSetDataAsset> TileSet = nullptr;

	/** LRU cache keyed by LOD/X/Y tile coordinate. */
	UPROPERTY()
	TMap<FString, FOBMapTileCacheEntry> TileCache;

	/** Active tile list generated by the most recent UpdateActiveTiles call. */
	UPROPERTY()
	TArray<FOBActiveMapTile> ActiveTiles;

	/** Stream handle for the definition async load. */
	TSharedPtr<FStreamableHandle> DefinitionStreamHandle;

	/** Stream handle for the tile set async load. */
	TSharedPtr<FStreamableHandle> TileSetStreamHandle;

	/** Current manager loading and validation state. */
	EOBMapTileManagerState State = EOBMapTileManagerState::Uninitialized;

	/** Localized failure reason when State is Failed. */
	FText FailureReason;

	/** Maximum number of cached tile textures retained. */
	int32 TileBudget = 25;

	/** LOD selected by the most recent active-tile query. */
	int32 ActiveLOD = INDEX_NONE;

	/** Maximum LOD available in the loaded tile set. */
	int32 MaxLOD = INDEX_NONE;

	/** Monotonic counter for LRU tile touches. */
	uint64 TouchCounter = 0;

	/** Incremented on Initialize/Shutdown to ignore stale async callbacks. */
	int32 LoadGeneration = 0;
};
