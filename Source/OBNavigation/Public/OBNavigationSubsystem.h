// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBMapMarker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OBNavigationSubsystem.generated.h"

class UOBMapLayerAsset;
class UOBMarkerConfigAsset;

// Delegate for broadcasting minimap layer changes
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMinimapLayerChanged, UOBMapLayerAsset*, NewLayer);

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
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Minimap")
	UOBMapLayerAsset* GetCurrentMinimapLayer() const { return CurrentMinimapLayer; }

	UFUNCTION(BlueprintPure, Category = "OBNavigation")
	APawn* GetTrackedPlayerPawn() const { return TrackedPlayerPawn.Get(); }

	UFUNCTION(BlueprintPure, Category = "OBNavigation|Markers")
	FGuid GetMarkerIDForActor(AActor* InActor) const;

	/**
	 * @brief Registers a new marker.
	 * @param InTrackedActor The actor to track. If nullptr, use InStaticLocation.
	 * @param InConfig The marker's configuration asset.
	 * @param InLayerName The logical layer name for this marker (e.g., "Quests", "Party").
	 * @param InStaticLocation If not tracking an actor, this is the fixed world location.
	 * @return The unique ID of the registered marker, or an invalid FGuid on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Markers")
	FGuid RegisterMapMarker(AActor* InTrackedActor, UOBMarkerConfigAsset* InConfig, FName InLayerName,
							FVector InStaticLocation = FVector::ZeroVector);

	/**
	 * @brief Unregisters a marker by its ID.
	 * @param MarkerID The ID of the marker to unregister.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Markers")
	void UnregisterMapMarker(const FGuid& MarkerID);

	// Get all active markers for UI display
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Markers")
	const TArray<UOBMapMarker*>& GetAllActiveMarkers() const { return ActiveMarkers; }

	// Utility to convert world location to map UV
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Utilities")
	bool WorldToMapUV(const UOBMapLayerAsset* MapLayer, const FVector& WorldLocation, FVector2D& OutMapUV) const;

	UPROPERTY(BlueprintAssignable, Category = "OBNavigation|Delegates")
	FOnMinimapLayerChanged OnMinimapLayerChanged;

	UPROPERTY(BlueprintAssignable, Category = "OBNavigation|Delegates")
	FOnMarkersUpdated OnMarkersUpdated; // Broadcast when markers are added/removed/updated

protected:
	bool Tick(float DeltaTime);

private:
	void UpdateActiveMinimapLayer();
	void UpdateAllMarkers(float DeltaTime);

	// All available map layer assets loaded at initialization
	UPROPERTY()
	TArray<TObjectPtr<UOBMapLayerAsset>> AllMapLayers;

	// All available markers config assets loaded at initialization (for a quick lookup)
	UPROPERTY()
	TMap<FName, TObjectPtr<UOBMarkerConfigAsset>> AllMarkerConfigs;

	TWeakObjectPtr<APawn> TrackedPlayerPawn;
	UPROPERTY()
	TObjectPtr<UOBMapLayerAsset> CurrentMinimapLayer;

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
};
