// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOBNavigation, Log, All);

class AActor;

namespace OBNavigation
{
	/**
	 * Resolves the world-space location navigation should use for an actor.
	 * Some Mover/Blueprint setups keep the actor root at an origin while the
	 * movement sync state or moved components contain the actual map position.
	 */
	OBNAVIGATION_API FVector ResolveActorNavigationLocation(const AActor* Actor);

	/**
	 * Resolves the world-space rotation navigation should use for an actor.
	 * Pawns can expose aim/control yaw independently from body rotation through
	 * IOBNavigationRotationProvider or a valid Controller.
	 */
	OBNAVIGATION_API FRotator ResolveActorNavigationRotation(const AActor* Actor);

	/** Formats candidate actor/component/Mover locations for startup navigation traces. */
	OBNAVIGATION_API FString DescribeActorNavigationLocationCandidates(const AActor* Actor);

	/** Formats the selected source used by ResolveActorNavigationLocation. */
	OBNAVIGATION_API FString DescribeActorNavigationLocationSource(const AActor* Actor);

	/** Formats a bounded scene-component location snapshot for startup navigation traces. */
	OBNAVIGATION_API FString DescribeActorNavigationComponentSnapshot(const AActor* Actor);
}

class FOBNavigationModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
