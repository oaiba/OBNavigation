// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/OBMapMarkerWidget.h"

#include "Components/Image.h"

void UOBMapMarkerWidget::InitializeMarker(UTexture2D* IdentifierTexture, UTexture2D* IndicatorTexture)
{
	// Set the static identifier icon's texture and visibility
	if (IdentifierIcon)
	{
		IdentifierIcon->SetBrushFromTexture(IdentifierTexture);
		IdentifierIcon->SetVisibility(IdentifierTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	
	// Set the directional indicator's texture and visibility
	if (DirectionalIndicator)
	{
		DirectionalIndicator->SetBrushFromTexture(IndicatorTexture);
		DirectionalIndicator->SetVisibility(IndicatorTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UOBMapMarkerWidget::UpdateRotation(const float IndicatorAngle)
{
	// Only update the angle of the directional indicator
	if (DirectionalIndicator)
	{
		DirectionalIndicator->SetRenderTransformAngle(IndicatorAngle);
	}
}

void UOBMapMarkerWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// This function runs in the editor, allowing us to see a preview.
	// We ensure the pivot points are set correctly for rotation.
	if (DirectionalIndicator)
	{
		DirectionalIndicator->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}
}