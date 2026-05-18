// Fill out your copyright notice in the Description page of Project Settings.


#include "OBNavigationSubsystem.h"

#include "OBNavigation.h"
#include "OBNavigationDeveloperSettings.h"
#include "Data/OBNavigationMapRegistryAsset.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

namespace
{
	FString FormatNavigationActorBrief(const TCHAR* Label, const AActor* Actor)
	{
		if (!Actor)
		{
			return FString::Printf(TEXT("%s=None"), Label);
		}

		return FString::Printf(TEXT("%s='%s' Class='%s' ActorLoc=%s Resolved=[%s]"),
		                       Label,
		                       *GetNameSafe(Actor),
		                       *GetNameSafe(Actor->GetClass()),
		                       *Actor->GetActorLocation().ToCompactString(),
		                       *OBNavigation::DescribeActorNavigationLocationSource(Actor));
	}

	FString DescribeLocalControllerNavigationContext(const UWorld* World)
	{
		if (!World)
		{
			return TEXT("World=None");
		}

		TArray<FString> ControllerDescriptions;
		int32 ControllerIndex = 0;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PlayerController = It->Get();
			if (!PlayerController)
			{
				continue;
			}

			const AActor* ViewTarget = PlayerController->GetViewTarget();
			const APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
			ControllerDescriptions.Add(FString::Printf(
				TEXT("PC%d='%s' Local=%s ControlRot=%s %s %s CameraLoc=%s"),
				ControllerIndex,
				*GetNameSafe(PlayerController),
				PlayerController->IsLocalController() ? TEXT("true") : TEXT("false"),
				*PlayerController->GetControlRotation().ToCompactString(),
				*FormatNavigationActorBrief(TEXT("Pawn"), PlayerController->GetPawn()),
				*FormatNavigationActorBrief(TEXT("ViewTarget"), ViewTarget),
				CameraManager ? *CameraManager->GetCameraLocation().ToCompactString() : TEXT("None")));
			++ControllerIndex;
		}

		return ControllerDescriptions.Num() > 0 ? FString::Join(ControllerDescriptions, TEXT(" | ")) : TEXT("NoPlayerControllers");
	}
}

void UOBNavigationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LoadRegistryData();

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

void UOBNavigationSubsystem::LoadRegistryData()
{
	MarkerConfigsByTag.Reset();
	bShowDebugMarkers = false;

	const UOBNavigationDeveloperSettings* Settings = GetDefault<UOBNavigationDeveloperSettings>();
	bShowDebugMarkers = Settings && Settings->bShowDebugMarkers;

	UOBNavigationMapRegistryAsset* Registry = Settings ? Settings->DefaultMapRegistry.LoadSynchronous() : nullptr;
	if (!Registry)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("[%s::%hs] - No DefaultMapRegistry configured. OBNavigation will run, but marker tag lookup must be provided at runtime."),
		       *GetName(), __FUNCTION__);
		return;
	}

	for (const FOBNavigationMarkerConfigEntry& Entry : Registry->MarkerConfigs)
	{
		if (Entry.MarkerType.IsValid() && Entry.Config)
		{
			MarkerConfigsByTag.Add(Entry.MarkerType, Entry.Config);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Loaded %d marker configs from registry '%s'."),
	       *GetName(), __FUNCTION__, MarkerConfigsByTag.Num(), *Registry->GetName());
}

void UOBNavigationSubsystem::RebuildMapLayerSpecs()
{
	AllMapLayerSpecs.Reset();
	AllMapLayerSpecs.Append(RuntimeMapLayerSpecs);
	AllMapLayerSpecs.Sort([](const FOBNavigationMapLayerSpec& A, const FOBNavigationMapLayerSpec& B)
	{
		return A.Priority > B.Priority;
	});
}

