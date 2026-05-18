#include "Widget/OBTacticalMapWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Data/OBMapMarker.h"
#include "Data/OBMinimapConfigAsset.h"
#include "Data/OBTacticalMapConfigAsset.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "OBNavigation.h"
#include "OBNavigationSubsystem.h"

void UOBTacticalMapWidget::InitializeTacticalMapAndStartTracking(UOBMinimapConfigAsset* InMinimapConfigAsset,
                                                                 UOBTacticalMapConfigAsset* InTacticalConfigAsset)
{
	if (!InTacticalConfigAsset)
	{
		UE_LOG(LogOBNavigation, Error,
		       TEXT("[%s::%hs] - Initialization failed: invalid tactical map config."), *GetName(),
		       __FUNCTION__);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	TacticalConfigAsset = InTacticalConfigAsset;
	ViewCenterUV = FVector2D(0.5f, 0.5f);
	PanInput = FVector2D::ZeroVector;
	bIsFollowingTrackedPlayer = TacticalConfigAsset->bStartFollowingTrackedPlayer;
	OverlayCategoryFilter = TacticalConfigAsset->DefaultOverlayCategoryFilter;
	OverlayTagFilter = TacticalConfigAsset->DefaultOverlayTagFilter;
	EnabledMarkerLayers.Reset();
	for (const FName LayerName : TacticalConfigAsset->DefaultEnabledMarkerLayers)
	{
		if (!LayerName.IsNone())
		{
			EnabledMarkerLayers.Add(LayerName);
		}
	}
	bHasMarkerLayerFilter = !EnabledMarkerLayers.IsEmpty();
	SelectedMarkerLayerName = EnabledMarkerLayers.IsEmpty() ? NAME_None : *EnabledMarkerLayers.CreateConstIterator();
	bHasTacticalLayerOverride = false;
	TacticalLayerOverrideName = NAME_None;

	InitializeMapWidget(InMinimapConfigAsset);
	SetMapZoom(GetInitialZoom());
	BindOptionalTacticalControls();
	PopulateLayerComboBox();
	PopulateMarkerLayerComboBox();

	if (bIsFollowingTrackedPlayer)
	{
		RecenterOnTrackedPlayer();
	}
	else
	{
		BroadcastViewChanged();
	}

	RefreshTacticalControlState();
}

void UOBTacticalMapWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	ApplyContinuousPanInput(InDeltaTime);
	Super::NativeTick(MyGeometry, InDeltaTime);
}

void UOBTacticalMapWidget::AddZoomInput(const float ZoomDelta)
{
	const float Step = TacticalConfigAsset ? FMath::Max(0.01f, TacticalConfigAsset->ZoomStep) : 0.25f;
	SetMapZoom(GetMapZoom() + ZoomDelta * Step);
	BroadcastViewChanged();
	RefreshTacticalControlState();
}

void UOBTacticalMapWidget::SetTacticalMapZoom(const float NewZoom)
{
	SetMapZoom(NewZoom);
	BroadcastViewChanged();
	RefreshTacticalControlState();
}

void UOBTacticalMapWidget::AddPanInput(const FVector2D PanDelta)
{
	UCanvasPanel* MarkerCanvas = GetMarkerCanvas();
	if (PanDelta.IsNearlyZero() || !MarkerCanvas)
	{
		return;
	}

	const FVector2D CanvasSize = MarkerCanvas->GetCachedGeometry().GetLocalSize();
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return;
	}

	const float AppliedRotation = -GetTotalStaticRotation();
	const FVector2D UnrotatedPanDelta = PanDelta.GetRotated(-AppliedRotation);
	const float SafeZoom = FMath::Max(GetMapZoom(), KINDA_SMALL_NUMBER);

	// Convert screen-space panning into UV distance; zoomed-in views move less per pixel.
	const FVector2D UVDelta(
		UnrotatedPanDelta.X / (CanvasSize.X * SafeZoom),
		UnrotatedPanDelta.Y / (CanvasSize.Y * SafeZoom));

	SetViewCenterUVInternal(ViewCenterUV - UVDelta, false);
}

