#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "OBNavigationTypes.generated.h"

class AActor;
class UOBMarkerConfigAsset;

UENUM(BlueprintType)
enum class EOBNavigationSurface : uint8
{
	Minimap,
	FullMap,
	Compass
};

UENUM(BlueprintType)
enum class EOBMarkerVisibilityPolicy : uint8
{
	LocalOnly,
	SquadOnly,
	Public,
	DebugOnly
};

UENUM(BlueprintType)
enum class EOBMapProjectionResult : uint8
{
	Projected,
	NoLayer,
	OutsideLayer,
	InvalidBounds
};

USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBNavigationMarkerSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FGuid MarkerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FGameplayTag MarkerType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FName LayerName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	TObjectPtr<AActor> TrackedActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FRotator WorldRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	TObjectPtr<UOBMarkerConfigAsset> ConfigAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	float LifeTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	int32 OwnerPlayerId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	int32 TeamId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	EOBMarkerVisibilityPolicy VisibilityPolicy = EOBMarkerVisibilityPolicy::Public;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	int32 SortPriority = 0;
};