void UOBNavigationSubsystem::SetTrackedPlayerPawn(APawn* PlayerPawn)
{
	if (PlayerPawn)
	{
		TrackedPlayerPawn = PlayerPawn;
		StartupTrackedPawnTraceLogCount = 0;
		UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Now tracking pawn: %s"), *GetName(), __FUNCTION__,
		       *PlayerPawn->GetName());
		UE_LOG(LogOBNavigation, Log,
		       TEXT("[%s::%hs] - TrackTrace Pawn='%s' ActorLoc=%s ResolveSource=[%s] LocationCandidates=[%s] ComponentSnapshot=[%s]"),
		       *GetName(), __FUNCTION__, *GetNameSafe(PlayerPawn), *PlayerPawn->GetActorLocation().ToString(),
		       *OBNavigation::DescribeActorNavigationLocationSource(PlayerPawn),
		       *OBNavigation::DescribeActorNavigationLocationCandidates(PlayerPawn),
		       *OBNavigation::DescribeActorNavigationComponentSnapshot(PlayerPawn));
		UE_LOG(LogOBNavigation, Log,
		       TEXT("[%s::%hs] - TrackControllerTrace Pawn='%s' ControllerContext=[%s]"),
		       *GetName(), __FUNCTION__, *GetNameSafe(PlayerPawn),
		       *DescribeLocalControllerNavigationContext(GetWorld()));
		// Force an immediate update
		UpdateActiveMinimapLayer();
	}
	else
	{
		TrackedPlayerPawn.Reset();
		if (bHasCurrentMapLayerSpec)
		{
			CurrentMapLayerSpec = FOBNavigationMapLayerSpec();
			bHasCurrentMapLayerSpec = false;
			OnNavigationMapLayerSpecChanged.Broadcast(CurrentMapLayerSpec);
		}
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Stopped tracking pawn."), *GetName(), __FUNCTION__);
	}
}

bool UOBNavigationSubsystem::GetCurrentMapLayerSpec(FOBNavigationMapLayerSpec& OutLayerSpec) const
{
	if (!bHasCurrentMapLayerSpec)
	{
		OutLayerSpec = FOBNavigationMapLayerSpec();
		return false;
	}

	OutLayerSpec = CurrentMapLayerSpec;
	return true;
}

void UOBNavigationSubsystem::GetAvailableMapLayerSpecs(TArray<FOBNavigationMapLayerSpec>& OutLayerSpecs) const
{
	OutLayerSpecs = AllMapLayerSpecs;
}

void UOBNavigationSubsystem::SetRuntimeMapLayers(const TArray<FOBNavigationMapLayerSpec>& InMapLayers)
{
	RuntimeMapLayerSpecs.Reset();
	for (FOBNavigationMapLayerSpec LayerSpec : InMapLayers)
	{
		if (LayerSpec.LayerName.IsNone())
		{
			LayerSpec.LayerName = LayerSpec.MapTexture ? LayerSpec.MapTexture->GetFName() : NAME_None;
		}
		RuntimeMapLayerSpecs.Add(LayerSpec);
	}

	RebuildMapLayerSpecs();
	UpdateActiveMinimapLayer();
}

void UOBNavigationSubsystem::ClearRuntimeMapLayers()
{
	RuntimeMapLayerSpecs.Reset();
	RebuildMapLayerSpecs();
	UpdateActiveMinimapLayer();
}

void UOBNavigationSubsystem::SetLocalNavigationContext(const int32 InLocalPlayerId, const int32 InLocalTeamId)
{
	LocalPlayerId = InLocalPlayerId;
	LocalTeamId = InLocalTeamId;
	OnMarkersUpdated.Broadcast();
}

