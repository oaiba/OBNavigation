// Copyright OBExtraction. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "OBNavigationRotationProvider.generated.h"

/** Blueprint interface class for navigation-specific actor rotation providers. */
UINTERFACE(BlueprintType)
class OBNAVIGATION_API UOBNavigationRotationProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Optional interface for actors that expose a navigation-facing rotation that
 * differs from their body or actor rotation.
 */
class OBNAVIGATION_API IOBNavigationRotationProvider
{
	GENERATED_BODY()

public:
	/**
	 * Returns the world-space rotation navigation widgets should use for this actor.
	 *
	 * Implement on pawns whose map/marker facing should differ from actor rotation.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "OBNavigation")
	FRotator GetNavigationWorldRotation() const;
};
