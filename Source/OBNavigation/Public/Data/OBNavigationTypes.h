#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "OBNavigationTypes.generated.h"

class AActor;
class UMinimapDefinitionDataAsset;
class UOBMarkerConfigAsset;
class UTexture2D;

struct FOBNavigationMapLayerSpec;

UENUM(BlueprintType)
enum class EOBNavigationSurface : uint8
{
	/** Compact player-centered minimap surface. */
	Minimap,

	/** Large tactical/full-map surface. */
	FullMap,

	/** Compass/radar strip surface. */
	Compass
};

/** Describes which clients are allowed to see a marker. */
UENUM(BlueprintType)
enum class EOBMarkerVisibilityPolicy : uint8
{
	/** Visible only to the owning local player. */
	LocalOnly,

	/** Visible to players on the same team or squad. */
	SquadOnly,

	/** Visible to every local viewer. */
	Public,

	/** Visible only when debug markers are enabled. */
	DebugOnly
};

/** Result codes returned by world-to-map projection queries. */
UENUM(BlueprintType)
enum class EOBMapProjectionResult : uint8
{
	/** Projection succeeded and returned a valid UV. */
	Projected,

	/** No map layer was available for the query. */
	NoLayer,

	/** The world location is outside the layer and clamping is disabled. */
	OutsideLayer,

	/** The layer bounds are missing or degenerate. */
	InvalidBounds
};

/** Shape used when projected points are clamped to the visible map viewport. */
UENUM(BlueprintType)
enum class EOBMapViewportClampShape : uint8
{
	/** Clamp to the rectangular viewport bounds. */
	Rect,

	/** Clamp to the circle inscribed in the viewport. */
	Circle
};

/**
 * Runtime view state used to project map UV coordinates into a widget canvas.
 */
struct OBNAVIGATION_API FOBNavigationMapViewContext
{
	/** Normalized map UV held at the widget's visual center. */
	FVector2D ViewCenterUV = FVector2D(0.5f, 0.5f);

	/** Zoom multiplier where larger values show a smaller world area. */
	float Zoom = 1.0f;

	/** UV span represented by a square unit of viewport space before zoom is applied. */
	FVector2D ViewUVScale = FVector2D(1.0f, 1.0f);

	/** Static rotation from map metadata and widget alignment settings, in degrees. */
	float TotalStaticRotation = 0.0f;

	/** Runtime yaw contributed by the tracked pawn, in degrees. */
	float DynamicMapYaw = 0.0f;

	/** Whether the rendered map should rotate around ViewCenterUV. */
	bool bShouldRotateMap = false;

	/** Whether projected points outside the viewport should clamp to the edge. */
	bool bClampToCanvas = true;

	/** Visible viewport shape used by edge-clamped markers. */
	EOBMapViewportClampShape ClampShape = EOBMapViewportClampShape::Rect;

	/** Surface that owns this view context. */
	EOBNavigationSurface Surface = EOBNavigationSurface::FullMap;

	/** Returns the signed rotation applied to projected map pixels. */
	float GetAppliedRotationDegrees() const;
};

/**
 * Result of projecting a map UV coordinate into widget-local canvas space.
 */
struct OBNAVIGATION_API FOBNavigationCanvasProjection
{
	/** Final widget-local canvas position in Slate units. */
	FVector2D CanvasPosition = FVector2D::ZeroVector;

	/** Pixel offset from the map viewport center after view rotation is applied. */
	FVector2D RotatedPixelOffset = FVector2D::ZeroVector;

	/** True when the point was outside the visible radius and got clamped. */
	bool bIsClampedToEdge = false;
};

/**
 * Aspect-preserving content rect used to project map UVs inside a widget canvas.
 */
struct OBNAVIGATION_API FOBNavigationMapViewport
{
	/** Full allocated canvas size before aspect preserving fit is applied. */
	FVector2D RawCanvasSize = FVector2D::ZeroVector;

	/** Top-left origin of the fitted map content rect inside the raw canvas. */
	FVector2D Origin = FVector2D::ZeroVector;

	/** Size of the fitted map content rect in Slate units. */
	FVector2D Size = FVector2D::ZeroVector;

	/** Map content aspect ratio used to fit Size within RawCanvasSize. */
	float AspectRatio = 1.0f;

	/** Returns the center of the fitted map content rect. */
	FVector2D GetCenter() const { return Origin + Size * 0.5f; }

	/** Returns true when the fitted content rect has non-zero dimensions. */
	bool IsValid() const { return Size.X > 0.0f && Size.Y > 0.0f; }
};

namespace OBNavigation::MapView
{
	/**
	 * Projects a map UV into a full-canvas viewport.
	 *
	 * @param MapUV Normalized map coordinate to project.
	 * @param CanvasSize Raw widget canvas size.
	 * @param ViewContext Current map view state.
	 * @param OutProjection Populated with the projected canvas position.
	 * @return True when CanvasSize and ViewContext are valid enough to project.
	 */
	OBNAVIGATION_API bool ProjectUVToCanvas(const FVector2D& MapUV, const FVector2D& CanvasSize,
	                                        const FOBNavigationMapViewContext& ViewContext,
	                                        FOBNavigationCanvasProjection& OutProjection);

