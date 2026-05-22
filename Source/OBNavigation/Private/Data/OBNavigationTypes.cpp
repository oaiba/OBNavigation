#include "Data/OBNavigationTypes.h"

#include "MinimapDefinitionDataAsset.h"

namespace
{
bool HasUsableOutputSize(const FIntPoint& OutputSize)
{
	return OutputSize.X > 0 && OutputSize.Y > 0;
}

bool HasUsableProjectionSize(const FVector2D& ProjectionWorldSize)
{
	return ProjectionWorldSize.X > 0.0f && ProjectionWorldSize.Y > 0.0f;
}

FVector2D GetSafeViewUVScale(const FVector2D& ViewUVScale)
{
	return FVector2D(
		FMath::Max(FMath::Abs(ViewUVScale.X), KINDA_SMALL_NUMBER),
		FMath::Max(FMath::Abs(ViewUVScale.Y), KINDA_SMALL_NUMBER));
}

FVector2D DivideByViewUVScale(const FVector2D& Value, const FVector2D& ViewUVScale)
{
	const FVector2D SafeScale = GetSafeViewUVScale(ViewUVScale);
	return FVector2D(Value.X / SafeScale.X, Value.Y / SafeScale.Y);
}

FVector2D CalculateProjectionWorldSize(const FBox& WorldBounds, const FIntPoint& OutputSize)
{
	const FVector WorldSize = WorldBounds.GetSize();
	if (!WorldBounds.IsValid || FMath::IsNearlyZero(WorldSize.X) || FMath::IsNearlyZero(WorldSize.Y))
	{
		return FVector2D::ZeroVector;
	}

	if (!HasUsableOutputSize(OutputSize))
	{
		return FVector2D(FMath::Abs(WorldSize.X), FMath::Abs(WorldSize.Y));
	}

	const float AspectRatio = static_cast<float>(OutputSize.X) / static_cast<float>(OutputSize.Y);
	if (AspectRatio >= 1.0f)
	{
		const float Width = FMath::Max(FMath::Abs(WorldSize.X), FMath::Abs(WorldSize.Y) * AspectRatio);
		return FVector2D(Width, Width / AspectRatio);
	}

	const float Height = FMath::Max(FMath::Abs(WorldSize.Y), FMath::Abs(WorldSize.X) / AspectRatio);
	return FVector2D(Height * AspectRatio, Height);
}

bool IsInsideOrOnBoundsXY(const FBox& Bounds, const FVector& WorldLocation)
{
	return WorldLocation.X >= Bounds.Min.X
		&& WorldLocation.X <= Bounds.Max.X
		&& WorldLocation.Y >= Bounds.Min.Y
		&& WorldLocation.Y <= Bounds.Max.Y;
}

FVector ClampWorldLocationToBoundsXY(const FVector& WorldLocation, const FBox& Bounds)
{
	if (!Bounds.IsValid)
	{
		return WorldLocation;
	}

	return FVector(
		FMath::Clamp(WorldLocation.X, Bounds.Min.X, Bounds.Max.X),
		FMath::Clamp(WorldLocation.Y, Bounds.Min.Y, Bounds.Max.Y),
		WorldLocation.Z);
}

FVector2D ProjectWorldToPanoramicMapUV(const FVector& ProjectionWorldCenter, const FVector2D& ProjectionWorldSize,
                                       const float MapRotationDegrees,
                                       const FVector& WorldLocation)
{
	if (!HasUsableProjectionSize(ProjectionWorldSize))
	{
		return FVector2D::ZeroVector;
	}

	const float Radians = FMath::DegreesToRadians(MapRotationDegrees);
	const float CosAngle = FMath::Cos(Radians);
	const float SinAngle = FMath::Sin(Radians);
	const FVector WorldDelta = WorldLocation - ProjectionWorldCenter;
	const float LocalX = WorldDelta.X * CosAngle + WorldDelta.Y * SinAngle;
	const float LocalY = -WorldDelta.X * SinAngle + WorldDelta.Y * CosAngle;

	return FVector2D(
		0.5f + LocalX / ProjectionWorldSize.X,
		0.5f + LocalY / ProjectionWorldSize.Y);
}

EOBNavigationOverlayElementType ConvertPanoramicOverlayElementType(const EMinimapOverlayElementType SourceType)
{
	switch (SourceType)
	{
	case EMinimapOverlayElementType::Zone:
		return EOBNavigationOverlayElementType::Zone;
	case EMinimapOverlayElementType::Path:
		return EOBNavigationOverlayElementType::Path;
	case EMinimapOverlayElementType::Freehand:
		return EOBNavigationOverlayElementType::Freehand;
	case EMinimapOverlayElementType::Marker:
	default:
		return EOBNavigationOverlayElementType::Marker;
	}
}

FOBNavigationOverlayElement ConvertPanoramicOverlayElement(const FMinimapOverlayElement& SourceElement)
{
	FOBNavigationOverlayElement Result;
	Result.Type = ConvertPanoramicOverlayElementType(SourceElement.Type);
	Result.Id = SourceElement.Id;
	Result.Label = SourceElement.Label;
	Result.Category = SourceElement.Category;
	Result.FilterTags = SourceElement.FilterTags;
	Result.WorldPoints = SourceElement.WorldPoints;
	Result.Style.Color = SourceElement.Style.Color;
	Result.Style.Opacity = SourceElement.Style.Opacity;
	Result.Style.LineWidth = SourceElement.Style.LineWidth;
	Result.Style.Icon = SourceElement.Style.Icon;
	Result.bVisibleByDefault = SourceElement.bVisibleByDefault;
	return Result;
}

FOBNavigationOverlayLayer ConvertPanoramicOverlayLayer(const FMinimapOverlayLayer& SourceLayer)
{
	FOBNavigationOverlayLayer Result;
	Result.LayerName = SourceLayer.LayerName;
	Result.bVisibleByDefault = SourceLayer.bVisibleByDefault;
	Result.Elements.Reserve(SourceLayer.Elements.Num());
	for (const FMinimapOverlayElement& SourceElement : SourceLayer.Elements)
	{
		Result.Elements.Add(ConvertPanoramicOverlayElement(SourceElement));
	}
	return Result;
}
}