void UOBTacticalMapWidget::SetPanInput(const FVector2D InPanInput)
{
	PanInput = InPanInput;
	RefreshTacticalControlState();
}

void UOBTacticalMapWidget::SetViewCenterWorldLocation(const FVector WorldLocation)
{
	if (!NavSubsystem)
	{
		return;
	}

	FOBNavigationMapLayerSpec CurrentLayer;
	if (!ResolveActiveLayer(CurrentLayer))
	{
		return;
	}

	FVector2D MapUV;
	EOBMapProjectionResult ProjectionResult = EOBMapProjectionResult::NoLayer;
	if (NavSubsystem->WorldToMapUVChecked(CurrentLayer, WorldLocation, MapUV, ProjectionResult))
	{
		SetViewCenterUVInternal(MapUV, false);
	}
}

void UOBTacticalMapWidget::SetViewCenterUV(const FVector2D MapUV)
{
	SetViewCenterUVInternal(MapUV, false);
}

void UOBTacticalMapWidget::RecenterOnTrackedPlayer()
{
	if (!NavSubsystem)
	{
		SetViewCenterUVInternal(ViewCenterUV, true);
		return;
	}

	const APawn* TrackedPawn = NavSubsystem->GetTrackedPlayerPawn();
	FOBNavigationMapLayerSpec CurrentLayer;
	if (!TrackedPawn || !ResolveActiveLayer(CurrentLayer))
	{
		SetViewCenterUVInternal(ViewCenterUV, true);
		return;
	}

	FVector2D PlayerUV;
	EOBMapProjectionResult ProjectionResult = EOBMapProjectionResult::NoLayer;
	if (NavSubsystem->WorldToMapUVChecked(CurrentLayer, OBNavigation::ResolveActorNavigationLocation(TrackedPawn), PlayerUV,
	                                      ProjectionResult))
	{
		SetViewCenterUVInternal(PlayerUV, true);
	}
	else
	{
		SetViewCenterUVInternal(ViewCenterUV, true);
	}
}

void UOBTacticalMapWidget::SetFollowTrackedPlayer(const bool bFollow)
{
	if (bFollow)
	{
		RecenterOnTrackedPlayer();
		return;
	}

	bIsFollowingTrackedPlayer = false;
	BroadcastViewChanged();
	RefreshTacticalControlState();
}

void UOBTacticalMapWidget::SetMarkerLayerFilter(const FName LayerName, const bool bEnabled)
{
	if (LayerName.IsNone())
	{
		return;
	}

	bHasMarkerLayerFilter = true;
	if (bEnabled)
	{
		EnabledMarkerLayers.Add(LayerName);
	}
	else
	{
		EnabledMarkerLayers.Remove(LayerName);
	}

	OnTacticalMapFilterChanged.Broadcast();
	RefreshTacticalControlState();
}

void UOBTacticalMapWidget::ClearMarkerLayerFilter()
{
	bHasMarkerLayerFilter = false;
	EnabledMarkerLayers.Reset();
	SelectedMarkerLayerName = NAME_None;
	OnTacticalMapFilterChanged.Broadcast();
	RefreshTacticalControlState();
}

void UOBTacticalMapWidget::SetOverlayCategoryFilter(const FName Category)
{
	if (OverlayCategoryFilter == Category)
	{
		return;
	}

	OverlayCategoryFilter = Category;
	OnTacticalMapFilterChanged.Broadcast();
	RefreshTacticalControlState();
}

void UOBTacticalMapWidget::SetOverlayTagFilter(const FName Tag)
{
	if (OverlayTagFilter == Tag)
	{
		return;
	}

	OverlayTagFilter = Tag;
	OnTacticalMapFilterChanged.Broadcast();
	RefreshTacticalControlState();
}

