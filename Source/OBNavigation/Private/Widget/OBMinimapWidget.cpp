#include "Widget/OBMinimapWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Data/OBMinimapConfigAsset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "OBNavigation.h"
#include "OBNavigationSubsystem.h"
#include "Widget/OBMapOverlayWidget.h"

void UOBMinimapWidget::InitializeAndStartTracking(UOBMinimapConfigAsset* InConfigAsset)
{
	if (bIsInitializedAndTracking)
	{
		UE_LOG(LogOBNavigation, Warning, TEXT("[%s::%hs] - Widget is already initialized."), *GetName(), __FUNCTION__);
		return;
	}

	if (!InConfigAsset)
	{
		UE_LOG(LogOBNavigation, Error, TEXT("[%s::%hs] - Initialization failed: Invalid ConfigAsset provided."), *GetName(),
		       __FUNCTION__);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	ConfigAsset = InConfigAsset;
	CurrentMapRotationOffset = ConfigAsset->MapRotationOffset;
	CurrentMinimapShape = ConfigAsset->MinimapShape;
	CurrentZoom = GetInitialZoom();

	if (MapImage && ConfigAsset->MinimapBackgroundMaterial)
	{
		MinimapMaterialInstance = UMaterialInstanceDynamic::Create(ConfigAsset->MinimapBackgroundMaterial, this);
		MapImage->SetBrushFromMaterial(MinimapMaterialInstance);
	}
	else
	{
		UE_LOG(LogOBNavigation, Error, TEXT("[%s::%hs] - Failed to set up MapImage material."), *GetName(), __FUNCTION__);
	}

	if (CompassRingImage && ConfigAsset->CompassRingTexture)
	{
		CompassRingImage->SetBrushFromTexture(ConfigAsset->CompassRingTexture);
	}

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		NavSubsystem = GameInstance->GetSubsystem<UOBNavigationSubsystem>();
		if (NavSubsystem)
		{
			NavSubsystem->OnNavigationMapLayerSpecChanged.AddDynamic(
				this, &UOBMinimapWidget::OnNavigationMapLayerSpecChanged);

			FOBNavigationMapLayerSpec CurrentLayerSpec;
			OnNavigationMapLayerSpecChanged(NavSubsystem->GetCurrentMapLayerSpec(CurrentLayerSpec)
				                                ? CurrentLayerSpec
				                                : FOBNavigationMapLayerSpec());

			if (APawn* TrackedPawn = NavSubsystem->GetTrackedPlayerPawn())
			{
				PlayerMarkerID = NavSubsystem->GetMarkerIDForActor(TrackedPawn);
			}
		}
	}

	SetMapRotationOffset(CurrentMapRotationOffset);
	SetMinimapShape(CurrentMinimapShape);
	SetMinimapZoom(CurrentZoom);
	EnsureOverlayWidget();

	if (!MinimapMaterialInstance || !NavSubsystem)
	{
		UE_LOG(LogOBNavigation, Error, TEXT("[%s::%hs] - Initialization failed due to missing subsystem or material instance."),
		       *GetName(), __FUNCTION__);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	bIsInitializedAndTracking = true;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UE_LOG(LogOBNavigation, Log, TEXT("[%s::%hs] - Minimap initialized and tracking started."), *GetName(), __FUNCTION__);
}

void UOBMinimapWidget::SetMapRotationOffset(const float NewOffsetYaw)
{
	CurrentMapRotationOffset = NewOffsetYaw;
	if (!MinimapMaterialInstance)
	{
		return;
	}

	const float TotalStaticRotation = CurrentMapRotationOffset + GetAlignmentAngle();
	MinimapMaterialInstance->SetScalarParameterValue("MapRotationOffsetRad",
	                                                 FMath::DegreesToRadians(TotalStaticRotation));
	if (CompassRingImage)
	{
		CompassRingImage->SetRenderTransformAngle(-TotalStaticRotation);
	}
}

void UOBMinimapWidget::SetMinimapShape(const EMinimapShape NewShape)
{
	CurrentMinimapShape = NewShape;
	if (MinimapMaterialInstance)
	{
		const float ShapeValue = CurrentMinimapShape == EMinimapShape::Circle ? 1.0f : 0.0f;
		MinimapMaterialInstance->SetScalarParameterValue("ShapeAlpha", ShapeValue);
	}
}

void UOBMinimapWidget::SetMinimapZoom(const float NewZoom)
{
	const float MinimumZoom = FMath::Min(GetMinimumZoom(), GetMaximumZoom());
	const float MaximumZoom = FMath::Max(GetMinimumZoom(), GetMaximumZoom());
	CurrentZoom = FMath::Clamp(NewZoom, MinimumZoom, MaximumZoom);
	if (MinimapMaterialInstance)
	{
		MinimapMaterialInstance->SetScalarParameterValue("Zoom", CurrentZoom);
	}
}

void UOBMinimapWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsInitializedAndTracking || !ConfigAsset || !NavSubsystem || !ShouldUpdateMapThisFrame())
	{
		return;
	}

	FOBNavigationMapLayerSpec CurrentLayer;
	if (!NavSubsystem->GetCurrentMapLayerSpec(CurrentLayer))
	{
		ClearMarkerWidgets();
		if (OverlayWidget)
		{
			OverlayWidget->ClearOverlayContext();
		}
		return;
	}

	const APawn* TrackedPawn = NavSubsystem->GetTrackedPlayerPawn();
	if (TrackedPawn && !PlayerMarkerID.IsValid())
	{
		PlayerMarkerID = NavSubsystem->GetMarkerIDForActor(const_cast<APawn*>(TrackedPawn));
	}

	FVector2D ViewCenterUV;
	if (!ResolveViewCenterUV(CurrentLayer, TrackedPawn, ViewCenterUV))
	{
		ClearMarkerWidgets();
		if (OverlayWidget)
		{
			OverlayWidget->ClearOverlayContext();
		}
		return;
	}

	FOBNavigationMapViewContext ViewContext;
	ViewContext.ViewCenterUV = ViewCenterUV;
	ViewContext.Zoom = CurrentZoom;
	ViewContext.TotalStaticRotation = CurrentMapRotationOffset + GetAlignmentAngle();
	ViewContext.DynamicMapYaw = GetDynamicMapYaw(TrackedPawn);
	ViewContext.bShouldRotateMap = ShouldRotateMap();
	ViewContext.bClampToCanvas = true;

	UpdateMapMaterial(ViewContext);
	OnViewContextUpdated(ViewContext, CurrentLayer, TrackedPawn);
	UpdateMapOverlays(CurrentLayer, ViewContext);

	TSet<FGuid> HandledMarkerIDs;
	if (MinimapMarkerCanvas)
	{
		UpdateMapMarkers(TrackedPawn, CurrentLayer, ViewContext, HandledMarkerIDs);
	}

	TArray<FGuid> MarkersToRemove;
	for (const auto& Pair : ActiveMinimapMarkerWidgets)
	{
		if (!HandledMarkerIDs.Contains(Pair.Key))
		{
			MarkersToRemove.Add(Pair.Key);
		}
	}

	for (const FGuid& MarkerID : MarkersToRemove)
	{
		if (UOBMapMarkerWidget* WidgetToRemove = ActiveMinimapMarkerWidgets.FindRef(MarkerID))
		{
			WidgetToRemove->RemoveFromParent();
		}
		ActiveMinimapMarkerWidgets.Remove(MarkerID);
	}

	if (GEngine && ConfigAsset->bShowDebugMessages && TrackedPawn)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, TEXT("--- MINIMAP DEBUG ---"));
		GEngine->AddOnScreenDebugMessage(
			-1, 0.0f, FColor::White,
			FString::Printf(TEXT("Character World Yaw: %.2f"), TrackedPawn->GetActorRotation().Yaw));
		GEngine->AddOnScreenDebugMessage(
			-1, 0.0f, FColor::Yellow,
			FString::Printf(TEXT("Map View Center UV: %s"), *ViewContext.ViewCenterUV.ToString()));
		GEngine->AddOnScreenDebugMessage(
			-1, 0.0f, FColor::Yellow,
			FString::Printf(TEXT("Map Static Rotation: %.2f deg"), ViewContext.TotalStaticRotation));
	}
}

