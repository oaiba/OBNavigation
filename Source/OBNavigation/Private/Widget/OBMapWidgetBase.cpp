#include "Widget/OBMapWidgetBase.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Data/OBMapMarker.h"
#include "Data/OBMinimapConfigAsset.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "OBNavigation.h"
#include "OBNavigationSubsystem.h"
#include "Widget/OBMapOverlayWidget.h"

void UOBMapWidgetBase::InitializeMapWidget(UOBMinimapConfigAsset* InVisualConfigAsset)
{
	if (bIsInitializedAndTracking)
	{
		UE_LOG(LogOBNavigation, Warning, TEXT("[%s::%hs] - Widget is already initialized."), *GetName(), __FUNCTION__);
		return;
	}

	if (!InVisualConfigAsset)
	{
		UE_LOG(LogOBNavigation, Error, TEXT("[%s::%hs] - Initialization failed: invalid visual config."), *GetName(),
		       __FUNCTION__);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	VisualConfigAsset = InVisualConfigAsset;
	CurrentZoom = GetInitialZoom();

		if (MapImage && VisualConfigAsset->MinimapBackgroundMaterial)
		{
			MapMaterialInstance = UMaterialInstanceDynamic::Create(VisualConfigAsset->MinimapBackgroundMaterial, this);
			MapImage->SetBrushFromMaterial(MapMaterialInstance);
			if (VisualConfigAsset->bShowDebugMessages)
			{
				UE_LOG(LogOBNavigation, Log,
				       TEXT("[%s::%hs] - Created map material instance from '%s'. MapImage='%s' MarkerCanvas='%s' InitialZoom=%.2f"),
				       *GetName(), __FUNCTION__, *GetNameSafe(VisualConfigAsset->MinimapBackgroundMaterial),
				       *GetNameSafe(MapImage), *GetNameSafe(GetMarkerCanvas()), CurrentZoom);
			}
		}
		else
		{
			UE_LOG(LogOBNavigation, Error, TEXT("[%s::%hs] - Failed to set up map image material."), *GetName(),
			       __FUNCTION__);
	}

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		NavSubsystem = GameInstance->GetSubsystem<UOBNavigationSubsystem>();
		if (NavSubsystem)
		{
			if (!NavSubsystem->OnNavigationMapLayerSpecChanged.IsAlreadyBound(this, &UOBMapWidgetBase::OnNavigationMapLayerSpecChanged))
			{
				NavSubsystem->OnNavigationMapLayerSpecChanged.AddDynamic(
					this, &UOBMapWidgetBase::OnNavigationMapLayerSpecChanged);
			}

			FOBNavigationMapLayerSpec CurrentLayerSpec;
			if (ResolveActiveLayer(CurrentLayerSpec))
			{
				ApplyMapLayer(CurrentLayerSpec);
			}
			else
			{
				ApplyMapLayer(FOBNavigationMapLayerSpec());
			}

				if (APawn* TrackedPawn = NavSubsystem->GetTrackedPlayerPawn())
				{
					PlayerMarkerID = NavSubsystem->GetMarkerIDForActor(TrackedPawn);
					if (VisualConfigAsset->bShowDebugMessages)
					{
						UE_LOG(LogOBNavigation, Log,
						       TEXT("[%s::%hs] - Initial tracked pawn='%s' PlayerMarkerID=%s"),
						       *GetName(), __FUNCTION__, *GetNameSafe(TrackedPawn), *PlayerMarkerID.ToString());
					}
				}
			}
		}

	SetMapZoom(CurrentZoom);
	EnsureOverlayWidget();

	if (!MapMaterialInstance || !NavSubsystem || !GetMarkerCanvas())
	{
		UE_LOG(LogOBNavigation, Error,
		       TEXT("[%s::%hs] - Initialization failed due to missing subsystem, material, or marker canvas."),
		       *GetName(), __FUNCTION__);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	bIsInitializedAndTracking = true;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UOBMapWidgetBase::SetMapZoom(const float NewZoom)
{
	const float MinimumZoom = FMath::Min(GetMinimumZoom(), GetMaximumZoom());
	const float MaximumZoom = FMath::Max(GetMinimumZoom(), GetMaximumZoom());
	CurrentZoom = FMath::Clamp(NewZoom, MinimumZoom, MaximumZoom);
	if (MapMaterialInstance)
	{
		MapMaterialInstance->SetScalarParameterValue("Zoom", CurrentZoom);
	}
}

UCanvasPanel* UOBMapWidgetBase::GetMarkerCanvas() const
{
	return MapMarkerCanvas.Get();
}

void UOBMapWidgetBase::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsInitializedAndTracking || !VisualConfigAsset || !NavSubsystem || !ShouldUpdateMapThisFrame())
	{
		return;
	}

	FOBNavigationMapLayerSpec CurrentLayer;
	if (!ResolveActiveLayer(CurrentLayer))
	{
		ClearMarkerWidgets();
		if (OverlayWidget)
		{
			OverlayWidget->ClearOverlayContext();
			}
			ApplyMapLayer(FOBNavigationMapLayerSpec());
			if (VisualConfigAsset->bShowDebugMessages || StartupTraceLogCount < 5)
			{
				++StartupTraceLogCount;
				UE_LOG(LogOBNavigation, Warning,
				       TEXT("[%s::%hs] - Trace #%d No active layer. TrackedPawn='%s' MapMaterial=%s Debug=%s"),
				       *GetName(), __FUNCTION__, StartupTraceLogCount,
				       *GetNameSafe(NavSubsystem ? NavSubsystem->GetTrackedPlayerPawn() : nullptr),
				       MapMaterialInstance ? TEXT("Valid") : TEXT("Invalid"),
				       VisualConfigAsset->bShowDebugMessages ? TEXT("true") : TEXT("false"));
			}
			return;
		}

	ApplyMapLayer(CurrentLayer);

	const APawn* TrackedPawn = NavSubsystem->GetTrackedPlayerPawn();
	if (TrackedPawn && !PlayerMarkerID.IsValid())
	{
		PlayerMarkerID = NavSubsystem->GetMarkerIDForActor(const_cast<APawn*>(TrackedPawn));
		if (VisualConfigAsset->bShowDebugMessages)
		{
			UE_LOG(LogOBNavigation, Log,
			       TEXT("[%s::%hs] - Resolved player marker after tick. Pawn='%s' PlayerMarkerID=%s"),
			       *GetName(), __FUNCTION__, *GetNameSafe(TrackedPawn), *PlayerMarkerID.ToString());
		}
	}

	FVector2D ViewCenterUV;
	if (!ResolveViewCenterUV(CurrentLayer, TrackedPawn, ViewCenterUV))
	{
		FVector2D DirectPawnUV = FVector2D::ZeroVector;
		EOBMapProjectionResult ProjectionResult = EOBMapProjectionResult::NoLayer;
		const bool bDirectProjectionSucceeded = TrackedPawn
			                                        && NavSubsystem->WorldToMapUVChecked(
				                                        CurrentLayer,
				                                        OBNavigation::ResolveActorNavigationLocation(TrackedPawn),
				                                        DirectPawnUV,
				                                        ProjectionResult);
			const FVector ResolvedPawnLocation = TrackedPawn
				                                      ? OBNavigation::ResolveActorNavigationLocation(TrackedPawn)
				                                      : FVector::ZeroVector;
			if (VisualConfigAsset->bShowDebugMessages || StartupTraceLogCount < 5)
			{
				++StartupTraceLogCount;
				UE_LOG(LogOBNavigation, Warning,
				       TEXT("[%s::%hs] - Trace #%d Failed to resolve view center. Layer='%s' Pawn='%s' ActorLoc=%s ResolvedLoc=%s ResolveSource=[%s] DirectProjection=%s Result=%s DirectUV=%s BoundsMin=%s BoundsMax=%s LocationCandidates=[%s] ComponentSnapshot=[%s] Debug=%s"),
				       *GetName(), __FUNCTION__, StartupTraceLogCount, *CurrentLayer.LayerName.ToString(),
				       *GetNameSafe(TrackedPawn),
				       TrackedPawn ? *TrackedPawn->GetActorLocation().ToString() : TEXT("None"),
				       *ResolvedPawnLocation.ToString(),
				       *OBNavigation::DescribeActorNavigationLocationSource(TrackedPawn),
				       bDirectProjectionSucceeded ? TEXT("true") : TEXT("false"),
				       *StaticEnum<EOBMapProjectionResult>()->GetNameStringByValue(static_cast<int64>(ProjectionResult)),
				       *DirectPawnUV.ToString(), *CurrentLayer.WorldBounds.Min.ToString(),
				       *CurrentLayer.WorldBounds.Max.ToString(),
				       *OBNavigation::DescribeActorNavigationLocationCandidates(TrackedPawn),
				       *OBNavigation::DescribeActorNavigationComponentSnapshot(TrackedPawn),
				       VisualConfigAsset->bShowDebugMessages ? TEXT("true") : TEXT("false"));
			}
		ClearMarkerWidgets();
		if (OverlayWidget)
		{
			OverlayWidget->ClearOverlayContext();
		}
		return;
	}

	const FOBNavigationMapViewContext ViewContext = BuildViewContext(CurrentLayer, TrackedPawn, ViewCenterUV);
	UpdateMapMaterial(ViewContext);
	OnViewContextUpdated(ViewContext, CurrentLayer, TrackedPawn);
	UpdateMapOverlays(CurrentLayer, ViewContext);

	const bool bShouldWriteTrace = VisualConfigAsset->bShowDebugMessages || StartupTraceLogCount < 5;
	if (bShouldWriteTrace)
	{
		const UWorld* World = GetWorld();
		const float CurrentTime = World ? World->GetTimeSeconds() : 0.0f;
		if (StartupTraceLogCount < 5 || CurrentTime - LastDebugTraceLogTime >= 1.0f)
		{
			LastDebugTraceLogTime = CurrentTime;
			++StartupTraceLogCount;
			const FVector2D CanvasSize = GetMarkerCanvas()
				                             ? GetMarkerCanvas()->GetCachedGeometry().GetLocalSize()
				                             : FVector2D::ZeroVector;
			const int32 VisibleMarkerCount = NavSubsystem->GetVisibleMarkers(GetNavigationSurface()).Num();
			const FVector ResolvedPawnLocation = TrackedPawn
				                                      ? OBNavigation::ResolveActorNavigationLocation(TrackedPawn)
				                                      : FVector::ZeroVector;
			UE_LOG(LogOBNavigation, Log,
			       TEXT("[%s::%hs] - Trace #%d Surface=%s Layer='%s' Texture='%s' Pawn='%s' ActorLoc=%s ResolvedLoc=%s ResolveSource=[%s] ViewCenterUV=%s Zoom=%.2f StaticRot=%.2f DynamicYaw=%.2f Rotate=%s Canvas=%s PlayerMarkerID=%s VisibleMarkers=%d LocationCandidates=[%s] ComponentSnapshot=[%s] Debug=%s"),
			       *GetName(), __FUNCTION__, StartupTraceLogCount,
			       *StaticEnum<EOBNavigationSurface>()->GetNameStringByValue(
				       static_cast<int64>(GetNavigationSurface())),
			       *CurrentLayer.LayerName.ToString(), *GetNameSafe(CurrentLayer.MapTexture), *GetNameSafe(TrackedPawn),
			       TrackedPawn ? *TrackedPawn->GetActorLocation().ToString() : TEXT("None"),
			       *ResolvedPawnLocation.ToString(), *OBNavigation::DescribeActorNavigationLocationSource(TrackedPawn),
			       *ViewCenterUV.ToString(),
			       CurrentZoom, ViewContext.TotalStaticRotation, ViewContext.DynamicMapYaw,
			       ViewContext.bShouldRotateMap ? TEXT("true") : TEXT("false"), *CanvasSize.ToString(),
			       *PlayerMarkerID.ToString(), VisibleMarkerCount,
			       *OBNavigation::DescribeActorNavigationLocationCandidates(TrackedPawn),
			       *OBNavigation::DescribeActorNavigationComponentSnapshot(TrackedPawn),
			       VisualConfigAsset->bShowDebugMessages ? TEXT("true") : TEXT("false"));
		}
	}

	TSet<FGuid> HandledMarkerIDs;
	if (GetMarkerCanvas())
	{
		UpdateMapMarkers(TrackedPawn, CurrentLayer, ViewContext, HandledMarkerIDs);
	}

	TArray<FGuid> MarkersToRemove;
	for (const auto& Pair : ActiveMapMarkerWidgets)
	{
		if (!HandledMarkerIDs.Contains(Pair.Key))
		{
			MarkersToRemove.Add(Pair.Key);
		}
	}

	for (const FGuid& MarkerID : MarkersToRemove)
	{
		if (UOBMapMarkerWidget* WidgetToRemove = ActiveMapMarkerWidgets.FindRef(MarkerID))
		{
			WidgetToRemove->RemoveFromParent();
		}
		ActiveMapMarkerWidgets.Remove(MarkerID);
	}

	if (GEngine && VisualConfigAsset->bShowDebugMessages && TrackedPawn)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, TEXT("--- NAV MAP DEBUG ---"));
		GEngine->AddOnScreenDebugMessage(
			-1, 0.0f, FColor::White,
			FString::Printf(TEXT("Navigation World Yaw: %.2f"),
			                OBNavigation::ResolveActorNavigationRotation(TrackedPawn).Yaw));
		GEngine->AddOnScreenDebugMessage(
			-1, 0.0f, FColor::Yellow,
			FString::Printf(TEXT("Map View Center UV: %s"), *ViewContext.ViewCenterUV.ToString()));
		GEngine->AddOnScreenDebugMessage(
			-1, 0.0f, FColor::Yellow,
			FString::Printf(TEXT("Map Static Rotation: %.2f deg"), ViewContext.TotalStaticRotation));
	}
}

