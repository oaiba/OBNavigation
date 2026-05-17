// Copyright OBExtraction. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/OBMapMarker.h"
#include "Data/OBNavigationTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OBNavigationSubsystem.generated.h"

class UOBMarkerConfigAsset;

/**
 * Broadcast when the active map layer changes (e.g., player moves between
 * floors or the subsystem selects a different map layer based on Z-height).
 * UI widgets bind to this to swap their map texture and reconfigure overlays.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNavigationMapLayerSpecChanged, FOBNavigationMapLayerSpec, NewLayerSpec);

/**
 * Broadcast whenever the active marker list changes (marker added, removed,
 * or updated). UI widgets bind to this for lightweight "dirty flag" refresh
 * rather than polling every frame.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMarkersUpdated);

/**
 * @class UOBNavigationSubsystem
 * @brief Central subsystem that manages all navigation data — map layers,
 *        markers, overlays, and compass state — for the local game instance.
 *
 * Lifetime: Created when the UGameInstance initializes and destroyed when the
 * game instance shuts down. There is exactly one instance per local process.
 *
 * Responsibilities:
 *  1. Load the UOBNavigationMapRegistryAsset from developer settings and
 *     populate internal lookup tables for marker configs.
 *  2. Manage an ordered list of FOBNavigationMapLayerSpec (from both the
 *     registry asset and runtime injections) and select the "current" layer
 *     based on the tracked pawn's world position.
 *  3. Own all active UOBMapMarker instances, handling registration,
 *     update (position tracking), visibility filtering, and removal.
 *  4. Provide pure query functions for UI widgets (GetAllActiveMarkers,
 *     GetVisibleMarkers, GetVisibleOverlayElements, WorldToMapUVChecked).
 *  5. Tick via FTSTicker (game-thread ticker) to update marker positions
 *     and re-evaluate the active map layer every frame.
 *
 * Data flow:
 * @code
 *   UOBNavigationSourceComponent
 *         |
 *         v
 *   UOBNavigationSubsystem  ←→  UOBMapMarker (pool)
 *         |
 *         v
 *   UOBMinimapWidget / UOBTacticalMapWidget (read-only queries)
 * @endcode
 *
 * @see UOBNavigationDeveloperSettings  – provides DefaultMapRegistry.
 * @see UOBNavigationSourceComponent    – actor component that registers markers.
 * @see UOBMinimapWidget                – primary consumer of map layer and marker data.
 * @see UOBMapMarker                    – per-marker runtime object.
 */
