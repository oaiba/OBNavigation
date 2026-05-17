// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBMapMarker.h"
#include "OBNavigationTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OBNavigationSubsystem.generated.h"

class UOBMarkerConfigAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNavigationMapLayerSpecChanged, FOBNavigationMapLayerSpec, NewLayerSpec);

// Delegate for broadcasting marker list changes
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMarkersUpdated);

/**
 * @class UOBNavigationSubsystem
 * @brief Manages all map, compass, marker, and navigation logic.
 * This subsystem is the single source of truth for all navigation UI elements.
 */
UCLASS()
class OBNAVIGATION_API UOBNavigationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Sets the pawn that the subsystem should track for local minimap display
	void SetTrackedPlayerPawn(APawn* PlayerPawn);

	UFUNCTION(BlueprintCallable, Category = "OBNavigation")
	void SetLocalNavigationContext(int32 InLocalPlayerId, int32 InLocalTeamId);

	UFUNCTION(BlueprintPure, Category = "OBNavigation|Minimap")
	bool GetCurrentMapLayerSpec(FOBNavigationMapLayerSpec& OutLayerSpec) const;

	UFUNCTION(BlueprintPure, Category = "OBNavigation")
	APawn* GetTrackedPlayerPawn() const { return TrackedPlayerPawn.Get(); }

	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Minimap")
	void SetRuntimeMapLayers(const TArray<FOBNavigationMapLayerSpec>& InMapLayers);

	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Minimap")
	void ClearRuntimeMapLayers();

	UFUNCTION(BlueprintPure, Category = "OBNavigation|Markers")
	FGuid GetMarkerIDForActor(AActor* InActor) const;

	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Markers")
	FGuid RegisterOrUpdateMarker(const FOBNavigationMarkerSpec& MarkerSpec);

	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Markers")
	void UnregisterMarker(const FGuid& MarkerID);

	// Get all active markers for UI display
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Markers")
	const TArray<UOBMapMarker*>& GetAllActiveMarkers() const { return ActiveMarkers; }

	UFUNCTION(BlueprintPure, Category = "OBNavigation|Markers")
	TArray<UOBMapMarker*> GetVisibleMarkers(EOBNavigationSurface Surface) const;

	UFUNCTION(BlueprintPure, Category = "OBNavigation|Overlay")
	TArray<FOBNavigationOverlayElement> GetVisibleOverlayElements(EOBNavigationSurface Surface,
	                                                              FName CategoryFilter = NAME_None,
	                                                              FName TagFilter = NAME_None) const;

	UFUNCTION(BlueprintPure, Category = "OBNavigation|Utilities")
	bool WorldToMapUVChecked(const FOBNavigationMapLayerSpec& MapLayerSpec, const FVector& WorldLocation,
	                         FVector2D& OutMapUV, EOBMapProjectionResult& OutResult) const;

	UPROPERTY(BlueprintAssignable, Category = "OBNavigation|Delegates")
	FOnNavigationMapLayerSpecChanged OnNavigationMapLayerSpecChanged;

	UPROPERTY(BlueprintAssignable, Category = "OBNavigation|Delegates")
	FOnMarkersUpdated OnMarkersUpdated; // Broadcast when markers are added/removed/updated

protected:
	bool Tick(float DeltaTime);

private:
	void LoadRegistryData();
	void RebuildMapLayerSpecs();
	void UpdateActiveMinimapLayer();
	void UpdateAllMarkers(float DeltaTime);
	bool UnregisterMarkerInternal(const FGuid& MarkerID);
	bool IsMarkerVisibleForLocalPlayer(const UOBMapMarker* Marker) const;
	UOBMarkerConfigAsset* ResolveMarkerConfig(const FOBNavigationMarkerSpec& MarkerSpec) const;
	bool IsDifferentCurrentLayer(const FOBNavigationMapLayerSpec* NewLayerSpec) const;

	UPROPERTY()
	TArray<FOBNavigationMapLayerSpec> RuntimeMapLayerSpecs;

	UPROPERTY()
	TArray<FOBNavigationMapLayerSpec> AllMapLayerSpecs;

	// All available markers config assets loaded at initialization (for a quick lookup)
	UPROPERTY()
	TMap<FName, TObjectPtr<UOBMarkerConfigAsset>> AllMarkerConfigs;

	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<UOBMarkerConfigAsset>> MarkerConfigsByTag;

	TWeakObjectPtr<APawn> TrackedPlayerPawn;

	UPROPERTY()
	FOBNavigationMapLayerSpec CurrentMapLayerSpec;

	bool bHasCurrentMapLayerSpec = false;

	// Store active markers in a TMap for faster lookup by FGuid
	UPROPERTY()
	TMap<FGuid, TObjectPtr<UOBMapMarker>> ActiveMarkersMap;

	// A cached array of markers for UI iteration (updated when ActiveMarkersMap changes)
	UPROPERTY()
	TArray<TObjectPtr<UOBMapMarker>> ActiveMarkers;

	FTickerDelegate TickerDelegate;
	FTSTicker::FDelegateHandle TickerHandle;

	void RebuildActiveMarkersArray(); // Helper to update ActiveMarkers array

	// Reverse lookup map to quickly find a marker's ID from the actor it tracks.
	UPROPERTY()
	TMap<TObjectPtr<AActor>, FGuid> TrackedActorToMarkerIDMap;

	int32 LocalPlayerId = INDEX_NONE;
	int32 LocalTeamId = INDEX_NONE;
	bool bShowDebugMarkers = false;
};