void UOBMinimapWidget::NativeDestruct()
{
	if (NavSubsystem)
	{
		NavSubsystem->OnNavigationMapLayerSpecChanged.RemoveDynamic(
			this, &UOBMinimapWidget::OnNavigationMapLayerSpecChanged);
	}

	ClearMarkerWidgets();
	if (OverlayWidget)
	{
		OverlayWidget->RemoveFromParent();
		OverlayWidget = nullptr;
	}

	Super::NativeDestruct();
}

float UOBMinimapWidget::GetInitialZoom() const
{
	return ConfigAsset ? ConfigAsset->Zoom : 1.0f;
}

float UOBMinimapWidget::GetMinimumZoom() const
{
	return ConfigAsset ? ConfigAsset->MinZoom : 0.1f;
}

float UOBMinimapWidget::GetMaximumZoom() const
{
	return ConfigAsset ? ConfigAsset->MaxZoom : 100.0f;
}

bool UOBMinimapWidget::ShouldUpdateMapThisFrame() const
{
	const ESlateVisibility CurrentVisibility = GetVisibility();
	return CurrentVisibility != ESlateVisibility::Collapsed && CurrentVisibility != ESlateVisibility::Hidden;
}

bool UOBMinimapWidget::ShouldRotateMap() const
{
	return ConfigAsset && ConfigAsset->bShouldRotateMap;
}

