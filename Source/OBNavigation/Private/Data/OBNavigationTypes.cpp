#include "Data/OBNavigationTypes.h"

#include "MinimapDefinitionDataAsset.h"

namespace
{
FVector2D ApplyMapRotation(const FVector2D& InUV, const float RotationDegrees)
{
	if (FMath::IsNearlyZero(RotationDegrees))
	{
		return InUV;
	}

	const FVector2D Center(0.5f, 0.5f);
	const float Radians = FMath::DegreesToRadians(RotationDegrees);
	const float CosAngle = FMath::Cos(Radians);
	const float SinAngle = FMath::Sin(Radians);
	const FVector2D Offset = InUV - Center;

	return Center + FVector2D(
		Offset.X * CosAngle - Offset.Y * SinAngle,
		Offset.X * SinAngle + Offset.Y * CosAngle);
}

bool IsInsideOrOnBoundsXY(const FBox& Bounds, const FVector& WorldLocation)
{
	return WorldLocation.X >= Bounds.Min.X
		&& WorldLocation.X <= Bounds.Max.X
		&& WorldLocation.Y >= Bounds.Min.Y
		&& WorldLocation.Y <= Bounds.Max.Y;
}

FVector2D ProjectWorldToPanoramicMapUV(const FBox& WorldBounds, const float MapRotationDegrees,
                                       const FVector& WorldLocation)
{
	const FVector BoundsMin = WorldBounds.Min;
	const FVector BoundsSize = WorldBounds.GetSize();

	const FVector2D RawUV(
		(WorldLocation.X - BoundsMin.X) / BoundsSize.X,
		1.0f - ((WorldLocation.Y - BoundsMin.Y) / BoundsSize.Y));
	const FVector2D RotatedUV = ApplyMapRotation(RawUV, MapRotationDegrees);
	return FVector2D(RotatedUV.X, 1.0f - RotatedUV.Y);
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

	return !PanoramicDefinition.IsNull() && MapTexture == nullptr;
}

bool FOBNavigationMapLayerSpec::UsesSingleTextureLayer() const
{
	return MapTexture != nullptr && !IsTiledLayer();
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
	MapTexture = MinimapDefinition->BaseMapTexture.IsNull() ? nullptr : MinimapDefinition->BaseMapTexture.LoadSynchronous();
	WorldBounds = MinimapDefinition->WorldBounds;
	OutputSize = MinimapDefinition->OutputSize;
	Priority = InPriority;
	MapRotationDegrees = MinimapDefinition->MapRotationDegrees;
	bClampQueriesToBounds = bForceClampQueriesToBounds || MinimapDefinition->bClampQueriesToBounds;

	OverlayLayers.Reserve(MinimapDefinition->OverlayLayers.Num());
	for (const FMinimapOverlayLayer& OverlayLayer : MinimapDefinition->OverlayLayers)
	{
		OverlayLayers.Add(ConvertPanoramicOverlayLayer(OverlayLayer));
	}

	return MapTexture != nullptr || MinimapDefinition->IsTiledDefinition() || !MinimapDefinition->TileSet.IsNull();
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

	const bool bInsideBounds = IsInsideOrOnBoundsXY(WorldBounds, WorldLocation);
	if (!bInsideBounds && !bClampQueriesToBounds)
	{
		OutResult = EOBMapProjectionResult::OutsideLayer;
		return false;
	}

	const FVector BoundsMin = WorldBounds.Min;
	const FVector BoundsSize = WorldBounds.GetSize();

	if (HasPanoramicDefinition())
	{
		OutMapUV = ProjectWorldToPanoramicMapUV(WorldBounds, MapRotationDegrees, WorldLocation);
	}
	else
	{
		OutMapUV = FVector2D(
			(WorldLocation.X - BoundsMin.X) / BoundsSize.X,
			1.0f - ((WorldLocation.Y - BoundsMin.Y) / BoundsSize.Y));

		OutMapUV = ApplyMapRotation(OutMapUV, MapRotationDegrees);
	}

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

bool OBNavigation::MapView::ProjectUVToCanvas(const FVector2D& MapUV, const FVector2D& CanvasSize,
                                              const FOBNavigationMapViewContext& ViewContext,
                                              FOBNavigationCanvasProjection& OutProjection)
{
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return false;
	}

	const FVector2D CanvasCenter = CanvasSize * 0.5f;
	const float SafeZoom = FMath::Max(ViewContext.Zoom, KINDA_SMALL_NUMBER);
	const FVector2D PixelOffset = (MapUV - ViewContext.ViewCenterUV) * CanvasSize * SafeZoom;
	const FVector2D RotatedPixelOffset = PixelOffset.GetRotated(ViewContext.GetAppliedRotationDegrees());

	OutProjection.RotatedPixelOffset = RotatedPixelOffset;
	OutProjection.CanvasPosition = CanvasCenter + RotatedPixelOffset;
	OutProjection.bIsClampedToEdge = false;

	const float MapRadius = FMath::Min(CanvasCenter.X, CanvasCenter.Y);
	if (ViewContext.bClampToCanvas && MapRadius > 0.0f && RotatedPixelOffset.SizeSquared() > FMath::Square(MapRadius))
	{
		OutProjection.CanvasPosition = CanvasCenter + RotatedPixelOffset.GetSafeNormal() * MapRadius;
		OutProjection.bIsClampedToEdge = true;
	}

	return true;
}
