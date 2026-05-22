#pragma once

#include "CoreMinimal.h"
#include "Data/OBNavigationTypes.h"
#include "MinimapDefinitionDataAsset.h"
#include "UObject/Object.h"
#include "OBMapTileManager.generated.h"

struct FStreamableHandle;

UENUM(BlueprintType)
enum class EOBMapTileManagerState : uint8
{
	Uninitialized,
	LoadingDefinition,
	LoadingTileSet,
	Ready,
	Failed
};

USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBActiveMapTile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FMinimapTileCoord Coord;

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FBox WorldBounds = FBox(ForceInit);

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FVector2D UVMin = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FVector2D UVMax = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	TSoftObjectPtr<UTexture2D> TextureRef;

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	TObjectPtr<UTexture2D> Texture = nullptr;
};

USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBMapTileRuntimeStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	EOBMapTileManagerState State = EOBMapTileManagerState::Uninitialized;

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	bool bIsReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FString FailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FString CaptureRunId;

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FString SourceMapName;

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FString DefinitionPath;

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	FString TileSetPath;

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	int32 MaxLOD = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	int32 ActiveLOD = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	int32 ActiveTileCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	int32 LoadedTileCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OBNavigation|Tiles")
	int32 CachedTileCount = 0;
};

USTRUCT()
struct OBNAVIGATION_API FOBMapTileCacheEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FMinimapTileRef TileRef;

	UPROPERTY()
	TObjectPtr<UTexture2D> Texture = nullptr;

	TSharedPtr<FStreamableHandle> StreamHandle;
	uint64 LastTouchedFrame = 0;
	bool bIsActive = false;
};

UCLASS()
class OBNAVIGATION_API UOBMapTileManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const FOBNavigationMapLayerSpec& InLayerSpec, int32 InTileBudget = 25);
	void Shutdown();

	bool IsInitialized() const { return State != EOBMapTileManagerState::Uninitialized; }
	bool IsReady() const { return State == EOBMapTileManagerState::Ready; }
	bool HasFailed() const { return State == EOBMapTileManagerState::Failed; }
	bool IsTiled() const { return IsReady() && TileSet != nullptr && TileSet->IsValidTileSet(); }

	int32 GetMaxLOD() const;
	int32 GetActiveLOD() const { return ActiveLOD; }
	int32 GetLoadedTileCount() const;
	int32 GetCachedTileCount() const { return TileCache.Num(); }
	EOBMapTileManagerState GetState() const { return State; }
	const FString& GetFailureReason() const { return FailureReason; }

	void UpdateActiveTiles(const FOBNavigationMapViewContext& ViewContext, const FVector2D& CanvasSize,
	                       EOBNavigationSurface Surface, int32 MinimapMaxLODTileLimit = 12);

	const TArray<FOBActiveMapTile>& GetActiveTiles() const { return ActiveTiles; }
	const UMinimapDefinitionDataAsset* GetDefinition() const { return Definition; }
	const UMinimapTileSetDataAsset* GetTileSet() const { return TileSet; }
	FOBMapTileRuntimeStats GetRuntimeStats() const;

private:
	static FString MakeTileKey(const FMinimapTileCoord& Coord);

	void StartDefinitionLoad(int32 Generation);
	void HandleDefinitionLoaded(int32 Generation);
	void StartTileSetLoad(int32 Generation);
	void HandleTileSetLoaded(int32 Generation);
	void SetFailed(const FString& Reason);

	bool BuildVisibleUVRect(const FOBNavigationMapViewContext& ViewContext, const FVector2D& CanvasSize,
	                        FVector2D& OutUVMin, FVector2D& OutUVMax) const;
	int32 ChooseLOD(const FOBNavigationMapViewContext& ViewContext, const FVector2D& CanvasSize,
	                EOBNavigationSurface Surface, const FVector2D& UVMin, const FVector2D& UVMax,
	                int32 MinimapMaxLODTileLimit) const;
	float CalculateWorldUnitsPerPixel(const FOBNavigationMapViewContext& ViewContext,
	                                  const FVector2D& CanvasSize) const;

	FOBMapTileCacheEntry& TouchTile(const FMinimapTileRef& TileRef);
	void RequestTileLoad(const FString& TileKey, FOBMapTileCacheEntry& CacheEntry);
	void HandleTileLoaded(FString TileKey, TSoftObjectPtr<UTexture2D> TextureRef);
	void EvictOldTiles();

	UPROPERTY()
	FOBNavigationMapLayerSpec LayerSpec;

	UPROPERTY()
	TObjectPtr<UMinimapDefinitionDataAsset> Definition = nullptr;

	UPROPERTY()
	TObjectPtr<UMinimapTileSetDataAsset> TileSet = nullptr;

	UPROPERTY()
	TMap<FString, FOBMapTileCacheEntry> TileCache;

	UPROPERTY()
	TArray<FOBActiveMapTile> ActiveTiles;

	TSharedPtr<FStreamableHandle> DefinitionStreamHandle;
	TSharedPtr<FStreamableHandle> TileSetStreamHandle;

	EOBMapTileManagerState State = EOBMapTileManagerState::Uninitialized;
	FString FailureReason;
	int32 TileBudget = 25;
	int32 ActiveLOD = INDEX_NONE;
	int32 MaxLOD = INDEX_NONE;
	uint64 TouchCounter = 0;
	int32 LoadGeneration = 0;
};