bool UOBMinimapWidget::ShouldCenterPlayerMarker() const
{
	return true;
}

bool UOBMinimapWidget::ShouldShowPlayerMarker() const
{
	return true;
}

float UOBMinimapWidget::GetMarkerScale() const
{
	return 1.0f;
}

bool UOBMinimapWidget::ResolveViewCenterUV(const FOBNavigationMapLayerSpec& CurrentLayer, const APawn* TrackedPawn,
                                           FVector2D& OutViewCenterUV) const
{
	if (!NavSubsystem || !TrackedPawn)
	{
		return false;
	}

	EOBMapProjectionResult ProjectionResult = EOBMapProjectionResult::NoLayer;
	return NavSubsystem->WorldToMapUVChecked(CurrentLayer, TrackedPawn->GetActorLocation(), OutViewCenterUV,
	                                         ProjectionResult);
}

void UOBMinimapWidget::OnViewContextUpdated(const FOBNavigationMapViewContext& ViewContext,
                                            const FOBNavigationMapLayerSpec& CurrentLayer,
                                            const APawn* TrackedPawn)
{
}

void UOBMinimapWidget::UpdateMapMaterial(const FOBNavigationMapViewContext& ViewContext)
{
	if (!MinimapMaterialInstance)
	{
		return;
	}

	const FLinearColor ViewCenterColor(ViewContext.ViewCenterUV.X, ViewContext.ViewCenterUV.Y, 0.0f, 0.0f);
	MinimapMaterialInstance->SetVectorParameterValue("PlayerPositionUV", ViewCenterColor);
	MinimapMaterialInstance->SetVectorParameterValue("ViewCenterUV", ViewCenterColor);
	MinimapMaterialInstance->SetScalarParameterValue("PlayerYaw", FMath::DegreesToRadians(ViewContext.DynamicMapYaw));
	MinimapMaterialInstance->SetScalarParameterValue("Zoom", CurrentZoom);
	MinimapMaterialInstance->SetScalarParameterValue(
		"MapRotationOffsetRad", FMath::DegreesToRadians(ViewContext.TotalStaticRotation));
}