void UOBTacticalMapWidget::ClearOverlayFilters()
{
	const bool bHadFilters = !OverlayCategoryFilter.IsNone() || !OverlayTagFilter.IsNone();
	OverlayCategoryFilter = NAME_None;
	OverlayTagFilter = NAME_None;
	if (bHadFilters)
	{
		OnTacticalMapFilterChanged.Broadcast();
	}
	RefreshTacticalControlState();
}

void UOBTacticalMapWidget::SetTacticalMapLayerByName(const FName LayerName)
{
	if (!TacticalConfigAsset || !TacticalConfigAsset->bAllowLayerSwitching || LayerName.IsNone())
	{
		return;
	}

	FOBNavigationMapLayerSpec LayerSpec;
	if (!FindAvailableLayerByName(LayerName, LayerSpec))
	{
		return;
	}

	bHasTacticalLayerOverride = true;
	TacticalLayerOverrideName = LayerName;
	ApplyMapLayer(LayerSpec);
	OnTacticalMapLayerChanged.Broadcast(TacticalLayerOverrideName);
	RefreshTacticalControlState();
}

void UOBTacticalMapWidget::ClearTacticalMapLayerOverride()
{
	if (!bHasTacticalLayerOverride)
	{
		return;
	}

	bHasTacticalLayerOverride = false;
	TacticalLayerOverrideName = NAME_None;

	FOBNavigationMapLayerSpec CurrentLayer;
	if (ResolveActiveLayer(CurrentLayer))
	{
		ApplyMapLayer(CurrentLayer);
		OnTacticalMapLayerChanged.Broadcast(CurrentLayer.LayerName);
	}
	else
	{
		ApplyMapLayer(FOBNavigationMapLayerSpec());
		OnTacticalMapLayerChanged.Broadcast(NAME_None);
	}
	RefreshTacticalControlState();
}

bool UOBTacticalMapWidget::ResolveActiveLayer(FOBNavigationMapLayerSpec& OutLayerSpec) const
{
	if (bHasTacticalLayerOverride && TacticalConfigAsset && TacticalConfigAsset->bAllowLayerSwitching)
	{
		return FindAvailableLayerByName(TacticalLayerOverrideName, OutLayerSpec);
	}

	return Super::ResolveActiveLayer(OutLayerSpec);
}

float UOBTacticalMapWidget::GetInitialZoom() const
{
	return TacticalConfigAsset ? TacticalConfigAsset->InitialZoom : Super::GetInitialZoom();
}

float UOBTacticalMapWidget::GetMinimumZoom() const
{
	return TacticalConfigAsset ? TacticalConfigAsset->MinZoom : Super::GetMinimumZoom();
}

float UOBTacticalMapWidget::GetMaximumZoom() const
{
	return TacticalConfigAsset ? TacticalConfigAsset->MaxZoom : Super::GetMaximumZoom();
}

bool UOBTacticalMapWidget::ShouldRotateMap() const
{
	return false;
}

bool UOBTacticalMapWidget::ShouldCenterPlayerMarker() const
{
	return false;
}

bool UOBTacticalMapWidget::ShouldShowPlayerMarker() const
{
	return TacticalConfigAsset && TacticalConfigAsset->bShowPlayerMarker;
}

bool UOBTacticalMapWidget::ShouldShowMarker(const UOBMapMarker* Marker) const
{
	if (!Super::ShouldShowMarker(Marker))
	{
		return false;
	}

	return !bHasMarkerLayerFilter || EnabledMarkerLayers.Contains(Marker->MarkerLayerName);
}

float UOBTacticalMapWidget::GetMarkerScale() const
{
	return TacticalConfigAsset ? FMath::Max(0.01f, TacticalConfigAsset->MarkerScale) : 1.0f;
}

FName UOBTacticalMapWidget::GetOverlayCategoryFilter() const
{
	return OverlayCategoryFilter;
}