void UOBMapWidgetBase::NativeDestruct()
{
	if (NavSubsystem)
	{
		NavSubsystem->OnNavigationMapLayerSpecChanged.RemoveDynamic(
			this, &UOBMapWidgetBase::OnNavigationMapLayerSpecChanged);
	}

	ClearMarkerWidgets();
	if (OverlayWidget)
	{
		OverlayWidget->RemoveFromParent();
		OverlayWidget = nullptr;
	}

	Super::NativeDestruct();
}

void UOBMapWidgetBase::OnNavigationMapLayerSpecChanged(const FOBNavigationMapLayerSpec NewLayerSpec)
{
	FOBNavigationMapLayerSpec ActiveLayerSpec;
	ApplyMapLayer(ResolveActiveLayer(ActiveLayerSpec) ? ActiveLayerSpec : NewLayerSpec);
}

bool UOBMapWidgetBase::ResolveActiveLayer(FOBNavigationMapLayerSpec& OutLayerSpec) const
{
	return NavSubsystem && NavSubsystem->GetCurrentMapLayerSpec(OutLayerSpec);
}

float UOBMapWidgetBase::GetInitialZoom() const
{
	return VisualConfigAsset ? VisualConfigAsset->Zoom : 1.0f;
}

float UOBMapWidgetBase::GetMinimumZoom() const
{
	return VisualConfigAsset ? VisualConfigAsset->MinZoom : 0.1f;
}

