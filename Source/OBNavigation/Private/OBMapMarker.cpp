// Fill out your copyright notice in the Description page of Project Settings.


#include "OBMapMarker.h"

#include "OBMapMarker.h" // Make sure this include is at the top

// ... other includes if you have them ...

void UOBMapMarker::Init(const FGuid& InID, AActor* InTrackedActor, UOBMarkerConfigAsset* InConfig, FName InLayerName, FVector InStaticLocation)
{
	// Assign all the provided data to this marker instance
	this->MarkerID = InID;
	this->TrackedActor = InTrackedActor;
	this->ConfigAsset = InConfig;
	this->MarkerLayerName = InLayerName;
	
	// If we are tracking an actor, get its initial location.
	// Otherwise, use the provided static location.
	if (InTrackedActor)
	{
		this->WorldLocation = InTrackedActor->GetActorLocation();
	}
	else
	{
		this->WorldLocation = InStaticLocation;
	}

	// Set the lifetime based on the config. If 0, it's infinite.
	if (ConfigAsset)
	{
		this->CurrentLifeTime = ConfigAsset->LifeTime;
	}
}

// You should also add the implementation for other functions declared in the header
void UOBMapMarker::UpdateLocation()
{
	// If this marker is tracking a valid actor, update its WorldLocation
	if (TrackedActor.IsValid())
	{
		WorldLocation = TrackedActor->GetActorLocation();
	}
}