bool FOBNavigationMapLayerSpec::HasValidWorldBounds() const
{
	const FVector WorldSize = WorldBounds.GetSize();
	return WorldBounds.IsValid
		&& !FMath::IsNearlyZero(WorldSize.X)
		&& !FMath::IsNearlyZero(WorldSize.Y);
}

bool FOBNavigationMapLayerSpec::HasValidProjectionFrame() const
{
	return HasUsableProjectionSize(GetProjectionWorldSize());
}

FVector FOBNavigationMapLayerSpec::GetProjectionWorldCenter() const
{
	return HasUsableProjectionSize(ProjectionWorldSize) ? ProjectionWorldCenter : WorldBounds.GetCenter();
}

FVector2D FOBNavigationMapLayerSpec::GetProjectionWorldSize() const
{
	return HasUsableProjectionSize(ProjectionWorldSize)
		       ? ProjectionWorldSize
		       : CalculateProjectionWorldSize(WorldBounds, OutputSize);
}

bool FOBNavigationMapLayerSpec::HasPanoramicDefinition() const
{
	return !PanoramicDefinition.IsNull();
}

bool FOBNavigationMapLayerSpec::IsTiledLayer() const
{
	if (const UMinimapDefinitionDataAsset* Definition = PanoramicDefinition.Get())
	{
		return Definition->IsTiledDefinition();
	}

	return false;
}

bool FOBNavigationMapLayerSpec::IsSingleTexturePanoramicLayer() const
{
	if (const UMinimapDefinitionDataAsset* Definition = PanoramicDefinition.Get())
	{
		return !Definition->BaseMapTexture.IsNull() && !Definition->IsTiledDefinition();
	}

	return false;
}

bool FOBNavigationMapLayerSpec::PopulateFromPanoramicDefinition(const UMinimapDefinitionDataAsset* MinimapDefinition,
                                                                const FName InLayerName, const int32 InPriority,
                                                                const bool bForceClampQueriesToBounds)
{
	*this = FOBNavigationMapLayerSpec();
	if (!MinimapDefinition || !MinimapDefinition->WorldBounds.IsValid)
	{
		return false;
	}

	const FVector WorldSize = MinimapDefinition->WorldBounds.GetSize();
	if (FMath::IsNearlyZero(WorldSize.X) || FMath::IsNearlyZero(WorldSize.Y)
		|| MinimapDefinition->OutputSize.X <= 0 || MinimapDefinition->OutputSize.Y <= 0)
	{
		return false;
	}

	LayerName = InLayerName.IsNone() ? MinimapDefinition->GetFName() : InLayerName;
	PanoramicDefinition = TSoftObjectPtr<UMinimapDefinitionDataAsset>(const_cast<UMinimapDefinitionDataAsset*>(MinimapDefinition));
	WorldBounds = MinimapDefinition->WorldBounds;
	OutputSize = MinimapDefinition->OutputSize;
	MinimapDefinition->ResolveProjectionFrame(ProjectionWorldCenter, ProjectionWorldSize);
	Priority = InPriority;
	MapRotationDegrees = MinimapDefinition->MapRotationDegrees;
	bClampQueriesToBounds = bForceClampQueriesToBounds || MinimapDefinition->bClampQueriesToBounds;

	OverlayLayers.Reserve(MinimapDefinition->OverlayLayers.Num());
	for (const FMinimapOverlayLayer& OverlayLayer : MinimapDefinition->OverlayLayers)
	{
		OverlayLayers.Add(ConvertPanoramicOverlayLayer(OverlayLayer));
	}

	return MinimapDefinition->IsTiledDefinition() || !MinimapDefinition->BaseMapTexture.IsNull();
}