float UOBMapWidgetBase::GetMaximumZoom() const
{
	return VisualConfigAsset ? VisualConfigAsset->MaxZoom : 100.0f;
}

bool UOBMapWidgetBase::ShouldUpdateMapThisFrame() const
{
	const ESlateVisibility CurrentVisibility = GetVisibility();
	return CurrentVisibility != ESlateVisibility::Collapsed && CurrentVisibility != ESlateVisibility::Hidden;
}

bool UOBMapWidgetBase::ShouldRotateMap() const
{
	return false;
}

bool UOBMapWidgetBase::ShouldCenterPlayerMarker() const
{
	return false;
}

bool UOBMapWidgetBase::ShouldShowPlayerMarker() const
{
	return true;
}

bool UOBMapWidgetBase::ShouldShowMarker(const UOBMapMarker* Marker) const
{
	return Marker != nullptr;
}

float UOBMapWidgetBase::GetMarkerScale() const
{
	return 1.0f;
}

FName UOBMapWidgetBase::GetOverlayCategoryFilter() const
{
	return NAME_None;
}

FName UOBMapWidgetBase::GetOverlayTagFilter() const
{
	return NAME_None;
}

bool UOBMapWidgetBase::ResolveViewCenterUV(const FOBNavigationMapLayerSpec& CurrentLayer, const APawn* TrackedPawn,
                                           FVector2D& OutViewCenterUV) const
{
	if (!NavSubsystem || !TrackedPawn)
	{
		UE_LOG(LogOBNavigation, Warning, TEXT("[%s::%hs] - ResolveViewCenterUV early exit: missing NavSubsystem or TrackedPawn."), *GetName(), __FUNCTION__);
		return false;
	}

	EOBMapProjectionResult ProjectionResult = EOBMapProjectionResult::NoLayer;
	const FVector PawnLocation = OBNavigation::ResolveActorNavigationLocation(TrackedPawn);
	bool bSuccess = NavSubsystem->WorldToMapUVChecked(CurrentLayer, PawnLocation, OutViewCenterUV, ProjectionResult);
	if (VisualConfigAsset && VisualConfigAsset->bShowDebugMessages)
	{
		// UE_LOG(LogOBNavigation, Log, TEXT("[%s::%hs] - WorldToMapUVChecked result=%s Success=%s UV=%s"), *GetName(), __FUNCTION__, *StaticEnum<EOBMapProjectionResult>()->GetNameStringByValue((int64)ProjectionResult), bSuccess ? TEXT("true") : TEXT("false"), *OutViewCenterUV.ToString());
	}
	return bSuccess;
}

