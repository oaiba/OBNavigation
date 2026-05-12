#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "OBNavigationMapRegistryAsset.generated.h"

class UOBMapLayerAsset;
class UOBMarkerConfigAsset;

USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBNavigationMarkerConfigEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation")
	FGameplayTag MarkerType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation")
	TObjectPtr<UOBMarkerConfigAsset> Config = nullptr;
};

UCLASS(BlueprintType)
class OBNAVIGATION_API UOBNavigationMapRegistryAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation")
	TArray<TObjectPtr<UOBMapLayerAsset>> MapLayers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation")
	TArray<FOBNavigationMarkerConfigEntry> MarkerConfigs;
};