bool FOBNavigationMapLayerSpec::ContainsWorldLocationXY(const FVector& WorldLocation) const
{
	return HasValidWorldBounds() && IsInsideOrOnBoundsXY(WorldBounds, WorldLocation);
}

bool FOBNavigationMapLayerSpec::CanProjectWorldLocation(const FVector& WorldLocation) const
{
	return ContainsWorldLocationXY(WorldLocation) || (bClampQueriesToBounds && HasValidWorldBounds());
}

bool FOBNavigationMapLayerSpec::ProjectWorldToMapUVChecked(const FVector& WorldLocation, FVector2D& OutMapUV,
                                                           EOBMapProjectionResult& OutResult) const
{
	if (!HasValidWorldBounds())
	{
		OutResult = EOBMapProjectionResult::InvalidBounds;
		return false;
	}
	if (!HasValidProjectionFrame())
	{
		OutResult = EOBMapProjectionResult::InvalidBounds;
		return false;
	}

	const bool bInsideBounds = IsInsideOrOnBoundsXY(WorldBounds, WorldLocation);
	if (!bInsideBounds && !bClampQueriesToBounds)
	{
		OutResult = EOBMapProjectionResult::OutsideLayer;
		return false;
	}

	const FVector ProjectedWorldLocation = bClampQueriesToBounds
		                                       ? ClampWorldLocationToBoundsXY(WorldLocation, WorldBounds)
		                                       : WorldLocation;
	OutMapUV = ProjectWorldToPanoramicMapUV(
		GetProjectionWorldCenter(),
		GetProjectionWorldSize(),
		MapRotationDegrees,
		ProjectedWorldLocation);

	if (bClampQueriesToBounds)
	{
		OutMapUV.X = FMath::Clamp(OutMapUV.X, 0.0f, 1.0f);
		OutMapUV.Y = FMath::Clamp(OutMapUV.Y, 0.0f, 1.0f);
	}

	OutResult = bInsideBounds ? EOBMapProjectionResult::Projected : EOBMapProjectionResult::OutsideLayer;
	return true;
}

float FOBNavigationMapViewContext::GetAppliedRotationDegrees() const
{
	return bShouldRotateMap ? -(TotalStaticRotation + DynamicMapYaw) : -TotalStaticRotation;
}

FOBNavigationMapViewport OBNavigation::MapView::CalculateMapViewport(const FVector2D& CanvasSize,
                                                                      const FOBNavigationMapLayerSpec& LayerSpec)
{
	return CalculateMapViewport(CanvasSize, LayerSpec, EOBNavigationSurface::FullMap);
}

FOBNavigationMapViewport OBNavigation::MapView::CalculateMapViewport(const FVector2D& CanvasSize,
                                                                      const FOBNavigationMapLayerSpec& LayerSpec,
                                                                      const EOBNavigationSurface Surface)
{
	FOBNavigationMapViewport Viewport;
	Viewport.RawCanvasSize = CanvasSize;
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return Viewport;
	}

	float DesiredAspectRatio = 1.0f;
	if (Surface == EOBNavigationSurface::Minimap)
	{
		DesiredAspectRatio = 1.0f;
	}
	else if (LayerSpec.OutputSize.X > 0 && LayerSpec.OutputSize.Y > 0)
	{
		DesiredAspectRatio = static_cast<float>(LayerSpec.OutputSize.X) / static_cast<float>(LayerSpec.OutputSize.Y);
	}
	else if (LayerSpec.HasValidWorldBounds())
	{
		const FVector2D ProjectionWorldSize = LayerSpec.GetProjectionWorldSize();
		if (!FMath::IsNearlyZero(ProjectionWorldSize.Y))
		{
			DesiredAspectRatio = FMath::Abs(ProjectionWorldSize.X / ProjectionWorldSize.Y);
		}
	}
	DesiredAspectRatio = FMath::Max(DesiredAspectRatio, KINDA_SMALL_NUMBER);

	const float CanvasAspectRatio = CanvasSize.X / CanvasSize.Y;
	if (CanvasAspectRatio > DesiredAspectRatio)
	{
		Viewport.Size.Y = CanvasSize.Y;
		Viewport.Size.X = CanvasSize.Y * DesiredAspectRatio;
	}
	else
	{
		Viewport.Size.X = CanvasSize.X;
		Viewport.Size.Y = CanvasSize.X / DesiredAspectRatio;
	}

	Viewport.Origin = (CanvasSize - Viewport.Size) * 0.5f;
	Viewport.AspectRatio = DesiredAspectRatio;
	return Viewport;
}

