// Fill out your copyright notice in the Description page of Project Settings.


#include "OBNavigationSubsystem.h"

#include "OBMapLayerAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"

void UOBNavigationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Load all map layer assets from the project
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
		"AssetRegistry");
	TArray<FAssetData> AssetData;
	AssetRegistryModule.Get().GetAssetsByClass(UOBMapLayerAsset::StaticClass()->GetClassPathName(), AssetData);

	for (const FAssetData& Data : AssetData)
	{
		if (UOBMapLayerAsset* LoadedLayer = Cast<UOBMapLayerAsset>(Data.GetAsset()))
		{
			AllMapLayers.Add(LoadedLayer);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Loaded %d map layer assets."), *GetName(), __FUNCTION__, AllMapLayers.Num());

	// Sort layers by priority to optimize the search later
	AllMapLayers.Sort([](const UOBMapLayerAsset& A, const UOBMapLayerAsset& B)
	{
		return A.Priority > B.Priority;
	});

	// Register our custom tick function
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UOBNavigationSubsystem::Tick));
}

void UOBNavigationSubsystem::Deinitialize()
{
	// Unregister the tick function
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	Super::Deinitialize();
}

void UOBNavigationSubsystem::SetTrackedPlayerPawn(APawn* PlayerPawn)
{
	if (PlayerPawn)
	{
		TrackedPlayerPawn = PlayerPawn;
		UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Now tracking pawn: %s"), *GetName(), __FUNCTION__,
		       *PlayerPawn->GetName());
		// Force an immediate update
		UpdateActiveMinimapLayer();
	}
	else
	{
		TrackedPlayerPawn.Reset();
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Stopped tracking pawn."), *GetName(), __FUNCTION__);
	}
}

FGuid UOBNavigationSubsystem::RegisterMapMarker(AActor* InTrackedActor, UOBMarkerConfigAsset* InConfig,
                                                FName InLayerName, FVector InStaticLocation)
{
	// Ensure the config is valid before proceeding
	if (!InConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Failed to register marker: InConfig is null."), *GetName(),
		       __FUNCTION__);
		return FGuid(); // Return invalid Guid
	}

	// Create a new marker object
	UOBMapMarker* NewMarker = NewObject<UOBMapMarker>(this);
	const FGuid NewGuid = FGuid::NewGuid();

	NewMarker->Init(NewGuid, InTrackedActor, InConfig, InLayerName, InStaticLocation);

	// Add to our map and broadcast changes
	ActiveMarkersMap.Add(NewGuid, NewMarker);
	RebuildActiveMarkersArray();
	OnMarkersUpdated.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Registered new marker with ID: %s"), *GetName(), __FUNCTION__,
	       *NewGuid.ToString());

	return NewGuid;
}

void UOBNavigationSubsystem::UnregisterMapMarker(const FGuid& MarkerID)
{
	if (!MarkerID.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Attempted to unregister an invalid marker ID."), *GetName(),
		       __FUNCTION__);
		return;
	}

	if (ActiveMarkersMap.Remove(MarkerID) > 0)
	{
		// If removal was successful, update the cached array and notify UI
		RebuildActiveMarkersArray();
		OnMarkersUpdated.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Unregistered marker with ID: %s"), *GetName(), __FUNCTION__,
		       *MarkerID.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Could not find marker with ID to unregister: %s"), *GetName(),
		       __FUNCTION__, *MarkerID.ToString());
	}
}

void UOBNavigationSubsystem::RebuildActiveMarkersArray()
{
	// Clear the old array
	ActiveMarkers.Empty();
	// Re-populate the array with the current values from the map
	ActiveMarkersMap.GenerateValueArray(ActiveMarkers);
}

bool UOBNavigationSubsystem::WorldToMapUV(const UOBMapLayerAsset* MapLayer, const FVector& WorldLocation,
                                          FVector2D& OutMapUV) const
{
	if (!MapLayer)
	{
		return false;
	}

	const FBox& Bounds = MapLayer->WorldBounds;
	if (!Bounds.IsInsideXY(WorldLocation))
	{
		return false;
	}

	const FVector WorldSize = Bounds.GetSize();
	if (FMath::IsNearlyZero(WorldSize.X) || FMath::IsNearlyZero(WorldSize.Y))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - MapLayer '%s' has zero size on X or Y axis."), *GetName(),
		       __FUNCTION__, *MapLayer->GetName());
		return false;
	}

	const double LocalX = WorldLocation.X - Bounds.Min.X;
	const double LocalY = WorldLocation.Y - Bounds.Min.Y;

	// STANDARD MAPPING:
	// World +Y (Right) maps to the horizontal U coordinate.
	OutMapUV.X = LocalY / WorldSize.Y;

	// World +X (Forward/North) maps to the vertical V coordinate.
	// We flip it so North (+X) is at the top of the map (V=0).
	OutMapUV.Y = 1.0 - (LocalX / WorldSize.X);

	// --- DEBUG LOG ---
	if (GEngine)
	{
		const FString DebugMsg = FString::Printf(TEXT("[WorldToMapUV] WorldLoc: %s -> UV: %s"), *WorldLocation.ToString(), *OutMapUV.ToString());
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, DebugMsg);
	}
	
	return true;
}

bool UOBNavigationSubsystem::Tick(float DeltaTime)
{
	// Only run logic if we are tracking a valid pawn
	if (TrackedPlayerPawn.IsValid())
	{
		UpdateActiveMinimapLayer();
	}
	// Return true to keep the ticker registered
	return true;
}

void UOBNavigationSubsystem::UpdateActiveMinimapLayer()
{
	if (!TrackedPlayerPawn.IsValid())
	{
		return;
	}

	const FVector PawnLocation = TrackedPlayerPawn->GetActorLocation();
	UOBMapLayerAsset* BestLayer = nullptr;

	// Since the array is pre-sorted by priority, the first valid layer we find is the best one.
	for (UOBMapLayerAsset* Layer : AllMapLayers)
	{
		if (Layer && Layer->WorldBounds.IsInsideXY(PawnLocation))
		{
			BestLayer = Layer;
			break; // Found the highest priority layer
		}
	}

	// If the best layer has changed, update it and notify listeners
	if (BestLayer != CurrentMinimapLayer)
	{
		CurrentMinimapLayer = BestLayer;
		UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Minimap layer changed to: %s"), *GetName(), __FUNCTION__,
		       BestLayer ? *BestLayer->GetName() : TEXT("None"));
		OnMinimapLayerChanged.Broadcast(CurrentMinimapLayer);
	}
}