UCLASS()
class OBNAVIGATION_API UOBNavigationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// -----------------------------------------------------------------------
	//  Subsystem Lifecycle
	// -----------------------------------------------------------------------

	/**
	 * Called by the engine when the GameInstance starts. Loads the default
	 * map registry, builds lookup tables, and starts the frame ticker.
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * Called when the GameInstance is torn down. Stops the ticker, clears
	 * all markers, and releases cached references.
	 */
	virtual void Deinitialize() override;

	// -----------------------------------------------------------------------
	//  Tracking
	// -----------------------------------------------------------------------

	/**
	 * Sets the locally-controlled pawn whose world position drives map-layer
	 * selection and "center of view" on the minimap. Typically called once
	 * from the player controller after possession.
	 *
	 * @param PlayerPawn  The pawn to track. May be null to temporarily disable tracking.
	 */
	void SetTrackedPlayerPawn(APawn* PlayerPawn);

	/**
	 * Stores the local player's identity so the subsystem can filter markers
	 * by visibility policy. Must be called before markers with LocalOnly
	 * or SquadOnly policies will display correctly.
	 *
	 * @param InLocalPlayerId  Unique player ID for the local client.
	 * @param InLocalTeamId    Team ID for the local client.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation")
	void SetLocalNavigationContext(int32 InLocalPlayerId, int32 InLocalTeamId);

	// -----------------------------------------------------------------------
	//  Map Layer Queries
	// -----------------------------------------------------------------------

	/**
	 * Returns the currently active map layer specification (texture, bounds,
	 * rotation, overlays). Fails gracefully if no layer is active yet.
	 *
	 * @param OutLayerSpec  Populated with the current layer on success.
	 * @return true if a valid layer is active, false otherwise.
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Minimap")
	bool GetCurrentMapLayerSpec(FOBNavigationMapLayerSpec& OutLayerSpec) const;

	/** Returns the pawn currently being tracked for minimap centering. May be null. */
	UFUNCTION(BlueprintPure, Category = "OBNavigation")
	APawn* GetTrackedPlayerPawn() const { return TrackedPlayerPawn.Get(); }

	// -----------------------------------------------------------------------
	//  Runtime Map Layer Management
	// -----------------------------------------------------------------------

	/**
	 * Injects additional map-layer definitions at runtime (e.g., from a level
	 * streaming callback or a gameplay event). These layers are merged with
	 * the static layers from the registry asset and re-evaluated immediately.
	 *
	 * @param InMapLayers  Array of layer specs to add. Existing runtime layers
	 *                     are replaced wholesale.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Minimap")
	void SetRuntimeMapLayers(const TArray<FOBNavigationMapLayerSpec>& InMapLayers);

	/**
	 * Removes all previously injected runtime map layers, leaving only the
	 * static layers from the registry asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Minimap")
	void ClearRuntimeMapLayers();

	// -----------------------------------------------------------------------
	//  Marker Management
	// -----------------------------------------------------------------------

	/**
	 * Given an actor, returns the FGuid of the marker currently tracking it.
	 * Returns an invalid FGuid if the actor has no associated marker.
	 *
	 * Uses an internal reverse-lookup map (TrackedActorToMarkerIDMap) for
	 * O(1) performance.
	 *
	 * @param InActor  The actor to query.
	 * @return The marker's unique ID, or an invalid FGuid.
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Markers")
	FGuid GetMarkerIDForActor(AActor* InActor) const;

	/**
	 * Creates a new marker or updates an existing one based on the contents
	 * of MarkerSpec. If MarkerSpec.MarkerId is valid and already registered,
	 * the existing UOBMapMarker is updated in-place via ApplySpec(). Otherwise
	 * a new UOBMapMarker is created, initialized, and added to the active pool.
	 *
	 * Broadcasts OnMarkersUpdated after modification.
	 *
	 * @param MarkerSpec  Full specification for the marker to register or update.
	 * @return The unique FGuid identifying this marker.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Markers")
	FGuid RegisterOrUpdateMarker(const FOBNavigationMarkerSpec& MarkerSpec);

	/**
	 * Removes a marker from the subsystem by its unique ID. The UOBMapMarker
	 * object is released and removed from all internal lookup structures.
	 * No-op if the ID is invalid or not found.
	 *
	 * Broadcasts OnMarkersUpdated after removal.
	 *
	 * @param MarkerID  The unique ID previously returned by RegisterOrUpdateMarker().
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|Markers")
	void UnregisterMarker(const FGuid& MarkerID);

	/**
	 * Returns a reference to the full list of active markers (unfiltered).
	 * Intended for debug UIs or systems that need access to every marker
	 * regardless of visibility. For surface-specific filtering, use
	 * GetVisibleMarkers() instead.
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Markers")
	const TArray<UOBMapMarker*>& GetAllActiveMarkers() const { return ActiveMarkers; }

	/**
	 * Returns a filtered array of markers visible on the specified surface
	 * (Minimap, FullMap, or Compass). Filtering is based on both the marker's
	 * FMarkerVisibilityOptions (per-surface flags in the config asset) and
	 * the EOBMarkerVisibilityPolicy (ownership / team checks).
	 *
	 * @param Surface  Which navigation surface to filter for.
	 * @return A new array containing only the visible markers. Allocates per call.
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Markers")
	TArray<UOBMapMarker*> GetVisibleMarkers(EOBNavigationSurface Surface) const;

	// -----------------------------------------------------------------------
	//  Overlay Queries
	// -----------------------------------------------------------------------

	/**
	 * Returns overlay elements (zones, paths, freehand drawings) visible on
	 * the specified surface, optionally filtered by category and/or tag.
	 *
	 * @param Surface         Which navigation surface to query.
	 * @param CategoryFilter  If not NAME_None, only elements in this category are returned.
	 * @param TagFilter       If not NAME_None, only elements with this tag are returned.
	 * @return Array of matching overlay elements.
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Overlay")
	TArray<FOBNavigationOverlayElement> GetVisibleOverlayElements(EOBNavigationSurface Surface,
	                                                              FName CategoryFilter = NAME_None,
	                                                              FName TagFilter = NAME_None) const;

	// -----------------------------------------------------------------------
	//  Coordinate Projection
	// -----------------------------------------------------------------------

	/**
	 * Projects a world-space location to normalized map UV coordinates [0,1]
	 * using the specified map-layer's world bounds and rotation.
	 *
	 * @param MapLayerSpec   The map layer whose bounds define the projection.
	 * @param WorldLocation  3D world position to project.
	 * @param OutMapUV       Resulting UV coordinate (0,0 = top-left, 1,1 = bottom-right).
	 * @param OutResult      Projection result (Projected, NoLayer, OutsideLayer, InvalidBounds).
	 * @return true if projection succeeded (OutResult == Projected).
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|Utilities")
	bool WorldToMapUVChecked(const FOBNavigationMapLayerSpec& MapLayerSpec, const FVector& WorldLocation,
	                         FVector2D& OutMapUV, EOBMapProjectionResult& OutResult) const;

	// -----------------------------------------------------------------------
	//  Delegates
	// -----------------------------------------------------------------------

	/**
	 * Fired when the subsystem selects a different map layer as the "current"
	 * one (e.g., the tracked pawn moved to a different floor). UI widgets
	 * should rebind their map material and overlay data when this fires.
	 */
	UPROPERTY(BlueprintAssignable, Category = "OBNavigation|Delegates")
	FOnNavigationMapLayerSpecChanged OnNavigationMapLayerSpecChanged;

	/**
	 * Fired whenever the marker list is modified (add, remove, or update).
	 * Lightweight signal for widgets that only need to know "something changed"
	 * without receiving the full marker data in the delegate payload.
	 */
	UPROPERTY(BlueprintAssignable, Category = "OBNavigation|Delegates")
	FOnMarkersUpdated OnMarkersUpdated;

