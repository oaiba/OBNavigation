#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Types/SlateEnums.h"
#include "Widget/OBMapWidgetBase.h"
#include "OBTacticalMapWidget.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class UEditableTextBox;
class UOBMinimapConfigAsset;
class UOBTacticalMapConfigAsset;
class USlider;
class UTextBlock;
class UWidget;

/** Broadcast when tactical map center, zoom, or follow state changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTacticalMapViewChanged, FVector2D, ViewCenterUV, float, Zoom,
                                               bool, bIsFollowingTrackedPlayer);

/** Broadcast when the tactical map switches to a different map layer. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTacticalMapLayerChanged, FName, LayerName);

/** Broadcast when tactical marker or overlay filters change. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTacticalMapFilterChanged);

/**
 * Full-screen tactical map widget with free pan, zoom, north-up orientation,
 * layer overrides, and marker/overlay filtering.
 *
 * Extends UOBMapWidgetBase with tactical-specific behavior: the view center
 * can be freely panned via mouse drag (AddPanInput), gamepad stick (SetPanInput),
 * or Blueprint API (SetViewCenterUV / SetViewCenterWorldLocation). A follow-mode
 * toggle (bIsFollowingTrackedPlayer) lets the view snap back to the player via
 * RecenterOnTrackedPlayer(). Supports independent layer selection, marker-layer
 * filtering, and overlay category/tag filtering.
 *
 * =========================================================================
 * Visual ASCII Wireframe:
 *
	 *  +======================[Tactical Root]========================+
	 *  |                                                             |
	 *  |  [TopBar]                                                   |
	 *  |   [LayerComboBox] [ClearLayerOverrideButton]                |
	 *  |   [ActiveLayerText]                                         |
	 *  |                                                             | 
	 *  |  +--------------------[MapInputArea]---------------------+  |
	 *  |  | +------------------[MapImage]-----------------------+ |  |
	 *  |  | |   north-up map material, pan/zoom by ViewCenterUV | |  |
	 *  |  | +---------------------------------------------------+ |  |
	 *  |  | +---------------[MapMarkerCanvas]-------------------+ |  |
	 *  |  | | [OverlayWidget] Z:0, markers Z:1, player Z:10     | |  |
	 *  |  | +---------------------------------------------------+ |  |
	 *  |  +-------------------------------------------------------+  |
	 *  |                                                             |
	 *  |  [ZoomOutButton] [ZoomSlider] [ZoomInButton] [ZoomValue]    |
	 *  |  [PanUp/Down/Left/RightButton] [RecenterButton]             |
	 *  |  [FollowPlayerCheckBox] [FollowStateText]                   |
	 *  |                                                             |
	 *  |  [MarkerLayerComboBox] [MarkerLayerEnabledCheckBox]         |
	 *  |  [OverlayCategoryTextBox] [OverlayTagTextBox] [Clear]       |
	 *  |  [ViewCenterText] [PanInputText]                            |
	 *  +=============================================================+
 *
 * =========================================================================
 * Pan / Zoom / Follow state machine:
 *
 *       ┌───────────────────────────────────┐
 *       │       bIsFollowingTrackedPlayer   │
 *       │              = true               │
 *       │  ViewCenterUV ← player UV/tick    │
 *       └────────┬──────────────────────────┘
 *                │  AddPanInput() / SetViewCenterUV()
 *                ▼
 *       ┌───────────────────────────────────┐
 *       │       bIsFollowingTrackedPlayer   │
 *       │              = false              │
 *       │  ViewCenterUV ← manual input     │
 *       └────────┬──────────────────────────┘
 *                │  RecenterOnTrackedPlayer()
 *                ▼  (loops back to follow)
 *
 * =========================================================================
 */