FGuid UOBNavigationSubsystem::RegisterOrUpdateMarker(const FOBNavigationMarkerSpec& MarkerSpec)
{
	FOBNavigationMarkerSpec ResolvedSpec = MarkerSpec;
	ResolvedSpec.ConfigAsset = ResolveMarkerConfig(MarkerSpec);

	if (!ResolvedSpec.ConfigAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Failed to register marker: no config for marker type '%s'."),
		       *GetName(), __FUNCTION__, *ResolvedSpec.MarkerType.ToString());
		return FGuid();
	}

	if (!ResolvedSpec.MarkerId.IsValid() && ResolvedSpec.TrackedActor && TrackedActorToMarkerIDMap.Contains(ResolvedSpec.TrackedActor))
	{
		ResolvedSpec.MarkerId = TrackedActorToMarkerIDMap.FindRef(ResolvedSpec.TrackedActor);
	}

	const FGuid MarkerID = ResolvedSpec.MarkerId.IsValid() ? ResolvedSpec.MarkerId : FGuid::NewGuid();
	ResolvedSpec.MarkerId = MarkerID;

	if (UOBMapMarker* ExistingMarker = ActiveMarkersMap.FindRef(MarkerID))
	{
		if (ExistingMarker->TrackedActor.IsValid() && ExistingMarker->TrackedActor.Get() != ResolvedSpec.TrackedActor)
		{
			TrackedActorToMarkerIDMap.Remove(ExistingMarker->TrackedActor.Get());
		}

		ExistingMarker->ApplySpec(ResolvedSpec);
		if (ResolvedSpec.TrackedActor)
		{
			TrackedActorToMarkerIDMap.Add(ResolvedSpec.TrackedActor, MarkerID);
		}
	}
	else
	{
		UOBMapMarker* NewMarker = NewObject<UOBMapMarker>(this);
		NewMarker->InitFromSpec(ResolvedSpec);
		ActiveMarkersMap.Add(MarkerID, NewMarker);
		if (ResolvedSpec.TrackedActor)
		{
			TrackedActorToMarkerIDMap.Add(ResolvedSpec.TrackedActor, MarkerID);
		}
	}

	RebuildActiveMarkersArray();
	OnMarkersUpdated.Broadcast();
	return MarkerID;
}