protected:
	/**
	 * Frame tick driven by FTSTicker. Updates all tracked marker positions,
	 * re-evaluates the active map layer, and broadcasts delegates as needed.
	 *
	 * @param DeltaTime  Time elapsed since last tick, in seconds.
	 * @return true to keep the ticker alive; false to unregister it.
	 */
	bool Tick(float DeltaTime);

private:
	// -----------------------------------------------------------------------
	//  Internal Helpers
	// -----------------------------------------------------------------------

	/** Loads the DefaultMapRegistry from developer settings and populates AllMarkerConfigs / MarkerConfigsByTag. */
	void LoadRegistryData();

	/** Merges RuntimeMapLayerSpecs with static layers from the registry into AllMapLayerSpecs, sorted by priority. */
	void RebuildMapLayerSpecs();

	/** Re-evaluates which layer in AllMapLayerSpecs should be "current" based on the tracked pawn's position. */
	void UpdateActiveMinimapLayer();

	/** Iterates all active markers and calls UpdateLocation() on each. Handles lifetime expiration for temporary markers. */
	void UpdateAllMarkers(float DeltaTime);

	/**
	 * Internal removal path. Returns true if a marker was found and removed.
	 * Does NOT broadcast OnMarkersUpdated — callers are responsible for batching.
	 */
	bool UnregisterMarkerInternal(const FGuid& MarkerID);

	/**
	 * Evaluates whether the given marker should be visible to the local player
	 * based on its VisibilityPolicy, OwnerPlayerId, TeamId, and bShowDebugMarkers.
	 */
	bool IsMarkerVisibleForLocalPlayer(const UOBMapMarker* Marker) const;

	/**
	 * Resolves the UOBMarkerConfigAsset for a given marker spec. Priority:
	 *   1. MarkerSpec.ConfigAsset (explicit)
	 *   2. MarkerConfigsByTag lookup (by MarkerSpec.MarkerType)
	 *   3. AllMarkerConfigs lookup (by MarkerSpec.LayerName, legacy)
	 *   4. nullptr if nothing matches.
	 */
	UOBMarkerConfigAsset* ResolveMarkerConfig(const FOBNavigationMarkerSpec& MarkerSpec) const;

	/** Returns true if the candidate layer is meaningfully different from CurrentMapLayerSpec. */
	bool IsDifferentCurrentLayer(const FOBNavigationMapLayerSpec* NewLayerSpec) const;

	// -----------------------------------------------------------------------
	//  Map Layer Data
	// -----------------------------------------------------------------------

	/** Runtime-injected map layers provided via SetRuntimeMapLayers(). */
	UPROPERTY()
	TArray<FOBNavigationMapLayerSpec> RuntimeMapLayerSpecs;

	/** Combined (static + runtime) map layers, sorted by Priority descending. */
	UPROPERTY()
	TArray<FOBNavigationMapLayerSpec> AllMapLayerSpecs;

	// -----------------------------------------------------------------------
	//  Marker Config Lookup Tables
	// -----------------------------------------------------------------------

	/** All marker config assets indexed by asset FName. Populated from the registry at startup. */
	UPROPERTY()
	TMap<FName, TObjectPtr<UOBMarkerConfigAsset>> AllMarkerConfigs;

	/** Fast lookup from FGameplayTag → config asset. Built from the registry's MarkerConfigs array. */
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<UOBMarkerConfigAsset>> MarkerConfigsByTag;

	// -----------------------------------------------------------------------
	//  Tracked Pawn
	// -----------------------------------------------------------------------

	/**
	 * Weak reference to the locally-controlled pawn. Used for map-layer
	 * selection and as the "center" reference point for minimap projection.
	 * Weak to avoid preventing GC of the pawn on level transitions.
	 */
	TWeakObjectPtr<APawn> TrackedPlayerPawn;

	// -----------------------------------------------------------------------
	//  Active Map Layer
	// -----------------------------------------------------------------------

	/** The map layer currently displayed by the minimap widget. */
	UPROPERTY()
	FOBNavigationMapLayerSpec CurrentMapLayerSpec;

	/** Guard flag: true once at least one valid layer has been selected. */
	bool bHasCurrentMapLayerSpec = false;

	// -----------------------------------------------------------------------
	//  Marker Pool
	// -----------------------------------------------------------------------

	/** Primary storage for active markers. Provides O(1) lookup by FGuid. */
	UPROPERTY()
	TMap<FGuid, TObjectPtr<UOBMapMarker>> ActiveMarkersMap;

	/**
	 * Cached flat array mirroring ActiveMarkersMap values. Rebuilt via
	 * RebuildActiveMarkersArray() whenever the map changes. Exists to
	 * provide a stable array reference for UI iteration without TMap overhead.
	 */
	UPROPERTY()
	TArray<TObjectPtr<UOBMapMarker>> ActiveMarkers;

	/** FTSTicker delegate handle — bound in Initialize(), unbound in Deinitialize(). */
	FTickerDelegate TickerDelegate;
	FTSTicker::FDelegateHandle TickerHandle;

	/** Rebuilds the ActiveMarkers array from ActiveMarkersMap. Called after any add/remove operation. */
	void RebuildActiveMarkersArray();

	/**
	 * Reverse lookup: Actor → MarkerID. Enables O(1) queries in
	 * GetMarkerIDForActor() and prevents duplicate markers per actor.
	 */
	UPROPERTY()
	TMap<TObjectPtr<AActor>, FGuid> TrackedActorToMarkerIDMap;

	// -----------------------------------------------------------------------
	//  Local Player Context
	// -----------------------------------------------------------------------

	/** Local player's unique ID. Used for visibility filtering (LocalOnly policy). */
	int32 LocalPlayerId = INDEX_NONE;

	/** Local player's team ID. Used for visibility filtering (SquadOnly policy). */
	int32 LocalTeamId = INDEX_NONE;

	/** Mirror of UOBNavigationDeveloperSettings::bShowDebugMarkers, cached at Initialize(). */
	bool bShowDebugMarkers = false;
};
