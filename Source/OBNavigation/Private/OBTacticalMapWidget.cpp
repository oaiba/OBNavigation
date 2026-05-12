#include "OBTacticalMapWidget.h"

void UOBTacticalMapWidget::AddZoomInput(const float ZoomDelta)
{
	const UOBMinimapConfigAsset* MinimapConfig = GetConfig();
	const float BaseStep = MinimapConfig ? FMath::Max(0.1f, MinimapConfig->Zoom * 0.1f) : 0.5f;
	SetMinimapZoom(GetMinimapZoom() + ZoomDelta * BaseStep);
}

void UOBTacticalMapWidget::SetPanInput(const FVector2D InPanInput)
{
	PanInput = InPanInput;
}