	/**
	 * Projects a map UV into an aspect-preserving map viewport.
	 *
	 * @param MapUV Normalized map coordinate to project.
	 * @param MapViewport Fitted map viewport within the widget canvas.
	 * @param ViewContext Current map view state.
	 * @param OutProjection Populated with the projected canvas position.
	 * @return True when MapViewport and ViewContext are valid enough to project.
	 */
	OBNAVIGATION_API bool ProjectUVToCanvas(const FVector2D& MapUV, const FOBNavigationMapViewport& MapViewport,
	                                        const FOBNavigationMapViewContext& ViewContext,
	                                        FOBNavigationCanvasProjection& OutProjection);

	/**
	 * Calculates the surface-specific viewport for a map layer inside a canvas.
	 *
	 * @param CanvasSize Raw canvas size in Slate units.
	 * @param LayerSpec Layer metadata used to derive the map aspect ratio.
	 * @param Surface Surface policy to apply. Minimap is always square; FullMap preserves layer aspect.
	 * @return Centered fitted viewport for map rendering and projection.
	 */
	OBNAVIGATION_API FOBNavigationMapViewport CalculateMapViewport(const FVector2D& CanvasSize,
	                                                               const FOBNavigationMapLayerSpec& LayerSpec,
	                                                               EOBNavigationSurface Surface);
}

/** Runtime overlay primitive type painted over map imagery. */
UENUM(BlueprintType)
enum class EOBNavigationOverlayElementType : uint8
{
	/** Point marker rendered by the overlay widget. */
	Marker UMETA(DisplayName = "Marker"),

	/** Filled or stroked world-space polygon. */
	Zone UMETA(DisplayName = "Zone"),

	/** Polyline path through world-space points. */
	Path UMETA(DisplayName = "Path"),

	/** Freehand polyline drawn from captured world-space points. */
	Freehand UMETA(DisplayName = "Freehand")
};