FName UOBTacticalMapWidget::GetOverlayTagFilter() const
{
	return OverlayTagFilter;
}

bool UOBTacticalMapWidget::ResolveViewCenterUV(const FOBNavigationMapLayerSpec& CurrentLayer,
                                               const APawn* TrackedPawn, FVector2D& OutViewCenterUV) const
{
	if (bIsFollowingTrackedPlayer && NavSubsystem && TrackedPawn)
	{
		FVector2D PlayerUV;
		EOBMapProjectionResult ProjectionResult = EOBMapProjectionResult::NoLayer;
		if (NavSubsystem->WorldToMapUVChecked(CurrentLayer, OBNavigation::ResolveActorNavigationLocation(TrackedPawn), PlayerUV,
			                                      ProjectionResult))
		{
			OutViewCenterUV = PlayerUV;
			return true;
		}
	}

	OutViewCenterUV = ViewCenterUV;
	return true;
}

float UOBTacticalMapWidget::GetDynamicMapYaw(const APawn* TrackedPawn) const
{
	return 0.0f;
}

float UOBTacticalMapWidget::GetTotalStaticRotation() const
{
	const UOBMinimapConfigAsset* VisualConfig = GetVisualConfig();
	return (VisualConfig ? VisualConfig->MapRotationOffset : 0.0f) + GetAlignmentAngle();
}

void UOBTacticalMapWidget::OnViewContextUpdated(const FOBNavigationMapViewContext& ViewContext,
                                                const FOBNavigationMapLayerSpec& CurrentLayer,
                                                const APawn* TrackedPawn)
{
	if (bIsFollowingTrackedPlayer && !ViewCenterUV.Equals(ViewContext.ViewCenterUV, KINDA_SMALL_NUMBER))
	{
		ViewCenterUV = ViewContext.ViewCenterUV;
		BroadcastViewChanged();
		RefreshTacticalControlState();
	}
}

void UOBTacticalMapWidget::ApplyContinuousPanInput(const float DeltaTime)
{
	if (!bIsInitializedAndTracking || !ShouldUpdateMapThisFrame() || !TacticalConfigAsset || PanInput.IsNearlyZero())
	{
		return;
	}

	AddPanInput(PanInput * TacticalConfigAsset->PanSpeed * DeltaTime);
}

void UOBTacticalMapWidget::SetViewCenterUVInternal(const FVector2D MapUV, const bool bFollowTrackedPlayer)
{
	const bool bShouldClamp = !TacticalConfigAsset || TacticalConfigAsset->bClampViewToMapBounds;
	ViewCenterUV = bShouldClamp
			       ? FVector2D(FMath::Clamp(MapUV.X, 0.0f, 1.0f), FMath::Clamp(MapUV.Y, 0.0f, 1.0f))
			       : MapUV;
	bIsFollowingTrackedPlayer = bFollowTrackedPlayer;
	BroadcastViewChanged();
	RefreshTacticalControlState();
}

void UOBTacticalMapWidget::BroadcastViewChanged() const
{
	OnTacticalMapViewChanged.Broadcast(ViewCenterUV, GetMapZoom(), bIsFollowingTrackedPlayer);
}

bool UOBTacticalMapWidget::FindAvailableLayerByName(const FName LayerName, FOBNavigationMapLayerSpec& OutLayerSpec) const
{
	if (!NavSubsystem || LayerName.IsNone())
	{
		return false;
	}

	TArray<FOBNavigationMapLayerSpec> AvailableLayerSpecs;
	NavSubsystem->GetAvailableMapLayerSpecs(AvailableLayerSpecs);
	for (const FOBNavigationMapLayerSpec& LayerSpec : AvailableLayerSpecs)
	{
		if (LayerSpec.LayerName == LayerName)
		{
			OutLayerSpec = LayerSpec;
			return true;
		}
	}

	return false;
}

