#include "OBNavigationSourceComponent.h"

#include "OBNavigationSubsystem.h"

UOBNavigationSourceComponent::UOBNavigationSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOBNavigationSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		NavSubsystem = GameInstance->GetSubsystem<UOBNavigationSubsystem>();
	}

	if (bRegisterOnBeginPlay)
	{
		RegisterOrUpdateNavigationMarker();
	}
}

void UOBNavigationSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterNavigationMarker();
	Super::EndPlay(EndPlayReason);
}

FGuid UOBNavigationSourceComponent::RegisterOrUpdateNavigationMarker()
{
	if (!NavSubsystem)
	{
		return FGuid();
	}

	FOBNavigationMarkerSpec Spec;
	Spec.MarkerId = MarkerId;
	Spec.MarkerType = MarkerType;
	Spec.LayerName = LayerName;
	Spec.ConfigAsset = MarkerConfig;
	Spec.TrackedActor = bTrackOwner ? GetOwner() : nullptr;
	Spec.WorldLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	Spec.WorldRotation = GetOwner() ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator;
	Spec.OwnerPlayerId = OwnerPlayerId;
	Spec.TeamId = TeamId;
	Spec.VisibilityPolicy = VisibilityPolicy;
	Spec.SortPriority = SortPriority;

	MarkerId = NavSubsystem->RegisterOrUpdateMarker(Spec);
	return MarkerId;
}

void UOBNavigationSourceComponent::UnregisterNavigationMarker()
{
	if (NavSubsystem && MarkerId.IsValid())
	{
		NavSubsystem->UnregisterMarker(MarkerId);
		MarkerId.Invalidate();
	}
}
