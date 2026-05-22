#include "Data/OBMapTileManager.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "MinimapBlueprintLibrary.h"
#include "OBNavigation.h"

void UOBMapTileManager::Initialize(const FOBNavigationMapLayerSpec& InLayerSpec, const int32 InTileBudget)
{
	Shutdown();

	LayerSpec = InLayerSpec;
	TileBudget = FMath::Max(1, InTileBudget);
	FailureReason.Reset();
	++LoadGeneration;
	StartDefinitionLoad(LoadGeneration);
}

void UOBMapTileManager::StartDefinitionLoad(const int32 Generation)
{
	if (LayerSpec.PanoramicDefinition.IsNull())
	{
		SetFailed(TEXT("PanoramicDefinition is null."));
		return;
	}

	State = EOBMapTileManagerState::LoadingDefinition;
	if (UMinimapDefinitionDataAsset* LoadedDefinition = LayerSpec.PanoramicDefinition.Get())
	{
		Definition = LoadedDefinition;
		HandleDefinitionLoaded(Generation);
		return;
	}

	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	DefinitionStreamHandle = StreamableManager.RequestAsyncLoad(
		LayerSpec.PanoramicDefinition.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &UOBMapTileManager::HandleDefinitionLoaded, Generation));
}

void UOBMapTileManager::HandleDefinitionLoaded(const int32 Generation)
{
	if (Generation != LoadGeneration)
	{
		return;
	}

	Definition = LayerSpec.PanoramicDefinition.Get();
	if (DefinitionStreamHandle.IsValid())
	{
		DefinitionStreamHandle->ReleaseHandle();
		DefinitionStreamHandle.Reset();
	}

	if (!Definition)
	{
		SetFailed(TEXT("Failed to load PanoramicDefinition."));
		return;
	}

	if (Definition->TileSet.IsNull())
	{
		SetFailed(TEXT("PanoramicDefinition has no TileSet."));
		return;
	}

	StartTileSetLoad(Generation);
}

void UOBMapTileManager::StartTileSetLoad(const int32 Generation)
{
	State = EOBMapTileManagerState::LoadingTileSet;
	if (UMinimapTileSetDataAsset* LoadedTileSet = Definition ? Definition->TileSet.Get() : nullptr)
	{
		TileSet = LoadedTileSet;
		HandleTileSetLoaded(Generation);
		return;
	}

	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	TileSetStreamHandle = StreamableManager.RequestAsyncLoad(
		Definition->TileSet.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &UOBMapTileManager::HandleTileSetLoaded, Generation));
}

void UOBMapTileManager::HandleTileSetLoaded(const int32 Generation)
{
	if (Generation != LoadGeneration)
	{
		return;
	}

	TileSet = Definition ? Definition->TileSet.Get() : nullptr;
	if (TileSetStreamHandle.IsValid())
	{
		TileSetStreamHandle->ReleaseHandle();
		TileSetStreamHandle.Reset();
	}

	if (!TileSet || !TileSet->IsValidTileSet())
	{
		TileSet = nullptr;
		SetFailed(TEXT("Failed to load a valid TileSet."));
		return;
	}

	MaxLOD = TileSet->GetMaxLOD();
	ActiveLOD = MaxLOD;
	State = EOBMapTileManagerState::Ready;
}

void UOBMapTileManager::SetFailed(const FString& Reason)
{
	FailureReason = Reason;
	State = EOBMapTileManagerState::Failed;
}

void UOBMapTileManager::Shutdown()
{
	++LoadGeneration;
	if (DefinitionStreamHandle.IsValid())
	{
		DefinitionStreamHandle->ReleaseHandle();
		DefinitionStreamHandle.Reset();
	}
	if (TileSetStreamHandle.IsValid())
	{
		TileSetStreamHandle->ReleaseHandle();
		TileSetStreamHandle.Reset();
	}

	for (TPair<FString, FOBMapTileCacheEntry>& Pair : TileCache)
	{
		if (Pair.Value.StreamHandle.IsValid())
		{
			Pair.Value.StreamHandle->ReleaseHandle();
			Pair.Value.StreamHandle.Reset();
		}
	}

	LayerSpec = FOBNavigationMapLayerSpec();
	Definition = nullptr;
	TileSet = nullptr;
	TileCache.Reset();
	ActiveTiles.Reset();
	State = EOBMapTileManagerState::Uninitialized;
	FailureReason.Reset();
	ActiveLOD = INDEX_NONE;
	MaxLOD = INDEX_NONE;
	TouchCounter = 0;
}

