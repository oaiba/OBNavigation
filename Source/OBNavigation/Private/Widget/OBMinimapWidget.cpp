#include "Widget/OBMinimapWidget.h"

#include "Components/Image.h"
#include "Data/OBMinimapConfigAsset.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "OBNavigation.h"

void UOBMinimapWidget::InitializeAndStartTracking(UOBMinimapConfigAsset* InConfigAsset)
{
	if (!InConfigAsset)
	{
		UE_LOG(LogOBNavigation, Error, TEXT("[%s::%hs] - Initialization failed: invalid minimap config."), *GetName(),
		       __FUNCTION__);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	CurrentMapRotationOffset = InConfigAsset->MapRotationOffset;
	CurrentMinimapShape = InConfigAsset->MinimapShape;

	InitializeMapWidget(InConfigAsset);

	if (CompassRingImage && InConfigAsset->CompassRingTexture)
	{
		CompassRingImage->SetBrushFromTexture(InConfigAsset->CompassRingTexture);
	}

	SetMapRotationOffset(CurrentMapRotationOffset);
	SetMinimapShape(CurrentMinimapShape);
	SetMinimapZoom(GetMapZoom());

	if (bIsInitializedAndTracking)
	{
		UE_LOG(LogOBNavigation, Log, TEXT("[%s::%hs] - Minimap initialized and tracking started."), *GetName(),
		       __FUNCTION__);
	}
}

void UOBMinimapWidget::SetMapRotationOffset(const float NewOffsetYaw)
{
	CurrentMapRotationOffset = NewOffsetYaw;

	if (MapMaterialInstance)
	{
		const float TotalStaticRotation = GetTotalStaticRotation();
		MapMaterialInstance->SetScalarParameterValue("MapRotationOffsetRad",
		                                             FMath::DegreesToRadians(TotalStaticRotation));
		if (CompassRingImage)
		{
			CompassRingImage->SetRenderTransformAngle(-TotalStaticRotation);
		}
	}
}

void UOBMinimapWidget::SetMinimapShape(const EMinimapShape NewShape)
{
	CurrentMinimapShape = NewShape;
	if (MapMaterialInstance)
	{
		const float ShapeValue = CurrentMinimapShape == EMinimapShape::Circle ? 1.0f : 0.0f;
		MapMaterialInstance->SetScalarParameterValue("ShapeAlpha", ShapeValue);
	}
}

void UOBMinimapWidget::SetMinimapZoom(const float NewZoom)
{
	SetMapZoom(NewZoom);
}

bool UOBMinimapWidget::ShouldRotateMap() const
{
	const UOBMinimapConfigAsset* ConfigAsset = GetVisualConfig();
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

float UOBMinimapWidget::GetDynamicMapYaw(const APawn* TrackedPawn) const
{
	const UOBMinimapConfigAsset* ConfigAsset = GetVisualConfig();
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
		return OBNavigation::ResolveActorNavigationRotation(TrackedPawn).Yaw;
	}
}

float UOBMinimapWidget::GetTotalStaticRotation() const
{
	return CurrentMapRotationOffset + GetAlignmentAngle();
}