UCLASS()
class OBNAVIGATION_API UOBTacticalMapWidget : public UOBMapWidgetBase
{
	GENERATED_BODY()

public:
	/**
	 * Initializes the tactical map using shared visual assets and tactical behavior settings.
	 *
	 * @param InMinimapConfigAsset Visual config that supplies map material and alignment settings.
	 * @param InTacticalConfigAsset Tactical config that supplies zoom, pan, filters, and follow defaults.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void InitializeTacticalMapAndStartTracking(UOBMinimapConfigAsset* InMinimapConfigAsset,
	                                           UOBTacticalMapConfigAsset* InTacticalConfigAsset);

	/**
	 * Applies a discrete zoom step. Positive values zoom in, negative values zoom out.
	 *
	 * @param ZoomDelta Signed input step, usually +1 or -1.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void AddZoomInput(float ZoomDelta);

	/** Sets the tactical map zoom directly, clamped by the configured zoom range. */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void SetTacticalMapZoom(float NewZoom);

	/**
	 * Applies a one-shot screen-space pan delta in Slate units.
	 *
	 * @param PanDelta Screen-space delta in pixels or Slate units.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void AddPanInput(FVector2D PanDelta);

	/**
	 * Sets normalized continuous pan input applied each tick.
	 *
	 * @param InPanInput Direction vector, typically in [-1, 1] per axis.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void SetPanInput(FVector2D InPanInput);

	/** Returns the current continuous pan input vector. */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|TacticalMap")
	FVector2D GetPanInput() const { return PanInput; }

	/**
	 * Centers the tactical map on a world location in the active tactical layer.
	 *
	 * @param WorldLocation World-space location to project into map UV.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void SetViewCenterWorldLocation(FVector WorldLocation);

	/**
	 * Sets the tactical map center in normalized map UV coordinates.
	 *
	 * @param MapUV Normalized map coordinate where (0,0) is top-left and (1,1) is bottom-right.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void SetViewCenterUV(FVector2D MapUV);

	/** Recenters the tactical map on the tracked player and enables follow mode. */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void RecenterOnTrackedPlayer();

	/**
	 * Enables or disables follow mode.
	 *
	 * @param bFollow True to follow the tracked player, false to keep the current free-pan center.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void SetFollowTrackedPlayer(bool bFollow);

	/**
	 * Returns true while the tactical map view center follows the tracked player.
	 * 
	 * @return True if follow mode is active.
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|TacticalMap")
	bool IsFollowingTrackedPlayer() const { return bIsFollowingTrackedPlayer; }

	/**
	 * Returns the current tactical map center in normalized UV coordinates.
	 * 
	 * @return The map view center UV (in [0, 1] range).
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|TacticalMap")
	FVector2D GetViewCenterUV() const { return ViewCenterUV; }

	/**
	 * Returns the current tactical map zoom multiplier.
	 * 
	 * @return The zoom multiplier applied to the map.
	 */
	UFUNCTION(BlueprintPure, Category = "OBNavigation|TacticalMap")
	float GetTacticalMapZoom() const { return GetMapZoom(); }

	/**
	 * Enables or disables a tactical marker layer filter.
	 *
	 * @param LayerName Marker layer name from UOBMapMarker::MarkerLayerName.
	 * @param bEnabled True to show this layer, false to hide it.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void SetMarkerLayerFilter(FName LayerName, bool bEnabled);

	/** Clears all marker-layer filtering so every visible full-map marker layer is shown. */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void ClearMarkerLayerFilter();

	/**
	 * Filters tactical overlays by category.
	 *
	 * @param Category Category to show, or NAME_None to show all categories.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void SetOverlayCategoryFilter(FName Category);

	/**
	 * Filters tactical overlays by tag.
	 *
	 * @param Tag Tag to show, or NAME_None to show all tags.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void SetOverlayTagFilter(FName Tag);

	/** Clears tactical overlay category and tag filters. */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void ClearOverlayFilters();

	/**
	 * Overrides the tactical map layer by name without affecting minimap layer selection.
	 *
	 * @param LayerName Layer/floor name to display.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void SetTacticalMapLayerByName(FName LayerName);

	/**
	 * Clears the tactical layer override so the map follows the subsystem current layer again.
	 */
	UFUNCTION(BlueprintCallable, Category = "OBNavigation|TacticalMap")
	void ClearTacticalMapLayerOverride();

