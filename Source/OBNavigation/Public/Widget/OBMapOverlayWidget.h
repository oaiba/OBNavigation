#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/OBNavigationTypes.h"
#include "OBMapOverlayWidget.generated.h"

/**
 * Handles custom painting of overlay elements on top of a map widget.
 *
 * =========================================================================
 * Visual ASCII Wireframe:
 *
 *  +-------------------[Root]-------------------+
 *  |                                            |
 *  |  (No bound child widgets — all content     |
 *  |   is drawn procedurally via NativePaint)   |
 *  |                                            |
 *  |      .------.                              |
 *  |     /  Zone  \    <-- Painted polygon      |
 *  |    |          |                            |
 *  |     \        /                             |
 *  |      '------'                              |
 *  |                                            |
 *  |    ------*------*------   <-- Painted path |
 *  |                                            |
 *  |         X  <-- Painted marker point        |
 *  |                                            |
 *  +--------------------------------------------+
 *
 *  Note: This widget is layered on top of a MapImage inside
 *  the parent minimap/tactical-map widget. It matches the
 *  parent's geometry and applies the same view context
 *  (zoom, rotation, center UV) for correct projection.
 * =========================================================================
 */
UCLASS()
class OBNAVIGATION_API UOBMapOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Sets the map layer, visible overlay elements, and current map view state used for custom painting.
	 */
	void SetOverlayContext(const FOBNavigationMapLayerSpec& InLayerSpec,
	                       const TArray<FOBNavigationOverlayElement>& InOverlayElements,
	                       const FOBNavigationMapViewContext& InViewContext);

	/**
	 * Clears all overlay draw state until a valid map layer is available again.
	 */
	void ClearOverlayContext();

protected:
	/** Paints overlay primitives using the current layer and view context. */
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	                          const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	                          int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	/** Projects a world-space overlay point into widget-local canvas space. */
	bool ProjectWorldToCanvas(const FVector& WorldLocation, const FOBNavigationMapViewport& MapViewport,
	                          FVector2D& OutCanvasPosition) const;

	/** Active layer metadata used to project overlay world points. */
	UPROPERTY(Transient)
	FOBNavigationMapLayerSpec LayerSpec;

	/** Overlay elements selected for this widget surface and filters. */
	UPROPERTY(Transient)
	TArray<FOBNavigationOverlayElement> OverlayElements;

	/** Current map view state shared with markers and tiles. */
	FOBNavigationMapViewContext ViewContext;

	/** True when LayerSpec, OverlayElements, and ViewContext are valid for painting. */
	bool bHasOverlayContext = false;
};
