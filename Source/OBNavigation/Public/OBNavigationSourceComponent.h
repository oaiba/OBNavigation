#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OBNavigationTypes.h"
#include "OBNavigationSourceComponent.generated.h"

class UOBMarkerConfigAsset;
class UOBNavigationSubsystem;

UCLASS(ClassGroup = "OBNavigation", meta = (BlueprintSpawnableComponent))
class OBNAVIGATION_API UOBNavigationSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOBNavigationSourceComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FGameplayTag MarkerType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	TObjectPtr<UOBMarkerConfigAsset> MarkerConfig = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FName LayerName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	EOBMarkerVisibilityPolicy VisibilityPolicy = EOBMarkerVisibilityPolicy::Public;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	bool bTrackOwner = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	bool bRegisterOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	int32 OwnerPlayerId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	int32 TeamId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	int32 SortPriority = 0;

	UFUNCTION(BlueprintCallable, Category = "OBNavigation")
	FGuid RegisterOrUpdateNavigationMarker();

	UFUNCTION(BlueprintCallable, Category = "OBNavigation")
	void UnregisterNavigationMarker();

	UFUNCTION(BlueprintPure, Category = "OBNavigation")
	FGuid GetMarkerId() const { return MarkerId; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UOBNavigationSubsystem> NavSubsystem = nullptr;

	FGuid MarkerId;
};
