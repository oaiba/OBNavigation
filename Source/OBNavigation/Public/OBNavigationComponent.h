// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OBNavigationComponent.generated.h"

class UOBMarkerConfigAsset;
class UOBNavigationSubsystem;

/**
 * @class UOBNavigationComponent
 * @brief Component attached to ACharacter to handle local player navigation aspects
 * and register/update character-specific markers with the global subsystem.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class OBNAVIGATION_API UOBNavigationComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UOBNavigationComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// The configuration asset for this character's marker on the map/compass.
	// E.g., a "Player Icon" or "Team Member" icon.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation")
	TObjectPtr<UOBMarkerConfigAsset> CharacterMapMarkerConfig;

	// The layer name for this character's marker (e.g., "PartyMembers")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation")
	FName CharacterMapMarkerLayerName = TEXT("Players");

protected:
	// Registers this character's marker with the subsystem.
	void RegisterCharacterMarker();

	// Unregisters this character's marker from the subsystem.
	void UnregisterCharacterMarker();

private:
	UPROPERTY(Transient)
	TObjectPtr<UOBNavigationSubsystem> NavSubsystem;

	// The ID of the marker registered for this character (if any)
	FGuid CharacterMarkerID;

	// Only register character marker for relevant clients.
	// For multiplayer, Server and Autonomous Proxy will handle this.
	// Dedicated Server doesn't need to display itself on maps.
	bool ShouldRegisterCharacterMarker() const;
};