FReply UOBTacticalMapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (MapInputArea && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && IsPointerOverMapInputArea(InMouseEvent))
	{
		bIsDraggingMapInputArea = true;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UOBTacticalMapWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDraggingMapInputArea && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDraggingMapInputArea = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UOBTacticalMapWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDraggingMapInputArea && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		AddPanInput(InMouseEvent.GetCursorDelta());
		return FReply::Handled();
	}

	if (bIsDraggingMapInputArea)
	{
		bIsDraggingMapInputArea = false;
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UOBTacticalMapWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (MapInputArea && IsPointerOverMapInputArea(InMouseEvent))
	{
		AddZoomInput(InMouseEvent.GetWheelDelta());
		return FReply::Handled();
	}

	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void UOBTacticalMapWidget::BindOptionalTacticalControls()
{
	if (bAreOptionalTacticalControlsBound)
	{
		return;
	}

	if (ZoomInButton)
	{
		ZoomInButton->OnClicked.AddDynamic(this, &UOBTacticalMapWidget::HandleZoomInClicked);
	}
	if (ZoomOutButton)
	{
		ZoomOutButton->OnClicked.AddDynamic(this, &UOBTacticalMapWidget::HandleZoomOutClicked);
	}
	if (ZoomSlider)
	{
		ZoomSlider->OnValueChanged.AddDynamic(this, &UOBTacticalMapWidget::HandleZoomSliderChanged);
	}
	if (PanUpButton)
	{
		PanUpButton->OnClicked.AddDynamic(this, &UOBTacticalMapWidget::HandlePanUpClicked);
	}
	if (PanDownButton)
	{
		PanDownButton->OnClicked.AddDynamic(this, &UOBTacticalMapWidget::HandlePanDownClicked);
	}
	if (PanLeftButton)
	{
		PanLeftButton->OnClicked.AddDynamic(this, &UOBTacticalMapWidget::HandlePanLeftClicked);
	}
	if (PanRightButton)
	{
		PanRightButton->OnClicked.AddDynamic(this, &UOBTacticalMapWidget::HandlePanRightClicked);
	}
	if (RecenterButton)
	{
		RecenterButton->OnClicked.AddDynamic(this, &UOBTacticalMapWidget::HandleRecenterClicked);
	}
	if (FollowPlayerCheckBox)
	{
		FollowPlayerCheckBox->OnCheckStateChanged.AddDynamic(this, &UOBTacticalMapWidget::HandleFollowCheckChanged);
	}
	if (LayerComboBox)
	{
		LayerComboBox->OnSelectionChanged.AddDynamic(this, &UOBTacticalMapWidget::HandleLayerSelectionChanged);
	}
	if (ClearLayerOverrideButton)
	{
		ClearLayerOverrideButton->OnClicked.AddDynamic(this, &UOBTacticalMapWidget::HandleClearLayerOverrideClicked);
	}
	if (MarkerLayerComboBox)
	{
		MarkerLayerComboBox->OnSelectionChanged.AddDynamic(this, &UOBTacticalMapWidget::HandleMarkerLayerSelectionChanged);
	}
	if (MarkerLayerEnabledCheckBox)
	{
		MarkerLayerEnabledCheckBox->OnCheckStateChanged.AddDynamic(this, &UOBTacticalMapWidget::HandleMarkerLayerEnabledChanged);
	}
	if (OverlayCategoryTextBox)
	{
		OverlayCategoryTextBox->OnTextCommitted.AddDynamic(this, &UOBTacticalMapWidget::HandleOverlayCategoryCommitted);
	}
	if (OverlayTagTextBox)
	{
		OverlayTagTextBox->OnTextCommitted.AddDynamic(this, &UOBTacticalMapWidget::HandleOverlayTagCommitted);
	}
	if (ClearOverlayFiltersButton)
	{
		ClearOverlayFiltersButton->OnClicked.AddDynamic(this, &UOBTacticalMapWidget::HandleClearOverlayFiltersClicked);
	}

	bAreOptionalTacticalControlsBound = true;
}

void UOBTacticalMapWidget::RefreshTacticalControlState()
{
	bIsRefreshingTacticalControls = true;

	if (ZoomSlider)
	{
		ZoomSlider->SetValue(GetSliderValueFromZoom());
	}
	if (ZoomValueText)
	{
		ZoomValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2fx"), GetMapZoom())));
	}
	if (FollowPlayerCheckBox)
	{
		FollowPlayerCheckBox->SetIsChecked(bIsFollowingTrackedPlayer);
	}
	if (FollowStateText)
	{
		FollowStateText->SetText(FText::FromString(bIsFollowingTrackedPlayer ? TEXT("Following") : TEXT("Free Pan")));
	}
	if (LayerComboBox)
	{
		LayerComboBox->SetSelectedOption(bHasTacticalLayerOverride ? TacticalLayerOverrideName.ToString() : AutoLayerOption);
	}
	if (ActiveLayerText)
	{
		FName ActiveLayerName = NAME_None;
		if (bHasTacticalLayerOverride)
		{
			ActiveLayerName = TacticalLayerOverrideName;
		}
		else
		{
			FOBNavigationMapLayerSpec CurrentLayer;
			if (ResolveActiveLayer(CurrentLayer))
			{
				ActiveLayerName = CurrentLayer.LayerName;
			}
		}

		ActiveLayerText->SetText(FText::FromString(ActiveLayerName.IsNone()
			                                           ? FString(TEXT("Layer: None"))
			                                           : FString::Printf(TEXT("Layer: %s"), *ActiveLayerName.ToString())));
	}
	if (MarkerLayerComboBox)
	{
		MarkerLayerComboBox->SetSelectedOption(SelectedMarkerLayerName.IsNone()
			                                       ? FString(AllMarkerLayersOption)
			                                       : SelectedMarkerLayerName.ToString());
	}
	if (MarkerLayerEnabledCheckBox)
	{
		const bool bSelectedLayerEnabled = !SelectedMarkerLayerName.IsNone() && EnabledMarkerLayers.Contains(SelectedMarkerLayerName);
		MarkerLayerEnabledCheckBox->SetIsChecked(!bHasMarkerLayerFilter || bSelectedLayerEnabled);
	}
	if (ActiveMarkerFilterText)
	{
		if (!bHasMarkerLayerFilter)
		{
			ActiveMarkerFilterText->SetText(FText::FromString(TEXT("Marker Filter: All")));
		}
		else
		{
			TArray<FString> LayerNames;
			for (const FName LayerName : EnabledMarkerLayers)
			{
				LayerNames.Add(LayerName.ToString());
			}
			LayerNames.Sort();
			ActiveMarkerFilterText->SetText(FText::FromString(FString::Printf(TEXT("Marker Filter: %s"),
				LayerNames.IsEmpty() ? TEXT("None") : *FString::Join(LayerNames, TEXT(", ")))));
		}
	}
	if (OverlayCategoryTextBox)
	{
		OverlayCategoryTextBox->SetText(OverlayCategoryFilter.IsNone()
			                                ? FText::GetEmpty()
			                                : FText::FromName(OverlayCategoryFilter));
	}
	if (OverlayTagTextBox)
	{
		OverlayTagTextBox->SetText(OverlayTagFilter.IsNone()
			                           ? FText::GetEmpty()
			                           : FText::FromName(OverlayTagFilter));
	}
	if (ActiveOverlayFilterText)
	{
		const FString Category = OverlayCategoryFilter.IsNone() ? TEXT("All") : OverlayCategoryFilter.ToString();
		const FString Tag = OverlayTagFilter.IsNone() ? TEXT("All") : OverlayTagFilter.ToString();
		ActiveOverlayFilterText->SetText(FText::FromString(FString::Printf(TEXT("Overlay: Category=%s Tag=%s"), *Category, *Tag)));
	}
	if (ViewCenterText)
	{
		ViewCenterText->SetText(FText::FromString(FString::Printf(TEXT("Center: %.3f, %.3f"), ViewCenterUV.X, ViewCenterUV.Y)));
	}
	if (PanInputText)
	{
		PanInputText->SetText(FText::FromString(FString::Printf(TEXT("Pan: %.2f, %.2f"), PanInput.X, PanInput.Y)));
	}

	bIsRefreshingTacticalControls = false;
}

void UOBTacticalMapWidget::PopulateLayerComboBox()
{
	if (!LayerComboBox)
	{
		return;
	}

	bIsRefreshingTacticalControls = true;
	LayerComboBox->ClearOptions();
	LayerComboBox->AddOption(AutoLayerOption);

	if (NavSubsystem)
	{
		TArray<FOBNavigationMapLayerSpec> AvailableLayerSpecs;
		NavSubsystem->GetAvailableMapLayerSpecs(AvailableLayerSpecs);
		for (const FOBNavigationMapLayerSpec& LayerSpec : AvailableLayerSpecs)
		{
			if (!LayerSpec.LayerName.IsNone())
			{
				LayerComboBox->AddOption(LayerSpec.LayerName.ToString());
			}
		}
	}

	LayerComboBox->SetSelectedOption(AutoLayerOption);
	bIsRefreshingTacticalControls = false;
}

void UOBTacticalMapWidget::PopulateMarkerLayerComboBox()
{
	if (!MarkerLayerComboBox)
	{
		return;
	}

	bIsRefreshingTacticalControls = true;
	MarkerLayerComboBox->ClearOptions();
	MarkerLayerComboBox->AddOption(AllMarkerLayersOption);

	TArray<FName> LayerNames;
	if (NavSubsystem)
	{
		for (const UOBMapMarker* Marker : NavSubsystem->GetAllActiveMarkers())
		{
			if (Marker && !Marker->MarkerLayerName.IsNone())
			{
				LayerNames.AddUnique(Marker->MarkerLayerName);
			}
		}
	}
	LayerNames.Sort([](const FName& A, const FName& B)
	{
		return A.LexicalLess(B);
	});

	for (const FName LayerName : LayerNames)
	{
		MarkerLayerComboBox->AddOption(LayerName.ToString());
	}

	MarkerLayerComboBox->SetSelectedOption(SelectedMarkerLayerName.IsNone()
		                                       ? FString(AllMarkerLayersOption)
		                                       : SelectedMarkerLayerName.ToString());
	bIsRefreshingTacticalControls = false;
}

bool UOBTacticalMapWidget::IsPointerOverMapInputArea(const FPointerEvent& InMouseEvent) const
{
	return MapInputArea && MapInputArea->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
}

float UOBTacticalMapWidget::GetZoomFromSliderValue(const float SliderValue) const
{
	const float MinimumZoom = FMath::Min(GetMinimumZoom(), GetMaximumZoom());
	const float MaximumZoom = FMath::Max(GetMinimumZoom(), GetMaximumZoom());
	return FMath::Lerp(MinimumZoom, MaximumZoom, FMath::Clamp(SliderValue, 0.0f, 1.0f));
}

float UOBTacticalMapWidget::GetSliderValueFromZoom() const
{
	const float MinimumZoom = FMath::Min(GetMinimumZoom(), GetMaximumZoom());
	const float MaximumZoom = FMath::Max(GetMinimumZoom(), GetMaximumZoom());
	if (FMath::IsNearlyEqual(MinimumZoom, MaximumZoom))
	{
		return 0.0f;
	}

	return FMath::Clamp((GetMapZoom() - MinimumZoom) / (MaximumZoom - MinimumZoom), 0.0f, 1.0f);
}

void UOBTacticalMapWidget::HandleZoomInClicked()
{
	AddZoomInput(1.0f);
}

void UOBTacticalMapWidget::HandleZoomOutClicked()
{
	AddZoomInput(-1.0f);
}

void UOBTacticalMapWidget::HandleZoomSliderChanged(const float SliderValue)
{
	if (!bIsRefreshingTacticalControls)
	{
		SetTacticalMapZoom(GetZoomFromSliderValue(SliderValue));
	}
}

void UOBTacticalMapWidget::HandlePanUpClicked()
{
	const float Step = TacticalConfigAsset ? TacticalConfigAsset->PanSpeed : 600.0f;
	AddPanInput(FVector2D(0.0f, -Step));
}

void UOBTacticalMapWidget::HandlePanDownClicked()
{
	const float Step = TacticalConfigAsset ? TacticalConfigAsset->PanSpeed : 600.0f;
	AddPanInput(FVector2D(0.0f, Step));
}

void UOBTacticalMapWidget::HandlePanLeftClicked()
{
	const float Step = TacticalConfigAsset ? TacticalConfigAsset->PanSpeed : 600.0f;
	AddPanInput(FVector2D(-Step, 0.0f));
}

void UOBTacticalMapWidget::HandlePanRightClicked()
{
	const float Step = TacticalConfigAsset ? TacticalConfigAsset->PanSpeed : 600.0f;
	AddPanInput(FVector2D(Step, 0.0f));
}

void UOBTacticalMapWidget::HandleRecenterClicked()
{
	RecenterOnTrackedPlayer();
}

void UOBTacticalMapWidget::HandleFollowCheckChanged(const bool bIsChecked)
{
	if (!bIsRefreshingTacticalControls)
	{
		SetFollowTrackedPlayer(bIsChecked);
	}
}

void UOBTacticalMapWidget::HandleLayerSelectionChanged(const FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bIsRefreshingTacticalControls || SelectionType == ESelectInfo::Direct)
	{
		return;
	}

	if (SelectedItem == AutoLayerOption)
	{
		ClearTacticalMapLayerOverride();
	}
	else
	{
		SetTacticalMapLayerByName(FName(*SelectedItem));
	}
}

