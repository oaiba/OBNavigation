// Fill out your copyright notice in the Description page of Project Settings.


#include "OBNavigationComponent.h"

#include "OBNavigation.h"
#include "OBNavigationSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"


UOBNavigationComponent::UOBNavigationComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // Not ticking this component directly, subsystem handles updates
	bWantsInitializeComponent = true;
}

void UOBNavigationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		NavSubsystem = GI->GetSubsystem<UOBNavigationSubsystem>();
	}

	if (!NavSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - OBNavigationSubsystem is not valid! Cannot perform navigation tasks."),
			   *GetName(), __FUNCTION__);
		return;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return;

	// --- Handle tracking for a local player ---
	// The local player's pawn is tracked by the subsystem for minimap/compass display.
	if (OwnerCharacter->IsLocallyControlled())
	{
		NavSubsystem->SetTrackedPlayerPawn(OwnerCharacter);
		UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Local player '%s' assigned to OBNavigationSubsystem."), *GetName(),
			   __FUNCTION__, *OwnerCharacter->GetName());
	}

	// --- Handle character marker registration ---
	// Register a marker for this character if it should appear on maps for other players/itself.
	// This usually happens on all clients for other players (proxies) and on the owning client for itself.
	if (ShouldRegisterCharacterMarker())
	{
		RegisterCharacterMarker();
	}
}

void UOBNavigationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterCharacterMarker();

	// If this was the tracked player, clear it from the subsystem
	if (NavSubsystem && GetOwner() == NavSubsystem->GetTrackedPlayerPawn())
	{
		NavSubsystem->SetTrackedPlayerPawn(nullptr);
	}

	Super::EndPlay(EndPlayReason);
}

void UOBNavigationComponent::RegisterCharacterMarker()
{
	if (!NavSubsystem || !CharacterMapMarkerConfig || !GetOwner())
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("[%s::%hs] - Failed to register character marker for '%s'. Subsystem, config, or owner is invalid."
			   ), *GetName(), __FUNCTION__, *GetNameSafe(GetOwner()));
		return;
	}

	// Ensure we only register once
	if (!CharacterMarkerID.IsValid())
	{
			const APawn* OwnerPawn = Cast<APawn>(GetOwner());
			FOBNavigationMarkerSpec Spec;
			Spec.MarkerId = CharacterMarkerID;
				Spec.TrackedActor = GetOwner();
				Spec.ConfigAsset = CharacterMapMarkerConfig;
				Spec.LayerName = CharacterMapMarkerLayerName;
				Spec.WorldLocation = OBNavigation::ResolveActorNavigationLocation(GetOwner());
				Spec.WorldRotation = GetOwner()->GetActorRotation();
			Spec.VisibilityPolicy = EOBMarkerVisibilityPolicy::LocalOnly;
			Spec.OwnerPlayerId = OwnerPawn && OwnerPawn->GetPlayerState() ? OwnerPawn->GetPlayerState()->GetPlayerId() : INDEX_NONE;
			Spec.SortPriority = 100;
			CharacterMarkerID = NavSubsystem->RegisterOrUpdateMarker(Spec);
		if (CharacterMarkerID.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Registered character marker for '%s' (ID: %s)."), *GetName(),
				   __FUNCTION__, *GetNameSafe(GetOwner()), *CharacterMarkerID.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Error,
				   TEXT("[%s::%hs] - Failed to register character marker for '%s'. Subsystem returned invalid ID."),
				   *GetName(), __FUNCTION__, *GetNameSafe(GetOwner()));
		}
	}
}

void UOBNavigationComponent::UnregisterCharacterMarker()
{
	if (NavSubsystem && CharacterMarkerID.IsValid())
	{
		NavSubsystem->UnregisterMarker(CharacterMarkerID);
		UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Unregistered character marker for '%s' (ID: %s)."), *GetName(),
			   __FUNCTION__, *GetNameSafe(GetOwner()), *CharacterMarkerID.ToString());
		CharacterMarkerID.Invalidate();
	}
}

bool UOBNavigationComponent::ShouldRegisterCharacterMarker() const
{
	// On a dedicated server, no need to display character markers
	if (GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return false;

	return OwnerCharacter->IsLocallyControlled();
}