FOBNavigationMapViewContext UOBMapWidgetBase::BuildViewContext(const FOBNavigationMapLayerSpec& CurrentLayer,
                                                               const APawn* TrackedPawn,
                                                               const FVector2D& ViewCenterUV) const
{
	FOBNavigationMapViewContext ViewContext;
	ViewContext.ViewCenterUV = ViewCenterUV;
	ViewContext.Zoom = CurrentZoom;
	ViewContext.TotalStaticRotation = GetTotalStaticRotation();
	ViewContext.DynamicMapYaw = GetDynamicMapYaw(TrackedPawn);
	ViewContext.bShouldRotateMap = ShouldRotateMap();
	ViewContext.bClampToCanvas = true;
	return ViewContext;
}

void UOBMapWidgetBase::OnViewContextUpdated(const FOBNavigationMapViewContext& ViewContext,
                                            const FOBNavigationMapLayerSpec& CurrentLayer,
                                            const APawn* TrackedPawn)
{
}

float UOBMapWidgetBase::GetAlignmentAngle() const
{
	if (!VisualConfigAsset)
	{
		return 0.0f;
	}

	switch (VisualConfigAsset->MapAlignment)
	{
	case EMapAlignment::Forward_PlusX:
		return 0.0f;
	case EMapAlignment::Right_PlusY:
		return 90.0f;
	case EMapAlignment::Backward_MinusX:
		return 180.0f;
	case EMapAlignment::Left_MinusY:
		return -90.0f;
	default:
		return 0.0f;
	}
}