	/** Fired when tactical map center, zoom, or follow state changes. */
	UPROPERTY(BlueprintAssignable, Category = "OBNavigation|TacticalMap")
	FOnTacticalMapViewChanged OnTacticalMapViewChanged;

	/** Fired when the tactical map layer override changes. */
	UPROPERTY(BlueprintAssignable, Category = "OBNavigation|TacticalMap")
	FOnTacticalMapLayerChanged OnTacticalMapLayerChanged;

	/** Fired when marker or overlay filters change. */
	UPROPERTY(BlueprintAssignable, Category = "OBNavigation|TacticalMap")
	FOnTacticalMapFilterChanged OnTacticalMapFilterChanged;

protected:
	/**
	 * Applies continuous pan input before the shared map update path.
	 * 
	 * @param MyGeometry The widget geometry.
	 * @param InDeltaTime The time elapsed since the last tick.
	 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Handles direct mouse press input on the optional map input area. */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Handles direct mouse release input on the optional map input area. */
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Handles direct mouse drag input on the optional map input area. */
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** Handles direct mouse-wheel zoom input on the optional map input area. */
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/**
	 * Returns the full-map navigation surface.
	 * 
	 * @return The navigation surface identifier for the tactical map.
	 */
	virtual EOBNavigationSurface GetNavigationSurface() const override { return EOBNavigationSurface::FullMap; }

	/**
	 * Resolves the tactical active layer, honoring any local layer override.
	 * 
	 * @param OutLayerSpec The resulting layer specification to apply.
	 * @return True if a valid layer was resolved.
	 */
	virtual bool ResolveActiveLayer(FOBNavigationMapLayerSpec& OutLayerSpec) const override;

	/**
	 * Returns tactical initial zoom.
	 * 
	 * @return The initial zoom level specific to the tactical map.
	 */
	virtual float GetInitialZoom() const override;

	/**
	 * Returns tactical minimum zoom.
	 * 
	 * @return The minimum boundary for the tactical zoom.
	 */
	virtual float GetMinimumZoom() const override;

	/**
	 * Returns tactical maximum zoom.
	 * 
	 * @return The maximum boundary for the tactical zoom.
	 */
	virtual float GetMaximumZoom() const override;

	/** Returns the tactical tile-cache budget. */
	virtual int32 GetTileBudget() const override;

	/**
	 * Always returns false because tactical map v1 is north-up.
	 * 
	 * @return Always false.
	 */
	virtual bool ShouldRotateMap() const override;

	/**
	 * Always returns false so the player marker floats on the free-pan map.
	 * 
	 * @return Always false.
	 */
	virtual bool ShouldCenterPlayerMarker() const override;

	/**
	 * Returns the tactical player-marker visibility setting.
	 * 
	 * @return True if the player marker is allowed to be shown.
	 */
	virtual bool ShouldShowPlayerMarker() const override;

	/**
	 * Applies tactical marker layer filters.
	 * 
	 * @param Marker The map marker to evaluate.
	 * @return True if the marker passes filtering conditions.
	 */
	virtual bool ShouldShowMarker(const UOBMapMarker* Marker) const override;

	/**
	 * Returns the tactical marker size multiplier.
	 * 
	 * @return The scale applied to tactical map markers.
	 */
	virtual float GetMarkerScale() const override;

	/**
	 * Returns the tactical overlay category filter.
	 * 
	 * @return The category name currently being filtered.
	 */
	virtual FName GetOverlayCategoryFilter() const override;

	/**
	 * Returns the tactical overlay tag filter.
	 * 
	 * @return The tag name currently being filtered.
	 */
	virtual FName GetOverlayTagFilter() const override;

	/**
	 * Resolves player UV in follow mode or returns cached free-pan center.
	 * 
	 * @param CurrentLayer The active navigation map layer.
	 * @param TrackedPawn The tracked player pawn.
	 * @param OutViewCenterUV The resulting UV coordinate.
	 * @return True if a valid UV was resolved.
	 */
	virtual bool ResolveViewCenterUV(const FOBNavigationMapLayerSpec& CurrentLayer, const APawn* TrackedPawn,
	                                 FVector2D& OutViewCenterUV) const override;