void UOBMinimapWidget::UpdateMapMarkers(const APawn* TrackedPawn, const FOBNavigationMapLayerSpec& CurrentLayer,
                                        const FOBNavigationMapViewContext& ViewContext,
                                        TSet<FGuid>& OutHandledMarkerIDs)
{
	if (!MarkerWidgetClass || !MinimapMarkerCanvas || !NavSubsystem || !ConfigAsset)
	{
		return;
	}

	const FVector2D CanvasSize = MinimapMarkerCanvas->GetCachedGeometry().GetLocalSize();
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return;
	}

	const FVector2D CanvasCenter = CanvasSize * 0.5f;
	const TArray<UOBMapMarker*> VisibleMarkers = NavSubsystem->GetVisibleMarkers(GetNavigationSurface());
	for (UOBMapMarker* Marker : VisibleMarkers)
	{
		if (!Marker || !Marker->ConfigAsset)
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
			IndicatorAngle = ShouldRotateMap() ? 0.0f : TrackedPawn->GetActorRotation().Yaw - ViewContext.TotalStaticRotation;
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

		UOBMapMarkerWidget* MarkerWidget = ActiveMinimapMarkerWidgets.FindRef(Marker->MarkerID);
		if (!MarkerWidget)
		{
			MarkerWidget = CreateWidget<UOBMapMarkerWidget>(this, MarkerWidgetClass);
			if (!MarkerWidget)
			{
				continue;
			}

			if (UCanvasPanelSlot* NewSlot = Cast<UCanvasPanelSlot>(MinimapMarkerCanvas->AddChild(MarkerWidget)))
			{
				NewSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			}

			ActiveMinimapMarkerWidgets.Add(Marker->MarkerID, MarkerWidget);
			MarkerWidget->InitializeMarker(Marker->ConfigAsset->IdentifierIconTexture,
			                               Marker->ConfigAsset->IndicatorMaterial);
		}

		MarkerWidget->UpdateVisuals(IndicatorAngle, 90.0f, 1.0f);
		const float DistanceMeters = TrackedPawn
			                             ? FVector::Dist(TrackedPawn->GetActorLocation(), Marker->WorldLocation) / 100.0f
			                             : 0.0f;
		MarkerWidget->UpdateDistance(DistanceMeters, bIsClampedToEdge);

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget->Slot))
		{
			const FVector2D MarkerSize = Marker->ConfigAsset->Size * GetMarkerScale();
			FVector2D SlotPosition = FinalPosition;

			// Offset non-centered markers so custom indicator pivots stay visually anchored after rotation.
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

		if (GEngine && ConfigAsset->bShowDebugMessages)
		{
			const FColor DebugColor = bIsPlayerMarker ? FColor::Magenta : FColor::Green;
			GEngine->AddOnScreenDebugMessage(
				-1, 0.0f, DebugColor,
				FString::Printf(TEXT("Marker [%s]: Final Pos: %s"),
				                *Marker->MarkerID.ToString().Left(8), *FinalPosition.ToString()));
		}
	}
}

void UOBMinimapWidget::OnNavigationMapLayerSpecChanged(FOBNavigationMapLayerSpec NewLayerSpec)
{
	if (!MinimapMaterialInstance || !MapImage)
	{
		return;
	}

	if (NewLayerSpec.MapTexture)
	{
		MinimapMaterialInstance->SetTextureParameterValue("MapTexture", NewLayerSpec.MapTexture);
		MapImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		MapImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}

float UOBMinimapWidget::GetDynamicMapYaw(const APawn* TrackedPawn) const
{
	if (!ConfigAsset || !TrackedPawn || !ShouldRotateMap())
	{
		return 0.0f;
	}

	switch (ConfigAsset->RotationSource)
	{
	case EMinimapRotationSource::ControlRotation:
		return TrackedPawn->GetControlRotation().Yaw;
	case EMinimapRotationSource::ActorRotation:
	default:
		return TrackedPawn->GetActorRotation().Yaw;
	}
}

void UOBMinimapWidget::UpdateMapOverlays(const FOBNavigationMapLayerSpec& CurrentLayer,
                                         const FOBNavigationMapViewContext& ViewContext)
{
	if (!NavSubsystem || !MinimapMarkerCanvas)
	{
		return;
	}

	EnsureOverlayWidget();
	if (!OverlayWidget)
	{
		return;
	}

	OverlayWidget->SetOverlayContext(
		CurrentLayer,
		NavSubsystem->GetVisibleOverlayElements(GetNavigationSurface()),
		ViewContext);
}

void UOBMinimapWidget::EnsureOverlayWidget()
{
	if (OverlayWidget || !MinimapMarkerCanvas)
	{
		return;
	}

	OverlayWidget = CreateWidget<UOBMapOverlayWidget>(this, UOBMapOverlayWidget::StaticClass());
	if (!OverlayWidget)
	{
		return;
	}

	OverlayWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* OverlaySlot = Cast<UCanvasPanelSlot>(MinimapMarkerCanvas->AddChild(OverlayWidget)))
	{
		OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		OverlaySlot->SetOffsets(FMargin(0.0f));
		OverlaySlot->SetAlignment(FVector2D::ZeroVector);
		OverlaySlot->SetZOrder(0);
	}
}

void UOBMinimapWidget::ClearMarkerWidgets()
{
	for (const auto& Pair : ActiveMinimapMarkerWidgets)
	{
		if (Pair.Value)
		{
			Pair.Value->RemoveFromParent();
		}
	}
	ActiveMinimapMarkerWidgets.Reset();
}

float UOBMinimapWidget::GetAlignmentAngle() const
{
	if (!ConfigAsset)
	{
		return 0.0f;
	}

	switch (ConfigAsset->MapAlignment)
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
