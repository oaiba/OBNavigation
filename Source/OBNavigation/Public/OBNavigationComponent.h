// Copyright OBExtraction. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OBNavigationComponent.generated.h"

class UOBMarkerConfigAsset;
class UOBNavigationSubsystem;

/**
 * Component attached to any APawn, including Mover-based pawns, to handle local
 * player navigation and register/update the pawn marker with the global subsystem.
 * Deliberately uses APawn instead of ACharacter so that non-ACharacter Mover pawns are supported.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class OBNAVIGATION_API UOBNavigationComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	/** Sets default component ticking behavior. */
	UOBNavigationComponent();

	/** Resolves the navigation subsystem and registers this pawn marker when relevant. */
	virtual void BeginPlay() override;

	/** Unregisters this pawn marker before the component leaves play. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Marker config used for this pawn's map and compass marker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation")
	TObjectPtr<UOBMarkerConfigAsset> CharacterMapMarkerConfig;

	/** Logical layer name assigned to this pawn marker. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation")
	FName CharacterMapMarkerLayerName = TEXT("Players");

protected:
	/** Registers this character's marker with the subsystem. */
	void RegisterCharacterMarker();

	/** Unregisters this character's marker from the subsystem. */
	void UnregisterCharacterMarker();

private:
	/** Cached local navigation subsystem reference. */
	UPROPERTY(Transient)
	TObjectPtr<UOBNavigationSubsystem> NavSubsystem;

	/** ID of the marker registered for this character, if any. */
	FGuid CharacterMarkerID;

	/** Returns true when this net role should register a local navigation marker. */
	bool ShouldRegisterCharacterMarker() const;
};
