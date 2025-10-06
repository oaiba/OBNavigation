// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OBMapMarker.generated.h"

/**
 * @struct FMarkerVisibilityOptions
 * @brief A struct to clearly define where a marker should be visible.
 * This replaces the enum bitmask for better stability and readability.
 */
USTRUCT(BlueprintType)
struct FMarkerVisibilityOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visibility")
	bool bShowOnMinimap = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visibility")
	bool bShowOnFullMap = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visibility")
	bool bShowOnCompass = false;
};

UCLASS(BlueprintType)
class OBNAVIGATION_API UOBMarkerConfigAsset : public UDataAsset
{
	GENERATED_BODY()

public:

	// The icon that identifies the object (e.g., a quest exclamation mark, a player number).
	// This part will NOT rotate.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	TObjectPtr<UTexture2D> IdentifierIconTexture;

	// The icon that indicates the direction (e.g., an arrow, a cone).
	// This part WILL rotate. If null, no directional indicator will be shown.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	TObjectPtr<UTexture2D> DirectionalIndicatorTexture;
	
	// The pivot point for the Directional Indicator's rotation, in normalized 0-1 space.
	// (0.5, 0.5) is the center. (0.5, 0.0) is the top-center edge.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	FVector2D IndicatorPivot = FVector2D(0.5f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	FVector2D Size = FVector2D(32.f, 32.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	FMarkerVisibilityOptions Visibility;
	
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

	// Remaining lifetime for temporary markers (e.g., pings)
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	float CurrentLifeTime;

	// Initializes the marker. Called by the subsystem.
	void Init(const FGuid& InID, AActor* InTrackedActor, UOBMarkerConfigAsset* InConfig, FName InLayerName,
			  FVector InStaticLocation = FVector::ZeroVector);

	// Updates the marker's world location (if tracking an actor)
	void UpdateLocation();

};