void UOBTacticalMapWidget::HandleClearLayerOverrideClicked()
{
	ClearTacticalMapLayerOverride();
}

void UOBTacticalMapWidget::HandleMarkerLayerSelectionChanged(const FString SelectedItem, const ESelectInfo::Type SelectionType)
{
	if (bIsRefreshingTacticalControls || SelectionType == ESelectInfo::Direct)
	{
		return;
	}

	if (SelectedItem == AllMarkerLayersOption)
	{
		ClearMarkerLayerFilter();
	}
	else
	{
		SelectedMarkerLayerName = FName(*SelectedItem);
		if (MarkerLayerEnabledCheckBox && MarkerLayerEnabledCheckBox->IsChecked())
		{
			SetMarkerLayerFilter(SelectedMarkerLayerName, true);
		}
		else
		{
			RefreshTacticalControlState();
		}
	}
}

void UOBTacticalMapWidget::HandleMarkerLayerEnabledChanged(const bool bIsChecked)
{
	if (!bIsRefreshingTacticalControls && !SelectedMarkerLayerName.IsNone())
	{
		SetMarkerLayerFilter(SelectedMarkerLayerName, bIsChecked);
	}
}

void UOBTacticalMapWidget::HandleOverlayCategoryCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	if (!bIsRefreshingTacticalControls)
	{
		const FString Value = Text.ToString().TrimStartAndEnd();
		SetOverlayCategoryFilter(Value.IsEmpty() ? NAME_None : FName(*Value));
	}
}

void UOBTacticalMapWidget::HandleOverlayTagCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	if (!bIsRefreshingTacticalControls)
	{
		const FString Value = Text.ToString().TrimStartAndEnd();
		SetOverlayTagFilter(Value.IsEmpty() ? NAME_None : FName(*Value));
	}
}

void UOBTacticalMapWidget::HandleClearOverlayFiltersClicked()
{
	ClearOverlayFilters();
}