float UOBMapWidgetBase::GetDynamicMapYaw(const APawn* TrackedPawn) const
{
	if (!TrackedPawn)
	{
		return 0.0f;
	}

	if (VisualConfigAsset && VisualConfigAsset->RotationSource == EMinimapRotationSource::ControlRotation)
	{
		return TrackedPawn->GetControlRotation().Yaw;
	}
	return OBNavigation::ResolveActorNavigationRotation(TrackedPawn).Yaw;
}

float UOBMapWidgetBase::GetTotalStaticRotation() const
{
	const float Alignment = GetAlignmentAngle();
	const float Offset = (VisualConfigAsset) ? VisualConfigAsset->MapRotationOffset : 0.0f;
	return Alignment + Offset;
}

void UOBMapWidgetBase::UpdateMapMaterial(const FOBNavigationMapViewContext& ViewContext)
{
	if (!MapMaterialInstance)
	{
		return;
	}

	// View‑center UV – drives the map scrolling.
	const FLinearColor ViewCenterUVColor(ViewContext.ViewCenterUV.X, ViewContext.ViewCenterUV.Y, 0.0f, 0.0f);
	MapMaterialInstance->SetVectorParameterValue(TEXT("ViewCenterUV"), ViewCenterUVColor);

	// Dynamic player yaw (radians).
	MapMaterialInstance->SetScalarParameterValue(TEXT("PlayerYawRad"), FMath::DegreesToRadians(ViewContext.DynamicMapYaw));

	// Zoom multiplier.
	MapMaterialInstance->SetScalarParameterValue(TEXT("ZoomAmount"), CurrentZoom);

	// Static rotation offset (alignment + custom offset) in radians.
	MapMaterialInstance->SetScalarParameterValue(TEXT("MapRotationOffsetRad"), FMath::DegreesToRadians(ViewContext.TotalStaticRotation));
}