	/**
	 * Tactical map uses static north-up alignment only; no pawn yaw is applied.
	 * 
	 * @param TrackedPawn The pawn tracking the rotation.
	 * @return Always 0.0f.
	 */
	virtual float GetDynamicMapYaw(const APawn* TrackedPawn) const override;

	/**
	 * Returns static map alignment and visual config offset without pawn yaw.
	 * 
	 * @return The total static rotation in degrees.
	 */
	virtual float GetTotalStaticRotation() const override;

	/**
	 * Caches the followed player UV and notifies listeners when the view changes.
	 * 
	 * @param ViewContext The newly created map view context.
	 * @param CurrentLayer The active navigation map layer.
	 * @param TrackedPawn The tracked player pawn.
	 */
	virtual void OnViewContextUpdated(const FOBNavigationMapViewContext& ViewContext,
	                                  const FOBNavigationMapLayerSpec& CurrentLayer, const APawn* TrackedPawn) override;

private:
	/** Combo-box item that clears the tactical layer override. */
	static constexpr const TCHAR* AutoLayerOption = TEXT("Auto");

	/** Combo-box item that disables marker-layer filtering. */
	static constexpr const TCHAR* AllMarkerLayersOption = TEXT("All");

	/** Applies continuous pan input scaled by PanSpeed and DeltaTime. */
	void ApplyContinuousPanInput(float DeltaTime);

	/** Sets ViewCenterUV, optionally clamps to map bounds, and updates follow state. */
	void SetViewCenterUVInternal(FVector2D MapUV, bool bFollowTrackedPlayer);

	/** Broadcasts the current tactical view state. */
	void BroadcastViewChanged() const;

	/** Finds an available map layer by name. */
	bool FindAvailableLayerByName(FName LayerName, FOBNavigationMapLayerSpec& OutLayerSpec) const;

	/** Binds optional Blueprint controls when present. */
	void BindOptionalTacticalControls();

	/** Refreshes optional control values and status labels from current state. */
	void RefreshTacticalControlState();

	/** Refreshes optional tiled runtime status labels from current tile manager stats. */
	void RefreshTileDebugState();

	/** Populates the optional map-layer combo box from navigation layers. */
	void PopulateLayerComboBox();

	/** Populates the optional marker-layer combo box from active full-map markers. */
	void PopulateMarkerLayerComboBox();

	/** Returns whether the screen-space position is inside the optional map input area. */
	bool IsPointerOverMapInputArea(const FPointerEvent& InMouseEvent) const;

	/** Converts a normalized slider value into the configured zoom range. */
	float GetZoomFromSliderValue(float SliderValue) const;

	/** Converts current zoom into a normalized slider value. */
	float GetSliderValueFromZoom() const;

	/** Handles the optional zoom-in button click. */
	UFUNCTION()
	void HandleZoomInClicked();

	/** Handles the optional zoom-out button click. */
	UFUNCTION()
	void HandleZoomOutClicked();

	/** Converts optional zoom slider changes into tactical map zoom. */
	UFUNCTION()
	void HandleZoomSliderChanged(float SliderValue);

	/** Handles the optional one-step pan-up button click. */
	UFUNCTION()
	void HandlePanUpClicked();

	/** Handles the optional one-step pan-down button click. */
	UFUNCTION()
	void HandlePanDownClicked();

	/** Handles the optional one-step pan-left button click. */
	UFUNCTION()
	void HandlePanLeftClicked();

	/** Handles the optional one-step pan-right button click. */
	UFUNCTION()
	void HandlePanRightClicked();

	/** Handles the optional recenter button click. */
	UFUNCTION()
	void HandleRecenterClicked();

	/** Handles optional follow checkbox state changes. */
	UFUNCTION()
	void HandleFollowCheckChanged(bool bIsChecked);

	/** Handles optional tactical layer combo-box selection changes. */
	UFUNCTION()
	void HandleLayerSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	/** Handles optional clear-layer-override button click. */
	UFUNCTION()
	void HandleClearLayerOverrideClicked();