void UOBNavigationSubsystem::UnregisterMarker(const FGuid& MarkerID)
{
	if (UnregisterMarkerInternal(MarkerID))
	{
		RebuildActiveMarkersArray();
		OnMarkersUpdated.Broadcast();
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
	ActiveMarkers.Sort([](const UOBMapMarker& A, const UOBMapMarker& B)
	{
		return A.SortPriority > B.SortPriority;
	});
}

bool UOBNavigationSubsystem::WorldToMapUVChecked(const FOBNavigationMapLayerSpec& MapLayerSpec,
                                                 const FVector& WorldLocation, FVector2D& OutMapUV,
                                                 EOBMapProjectionResult& OutResult) const
{
	if (!MapLayerSpec.HasValidWorldBounds())
	{
		OutResult = EOBMapProjectionResult::InvalidBounds;
		return false;
	}

	return MapLayerSpec.ProjectWorldToMapUVChecked(WorldLocation, OutMapUV, OutResult);
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

			const FVector TrackedPawnLocation = OBNavigation::ResolveActorNavigationLocation(TrackedPlayerPawn.Get());
			const bool bTrackedPawnHasNoXY = FMath::Abs(TrackedPawnLocation.X) <= 1.0f
				&& FMath::Abs(TrackedPawnLocation.Y) <= 1.0f;
			const float CurrentTime = MyWorld->GetTimeSeconds();
			if (bTrackedPawnHasNoXY
				&& ZeroXYFollowTraceLogCount < 20
				&& CurrentTime - LastZeroXYFollowTraceTime >= 1.0f)
			{
				LastZeroXYFollowTraceTime = CurrentTime;
				++ZeroXYFollowTraceLogCount;
				UE_LOG(LogOBNavigation, Warning,
				       TEXT("[%s::%hs] - ZeroXYFollowTrace #%d TrackedPawn='%s' ResolvedLoc=%s ResolveSource=[%s] ControllerContext=[%s]"),
				       *GetName(), __FUNCTION__, ZeroXYFollowTraceLogCount,
				       *GetNameSafe(TrackedPlayerPawn.Get()), *TrackedPawnLocation.ToString(),
				       *OBNavigation::DescribeActorNavigationLocationSource(TrackedPlayerPawn.Get()),
				       *DescribeLocalControllerNavigationContext(MyWorld));
			}
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
		if (IsDifferentCurrentLayer(nullptr))
		{
			CurrentMapLayerSpec = FOBNavigationMapLayerSpec();
			bHasCurrentMapLayerSpec = false;
			OnNavigationMapLayerSpecChanged.Broadcast(CurrentMapLayerSpec);
		}
		return;
	}

	const FVector PawnLocation = OBNavigation::ResolveActorNavigationLocation(TrackedPlayerPawn.Get());
	const FOBNavigationMapLayerSpec* BestLayerSpec = nullptr;

	// Since the array is pre-sorted by priority, the first valid layer we find is the best one.
	for (const FOBNavigationMapLayerSpec& LayerSpec : AllMapLayerSpecs)
	{
		if (LayerSpec.ContainsWorldLocationXY(PawnLocation))
		{
			BestLayerSpec = &LayerSpec;
			break; // Found the highest priority layer
		}
	}

	static float LastLayerTraceLogTime = -1000.0f;
	const UWorld* World = GetWorld();
	const float CurrentTime = World ? World->GetTimeSeconds() : 0.0f;
	if (StartupTrackedPawnTraceLogCount < 5 || CurrentTime - LastLayerTraceLogTime >= 1.0f)
	{
		LastLayerTraceLogTime = CurrentTime;
		++StartupTrackedPawnTraceLogCount;
		UE_LOG(LogOBNavigation, Log,
		       TEXT("[%s::%hs] - LayerTrace #%d Pawn='%s' ResolvedLoc=%s ResolveSource=[%s] BestLayer='%s' CandidateLayerCount=%d CurrentLayer='%s' LocationCandidates=[%s] ComponentSnapshot=[%s]"),
		       *GetName(), __FUNCTION__, StartupTrackedPawnTraceLogCount,
		       *GetNameSafe(TrackedPlayerPawn.Get()), *PawnLocation.ToString(),
		       *OBNavigation::DescribeActorNavigationLocationSource(TrackedPlayerPawn.Get()),
		       BestLayerSpec ? *BestLayerSpec->LayerName.ToString() : TEXT("None"), AllMapLayerSpecs.Num(),
		       bHasCurrentMapLayerSpec ? *CurrentMapLayerSpec.LayerName.ToString() : TEXT("None"),
		       *OBNavigation::DescribeActorNavigationLocationCandidates(TrackedPlayerPawn.Get()),
		       *OBNavigation::DescribeActorNavigationComponentSnapshot(TrackedPlayerPawn.Get()));
	}

	// If the best layer has changed, update it and notify listeners
	if (IsDifferentCurrentLayer(BestLayerSpec))
	{
		if (BestLayerSpec)
		{
			CurrentMapLayerSpec = *BestLayerSpec;
			bHasCurrentMapLayerSpec = true;
		}
		else
		{
			CurrentMapLayerSpec = FOBNavigationMapLayerSpec();
			bHasCurrentMapLayerSpec = false;
		}

		UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Minimap layer changed to: %s"), *GetName(), __FUNCTION__,
		       BestLayerSpec ? *BestLayerSpec->LayerName.ToString() : TEXT("None"));
		OnNavigationMapLayerSpecChanged.Broadcast(CurrentMapLayerSpec);
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
			UnregisterMarkerInternal(MarkerID);
		}

		// After removing, rebuild the array and broadcast a single update.
		RebuildActiveMarkersArray();
		OnMarkersUpdated.Broadcast();
	}
}

bool UOBNavigationSubsystem::UnregisterMarkerInternal(const FGuid& MarkerID)
{
	if (!MarkerID.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Attempted to unregister an invalid marker ID."), *GetName(),
		       __FUNCTION__);
		return false;
	}

	if (const UOBMapMarker* MarkerToRemove = ActiveMarkersMap.FindRef(MarkerID))
	{
		if (MarkerToRemove->TrackedActor.IsValid())
		{
			TrackedActorToMarkerIDMap.Remove(MarkerToRemove->TrackedActor.Get());
		}
	}

	const bool bRemoved = ActiveMarkersMap.Remove(MarkerID) > 0;
	if (!bRemoved)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Could not find marker with ID to unregister: %s"), *GetName(),
		       __FUNCTION__, *MarkerID.ToString());
	}
	return bRemoved;
}

