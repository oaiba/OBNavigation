// Copyright Epic Games, Inc. All Rights Reserved.

#include "OBNavigation.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"

DEFINE_LOG_CATEGORY(LogOBNavigation);

namespace
{
	bool IsUsableNavigationLocation(const FVector& Location)
	{
		return !Location.ContainsNaN();
	}

	bool ShouldUseNavigationLocationCandidate(const FVector& CandidateLocation, const FVector& ActorLocation)
	{
		if (!IsUsableNavigationLocation(CandidateLocation))
		{
			return false;
		}

		// Reject the candidate if it is nearly zero (uninitialized state)
		// unless the ActorLocation itself is also nearly zero.
		if (CandidateLocation.IsNearlyZero() && !ActorLocation.IsNearlyZero())
		{
			return false;
		}

		return true;
	}

	FString FormatLocationCandidate(const TCHAR* Label, const FVector& Location)
	{
		return FString::Printf(TEXT("%s=%s"), Label, *Location.ToCompactString());
	}

	bool TryResolveActorNavigationLocation(const AActor* Actor, FVector& OutLocation, FString* OutSource)
	{
		if (!Actor)
		{
			OutLocation = FVector::ZeroVector;
			if (OutSource)
			{
				*OutSource = TEXT("NoActor");
			}
			return false;
		}

		const FVector ActorLocation = Actor->GetActorLocation();

		if (const UMoverComponent* MoverComponent = Actor->FindComponentByClass<UMoverComponent>())
		{
			if (const FMoverDefaultSyncState* DefaultSyncState =
				    MoverComponent->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
			{
				const FVector MoverSyncLocation = DefaultSyncState->GetLocation_WorldSpace();
				if (ShouldUseNavigationLocationCandidate(MoverSyncLocation, ActorLocation))
				{
					OutLocation = MoverSyncLocation;
					if (OutSource)
					{
						*OutSource = TEXT("MoverSyncState");
					}
					return true;
				}
			}

			const FVector UpdatedComponentLocation = MoverComponent->GetUpdatedComponentTransform().GetLocation();
			if (ShouldUseNavigationLocationCandidate(UpdatedComponentLocation, ActorLocation))
			{
				OutLocation = UpdatedComponentLocation;
				if (OutSource)
				{
					*OutSource = TEXT("MoverUpdatedTransform");
				}
				return true;
			}

			if (const USceneComponent* UpdatedComponent = MoverComponent->GetUpdatedComponent())
			{
				const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
				if (ShouldUseNavigationLocationCandidate(ComponentLocation, ActorLocation))
				{
					OutLocation = ComponentLocation;
					if (OutSource)
					{
						*OutSource = FString::Printf(TEXT("MoverUpdatedComponent:%s"), *GetNameSafe(UpdatedComponent));
					}
					return true;
				}
			}

			if (const USceneComponent* PrimaryVisualComponent = MoverComponent->GetPrimaryVisualComponent())
			{
				const FVector ComponentLocation = PrimaryVisualComponent->GetComponentLocation();
				if (ShouldUseNavigationLocationCandidate(ComponentLocation, ActorLocation))
				{
					OutLocation = ComponentLocation;
					if (OutSource)
					{
						*OutSource = FString::Printf(TEXT("MoverPrimaryVisualComponent:%s"),
						                             *GetNameSafe(PrimaryVisualComponent));
					}
					return true;
				}
			}
		}

		const FBox CollisionBounds = Actor->GetComponentsBoundingBox(false, false);
		if (CollisionBounds.IsValid)
		{
			const FVector BoundsCenter = CollisionBounds.GetCenter();
			if (!BoundsCenter.ContainsNaN())
			{
				OutLocation = BoundsCenter;
				if (OutSource)
				{
					*OutSource = TEXT("ComponentBoundsCenter");
				}
				return true;
			}
		}

		OutLocation = ActorLocation;
		if (OutSource)
		{
			*OutSource = TEXT("ActorLocation");
		}
		return true;
	}
}

FVector OBNavigation::ResolveActorNavigationLocation(const AActor* Actor)
{
	if (!Actor)
	{
		return FVector::ZeroVector;
	}

	FVector ResolvedLocation = FVector::ZeroVector;
	TryResolveActorNavigationLocation(Actor, ResolvedLocation, nullptr);
	return ResolvedLocation;
}

FString OBNavigation::DescribeActorNavigationLocationCandidates(const AActor* Actor)
{
	if (!Actor)
	{
		return TEXT("Actor=None");
	}

	TArray<FString> CandidateDescriptions;
	CandidateDescriptions.Add(FormatLocationCandidate(TEXT("Actor"), Actor->GetActorLocation()));

	if (const USceneComponent* RootComponent = Actor->GetRootComponent())
	{
		CandidateDescriptions.Add(FormatLocationCandidate(TEXT("Root"), RootComponent->GetComponentLocation()));
	}
	else
	{
		CandidateDescriptions.Add(TEXT("Root=None"));
	}

	if (const UMoverComponent* MoverComponent = Actor->FindComponentByClass<UMoverComponent>())
	{
		if (const FMoverDefaultSyncState* DefaultSyncState =
			    MoverComponent->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
		{
			CandidateDescriptions.Add(FormatLocationCandidate(TEXT("MoverSync"),
			                                                  DefaultSyncState->GetLocation_WorldSpace()));
			CandidateDescriptions.Add(FormatLocationCandidate(TEXT("MoverVelocity"),
			                                                  DefaultSyncState->GetVelocity_WorldSpace()));
			CandidateDescriptions.Add(FormatLocationCandidate(TEXT("MoverIntent"),
			                                                  DefaultSyncState->GetIntent_WorldSpace()));
		}
		else
		{
			CandidateDescriptions.Add(TEXT("MoverSync=None"));
		}

		CandidateDescriptions.Add(FString::Printf(TEXT("MoverMode='%s'"), *MoverComponent->GetMovementModeName().ToString()));
		CandidateDescriptions.Add(FormatLocationCandidate(TEXT("MoverUpdatedTransform"),
		                                                  MoverComponent->GetUpdatedComponentTransform().GetLocation()));
		CandidateDescriptions.Add(FString::Printf(TEXT("MoverUpdatedComp=%s"),
		                                          *GetNameSafe(MoverComponent->GetUpdatedComponent())));
		if (const USceneComponent* UpdatedComponent = MoverComponent->GetUpdatedComponent())
		{
			CandidateDescriptions.Add(FormatLocationCandidate(TEXT("MoverUpdatedCompLoc"),
			                                                  UpdatedComponent->GetComponentLocation()));
		}

		CandidateDescriptions.Add(FString::Printf(TEXT("MoverVisualComp=%s"),
		                                          *GetNameSafe(MoverComponent->GetPrimaryVisualComponent())));
		if (const USceneComponent* PrimaryVisualComponent = MoverComponent->GetPrimaryVisualComponent())
		{
			CandidateDescriptions.Add(FormatLocationCandidate(TEXT("MoverVisualCompLoc"),
			                                                  PrimaryVisualComponent->GetComponentLocation()));
		}
	}
	else
	{
		CandidateDescriptions.Add(TEXT("Mover=None"));
	}

	const FBox CollisionBounds = Actor->GetComponentsBoundingBox(false, false);
	if (CollisionBounds.IsValid)
	{
		CandidateDescriptions.Add(FormatLocationCandidate(TEXT("BoundsCenter"), CollisionBounds.GetCenter()));
	}
	else
	{
		CandidateDescriptions.Add(TEXT("Bounds=None"));
	}

	return FString::Join(CandidateDescriptions, TEXT(" "));
}

FString OBNavigation::DescribeActorNavigationLocationSource(const AActor* Actor)
{
	FVector ResolvedLocation = FVector::ZeroVector;
	FString Source;
	TryResolveActorNavigationLocation(Actor, ResolvedLocation, &Source);
	return FString::Printf(TEXT("%s Location=%s"), *Source, *ResolvedLocation.ToCompactString());
}

FString OBNavigation::DescribeActorNavigationComponentSnapshot(const AActor* Actor)
{
	if (!Actor)
	{
		return TEXT("Actor=None");
	}

	TInlineComponentArray<USceneComponent*> SceneComponents;
	Actor->GetComponents(SceneComponents);
	TArray<FString> ComponentDescriptions;
	constexpr int32 MaxComponentsToLog = 12;

	for (int32 Index = 0; Index < SceneComponents.Num() && Index < MaxComponentsToLog; ++Index)
	{
		const USceneComponent* SceneComponent = SceneComponents[Index];
		if (!SceneComponent)
		{
			continue;
		}

		FString PrimitiveDetails;
		if (const UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(SceneComponent))
		{
			PrimitiveDetails = FString::Printf(TEXT(" Collision=%s SimPhys=%s"),
			                                   *StaticEnum<ECollisionEnabled::Type>()->GetNameStringByValue(
				                                   static_cast<int64>(PrimitiveComponent->GetCollisionEnabled())),
			                                   PrimitiveComponent->IsSimulatingPhysics() ? TEXT("true") : TEXT("false"));
		}

		ComponentDescriptions.Add(FString::Printf(
			TEXT("#%d %s Class=%s Loc=%s RelLoc=%s Registered=%s Visible=%s%s"),
			Index,
			*GetNameSafe(SceneComponent),
			*GetNameSafe(SceneComponent->GetClass()),
			*SceneComponent->GetComponentLocation().ToCompactString(),
			*SceneComponent->GetRelativeLocation().ToCompactString(),
			SceneComponent->IsRegistered() ? TEXT("true") : TEXT("false"),
			SceneComponent->IsVisible() ? TEXT("true") : TEXT("false"),
			*PrimitiveDetails));
	}

	if (SceneComponents.Num() > MaxComponentsToLog)
	{
		ComponentDescriptions.Add(FString::Printf(TEXT("...%d more scene components"), SceneComponents.Num() - MaxComponentsToLog));
	}

	return ComponentDescriptions.Num() > 0 ? FString::Join(ComponentDescriptions, TEXT(" | ")) : TEXT("NoSceneComponents");
}

#define LOCTEXT_NAMESPACE "FOBNavigationModule"

void FOBNavigationModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FOBNavigationModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FOBNavigationModule, OBNavigation)
