// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/OBMapMarkerWidget.h"

#include "Components/Image.h"

void UOBMapMarkerWidget::UpdateMarkerVisuals(UTexture2D* IdentifierTexture, UTexture2D* IndicatorTexture, float IndicatorAngle)
{
	// Update the static identifier icon
	if (IdentifierIcon)
	{
		IdentifierIcon->SetBrushFromTexture(IdentifierTexture);
		// Hide the icon if no texture is provided
		IdentifierIcon->SetVisibility(IdentifierTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	
	// Update the dynamic directional indicator
	if (DirectionalIndicator)
	{
		DirectionalIndicator->SetBrushFromTexture(IndicatorTexture);
		DirectionalIndicator->SetRenderTransformAngle(IndicatorAngle);
		// Hide the indicator if no texture is provided
		DirectionalIndicator->SetVisibility(IndicatorTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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