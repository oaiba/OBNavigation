// Copyright OBExtraction. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/OBNavigationTypes.h"
#include "OBNavigationSourceComponent.generated.h"

class UOBMarkerConfigAsset;
class UOBNavigationSubsystem;

/**
 * Actor component that automatically registers its owning actor as a navigation
 * marker in UOBNavigationSubsystem.
 *
 * Attach this component to any actor that should appear on the minimap,
 * full map, or compass (e.g., players, enemies, objectives, vehicles).
 * The component handles the full lifecycle:
 *
 *   BeginPlay  → RegisterOrUpdateNavigationMarker() (if bRegisterOnBeginPlay)
 *   EndPlay    → UnregisterNavigationMarker()
 *
 * Designers configure the marker's appearance and behavior entirely from
 * the Details panel:
 *  - MarkerType        – selects the visual config from the registry.
 *  - MarkerConfig      – optional explicit override (bypasses tag lookup).
 *  - VisibilityPolicy  – controls who can see this marker (LocalOnly, Squad, Public, Debug).
 *  - bTrackOwner       – when true, the marker follows the owning actor every frame.
 *
 * @note This component does NOT replicate. Marker registration is handled
 *       locally on each client via their own subsystem instance.
 *
 * @see UOBNavigationSubsystem::RegisterOrUpdateMarker() – the subsystem API this calls.
 * @see UOBMarkerConfigAsset – data asset defining the marker's visual properties.
 * @see FOBNavigationMarkerSpec – the spec struct built from this component's properties.
 */
UCLASS(ClassGroup = "OBNavigation", meta = (BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class OBNAVIGATION_API UOBNavigationSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOBNavigationSourceComponent();

	/** Called when the component is first activated in the world. Optionally registers the marker. */
	virtual void BeginPlay() override;

	/** Called when the component or its owner is removed from the world. Unregisters the marker. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// -----------------------------------------------------------------------
	//  Configuration Properties
	// -----------------------------------------------------------------------

	/**
	 * Gameplay tag identifying the marker category (e.g., "OBNavigation.Marker.Player").
	 * The subsystem uses this to look up the default UOBMarkerConfigAsset
	 * from the map registry when MarkerConfig is null.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FGameplayTag MarkerType;

	/**
	 * Optional explicit marker configuration asset. When set, this takes
	 * priority over the tag-based lookup from the registry. Useful for
	 * one-off markers that do not fit a shared category.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	TObjectPtr<UOBMarkerConfigAsset> MarkerConfig = nullptr;

	/**
	 * Logical layer name this marker belongs to (e.g., "Quests", "PartyMembers").
	 * Layers are used to group markers and toggle visibility per-surface.
	 * NAME_None means the marker is layer-agnostic and always considered visible.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FName LayerName = NAME_None;

	/**
	 * Determines who can see this marker. The subsystem filters markers
	 * based on this policy and the local player's ID / team ID.
	 * @see EOBMarkerVisibilityPolicy
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	EOBMarkerVisibilityPolicy VisibilityPolicy = EOBMarkerVisibilityPolicy::Public;

	/**
	 * When true, the subsystem updates this marker's WorldLocation every
	 * frame to match the owning actor's position. When false, the marker
	 * remains at the location it was registered at.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	bool bTrackOwner = true;

	/**
	 * When true, the component automatically calls RegisterOrUpdateNavigationMarker()
	 * during BeginPlay. Set to false for markers that should be registered
	 * manually at a later point (e.g., after some gameplay event).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	bool bRegisterOnBeginPlay = true;

	/**
	 * The player ID of the marker's owner. Used by the visibility system
	 * to implement LocalOnly and SquadOnly policies. INDEX_NONE means
	 * the marker is not associated with any specific player.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	int32 OwnerPlayerId = INDEX_NONE;

	/**
	 * The team ID of the marker's owner. Combined with VisibilityPolicy
	 * to filter markers by team affiliation. INDEX_NONE means no team.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	int32 TeamId = INDEX_NONE;

	/**
	 * Draw order priority for this marker. Higher values are drawn on top
	 * of lower ones. Markers with equal priority use insertion order.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	int32 SortPriority = 0;

	// -----------------------------------------------------------------------
	//  Runtime API
	// -----------------------------------------------------------------------

	/**
	 * Builds a FOBNavigationMarkerSpec from this component's properties and
	 * calls UOBNavigationSubsystem::RegisterOrUpdateMarker(). If the marker
	 * was already registered (MarkerId is valid), the existing marker is
	 * updated in-place instead of creating a new one.
	 *
	 * @return The unique FGuid identifying this marker in the subsystem.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation")
	FGuid RegisterOrUpdateNavigationMarker();

	/**
	 * Removes this component's marker from the subsystem. Safe to call
	 * even if the marker was never registered (no-op in that case).
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation")
	void UnregisterNavigationMarker();

	/** Returns the unique ID assigned to this marker, or an invalid FGuid if not yet registered. */
	UFUNCTION(BlueprintPure, Category = "OBNavigation")
	FGuid GetMarkerId() const { return MarkerId; }

private:
	/** Cached pointer to the local navigation subsystem for fast access. */
	UPROPERTY(Transient)
	TObjectPtr<UOBNavigationSubsystem> NavSubsystem = nullptr;

	/** Unique marker identifier assigned by the subsystem on first registration. */
	FGuid MarkerId;
};
