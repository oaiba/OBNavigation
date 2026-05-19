#include "Data/OBNavigationTypes.h"

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
}

bool FOBNavigationMapLayerSpec::HasValidWorldBounds() const
{
	const FVector WorldSize = WorldBounds.GetSize();
	return WorldBounds.IsValid
		&& !FMath::IsNearlyZero(WorldSize.X)
		&& !FMath::IsNearlyZero(WorldSize.Y);
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

	OutMapUV = FVector2D(
		(WorldLocation.X - BoundsMin.X) / BoundsSize.X,
		1.0f - ((WorldLocation.Y - BoundsMin.Y) / BoundsSize.Y));

	OutMapUV = ApplyMapRotation(OutMapUV, MapRotationDegrees);

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
