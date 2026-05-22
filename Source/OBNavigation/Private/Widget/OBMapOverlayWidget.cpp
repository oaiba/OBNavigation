#include "Widget/OBMapOverlayWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"

void UOBMapOverlayWidget::SetOverlayContext(const FOBNavigationMapLayerSpec& InLayerSpec,
                                            const TArray<FOBNavigationOverlayElement>& InOverlayElements,
                                            const FOBNavigationMapViewContext& InViewContext)
{
	LayerSpec = InLayerSpec;
	OverlayElements = InOverlayElements;
	ViewContext = InViewContext;
	ViewContext.bClampToCanvas = false;
	bHasOverlayContext = true;
	InvalidateLayoutAndVolatility();
}

void UOBMapOverlayWidget::ClearOverlayContext()
{
	OverlayElements.Reset();
	bHasOverlayContext = false;
	InvalidateLayoutAndVolatility();
}

int32 UOBMapOverlayWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                                       const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                                       const int32 LayerId, const FWidgetStyle& InWidgetStyle,
                                       const bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
	                                          InWidgetStyle, bParentEnabled);
	if (!bHasOverlayContext || OverlayElements.IsEmpty())
	{
		return MaxLayer;
	}

	const FVector2D CanvasSize = AllottedGeometry.GetLocalSize();
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return MaxLayer;
	}

	const FOBNavigationMapViewport MapViewport = OBNavigation::MapView::CalculateMapViewport(CanvasSize, LayerSpec);
	if (!MapViewport.IsValid())
	{
		return MaxLayer;
	}

	int32 DrawLayer = MaxLayer + 1;
	for (const FOBNavigationOverlayElement& Element : OverlayElements)
	{
		if (Element.WorldPoints.IsEmpty())
		{
			continue;
		}

		FLinearColor DrawColor = Element.Style.Color;
		DrawColor.A *= Element.Style.Opacity;
		const float LineWidth = FMath::Max(1.0f, Element.Style.LineWidth);

		if (Element.Type == EOBNavigationOverlayElementType::Marker)
		{
			FVector2D MarkerPosition;
			if (!ProjectWorldToCanvas(Element.WorldPoints[0], MapViewport, MarkerPosition))
			{
				continue;
			}

			const FVector2D IconSize(
				FMath::Max(Element.Style.IconSize.X, 4.0f),
				FMath::Max(Element.Style.IconSize.Y, 4.0f));
			const FVector2D IconPosition = MarkerPosition - IconSize * 0.5f;

			if (UTexture2D* IconTexture = Element.Style.Icon.Get())
			{
				FSlateBrush IconBrush;
				IconBrush.SetResourceObject(IconTexture);
				IconBrush.ImageSize = IconSize;
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					DrawLayer,
					AllottedGeometry.ToPaintGeometry(
						FVector2f(static_cast<float>(IconSize.X), static_cast<float>(IconSize.Y)),
						FSlateLayoutTransform(FVector2f(static_cast<float>(IconPosition.X),
						                                static_cast<float>(IconPosition.Y)))),
					&IconBrush,
					ESlateDrawEffect::None,
					DrawColor);
			}
			else
			{
				const FSlateColorBrush FallbackBrush(DrawColor);
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					DrawLayer,
					AllottedGeometry.ToPaintGeometry(
						FVector2f(static_cast<float>(IconSize.X), static_cast<float>(IconSize.Y)),
						FSlateLayoutTransform(FVector2f(static_cast<float>(IconPosition.X),
						                                static_cast<float>(IconPosition.Y)))),
					&FallbackBrush,
					ESlateDrawEffect::None,
					DrawColor);
			}
			++DrawLayer;
			continue;
		}

		TArray<FVector2D> CanvasPoints;
		CanvasPoints.Reserve(Element.WorldPoints.Num() + 1);
		for (const FVector& WorldPoint : Element.WorldPoints)
		{
			FVector2D CanvasPoint;
			if (ProjectWorldToCanvas(WorldPoint, MapViewport, CanvasPoint))
			{
				CanvasPoints.Add(CanvasPoint);
			}
		}

		if (Element.Type == EOBNavigationOverlayElementType::Zone && CanvasPoints.Num() > 2)
		{
			CanvasPoints.Add(CanvasPoints[0]);
		}

		if (CanvasPoints.Num() < 2)
		{
			continue;
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			DrawLayer,
			AllottedGeometry.ToPaintGeometry(),
			CanvasPoints,
			ESlateDrawEffect::None,
			DrawColor,
			true,
			LineWidth);
		++DrawLayer;
	}

	return DrawLayer;
}

bool UOBMapOverlayWidget::ProjectWorldToCanvas(const FVector& WorldLocation,
                                               const FOBNavigationMapViewport& MapViewport,
                                               FVector2D& OutCanvasPosition) const
{
	FVector2D PointUV;
	EOBMapProjectionResult ProjectionResult = EOBMapProjectionResult::NoLayer;
	if (!LayerSpec.ProjectWorldToMapUVChecked(WorldLocation, PointUV, ProjectionResult))
	{
		return false;
	}

	FOBNavigationCanvasProjection Projection;
	if (!OBNavigation::MapView::ProjectUVToCanvas(PointUV, MapViewport, ViewContext, Projection))
	{
		return false;
	}

	OutCanvasPosition = Projection.CanvasPosition;
	return true;
}