bool OBNavigation::MapView::ProjectUVToCanvas(const FVector2D& MapUV, const FVector2D& CanvasSize,
                                              const FOBNavigationMapViewContext& ViewContext,
                                              FOBNavigationCanvasProjection& OutProjection)
{
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return false;
	}

	FOBNavigationMapViewport FullViewport;
	FullViewport.RawCanvasSize = CanvasSize;
	FullViewport.Origin = FVector2D::ZeroVector;
	FullViewport.Size = CanvasSize;
	FullViewport.AspectRatio = CanvasSize.Y > 0.0f ? CanvasSize.X / CanvasSize.Y : 1.0f;
	return ProjectUVToCanvas(MapUV, FullViewport, ViewContext, OutProjection);
}

bool OBNavigation::MapView::ProjectUVToCanvas(const FVector2D& MapUV,
                                              const FOBNavigationMapViewport& MapViewport,
                                              const FOBNavigationMapViewContext& ViewContext,
                                              FOBNavigationCanvasProjection& OutProjection)
{
	if (!MapViewport.IsValid())
	{
		return false;
	}

	const FVector2D CanvasCenter = MapViewport.GetCenter();
	const float SafeZoom = FMath::Max(ViewContext.Zoom, KINDA_SMALL_NUMBER);
	const FVector2D ScaledUVOffset = DivideByViewUVScale(MapUV - ViewContext.ViewCenterUV, ViewContext.ViewUVScale);
	const FVector2D PixelOffset = ScaledUVOffset * MapViewport.Size * SafeZoom;
	const FVector2D RotatedPixelOffset = PixelOffset.GetRotated(ViewContext.GetAppliedRotationDegrees());

	OutProjection.RotatedPixelOffset = RotatedPixelOffset;
	OutProjection.CanvasPosition = CanvasCenter + RotatedPixelOffset;
	OutProjection.bIsClampedToEdge = false;

	if (ViewContext.bClampToCanvas)
	{
		if (ViewContext.ClampShape == EOBMapViewportClampShape::Circle)
		{
			const float MapRadius = FMath::Min(MapViewport.Size.X, MapViewport.Size.Y) * 0.5f;
			if (MapRadius > 0.0f && RotatedPixelOffset.SizeSquared() > FMath::Square(MapRadius))
			{
				OutProjection.CanvasPosition = CanvasCenter + RotatedPixelOffset.GetSafeNormal() * MapRadius;
				OutProjection.bIsClampedToEdge = true;
			}
		}
		else
		{
			const FVector2D HalfSize = MapViewport.Size * 0.5f;
			const float AbsX = FMath::Abs(RotatedPixelOffset.X);
			const float AbsY = FMath::Abs(RotatedPixelOffset.Y);
			if ((AbsX > HalfSize.X || AbsY > HalfSize.Y) && (AbsX > 0.0f || AbsY > 0.0f))
			{
				float ScaleToEdge = TNumericLimits<float>::Max();
				if (AbsX > KINDA_SMALL_NUMBER)
				{
					ScaleToEdge = FMath::Min(ScaleToEdge, HalfSize.X / AbsX);
				}
				if (AbsY > KINDA_SMALL_NUMBER)
				{
					ScaleToEdge = FMath::Min(ScaleToEdge, HalfSize.Y / AbsY);
				}

				OutProjection.CanvasPosition = CanvasCenter + RotatedPixelOffset * FMath::Clamp(ScaleToEdge, 0.0f, 1.0f);
				OutProjection.bIsClampedToEdge = true;
			}
		}
	}

	return true;
}