/** Visual styling for a navigation overlay element. */
USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBNavigationOverlayStyle
{
	GENERATED_BODY()

	/** Base tint used when drawing the overlay element. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	FLinearColor Color = FLinearColor::White;

	/** Opacity multiplier in the [0, 1] range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	float Opacity = 1.0f;

	/** Stroke width in Slate units for paths and outlines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	float LineWidth = 2.0f;

	/** Optional icon texture used by marker overlay elements. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Icon draw size in Slate units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	FVector2D IconSize = FVector2D(24.0f, 24.0f);
};

/** Single overlay entry projected from world space onto a map widget. */
USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBNavigationOverlayElement
{
	GENERATED_BODY()

	/** Primitive type used by the overlay paint path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	EOBNavigationOverlayElementType Type = EOBNavigationOverlayElementType::Marker;

	/** Stable identifier for save data, filtering, and debug output. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	FName Id = NAME_None;

	/** User-facing label associated with this overlay element. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	FText Label;

	/** Category name used by tactical map filters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	FName Category = NAME_None;

	/** Optional tags used by tactical map filters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	TArray<FName> FilterTags;

	/** World-space points defining the marker, zone, path, or freehand shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	TArray<FVector> WorldPoints;

	/** Visual style used by this overlay element. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	FOBNavigationOverlayStyle Style;

	/** Whether the element is visible before tactical filters are applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	bool bVisibleByDefault = true;
};

/** Named collection of overlay elements that can be toggled or filtered together. */
USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBNavigationOverlayLayer
{
	GENERATED_BODY()

	/** Stable overlay layer name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	FName LayerName = TEXT("Default");

	/** Whether this layer starts visible in map widgets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	bool bVisibleByDefault = true;

	/** Overlay elements owned by this layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	TArray<FOBNavigationOverlayElement> Elements;
};

/** Runtime map layer metadata consumed by minimap and tactical map widgets. */
USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBNavigationMapLayerSpec
{
	GENERATED_BODY()

	/** Stable layer or floor name used for selection and tactical overrides. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	FName LayerName = NAME_None;

	/** Panoramic minimap definition that owns texture, tile, bounds, and overlay metadata. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	TSoftObjectPtr<UMinimapDefinitionDataAsset> PanoramicDefinition;

	/** World-space XY bounds covered by this map layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	FBox WorldBounds = FBox(ForceInit);

	/** Pixel output size of the Panoramic map capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	FIntPoint OutputSize = FIntPoint::ZeroValue;

	/** World-space center of the actual captured texture footprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	FVector ProjectionWorldCenter = FVector::ZeroVector;

	/** World-space XY size of the actual captured texture footprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	FVector2D ProjectionWorldSize = FVector2D::ZeroVector;

	/** Selection priority; larger values win when multiple layers contain the pawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	int32 Priority = 0;

	/** Static rotation baked into the Panoramic map definition, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	float MapRotationDegrees = 0.0f;

	/** Whether projections outside WorldBounds should clamp instead of failing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Map")
	bool bClampQueriesToBounds = false;

	/** Overlay layers copied from the Panoramic definition for runtime drawing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation|Overlay")
	TArray<FOBNavigationOverlayLayer> OverlayLayers;

	/** Returns true when WorldBounds is valid and has non-zero XY extent. */
	bool HasValidWorldBounds() const;

	/** Returns true when the texture projection footprint has non-zero XY extent. */
	bool HasValidProjectionFrame() const;

	/** Returns the projection center, deriving from WorldBounds when older specs omit explicit frame data. */
	FVector GetProjectionWorldCenter() const;

	/** Returns the projection size, deriving from WorldBounds/OutputSize when older specs omit explicit frame data. */
	FVector2D GetProjectionWorldSize() const;

	/** Returns true when this layer references a Panoramic definition. */
	bool HasPanoramicDefinition() const;

	/** Returns true when the Panoramic definition contains a valid tile set. */
	bool IsTiledLayer() const;

	/** Returns true when the Panoramic definition uses BaseMapTexture without a tile set. */
	bool IsSingleTexturePanoramicLayer() const;

	/**
	 * Populates this layer from a loaded Panoramic definition.
	 *
	 * @param MinimapDefinition Loaded source definition to copy from.
	 * @param InLayerName Optional override name; NAME_None uses the definition asset name.
	 * @param InPriority Layer selection priority.
	 * @param bForceClampQueriesToBounds Forces clamped projection even if the definition disables it.
	 * @return True when the definition has valid bounds and either tiles or a base texture.
	 */
	bool PopulateFromPanoramicDefinition(const UMinimapDefinitionDataAsset* MinimapDefinition, FName InLayerName,
	                                     int32 InPriority, bool bForceClampQueriesToBounds);

	/** Returns true when WorldLocation lies inside the layer XY bounds. */
	bool ContainsWorldLocationXY(const FVector& WorldLocation) const;

	/** Returns true when WorldLocation can be projected directly or by clamping. */
	bool CanProjectWorldLocation(const FVector& WorldLocation) const;

	/**
	 * Projects a world-space location into normalized Panoramic map UV.
	 *
	 * @param WorldLocation Location to project.
	 * @param OutMapUV Projected UV in [0, 1] when successful.
	 * @param OutResult Detailed projection result.
	 * @return True when projection produced a usable UV.
	 */
	bool ProjectWorldToMapUVChecked(const FVector& WorldLocation, FVector2D& OutMapUV,
	                                EOBMapProjectionResult& OutResult) const;
};

namespace OBNavigation::MapView
{
	/**
	 * Calculates the aspect-preserving viewport for a map layer inside a canvas.
	 *
	 * @param CanvasSize Raw canvas size in Slate units.
	 * @param LayerSpec Layer metadata used to derive the map aspect ratio.
	 * @return Centered fitted viewport for map rendering and projection.
	 */
	OBNAVIGATION_API FOBNavigationMapViewport CalculateMapViewport(const FVector2D& CanvasSize,
	                                                               const FOBNavigationMapLayerSpec& LayerSpec);
}

/** Runtime marker registration and projection state. */
USTRUCT(BlueprintType)
struct OBNAVIGATION_API FOBNavigationMarkerSpec
{
	GENERATED_BODY()

	/** Stable marker ID. Invalid IDs create new markers when registered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FGuid MarkerId;

	/** Marker type tag used to resolve a config from the registry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FGameplayTag MarkerType;

	/** Logical marker layer used by tactical filters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FName LayerName = NAME_None;

	/** Actor followed by this marker; null means the marker uses WorldLocation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	TObjectPtr<AActor> TrackedActor = nullptr;

	/** Static world location used when TrackedActor is null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FVector WorldLocation = FVector::ZeroVector;

	/** World rotation used by directional marker indicators. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	FRotator WorldRotation = FRotator::ZeroRotator;

	/** Optional explicit visual config overriding MarkerType lookup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	TObjectPtr<UOBMarkerConfigAsset> ConfigAsset = nullptr;

	/** Marker lifetime in seconds; 0 means persistent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	float LifeTime = 0.0f;

	/** Owning player ID used by visibility policies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	int32 OwnerPlayerId = INDEX_NONE;

	/** Owning team ID used by squad visibility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	int32 TeamId = INDEX_NONE;

	/** Visibility policy applied before surface-specific marker config checks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	EOBMarkerVisibilityPolicy VisibilityPolicy = EOBMarkerVisibilityPolicy::Public;

	/** Draw priority; higher values are rendered above lower values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OBNavigation")
	int32 SortPriority = 0;
};