int32 UOBMapTileManager::GetMaxLOD() const
{
	return MaxLOD;
}

int32 UOBMapTileManager::GetLoadedTileCount() const
{
	int32 Count = 0;
	for (const TPair<FString, FOBMapTileCacheEntry>& Pair : TileCache)
	{
		if (Pair.Value.Texture)
		{
			++Count;
		}
	}
	return Count;
}

void UOBMapTileManager::UpdateActiveTiles(const FOBNavigationMapViewContext& ViewContext,
                                          const FVector2D& CanvasSize,
                                          const EOBNavigationSurface Surface,
                                          const int32 MinimapMaxLODTileLimit)
{
	ActiveTiles.Reset();
	if (!IsReady() || !IsTiled() || CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return;
	}

	for (TPair<FString, FOBMapTileCacheEntry>& Pair : TileCache)
	{
		Pair.Value.bIsActive = false;
	}

	FVector2D UVMin;
	FVector2D UVMax;
	if (!BuildVisibleUVRect(ViewContext, CanvasSize, UVMin, UVMax))
	{
		return;
	}

	ActiveLOD = ChooseLOD(ViewContext, CanvasSize, Surface, UVMin, UVMax, MinimapMaxLODTileLimit);
	if (ActiveLOD == INDEX_NONE)
	{
		return;
	}

	TArray<FMinimapTileRef> IntersectingTiles;
	UMinimapBlueprintLibrary::GetTilesIntersectingUVRect(
		TileSet, UVMin, UVMax, ActiveLOD, IntersectingTiles, LayerSpec.bClampQueriesToBounds);

	UE_LOG(LogOBNavigation, VeryVerbose,
	       TEXT("[UOBMapTileManager::%hs] - Query active tiles. Layer='%s' RunId='%s' Surface=%d ViewCenter=%s Zoom=%.3f Canvas=%s UVMin=%s UVMax=%s ActiveLOD=%d MaxLOD=%d Intersections=%d Budget=%d"),
	       __FUNCTION__, *LayerSpec.LayerName.ToString(), Definition ? *Definition->CaptureRunId : TEXT(""),
	       static_cast<int32>(Surface), *ViewContext.ViewCenterUV.ToString(), ViewContext.Zoom, *CanvasSize.ToString(),
	       *UVMin.ToString(), *UVMax.ToString(), ActiveLOD, MaxLOD, IntersectingTiles.Num(), TileBudget);

	ActiveTiles.Reserve(IntersectingTiles.Num());
	for (const FMinimapTileRef& TileRef : IntersectingTiles)
	{
		FOBMapTileCacheEntry& CacheEntry = TouchTile(TileRef);
		RequestTileLoad(MakeTileKey(TileRef.Coord), CacheEntry);

		FOBActiveMapTile ActiveTile;
		ActiveTile.Coord = TileRef.Coord;
		ActiveTile.WorldBounds = TileRef.WorldBounds;
		ActiveTile.UVMin = TileRef.UVMin;
		ActiveTile.UVMax = TileRef.UVMax;
		ActiveTile.TextureRef = TileRef.Texture;
		ActiveTile.Texture = CacheEntry.Texture;
		ActiveTiles.Add(ActiveTile);
	}

	EvictOldTiles();
}

FString UOBMapTileManager::MakeTileKey(const FMinimapTileCoord& Coord)
{
	return FString::Printf(TEXT("%d:%d:%d"), Coord.LOD, Coord.X, Coord.Y);
}