void UOBMapWidgetBase::UpdateMapMarkers(const APawn* TrackedPawn, const FOBNavigationMapLayerSpec& CurrentLayer,
                                        const FOBNavigationMapViewContext& ViewContext,
                                        TSet<FGuid>& OutHandledMarkerIDs)
{
	UCanvasPanel* MarkerCanvas = GetMarkerCanvas();
	if (!MarkerWidgetClass || !MarkerCanvas || !NavSubsystem || !VisualConfigAsset)
	{
		if (VisualConfigAsset && VisualConfigAsset->bShowDebugMessages)
		{
			UE_LOG(LogOBNavigation, Warning,
			       TEXT("[%s::%hs] - Marker update skipped. MarkerWidgetClass='%s' MarkerCanvas='%s' NavSubsystem='%s' VisualConfig='%s'"),
			       *GetName(), __FUNCTION__, *GetNameSafe(MarkerWidgetClass), *GetNameSafe(MarkerCanvas),
			       *GetNameSafe(NavSubsystem), *GetNameSafe(VisualConfigAsset));
		}
		return;
	}

	const FVector2D CanvasSize = MarkerCanvas->GetCachedGeometry().GetLocalSize();
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		if (VisualConfigAsset->bShowDebugMessages)
		{
			UE_LOG(LogOBNavigation, Warning,
			       TEXT("[%s::%hs] - Marker update skipped because marker canvas has no size. CanvasSize=%s"),
			       *GetName(), __FUNCTION__, *CanvasSize.ToString());
		}
		return;
	}

	const FVector2D CanvasCenter = CanvasSize * 0.5f;
	const TArray<UOBMapMarker*> VisibleMarkers = NavSubsystem->GetVisibleMarkers(GetNavigationSurface());
	if (VisualConfigAsset->bShowDebugMessages && PlayerMarkerID.IsValid())
	{
		bool bFoundPlayerMarker = false;
		for (const UOBMapMarker* Marker : VisibleMarkers)
		{
			if (Marker && Marker->MarkerID == PlayerMarkerID)
			{
				bFoundPlayerMarker = true;
				break;
			}
		}

		if (!bFoundPlayerMarker)
		{
			const UWorld* World = GetWorld();
			const float CurrentTime = World ? World->GetTimeSeconds() : 0.0f;
			if (CurrentTime - LastMarkerWarningLogTime >= 1.0f)
			{
				LastMarkerWarningLogTime = CurrentTime;
				UE_LOG(LogOBNavigation, Warning,
				       TEXT("[%s::%hs] - Player marker id is valid but not visible on this surface. Surface=%s PlayerMarkerID=%s VisibleMarkers=%d"),
				       *GetName(), __FUNCTION__, *StaticEnum<EOBNavigationSurface>()->GetNameStringByValue(
					       static_cast<int64>(GetNavigationSurface())),
				       *PlayerMarkerID.ToString(), VisibleMarkers.Num());
			}
		}
	}
	for (UOBMapMarker* Marker : VisibleMarkers)
	{
		if (!Marker || !Marker->ConfigAsset || !ShouldShowMarker(Marker))
		{
			continue;
		}

		const bool bIsPlayerMarker = PlayerMarkerID.IsValid() && Marker->MarkerID == PlayerMarkerID;
		if (bIsPlayerMarker && !ShouldShowPlayerMarker())
		{
			continue;
		}

		FVector2D FinalPosition = CanvasCenter;
		float IndicatorAngle = 0.0f;
		bool bIsClampedToEdge = false;

		if (bIsPlayerMarker && ShouldCenterPlayerMarker() && TrackedPawn)
		{
			float TargetYaw = OBNavigation::ResolveActorNavigationRotation(TrackedPawn).Yaw;
			if (VisualConfigAsset && VisualConfigAsset->RotationSource == EMinimapRotationSource::ControlRotation)
			{
				TargetYaw = TrackedPawn->GetControlRotation().Yaw;
			}
			IndicatorAngle = ShouldRotateMap() ? 0.0f : TargetYaw - ViewContext.TotalStaticRotation;
		}
		else
		{
			FVector2D MarkerUV;
			EOBMapProjectionResult MarkerProjectionResult = EOBMapProjectionResult::NoLayer;
			if (!NavSubsystem->WorldToMapUVChecked(CurrentLayer, Marker->WorldLocation, MarkerUV, MarkerProjectionResult))
			{
				continue;
			}

			FOBNavigationCanvasProjection CanvasProjection;
			if (!OBNavigation::MapView::ProjectUVToCanvas(MarkerUV, CanvasSize, ViewContext, CanvasProjection))
			{
				continue;
			}

			FinalPosition = CanvasProjection.CanvasPosition;
			bIsClampedToEdge = CanvasProjection.bIsClampedToEdge;
			if (bIsClampedToEdge)
			{
				IndicatorAngle = FMath::RadiansToDegrees(
					FMath::Atan2(CanvasProjection.RotatedPixelOffset.Y, CanvasProjection.RotatedPixelOffset.X));
			}
			else
			{
				IndicatorAngle = ViewContext.bShouldRotateMap
					                 ? Marker->WorldRotation.Yaw - ViewContext.DynamicMapYaw
					                 : Marker->WorldRotation.Yaw - ViewContext.TotalStaticRotation;
			}
		}

		OutHandledMarkerIDs.Add(Marker->MarkerID);

		UOBMapMarkerWidget* MarkerWidget = ActiveMapMarkerWidgets.FindRef(Marker->MarkerID);
		if (!MarkerWidget)
		{
			MarkerWidget = CreateWidget<UOBMapMarkerWidget>(this, MarkerWidgetClass);
			if (!MarkerWidget)
			{
				continue;
			}

			if (UCanvasPanelSlot* NewSlot = Cast<UCanvasPanelSlot>(MarkerCanvas->AddChild(MarkerWidget)))
			{
				NewSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			}

			ActiveMapMarkerWidgets.Add(Marker->MarkerID, MarkerWidget);
			MarkerWidget->InitializeMarker(Marker->ConfigAsset->IdentifierIconTexture,
			                               Marker->ConfigAsset->IndicatorMaterial);
		}

			MarkerWidget->UpdateVisuals(IndicatorAngle, 90.0f, 1.0f);
			const float DistanceMeters = TrackedPawn
				                             ? FVector::Dist(OBNavigation::ResolveActorNavigationLocation(TrackedPawn),
				                                             Marker->WorldLocation) / 100.0f
				                             : 0.0f;
		MarkerWidget->UpdateDistance(DistanceMeters, bIsClampedToEdge);

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget->Slot))
		{
			const FVector2D MarkerSize = Marker->ConfigAsset->Size * GetMarkerScale();
			FVector2D SlotPosition = FinalPosition;

			// Offset non-centered markers so custom indicator pivots stay anchored after rotation.
			if (!(bIsPlayerMarker && ShouldCenterPlayerMarker()))
			{
				const FVector2D PivotOffset = (Marker->ConfigAsset->IndicatorPivot - FVector2D(0.5f, 0.5f)) * MarkerSize;
				const FVector2D RotatedPivotOffset = PivotOffset.GetRotated(IndicatorAngle);
				SlotPosition = FinalPosition - (RotatedPivotOffset - PivotOffset);
			}

			CanvasSlot->SetSize(MarkerSize);
			CanvasSlot->SetPosition(SlotPosition);
			CanvasSlot->SetZOrder(bIsPlayerMarker ? 10 : 1);
		}

		if (GEngine && VisualConfigAsset->bShowDebugMessages)
		{
			const FColor DebugColor = bIsPlayerMarker ? FColor::Magenta : FColor::Green;
			GEngine->AddOnScreenDebugMessage(
				-1, 0.0f, DebugColor,
				FString::Printf(TEXT("Marker [%s]: Final Pos: %s"),
				                *Marker->MarkerID.ToString().Left(8), *FinalPosition.ToString()));
		}
	}
}

