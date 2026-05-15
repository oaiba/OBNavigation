#include "OBNavigationTypes.h"

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
	return HasValidWorldBounds() && WorldBounds.IsInsideXY(WorldLocation);
}

bool FOBNavigationMapLayerSpec::ProjectWorldToMapUVChecked(const FVector& WorldLocation, FVector2D& OutMapUV,
                                                           EOBMapProjectionResult& OutResult) const
{
	if (!HasValidWorldBounds())
	{
		OutResult = EOBMapProjectionResult::InvalidBounds;
		return false;
	}

	const bool bInsideBounds = WorldBounds.IsInsideXY(WorldLocation);
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
