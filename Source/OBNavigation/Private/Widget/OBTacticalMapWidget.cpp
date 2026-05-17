#include "Widget/OBTacticalMapWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Data/OBTacticalMapConfigAsset.h"
#include "OBNavigation.h"
#include "OBNavigationSubsystem.h"

void UOBTacticalMapWidget::InitializeTacticalMapAndStartTracking(UOBMinimapConfigAsset* InMinimapConfigAsset,
                                                                 UOBTacticalMapConfigAsset* InTacticalConfigAsset)
{
	if (!InTacticalConfigAsset)
	{
		UE_LOG(LogOBNavigation, Error,
		       TEXT("[%s::%hs] - Initialization failed: Invalid TacticalConfigAsset provided."), *GetName(),
		       __FUNCTION__);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	TacticalConfigAsset = InTacticalConfigAsset;
	ViewCenterUV = FVector2D(0.5f, 0.5f);
	PanInput = FVector2D::ZeroVector;
	bIsFollowingTrackedPlayer = true;

	Super::InitializeAndStartTracking(InMinimapConfigAsset);

	if (CompassRingImage)
	{
		CompassRingImage->SetVisibility(TacticalConfigAsset->bShowCompassRing
			                                ? ESlateVisibility::HitTestInvisible
			                                : ESlateVisibility::Collapsed);
	}

	RecenterOnTrackedPlayer();
}

void UOBTacticalMapWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	ApplyContinuousPanInput(InDeltaTime);
	Super::NativeTick(MyGeometry, InDeltaTime);
}

void UOBTacticalMapWidget::AddZoomInput(const float ZoomDelta)
{
	const float Step = TacticalConfigAsset ? FMath::Max(0.01f, TacticalConfigAsset->ZoomStep) : 0.25f;
	SetMinimapZoom(GetMinimapZoom() + ZoomDelta * Step);
}

void UOBTacticalMapWidget::AddPanInput(const FVector2D PanDelta)
{
	if (PanDelta.IsNearlyZero() || !MinimapMarkerCanvas)
	{
		return;
	}

	const FVector2D CanvasSize = MinimapMarkerCanvas->GetCachedGeometry().GetLocalSize();
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return;
	}

	const APawn* TrackedPawn = NavSubsystem ? NavSubsystem->GetTrackedPlayerPawn() : nullptr;
	const float TotalStaticRotation = CurrentMapRotationOffset + GetAlignmentAngle();
	const float DynamicMapYaw = GetDynamicMapYaw(TrackedPawn);
	const float AppliedRotation = ShouldRotateMap() ? -(TotalStaticRotation + DynamicMapYaw) : -TotalStaticRotation;
	const FVector2D UnrotatedPanDelta = PanDelta.GetRotated(-AppliedRotation);
	const float SafeZoom = FMath::Max(GetMinimapZoom(), KINDA_SMALL_NUMBER);
	const FVector2D UVDelta(
		UnrotatedPanDelta.X / (CanvasSize.X * SafeZoom),
		UnrotatedPanDelta.Y / (CanvasSize.Y * SafeZoom));

	SetViewCenterUVInternal(ViewCenterUV - UVDelta, false);
}

void UOBTacticalMapWidget::SetPanInput(const FVector2D InPanInput)
{
	PanInput = InPanInput;
}

void UOBTacticalMapWidget::SetViewCenterWorldLocation(const FVector WorldLocation)
{
	if (!NavSubsystem)
	{
		return;
	}

	FOBNavigationMapLayerSpec CurrentLayer;
	if (!NavSubsystem->GetCurrentMapLayerSpec(CurrentLayer))
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
	if (!TrackedPawn || !NavSubsystem->GetCurrentMapLayerSpec(CurrentLayer))
	{
		SetViewCenterUVInternal(ViewCenterUV, true);
		return;
	}

	FVector2D PlayerUV;
	EOBMapProjectionResult ProjectionResult = EOBMapProjectionResult::NoLayer;
	if (NavSubsystem->WorldToMapUVChecked(CurrentLayer, TrackedPawn->GetActorLocation(), PlayerUV, ProjectionResult))
	{
		SetViewCenterUVInternal(PlayerUV, true);
	}
	else
	{
		SetViewCenterUVInternal(ViewCenterUV, true);
	}
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
	return TacticalConfigAsset && TacticalConfigAsset->bRotateWithPlayer;
}

bool UOBTacticalMapWidget::ShouldCenterPlayerMarker() const
{
	return false;
}

bool UOBTacticalMapWidget::ShouldShowPlayerMarker() const
{
	return TacticalConfigAsset && TacticalConfigAsset->bShowPlayerMarker;
}

float UOBTacticalMapWidget::GetMarkerScale() const
{
	return TacticalConfigAsset ? FMath::Max(0.01f, TacticalConfigAsset->MarkerScale) : 1.0f;
}

bool UOBTacticalMapWidget::ResolveViewCenterUV(const FOBNavigationMapLayerSpec& CurrentLayer,
                                               const APawn* TrackedPawn, FVector2D& OutViewCenterUV) const
{
	if (bIsFollowingTrackedPlayer && NavSubsystem && TrackedPawn)
	{
		FVector2D PlayerUV;
		EOBMapProjectionResult ProjectionResult = EOBMapProjectionResult::NoLayer;
		if (NavSubsystem->WorldToMapUVChecked(CurrentLayer, TrackedPawn->GetActorLocation(), PlayerUV,
		                                      ProjectionResult))
		{
			OutViewCenterUV = PlayerUV;
			return true;
		}
	}

	OutViewCenterUV = ViewCenterUV;
	return true;
}

void UOBTacticalMapWidget::OnViewContextUpdated(const FOBNavigationMapViewContext& ViewContext,
                                                const FOBNavigationMapLayerSpec& CurrentLayer,
                                                const APawn* TrackedPawn)
{
	if (bIsFollowingTrackedPlayer)
	{
		ViewCenterUV = ViewContext.ViewCenterUV;
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
}
