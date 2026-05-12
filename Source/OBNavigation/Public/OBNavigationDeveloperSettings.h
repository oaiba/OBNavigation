#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OBNavigationDeveloperSettings.generated.h"

class UOBNavigationMapRegistryAsset;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "OB Navigation"))
class OBNAVIGATION_API UOBNavigationDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "OBNavigation")
	TSoftObjectPtr<UOBNavigationMapRegistryAsset> DefaultMapRegistry;

	UPROPERTY(Config, EditAnywhere, Category = "OBNavigation")
	bool bShowDebugMarkers = false;
};
