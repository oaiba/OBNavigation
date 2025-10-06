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
                                                const FName InLayerName, const FVector InStaticLocation)
{
	// Ensure the config is valid before proceeding
	if (!InConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Failed to register marker: InConfig is null."), *GetName(),
		       __FUNCTION__);
		return FGuid(); // Return invalid Guid
	}

	if (InTrackedActor && TrackedActorToMarkerIDMap.Contains(InTrackedActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Actor '%s' already has a registered marker. Skipping."), *GetName(), __FUNCTION__, *InTrackedActor->GetName());
		return TrackedActorToMarkerIDMap.FindRef(InTrackedActor);
	}

	// Create a new marker object
	UOBMapMarker* NewMarker = NewObject<UOBMapMarker>(this);
	const FGuid NewGuid = FGuid::NewGuid();

	NewMarker->Init(NewGuid, InTrackedActor, InConfig, InLayerName, InStaticLocation);

	// Add to our map and broadcast changes
	ActiveMarkersMap.Add(NewGuid, NewMarker);
	if (InTrackedActor)
	{
		TrackedActorToMarkerIDMap.Add(InTrackedActor, NewGuid);
	}

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

	// Tìm marker trước khi xóa
	if (const auto FoundMarkerPtr = ActiveMarkersMap.Find(MarkerID))
	{
		if (const UOBMapMarker* MarkerToRemove = *FoundMarkerPtr; MarkerToRemove && MarkerToRemove->TrackedActor.IsValid())
		{
			// Xóa khỏi map tra cứu ngược
			TrackedActorToMarkerIDMap.Remove(MarkerToRemove->TrackedActor.Get());
		}
	}

	if (ActiveMarkersMap.Remove(MarkerID) > 0)
	{
		// If removal was successful, update the cached array and notify the UI
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

FGuid UOBNavigationSubsystem::GetMarkerIDForActor(AActor* InActor) const
{
	if (InActor)
	{
		return TrackedActorToMarkerIDMap.FindRef(InActor);
	}
	return FGuid();
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

	// // --- DEBUG LOG ---
	// if (GEngine)
	// {
	// 	const FString DebugMsg = FString::Printf(
	// 		TEXT("[WorldToMapUV] WorldLoc: %s -> UV: %s"), *WorldLocation.ToString(), *OutMapUV.ToString());
	// 	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, DebugMsg);
	// }

	return true;
}

bool UOBNavigationSubsystem::Tick(float DeltaTime)
{
	// Get the world context
	const UWorld* MyWorld = GetWorld();
	if (!MyWorld)
	{
		return true; // Cannot proceed without a world, but keep the ticker alive
	}

	// Get the current network mode

	// Update the active layer based on the tracked pawn.
	// This is purely client-side visual logic and should not run on a dedicated server.
	// It will run on NM_Client (a client connected to a dedicated server)
	// and NM_ListenServer (the server that is also a player).
	// NM_Standalone is also effectively a client.
	if (const ENetMode NetMode = MyWorld->GetNetMode(); NetMode != NM_DedicatedServer)
	{
		if (TrackedPlayerPawn.IsValid())
		{
			UpdateActiveMinimapLayer();
		}
	}

	// Update all registered markers (position, lifetime, etc.).
	// This needs to run on all instances:
	// - Clients need it to update visual positions.
	// - Server needs it to manage authoritative markers (like Ping lifetime).
	UpdateAllMarkers(DeltaTime);

	return true; // Keep the ticker registered
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

void UOBNavigationSubsystem::UpdateAllMarkers(const float DeltaTime)
{
	// A list to store IDs of markers that need to be removed (e.g., expired lifetime)
	TArray<FGuid> MarkersToRemove;

	// Iterate through all active markers using the TMap for efficiency
	for (auto& Pair : ActiveMarkersMap)
	{
		UOBMapMarker* Marker = Pair.Value;
		if (!Marker)
		{
			// This case should be rare but good to handle. Mark for removal.
			MarkersToRemove.Add(Pair.Key);
			continue;
		}

		// --- 1. Update Position ---
		// Call the marker's own update logic.
		Marker->UpdateLocation();

		// --- 2. Update Lifetime ---
		// If the marker has a limited lifetime (e.g., Pings)
		if (Marker->CurrentLifeTime > 0.0f)
		{
			Marker->CurrentLifeTime -= DeltaTime;
			if (Marker->CurrentLifeTime <= 0.0f)
			{
				// Mark for removal if lifetime has expired
				MarkersToRemove.Add(Pair.Key);
			}
		}

		// --- 3. (Optional) Check for invalid tracked actors ---
		// If a marker is tracking an actor that has been destroyed.
		if (Marker->TrackedActor.IsStale() && !Marker->TrackedActor.IsValid())
		{
			// Depending on the design, you might want to remove the marker or keep it at its last known location.
			// For now, let's remove it.
			UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Tracked actor for marker %s is stale. Removing marker."), *GetName(),
			       __FUNCTION__, *Marker->MarkerID.ToString());
			MarkersToRemove.Add(Pair.Key);
		}
	}

	// --- Cleanup ---
	// Remove all markers that were marked for removal in a single batch operation.
	// This is safer than removing them during the loop.
	if (!MarkersToRemove.IsEmpty())
	{
		for (const FGuid& MarkerID : MarkersToRemove)
		{
			// Use the existing Unregister function, but we can optimize by not broadcasting for every single one.
			if (ActiveMarkersMap.Remove(MarkerID) > 0)
			{
				UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Automatically unregistered marker with ID: %s"), *GetName(),
				       __FUNCTION__, *MarkerID.ToString());
			}
		}

		// After removing, rebuild the array and broadcast a single update.
		RebuildActiveMarkersArray();
		OnMarkersUpdated.Broadcast();
	}
}