	/** Handles optional marker-layer combo-box selection changes. */
	UFUNCTION()
	void HandleMarkerLayerSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	/** Handles optional marker-layer enabled checkbox changes. */
	UFUNCTION()
	void HandleMarkerLayerEnabledChanged(bool bIsChecked);

	/** Applies optional overlay category text-box commits. */
	UFUNCTION()
	void HandleOverlayCategoryCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	/** Applies optional overlay tag text-box commits. */
	UFUNCTION()
	void HandleOverlayTagCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	/** Handles optional clear-overlay-filters button click. */
	UFUNCTION()
	void HandleClearOverlayFiltersClicked();

	/** Tactical behavior configuration assigned at initialization. */
	UPROPERTY(Transient)
	TObjectPtr<UOBTacticalMapConfigAsset> TacticalConfigAsset;

	/** Current map view center in normalized UV space. */
	FVector2D ViewCenterUV = FVector2D(0.5f, 0.5f);

	/** Continuous pan direction input. */
	FVector2D PanInput = FVector2D::ZeroVector;

	/** True when ViewCenterUV follows the tracked player's projected UV. */
	bool bIsFollowingTrackedPlayer = true;

	/** True when marker layer filtering is active. */
	bool bHasMarkerLayerFilter = false;

	/** Enabled tactical marker layers. */
	TSet<FName> EnabledMarkerLayers;

	/** Overlay category filter for tactical overlays. */
	FName OverlayCategoryFilter = NAME_None;

	/** Overlay tag filter for tactical overlays. */
	FName OverlayTagFilter = NAME_None;

	/** True when a tactical layer override is active. */
	bool bHasTacticalLayerOverride = false;

	/** Tactical layer override name. */
	FName TacticalLayerOverrideName = NAME_None;

	/** Currently selected marker layer in the optional marker filter UI. */
	FName SelectedMarkerLayerName = NAME_None;

	/** Prevents optional widget event handlers from reacting to programmatic refreshes. */
	bool bIsRefreshingTacticalControls = false;

	/** Prevents duplicate delegate binding if initialization is called more than once. */
	bool bAreOptionalTacticalControlsBound = false;

	/** True while the optional map input area is being dragged. */
	bool bIsDraggingMapInputArea = false;

	/** Optional button for zooming in by one configured step. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> ZoomInButton;

	/** Optional button for zooming out by one configured step. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> ZoomOutButton;

	/** Optional normalized zoom slider. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<USlider> ZoomSlider;

	/** Optional text label for the current zoom value. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ZoomValueText;

	/** Optional one-shot pan buttons. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PanUpButton;

	/** Optional button that pans the tactical view down by one step. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PanDownButton;

	/** Optional button that pans the tactical view left by one step. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PanLeftButton;

	/** Optional button that pans the tactical view right by one step. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PanRightButton;

	/** Optional map hit area used for mouse wheel and drag input. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> MapInputArea;

	/** Optional recenter/follow controls. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> RecenterButton;

	/** Optional checkbox that toggles follow-tracked-player mode. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCheckBox> FollowPlayerCheckBox;

	/** Optional text label for current follow mode. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> FollowStateText;

	/** Optional layer controls. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UComboBoxString> LayerComboBox;

	/** Optional button that clears tactical layer override. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> ClearLayerOverrideButton;

	/** Optional text label for the active tactical layer. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ActiveLayerText;

	/** Optional marker filter controls. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UComboBoxString> MarkerLayerComboBox;

	/** Optional checkbox that enables or disables the selected marker layer. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCheckBox> MarkerLayerEnabledCheckBox;

	/** Optional text label summarizing active marker filters. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ActiveMarkerFilterText;

	/** Optional overlay filter controls. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UEditableTextBox> OverlayCategoryTextBox;

	/** Optional text box for overlay tag filter input. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UEditableTextBox> OverlayTagTextBox;

	/** Optional button that clears overlay filters. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> ClearOverlayFiltersButton;

	/** Optional text label summarizing active overlay filters. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ActiveOverlayFilterText;

	/** Optional debug/status labels. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ViewCenterText;

	/** Optional text label showing current continuous pan input. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PanInputText;

	/** Optional text label showing current tile LOD and tile counts. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TileLODText;
};
