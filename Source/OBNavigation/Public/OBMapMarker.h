// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OBMapMarker.generated.h"

// Enum to define where a marker can be displayed
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMasksInEditor = "true"))
enum class EOBMarkerVisibility : uint8
{
	None = 0 UMETA(Hidden),
	Minimap = 1 << 0,
	FullMap = 1 << 1,
	Compass = 1 << 2,
};

ENUM_CLASS_FLAGS(EOBMarkerVisibility);

UCLASS(BlueprintType)
class OBNAVIGATION_API UOBMarkerConfigAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	TObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	FVector2D Size = FVector2D(32.f, 32.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config",
		meta=(Bitmask, BitmaskEnum = "/Script/OBNavigation.EOBMarkerVisibility"))
	uint8 VisibilityFilter;

	// Optional: For markers that should disappear after a duration (like pings)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	float LifeTime = 0.0f; // 0.0 means infinite
};

/**
 * @class UOBMapMarker
 * @brief Represents a single marker object on the map/compass.
 * This object is owned and managed by the OBNavigationSubsystem.
 */
UCLASS(BlueprintType, Blueprintable)
class OBNAVIGATION_API UOBMapMarker : public UObject
{
	GENERATED_BODY()

public:
	// Unique ID for this marker instance
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Marker")
	FGuid MarkerID;

	// The world location of the marker. Updated every frame if attached to an actor.
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	FVector WorldLocation;

	// Optional: The actor this marker is tracking. If null, WorldLocation is static.
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	TWeakObjectPtr<AActor> TrackedActor;

	// The configuration asset that defines this marker's appearance
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	TObjectPtr<UOBMarkerConfigAsset> ConfigAsset;

	// The name of the logical layer this marker belongs to (e.g., "Quests", "PartyMembers", "Pings")
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	FName MarkerLayerName;

	// Remaining life time for temporary markers (e.g., pings)
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	float CurrentLifeTime;

	// Initializes the marker. Called by the subsystem.
	void Init(const FGuid& InID, AActor* InTrackedActor, UOBMarkerConfigAsset* InConfig, FName InLayerName,
			  FVector InStaticLocation = FVector::ZeroVector);

	// Updates the marker's world location (if tracking an actor)
	void UpdateLocation();

	// Checks if this marker should be visible on a specific view
	bool IsVisibleOn(EOBMarkerVisibility ViewType) const;
};
