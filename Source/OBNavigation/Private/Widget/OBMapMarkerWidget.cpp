// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/OBMapMarkerWidget.h"

#include "Components/Image.h"

void UOBMapMarkerWidget::InitializeMarker(UTexture2D* IdentifierTexture, UMaterialInterface* IndicatorMaterial)
{
	// Set the static identifier icon's texture and visibility
	if (IdentifierIcon)
	{
		IdentifierIcon->SetBrushFromTexture(IdentifierTexture);
		IdentifierIcon->SetVisibility(IdentifierTexture
			                              ? ESlateVisibility::HitTestInvisible
			                              : ESlateVisibility::Collapsed);
	}

	if (DirectionalIndicator && IndicatorMaterial)
	{
		// Create a dynamic instance from the provided base material
		FOVMaterialInstance = UMaterialInstanceDynamic::Create(IndicatorMaterial, this);
		DirectionalIndicator->SetBrushFromMaterial(FOVMaterialInstance);
		DirectionalIndicator->SetVisibility(ESlateVisibility::HitTestInvisible);
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

void UOBMapMarkerWidget::UpdateVisuals(const float IndicatorAngle, const float InViewAngle, const float InViewDistance)
{
	if (DirectionalIndicator)
	{
		// Update the rotation of the entire image widget
		DirectionalIndicator->SetRenderTransformAngle(IndicatorAngle);
	}

	if (FOVMaterialInstance)
	{
		// Update the parameters INSIDE the material
		FOVMaterialInstance->SetScalarParameterValue("ViewAngle", InViewAngle);
		FOVMaterialInstance->SetScalarParameterValue("ViewDistance", InViewDistance);
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