bool UOBMapTileManager::BuildVisibleUVRect(const FOBNavigationMapViewContext& ViewContext,
                                           const FVector2D& CanvasSize,
                                           FVector2D& OutUVMin, FVector2D& OutUVMax) const
{
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return false;
	}

	const float SafeZoom = FMath::Max(ViewContext.Zoom, KINDA_SMALL_NUMBER);
	const FVector2D HalfUVExtent(0.5f / SafeZoom, 0.5f / SafeZoom);
	const float InverseRotationDegrees = -ViewContext.GetAppliedRotationDegrees();

	TArray<FVector2D, TInlineAllocator<4>> Corners;
	Corners.Add(FVector2D(-HalfUVExtent.X, -HalfUVExtent.Y).GetRotated(InverseRotationDegrees) + ViewContext.ViewCenterUV);
	Corners.Add(FVector2D(HalfUVExtent.X, -HalfUVExtent.Y).GetRotated(InverseRotationDegrees) + ViewContext.ViewCenterUV);
	Corners.Add(FVector2D(-HalfUVExtent.X, HalfUVExtent.Y).GetRotated(InverseRotationDegrees) + ViewContext.ViewCenterUV);
	Corners.Add(FVector2D(HalfUVExtent.X, HalfUVExtent.Y).GetRotated(InverseRotationDegrees) + ViewContext.ViewCenterUV);

	OutUVMin = Corners[0];
	OutUVMax = Corners[0];
	for (const FVector2D& Corner : Corners)
	{
		OutUVMin.X = FMath::Min(OutUVMin.X, Corner.X);
		OutUVMin.Y = FMath::Min(OutUVMin.Y, Corner.Y);
		OutUVMax.X = FMath::Max(OutUVMax.X, Corner.X);
		OutUVMax.Y = FMath::Max(OutUVMax.Y, Corner.Y);
	}

	if (LayerSpec.bClampQueriesToBounds || (TileSet && TileSet->bClampQueriesToBounds))
	{
		OutUVMin.X = FMath::Clamp(OutUVMin.X, 0.0f, 1.0f);
		OutUVMin.Y = FMath::Clamp(OutUVMin.Y, 0.0f, 1.0f);
		OutUVMax.X = FMath::Clamp(OutUVMax.X, 0.0f, 1.0f);
		OutUVMax.Y = FMath::Clamp(OutUVMax.Y, 0.0f, 1.0f);
	}

	return true;
}

int32 UOBMapTileManager::ChooseLOD(const FOBNavigationMapViewContext& ViewContext,
                                   const FVector2D& CanvasSize,
                                   const EOBNavigationSurface Surface,
                                   const FVector2D& UVMin,
                                   const FVector2D& UVMax,
                                   const int32 MinimapMaxLODTileLimit) const
{
	if (!TileSet)
	{
		return INDEX_NONE;
	}

	if (Surface == EOBNavigationSurface::Minimap)
	{
		const int32 FullDetailLOD = TileSet->GetMaxLOD();
		if (MinimapMaxLODTileLimit <= 0)
		{
			return FullDetailLOD;
		}

		TArray<FMinimapTileRef> FullDetailTiles;
		UMinimapBlueprintLibrary::GetTilesIntersectingUVRect(
			TileSet, UVMin, UVMax, FullDetailLOD, FullDetailTiles, LayerSpec.bClampQueriesToBounds);
		if (FullDetailTiles.Num() <= MinimapMaxLODTileLimit)
		{
			return FullDetailLOD;
		}

		const int32 SuggestedLOD = UMinimapBlueprintLibrary::ChooseTileLODForWorldUnitsPerPixel(
			TileSet, CalculateWorldUnitsPerPixel(ViewContext, CanvasSize));
		return SuggestedLOD == INDEX_NONE ? TileSet->GetMaxLOD() : SuggestedLOD;
	}

	return UMinimapBlueprintLibrary::ChooseTileLODForWorldUnitsPerPixel(
		TileSet, CalculateWorldUnitsPerPixel(ViewContext, CanvasSize));
}