bool UOBNavigationSubsystem::IsMarkerVisibleForLocalPlayer(const UOBMapMarker* Marker) const
{
	if (!Marker)
	{
		return false;
	}

	switch (Marker->VisibilityPolicy)
	{
	case EOBMarkerVisibilityPolicy::LocalOnly:
		return Marker->OwnerPlayerId == INDEX_NONE || Marker->OwnerPlayerId == LocalPlayerId;
	case EOBMarkerVisibilityPolicy::SquadOnly:
		return Marker->TeamId != INDEX_NONE && Marker->TeamId == LocalTeamId;
	case EOBMarkerVisibilityPolicy::Public:
		return true;
	case EOBMarkerVisibilityPolicy::DebugOnly:
		return bShowDebugMarkers;
	default:
		return false;
	}
}

UOBMarkerConfigAsset* UOBNavigationSubsystem::ResolveMarkerConfig(const FOBNavigationMarkerSpec& MarkerSpec) const
{
	if (MarkerSpec.ConfigAsset)
	{
		return MarkerSpec.ConfigAsset;
	}

	if (MarkerSpec.MarkerType.IsValid())
	{
		return MarkerConfigsByTag.FindRef(MarkerSpec.MarkerType);
	}

	return nullptr;
}

TArray<UOBMapMarker*> UOBNavigationSubsystem::GetVisibleMarkers(const EOBNavigationSurface Surface) const
{
	TArray<UOBMapMarker*> Result;
	for (UOBMapMarker* Marker : ActiveMarkers)
	{
		if (Marker && Marker->IsVisibleOnSurface(Surface) && IsMarkerVisibleForLocalPlayer(Marker))
		{
			Result.Add(Marker);
		}
	}
	return Result;
}

TArray<FOBNavigationOverlayElement> UOBNavigationSubsystem::GetVisibleOverlayElements(
	const EOBNavigationSurface Surface, const FName CategoryFilter, const FName TagFilter) const
{
	TArray<FOBNavigationOverlayElement> Result;
	if (!bHasCurrentMapLayerSpec || Surface == EOBNavigationSurface::Compass)
	{
		return Result;
	}

	for (const FOBNavigationOverlayLayer& Layer : CurrentMapLayerSpec.OverlayLayers)
	{
		if (!Layer.bVisibleByDefault)
		{
			continue;
		}

		for (const FOBNavigationOverlayElement& Element : Layer.Elements)
		{
			if (!Element.bVisibleByDefault)
			{
				continue;
			}
			if (!CategoryFilter.IsNone() && Element.Category != CategoryFilter)
			{
				continue;
			}
			if (!TagFilter.IsNone() && !Element.FilterTags.Contains(TagFilter))
			{
				continue;
			}

			Result.Add(Element);
		}
	}

	return Result;
}

bool UOBNavigationSubsystem::IsDifferentCurrentLayer(const FOBNavigationMapLayerSpec* NewLayerSpec) const
{
	if (!NewLayerSpec)
	{
		return bHasCurrentMapLayerSpec;
	}

	if (!bHasCurrentMapLayerSpec)
	{
		return true;
	}

	return CurrentMapLayerSpec.LayerName != NewLayerSpec->LayerName
		|| CurrentMapLayerSpec.MapTexture != NewLayerSpec->MapTexture
		|| CurrentMapLayerSpec.WorldBounds.Min != NewLayerSpec->WorldBounds.Min
		|| CurrentMapLayerSpec.WorldBounds.Max != NewLayerSpec->WorldBounds.Max
		|| CurrentMapLayerSpec.Priority != NewLayerSpec->Priority
		|| !FMath::IsNearlyEqual(CurrentMapLayerSpec.MapRotationDegrees, NewLayerSpec->MapRotationDegrees);
}