void UOBMapWidgetBase::UpdateMapOverlays(const FOBNavigationMapLayerSpec& CurrentLayer,
                                         const FOBNavigationMapViewContext& ViewContext)
{
	if (!GetMarkerCanvas())
	{
		return;
	}

	EnsureOverlayWidget();
	if (!OverlayWidget)
	{
		return;
	}

	TArray<FOBNavigationOverlayElement> VisibleElements;
	const FName CategoryFilter = GetOverlayCategoryFilter();
	const FName TagFilter = GetOverlayTagFilter();
	for (const FOBNavigationOverlayLayer& Layer : CurrentLayer.OverlayLayers)
	{
		if (!Layer.bVisibleByDefault)
		{
			continue;
		}

		for (const FOBNavigationOverlayElement& Element : Layer.Elements)
		{
			if (!Element.bVisibleByDefault)
			{
				continue;
			}
			if (!CategoryFilter.IsNone() && Element.Category != CategoryFilter)
			{
				continue;
			}
			if (!TagFilter.IsNone() && !Element.FilterTags.Contains(TagFilter))
			{
				continue;
			}

			VisibleElements.Add(Element);
		}
	}

	OverlayWidget->SetOverlayContext(CurrentLayer, VisibleElements, ViewContext);
}

void UOBMapWidgetBase::EnsureOverlayWidget()
{
	UCanvasPanel* MarkerCanvas = GetMarkerCanvas();
	if (OverlayWidget || !MarkerCanvas)
	{
		return;
	}

	OverlayWidget = CreateWidget<UOBMapOverlayWidget>(this, UOBMapOverlayWidget::StaticClass());
	if (!OverlayWidget)
	{
		return;
	}

	OverlayWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* OverlaySlot = Cast<UCanvasPanelSlot>(MarkerCanvas->AddChild(OverlayWidget)))
	{
		OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		OverlaySlot->SetOffsets(FMargin(0.0f));
		OverlaySlot->SetAlignment(FVector2D::ZeroVector);
		OverlaySlot->SetZOrder(0);
	}
}

