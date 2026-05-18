// Copyright OBExtraction. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "OBNavigationRotationProvider.generated.h"

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
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "OBNavigation")
	FRotator GetNavigationWorldRotation() const;
};
