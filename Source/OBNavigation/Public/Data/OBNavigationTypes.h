#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "OBNavigationTypes.generated.h"

class AActor;
class UMinimapDefinitionDataAsset;
class UOBMarkerConfigAsset;
class UTexture2D;

UENUM(BlueprintType)
enum class EOBNavigationSurface : uint8
{
	Minimap,
	FullMap,
	Compass
};

UENUM(BlueprintType)
enum class EOBMarkerVisibilityPolicy : uint8
{
	LocalOnly,
	SquadOnly,
	Public,
	DebugOnly
};

UENUM(BlueprintType)
enum class EOBMapProjectionResult : uint8
{
	Projected,
	NoLayer,
	OutsideLayer,
	InvalidBounds
};

/**
 * Runtime view state used to project map UV coordinates into a widget canvas.
 */
struct OBNAVIGATION_API FOBNavigationMapViewContext
{
	FVector2D ViewCenterUV = FVector2D(0.5f, 0.5f);
	float Zoom = 1.0f;
	float TotalStaticRotation = 0.0f;
	float DynamicMapYaw = 0.0f;
	bool bShouldRotateMap = false;
	bool bClampToCanvas = true;

	float GetAppliedRotationDegrees() const;
};

/**
 * Result of projecting a map UV coordinate into widget-local canvas space.
 */
struct OBNAVIGATION_API FOBNavigationCanvasProjection
{
	FVector2D CanvasPosition = FVector2D::ZeroVector;
	FVector2D RotatedPixelOffset = FVector2D::ZeroVector;
	bool bIsClampedToEdge = false;
};

namespace OBNavigation::MapView
{
	OBNAVIGATION_API bool ProjectUVToCanvas(const FVector2D& MapUV, const FVector2D& CanvasSize,
	                                        const FOBNavigationMapViewContext& ViewContext,
	                                        FOBNavigationCanvasProjection& OutProjection);
}

UENUM(BlueprintType)
enum class EOBNavigationOverlayElementType : uint8
{
	Marker UMETA(DisplayName = "Marker"),
	Zone UMETA(DisplayName = "Zone"),
	Path UMETA(DisplayName = "Path"),
	Freehand UMETA(DisplayName = "Freehand")
};

USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBNavigationOverlayStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	float Opacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	float LineWidth = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	FVector2D IconSize = FVector2D(24.0f, 24.0f);
};

USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBNavigationOverlayElement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	EOBNavigationOverlayElementType Type = EOBNavigationOverlayElementType::Marker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	FName Category = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	TArray<FName> FilterTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	TArray<FVector> WorldPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	FOBNavigationOverlayStyle Style;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	bool bVisibleByDefault = true;
};

USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBNavigationOverlayLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	FName LayerName = TEXT("Default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	bool bVisibleByDefault = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	TArray<FOBNavigationOverlayElement> Elements;
};

USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBNavigationMapLayerSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	FName LayerName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	TObjectPtr<UTexture2D> MapTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	TSoftObjectPtr<UMinimapDefinitionDataAsset> PanoramicDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	FBox WorldBounds = FBox(ForceInit);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	FIntPoint OutputSize = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	float MapRotationDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	bool bClampQueriesToBounds = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	TArray<FOBNavigationOverlayLayer> OverlayLayers;

	bool HasValidWorldBounds() const;
	bool HasPanoramicDefinition() const;
	bool IsTiledLayer() const;
	bool UsesSingleTextureLayer() const;
	bool PopulateFromPanoramicDefinition(const UMinimapDefinitionDataAsset* MinimapDefinition, FName InLayerName,
	                                     int32 InPriority, bool bForceClampQueriesToBounds);
	bool ContainsWorldLocationXY(const FVector& WorldLocation) const;
	bool CanProjectWorldLocation(const FVector& WorldLocation) const;
	bool ProjectWorldToMapUVChecked(const FVector& WorldLocation, FVector2D& OutMapUV,
	                                EOBMapProjectionResult& OutResult) const;
};

USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBNavigationMarkerSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FGuid MarkerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FGameplayTag MarkerType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FName LayerName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	TObjectPtr<AActor> TrackedActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FRotator WorldRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	TObjectPtr<UOBMarkerConfigAsset> ConfigAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	float LifeTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	int32 OwnerPlayerId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	int32 TeamId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	EOBMarkerVisibilityPolicy VisibilityPolicy = EOBMarkerVisibilityPolicy::Public;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	int32 SortPriority = 0;
};
