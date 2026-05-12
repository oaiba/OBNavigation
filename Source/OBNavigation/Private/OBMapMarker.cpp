// Fill out your copyright notice in the Description page of Project Settings.


#include "OBMapMarker.h"

void UOBMapMarker::Init(const FGuid& InID, AActor* InTrackedActor, UOBMarkerConfigAsset* InConfig, FName InLayerName, FVector InStaticLocation)
{
	FOBNavigationMarkerSpec Spec;
	Spec.MarkerId = InID;
	Spec.TrackedActor = InTrackedActor;
	Spec.ConfigAsset = InConfig;
	Spec.LayerName = InLayerName;
	Spec.WorldLocation = InStaticLocation;
	Spec.WorldRotation = InTrackedActor ? InTrackedActor->GetActorRotation() : FRotator::ZeroRotator;
	Spec.LifeTime = InConfig ? InConfig->LifeTime : 0.0f;
	InitFromSpec(Spec);
}

void UOBMapMarker::InitFromSpec(const FOBNavigationMarkerSpec& InSpec)
{
	MarkerID = InSpec.MarkerId.IsValid() ? InSpec.MarkerId : FGuid::NewGuid();
	ApplySpec(InSpec);
}

void UOBMapMarker::ApplySpec(const FOBNavigationMarkerSpec& InSpec)
{
	TrackedActor = InSpec.TrackedActor;
	ConfigAsset = InSpec.ConfigAsset;
	MarkerLayerName = InSpec.LayerName;
	MarkerType = InSpec.MarkerType;
	OwnerPlayerId = InSpec.OwnerPlayerId;
	TeamId = InSpec.TeamId;
	VisibilityPolicy = InSpec.VisibilityPolicy;
	SortPriority = InSpec.SortPriority;
	CurrentLifeTime = InSpec.LifeTime > 0.0f ? InSpec.LifeTime : (ConfigAsset ? ConfigAsset->LifeTime : 0.0f);

	if (TrackedActor.IsValid())
	{
		WorldLocation = TrackedActor->GetActorLocation();
		WorldRotation = TrackedActor->GetActorRotation();
	}
	else
	{
		WorldLocation = InSpec.WorldLocation;
		WorldRotation = InSpec.WorldRotation;
	}
}

void UOBMapMarker::UpdateLocation()
{
	if (TrackedActor.IsValid())
	{
		WorldLocation = TrackedActor->GetActorLocation();
		WorldRotation = TrackedActor->GetActorRotation();
	}
}

bool UOBMapMarker::IsVisibleOnSurface(const EOBNavigationSurface Surface) const
{
	if (!ConfigAsset)
	{
		return false;
	}

	switch (Surface)
	{
	case EOBNavigationSurface::Minimap:
		return ConfigAsset->Visibility.bShowOnMinimap;
	case EOBNavigationSurface::FullMap:
		return ConfigAsset->Visibility.bShowOnFullMap;
	case EOBNavigationSurface::Compass:
		return ConfigAsset->Visibility.bShowOnCompass;
	default:
		return false;
	}
}