float UOBMapTileManager::CalculateWorldUnitsPerPixel(const FOBNavigationMapViewContext& ViewContext,
                                                     const FVector2D& CanvasSize) const
{
	if (!LayerSpec.HasValidWorldBounds() || CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return 0.0f;
	}

	const FVector2D ProjectionWorldSize = LayerSpec.GetProjectionWorldSize();
	const float LargerWorldAxis = FMath::Max(ProjectionWorldSize.X, ProjectionWorldSize.Y);
	const float LargerCanvasAxis = FMath::Max(CanvasSize.X, CanvasSize.Y);
	return LargerWorldAxis / (LargerCanvasAxis * FMath::Max(ViewContext.Zoom, KINDA_SMALL_NUMBER));
}

FOBMapTileRuntimeStats UOBMapTileManager::GetRuntimeStats() const
{
	FOBMapTileRuntimeStats Stats;
	Stats.State = State;
	Stats.bIsReady = IsReady();
	Stats.FailureReason = FailureReason;
	Stats.DefinitionPath = LayerSpec.PanoramicDefinition.ToSoftObjectPath().ToString();
	Stats.TileSetPath = Definition ? Definition->TileSet.ToSoftObjectPath().ToString() : FString();
	Stats.CaptureRunId = Definition ? Definition->CaptureRunId : FString();
	Stats.SourceMapName = Definition ? Definition->SourceMapName : FString();
	Stats.MaxLOD = GetMaxLOD();
	Stats.ActiveLOD = ActiveLOD;
	Stats.ActiveTileCount = ActiveTiles.Num();
	Stats.LoadedTileCount = GetLoadedTileCount();
	Stats.CachedTileCount = GetCachedTileCount();
	return Stats;
}

FOBMapTileCacheEntry& UOBMapTileManager::TouchTile(const FMinimapTileRef& TileRef)
{
	const FString TileKey = MakeTileKey(TileRef.Coord);
	FOBMapTileCacheEntry& CacheEntry = TileCache.FindOrAdd(TileKey);
	CacheEntry.TileRef = TileRef;
	CacheEntry.LastTouchedFrame = ++TouchCounter;
	CacheEntry.bIsActive = true;
	return CacheEntry;
}

void UOBMapTileManager::RequestTileLoad(const FString& TileKey, FOBMapTileCacheEntry& CacheEntry)
{
	if (CacheEntry.Texture || CacheEntry.TileRef.Texture.IsNull())
	{
		return;
	}

	if (UTexture2D* LoadedTexture = CacheEntry.TileRef.Texture.Get())
	{
		CacheEntry.Texture = LoadedTexture;
		return;
	}

	if (CacheEntry.StreamHandle.IsValid())
	{
		return;
	}

	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	CacheEntry.StreamHandle = StreamableManager.RequestAsyncLoad(
		CacheEntry.TileRef.Texture.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &UOBMapTileManager::HandleTileLoaded, TileKey, CacheEntry.TileRef.Texture));
}

void UOBMapTileManager::HandleTileLoaded(const FString TileKey, const TSoftObjectPtr<UTexture2D> TextureRef)
{
	FOBMapTileCacheEntry* CacheEntry = TileCache.Find(TileKey);
	if (!CacheEntry)
	{
		return;
	}

	CacheEntry->Texture = TextureRef.Get();
	if (CacheEntry->StreamHandle.IsValid())
	{
		CacheEntry->StreamHandle->ReleaseHandle();
		CacheEntry->StreamHandle.Reset();
	}
}

void UOBMapTileManager::EvictOldTiles()
{
	if (TileCache.Num() <= TileBudget)
	{
		return;
	}

	while (TileCache.Num() > TileBudget)
	{
		FString OldestKey;
		uint64 OldestTouch = MAX_uint64;
		for (const TPair<FString, FOBMapTileCacheEntry>& Pair : TileCache)
		{
			if (!Pair.Value.bIsActive && Pair.Value.LastTouchedFrame < OldestTouch)
			{
				OldestTouch = Pair.Value.LastTouchedFrame;
				OldestKey = Pair.Key;
			}
		}

		if (OldestKey.IsEmpty())
		{
			return;
		}

		if (FOBMapTileCacheEntry* CacheEntry = TileCache.Find(OldestKey))
		{
			if (CacheEntry->StreamHandle.IsValid())
			{
				CacheEntry->StreamHandle->ReleaseHandle();
			}
		}
		TileCache.Remove(OldestKey);
	}
}