void UOBMapWidgetBase::ClearMarkerWidgets()
{
	for (const auto& Pair : ActiveMapMarkerWidgets)
	{
		if (Pair.Value)
		{
			Pair.Value->RemoveFromParent();
		}
	}
	ActiveMapMarkerWidgets.Reset();
}

void UOBMapWidgetBase::ApplyMapLayer(const FOBNavigationMapLayerSpec& NewLayerSpec)
{
	if (!MapMaterialInstance || !MapImage)
	{
		return;
	}

	if (AppliedLayerName == NewLayerSpec.LayerName && AppliedMapTexture == NewLayerSpec.MapTexture)
	{
		return;
	}

	AppliedLayerName = NewLayerSpec.LayerName;
	AppliedMapTexture = NewLayerSpec.MapTexture;

	if (NewLayerSpec.MapTexture)
	{
		MapMaterialInstance->SetTextureParameterValue("MapTexture", NewLayerSpec.MapTexture);
		MapImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (VisualConfigAsset && VisualConfigAsset->bShowDebugMessages)
		{
			UE_LOG(LogOBNavigation, Log,
			       TEXT("[%s::%hs] - Applied map layer. Layer='%s' Texture='%s' BoundsMin=%s BoundsMax=%s MapRotation=%.2f"),
			       *GetName(), __FUNCTION__, *NewLayerSpec.LayerName.ToString(), *GetNameSafe(NewLayerSpec.MapTexture),
			       *NewLayerSpec.WorldBounds.Min.ToString(), *NewLayerSpec.WorldBounds.Max.ToString(),
			       NewLayerSpec.MapRotationDegrees);
		}
	}
	else
	{
		MapImage->SetVisibility(ESlateVisibility::Collapsed);
		if (VisualConfigAsset && VisualConfigAsset->bShowDebugMessages)
		{
			UE_LOG(LogOBNavigation, Warning,
			       TEXT("[%s::%hs] - Applied empty map layer. MapImage collapsed. Layer='%s'"),
			       *GetName(), __FUNCTION__, *NewLayerSpec.LayerName.ToString());
		}
	}
}
