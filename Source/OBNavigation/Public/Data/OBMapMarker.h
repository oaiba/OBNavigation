// Copyright OBExtraction. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/OBNavigationTypes.h"
#include "UObject/Object.h"
#include "OBMapMarker.generated.h"

/** Per-surface visibility flags for a marker config asset. */
USTRUCT(BlueprintType)
struct FMarkerVisibilityOptions
{
	GENERATED_BODY()

	/** Whether markers using this config are visible on the minimap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visibility")
	bool bShowOnMinimap = false;

	/** Whether markers using this config are visible on the tactical full map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visibility")
	bool bShowOnFullMap = false;

	/** Whether markers using this config are visible on the compass surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visibility")
	bool bShowOnCompass = false;

	/** Constructs visibility options with explicit per-surface flags. */
	FMarkerVisibilityOptions(const bool bShowOnMinimap, const bool bShowOnFullMap, const bool bShowOnCompass)
		: bShowOnMinimap(bShowOnMinimap),
		  bShowOnFullMap(bShowOnFullMap),
		  bShowOnCompass(bShowOnCompass)
	{
	}

	/** Constructs hidden-by-default visibility options. */
	FMarkerVisibilityOptions()
	{
	}
};

/** Visual configuration for marker icon, indicator, size, color, and lifetime. */
UCLASS(BlueprintType)
class OBNAVIGATION_API UOBMarkerConfigAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Icon that identifies the object. This part does not rotate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	TObjectPtr<UTexture2D> IdentifierIconTexture;

	/** Material for the rotating directional indicator. Null disables the indicator. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	TObjectPtr<UMaterialInterface> IndicatorMaterial;

	/** Directional indicator pivot in normalized [0, 1] widget space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	FVector2D IndicatorPivot = FVector2D(0.5f, 0.5f);

	/** Marker widget size in Slate units before surface-specific scaling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	FVector2D Size = FVector2D(32.f, 32.f);

	/** Tint applied to marker icon and indicator. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	FLinearColor Color = FLinearColor::White;

	/** Per-surface visibility options. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	FMarkerVisibilityOptions Visibility = FMarkerVisibilityOptions(true, true, true);

	/** Optional lifetime in seconds for temporary markers. A value of 0 means infinite. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Marker Config")
	float LifeTime = 0.0f;
};

/** Runtime marker object owned and updated by UOBNavigationSubsystem. */
UCLASS(BlueprintType, Blueprintable)
class OBNAVIGATION_API UOBMapMarker : public UObject
{
	GENERATED_BODY()

public:
	/** Unique ID for this marker instance. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Marker")
	FGuid MarkerID;

	/** Current world location. Updated from TrackedActor when one is assigned. */
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	FVector WorldLocation;

	/** Actor this marker follows. Null means WorldLocation is static. */
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	TWeakObjectPtr<AActor> TrackedActor;

	/** Visual config that defines this marker's appearance and surface visibility. */
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	TObjectPtr<UOBMarkerConfigAsset> ConfigAsset;

	/** Logical marker layer name used by tactical filters. */
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	FName MarkerLayerName;

	/** Marker type tag originally used to resolve the config. */
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	FGameplayTag MarkerType;

	/** Current world rotation used by directional marker indicators. */
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	FRotator WorldRotation = FRotator::ZeroRotator;

	/** Owner player ID used by visibility policy filtering. */
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	int32 OwnerPlayerId = INDEX_NONE;

	/** Owner team ID used by squad visibility filtering. */
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	int32 TeamId = INDEX_NONE;

	/** Visibility policy evaluated before per-surface config visibility. */
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	EOBMarkerVisibilityPolicy VisibilityPolicy = EOBMarkerVisibilityPolicy::Public;

	/** Draw priority; larger values render above smaller values. */
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	int32 SortPriority = 0;

	/** Remaining lifetime for temporary markers. A value of 0 means persistent. */
	UPROPERTY(BlueprintReadOnly, Category="Marker")
	float CurrentLifeTime;

	/** Initializes a marker from legacy explicit parameters. Called by the subsystem. */
	void Init(const FGuid& InID, AActor* InTrackedActor, UOBMarkerConfigAsset* InConfig, FName InLayerName,
	          FVector InStaticLocation = FVector::ZeroVector);

	/** Initializes this marker from a full marker spec. */
	void InitFromSpec(const FOBNavigationMarkerSpec& InSpec);

	/** Applies an updated marker spec to an existing marker. */
	void ApplySpec(const FOBNavigationMarkerSpec& InSpec);

	/** Updates the marker's world location and rotation from its tracked actor. */
	void UpdateLocation();

	/** Returns true when this marker's config allows rendering on the requested surface. */
	bool IsVisibleOnSurface(EOBNavigationSurface Surface) const;
};
