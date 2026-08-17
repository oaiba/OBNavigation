#include "Widget/OBMapWidgetBase.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Data/OBMapMarker.h"
#include "Data/OBMinimapConfigAsset.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MinimapDefinitionDataAsset.h"
#include "OBNavigation.h"
#include "OBNavigationSubsystem.h"
#include "Widget/OBMapOverlayWidget.h"

namespace
{
FVector2D GetSafeMapWidgetViewUVScale(const FVector2D& ViewUVScale)
{
	return FVector2D(
		FMath::Max(FMath::Abs(ViewUVScale.X), KINDA_SMALL_NUMBER),
		FMath::Max(FMath::Abs(ViewUVScale.Y), KINDA_SMALL_NUMBER));
}

FVector2D DivideByMapWidgetViewUVScale(const FVector2D& Value, const FVector2D& ViewUVScale)
{
	const FVector2D SafeScale = GetSafeMapWidgetViewUVScale(ViewUVScale);
	return FVector2D(Value.X / SafeScale.X, Value.Y / SafeScale.Y);
}
}

FOBMapTileRuntimeStats UOBMapWidgetBase::GetTileRuntimeStats() const
{
	return TileManager ? TileManager->GetRuntimeStats() : FOBMapTileRuntimeStats();
}

void UOBMapWidgetBase::InitializeMapWidget(UOBMinimapConfigAsset* InVisualConfigAsset)
{
	if (bIsInitializedAndTracking)
	{
		UE_LOG(LogOBNavigation, Warning, TEXT("[%s::%hs] - Widget is already initialized."), *GetName(), __FUNCTION__);
		return;
	}

	if (!InVisualConfigAsset)
	{
		UE_LOG(LogOBNavigation, Error, TEXT("[%s::%hs] - Initialization failed: invalid visual config."), *GetName(),
		       __FUNCTION__);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	VisualConfigAsset = InVisualConfigAsset;
	CurrentZoom = GetInitialZoom();

		if (MapImage && VisualConfigAsset->MinimapBackgroundMaterial)
		{
			MapMaterialInstance = UMaterialInstanceDynamic::Create(VisualConfigAsset->MinimapBackgroundMaterial, this);
			MapImage->SetBrushFromMaterial(MapMaterialInstance);
			if (VisualConfigAsset->bShowDebugMessages)
			{
				UE_LOG(LogOBNavigation, Log,
				       TEXT("[%s::%hs] - Created map material instance from '%s'. MapImage='%s' MarkerCanvas='%s' InitialZoom=%.2f"),
				       *GetName(), __FUNCTION__, *GetNameSafe(VisualConfigAsset->MinimapBackgroundMaterial),
				       *GetNameSafe(MapImage), *GetNameSafe(GetMarkerCanvas()), CurrentZoom);
			}
		}
		else
		{
			UE_LOG(LogOBNavigation, Error, TEXT("[%s::%hs] - Failed to set up map image material."), *GetName(),
			       __FUNCTION__);
	}

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		NavSubsystem = GameInstance->GetSubsystem<UOBNavigationSubsystem>();
		if (NavSubsystem)
		{
			if (!NavSubsystem->OnNavigationMapLayerSpecChanged.IsAlreadyBound(this, &UOBMapWidgetBase::OnNavigationMapLayerSpecChanged))
			{
				NavSubsystem->OnNavigationMapLayerSpecChanged.AddDynamic(
					this, &UOBMapWidgetBase::OnNavigationMapLayerSpecChanged);
			}

			FOBNavigationMapLayerSpec CurrentLayerSpec;
			if (ResolveActiveLayer(CurrentLayerSpec))
			{
				ApplyMapLayer(CurrentLayerSpec);
			}
			else
			{
				ApplyMapLayer(FOBNavigationMapLayerSpec());
			}

				if (APawn* TrackedPawn = NavSubsystem->GetTrackedPlayerPawn())
				{
					PlayerMarkerID = NavSubsystem->GetMarkerIDForActor(TrackedPawn);
					if (VisualConfigAsset->bShowDebugMessages)
					{
						UE_LOG(LogOBNavigation, Log,
						       TEXT("[%s::%hs] - Initial tracked pawn='%s' PlayerMarkerID=%s"),
						       *GetName(), __FUNCTION__, *GetNameSafe(TrackedPawn), *PlayerMarkerID.ToString());
					}
				}
			}
		}

	SetMapZoom(CurrentZoom);
	EnsureOverlayWidget();

	if (!MapMaterialInstance || !NavSubsystem || !GetMarkerCanvas())
	{
		UE_LOG(LogOBNavigation, Error,
		       TEXT("[%s::%hs] - Initialization failed due to missing subsystem, material, or marker canvas."),
		       *GetName(), __FUNCTION__);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	bIsInitializedAndTracking = true;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UOBMapWidgetBase::SetMapZoom(const float NewZoom)
{
	const float MinimumZoom = FMath::Min(GetMinimumZoom(), GetMaximumZoom());
	const float MaximumZoom = FMath::Max(GetMinimumZoom(), GetMaximumZoom());
	CurrentZoom = FMath::Clamp(NewZoom, MinimumZoom, MaximumZoom);
	if (MapMaterialInstance)
	{
		MapMaterialInstance->SetScalarParameterValue("Zoom", CurrentZoom);
	}
}

UCanvasPanel* UOBMapWidgetBase::GetMarkerCanvas() const
{
	return MapMarkerCanvas.Get();
}

void UOBMapWidgetBase::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsInitializedAndTracking || !VisualConfigAsset || !NavSubsystem || !ShouldUpdateMapThisFrame())
	{
		return;
	}

	FOBNavigationMapLayerSpec CurrentLayer;
	if (!ResolveActiveLayer(CurrentLayer))
	{
		ClearMarkerWidgets();
		if (OverlayWidget)
		{
			OverlayWidget->ClearOverlayContext();
		}
		ApplyMapLayer(FOBNavigationMapLayerSpec());
		return;
	}

	ApplyMapLayer(CurrentLayer);

	const APawn* TrackedPawn = NavSubsystem->GetTrackedPlayerPawn();
	if (TrackedPawn && !PlayerMarkerID.IsValid())
	{
		PlayerMarkerID = NavSubsystem->GetMarkerIDForActor(const_cast<APawn*>(TrackedPawn));
	}

	FVector2D ViewCenterUV;
	if (!ResolveViewCenterUV(CurrentLayer, TrackedPawn, ViewCenterUV))
	{
		ClearMarkerWidgets();
		if (OverlayWidget)
		{
			OverlayWidget->ClearOverlayContext();
		}
		return;
	}

	const FOBNavigationMapViewContext ViewContext = BuildViewContext(CurrentLayer, TrackedPawn, ViewCenterUV);
	UpdateMapMaterial(ViewContext);
	UpdateMapImageViewport(CurrentLayer);
	UpdateMapTiles(CurrentLayer, ViewContext);
	OnViewContextUpdated(ViewContext, CurrentLayer, TrackedPawn);
	UpdateMapOverlays(CurrentLayer, ViewContext);

	TSet<FGuid> HandledMarkerIDs;
	if (GetMarkerCanvas())
	{
		UpdateMapMarkers(TrackedPawn, CurrentLayer, ViewContext, HandledMarkerIDs);
	}

	TArray<FGuid> MarkersToRemove;
	for (const auto& Pair : ActiveMapMarkerWidgets)
	{
		if (!HandledMarkerIDs.Contains(Pair.Key))
		{
			MarkersToRemove.Add(Pair.Key);
		}
	}

	for (const FGuid& MarkerID : MarkersToRemove)
	{
		if (UOBMapMarkerWidget* WidgetToRemove = ActiveMapMarkerWidgets.FindRef(MarkerID))
		{
			WidgetToRemove->RemoveFromParent();
		}
		ActiveMapMarkerWidgets.Remove(MarkerID);
	}

	if (GEngine && VisualConfigAsset->bShowDebugMessages && TrackedPawn)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, TEXT("--- NAV MAP DEBUG ---"));
		GEngine->AddOnScreenDebugMessage(
			-1, 0.0f, FColor::White,
			FString::Printf(TEXT("Navigation World Yaw: %.2f"),
			                OBNavigation::ResolveActorNavigationRotation(TrackedPawn).Yaw));
		GEngine->AddOnScreenDebugMessage(
			-1, 0.0f, FColor::Yellow,
			FString::Printf(TEXT("Map View Center UV: %s"), *ViewContext.ViewCenterUV.ToString()));
		GEngine->AddOnScreenDebugMessage(
			-1, 0.0f, FColor::Yellow,
			FString::Printf(TEXT("Map Static Rotation: %.2f deg"), ViewContext.TotalStaticRotation));
		if (const UCanvasPanel* MarkerCanvas = GetMarkerCanvas())
		{
			const FOBNavigationMapViewport DebugViewport = OBNavigation::MapView::CalculateMapViewport(
				MarkerCanvas->GetCachedGeometry().GetLocalSize(),
				CurrentLayer,
				GetNavigationSurface());
			GEngine->AddOnScreenDebugMessage(
				-1, 0.0f, FColor::Yellow,
				FString::Printf(TEXT("Map Viewport: Origin=%s Size=%s"),
					*DebugViewport.Origin.ToString(), *DebugViewport.Size.ToString()));
		}
	}
}

void UOBMapWidgetBase::NativeDestruct()
{
	if (NavSubsystem)
	{
		NavSubsystem->OnNavigationMapLayerSpecChanged.RemoveDynamic(
			this, &UOBMapWidgetBase::OnNavigationMapLayerSpecChanged);
	}

	ClearMarkerWidgets();
	ClearTileWidgets();
	if (OverlayWidget)
	{
		OverlayWidget->RemoveFromParent();
		OverlayWidget = nullptr;
	}

	Super::NativeDestruct();
}

void UOBMapWidgetBase::OnNavigationMapLayerSpecChanged(const FOBNavigationMapLayerSpec NewLayerSpec)
{
	FOBNavigationMapLayerSpec ActiveLayerSpec;
	ApplyMapLayer(ResolveActiveLayer(ActiveLayerSpec) ? ActiveLayerSpec : NewLayerSpec);
}

bool UOBMapWidgetBase::ResolveActiveLayer(FOBNavigationMapLayerSpec& OutLayerSpec) const
{
	return NavSubsystem && NavSubsystem->GetCurrentMapLayerSpec(OutLayerSpec);
}

float UOBMapWidgetBase::GetInitialZoom() const
{
	return VisualConfigAsset ? VisualConfigAsset->Zoom : 1.0f;
}

float UOBMapWidgetBase::GetMinimumZoom() const
{
	return VisualConfigAsset ? VisualConfigAsset->MinZoom : 0.1f;
}

float UOBMapWidgetBase::GetMaximumZoom() const
{
	return VisualConfigAsset ? VisualConfigAsset->MaxZoom : 100.0f;
}

bool UOBMapWidgetBase::ShouldUpdateMapThisFrame() const
{
	const ESlateVisibility CurrentVisibility = GetVisibility();
	return CurrentVisibility != ESlateVisibility::Collapsed && CurrentVisibility != ESlateVisibility::Hidden;
}

bool UOBMapWidgetBase::ShouldRotateMap() const
{
	return false;
}

EOBMapViewportClampShape UOBMapWidgetBase::GetViewportClampShape() const
{
	return EOBMapViewportClampShape::Rect;
}

bool UOBMapWidgetBase::ShouldCenterPlayerMarker() const
{
	return false;
}

bool UOBMapWidgetBase::ShouldShowPlayerMarker() const
{
	return true;
}

bool UOBMapWidgetBase::ShouldShowMarker(const UOBMapMarker* Marker) const
{
	return Marker != nullptr;
}

float UOBMapWidgetBase::GetMarkerScale() const
{
	return 1.0f;
}

FName UOBMapWidgetBase::GetOverlayCategoryFilter() const
{
	return NAME_None;
}

FName UOBMapWidgetBase::GetOverlayTagFilter() const
{
	return NAME_None;
}

bool UOBMapWidgetBase::ResolveViewCenterUV(const FOBNavigationMapLayerSpec& CurrentLayer, const APawn* TrackedPawn,
                                           FVector2D& OutViewCenterUV) const
{
	if (!NavSubsystem || !TrackedPawn)
	{
		return false;
	}

	EOBMapProjectionResult ProjectionResult = EOBMapProjectionResult::NoLayer;
	const FVector PawnLocation = OBNavigation::ResolveActorNavigationLocation(TrackedPawn);
	return NavSubsystem->WorldToMapUVChecked(CurrentLayer, PawnLocation, OutViewCenterUV, ProjectionResult);
}

FOBNavigationMapViewContext UOBMapWidgetBase::BuildViewContext(const FOBNavigationMapLayerSpec& CurrentLayer,
                                                               const APawn* TrackedPawn,
                                                               const FVector2D& ViewCenterUV) const
{
	FOBNavigationMapViewContext ViewContext;
	ViewContext.ViewCenterUV = ViewCenterUV;
	ViewContext.Zoom = CurrentZoom;
	if (GetNavigationSurface() == EOBNavigationSurface::Minimap)
	{
		const FVector2D ProjectionWorldSize = CurrentLayer.GetProjectionWorldSize();
		if (ProjectionWorldSize.X > 0.0f && ProjectionWorldSize.Y > 0.0f)
		{
			const float ShortWorldAxis = FMath::Min(ProjectionWorldSize.X, ProjectionWorldSize.Y);
			ViewContext.ViewUVScale = FVector2D(
				ShortWorldAxis / ProjectionWorldSize.X,
				ShortWorldAxis / ProjectionWorldSize.Y);
		}
	}
	ViewContext.TotalStaticRotation = GetTotalStaticRotation();
	ViewContext.DynamicMapYaw = GetDynamicMapYaw(TrackedPawn);
	ViewContext.bShouldRotateMap = ShouldRotateMap();
	ViewContext.bClampToCanvas = true;
	ViewContext.ClampShape = GetViewportClampShape();
	ViewContext.Surface = GetNavigationSurface();
	return ViewContext;
}

void UOBMapWidgetBase::OnViewContextUpdated(const FOBNavigationMapViewContext& ViewContext,
                                            const FOBNavigationMapLayerSpec& CurrentLayer,
                                            const APawn* TrackedPawn)
{
}

float UOBMapWidgetBase::GetAlignmentAngle() const
{
	if (!VisualConfigAsset)
	{
		return 0.0f;
	}

	switch (VisualConfigAsset->MapAlignment)
	{
	case EMapAlignment::Forward_PlusX:
		return 0.0f;
	case EMapAlignment::Right_PlusY:
		return 90.0f;
	case EMapAlignment::Backward_MinusX:
		return 180.0f;
	case EMapAlignment::Left_MinusY:
		return -90.0f;
	default:
		return 0.0f;
	}
}

float UOBMapWidgetBase::GetDynamicMapYaw(const APawn* TrackedPawn) const
{
	if (!TrackedPawn)
	{
		return 0.0f;
	}

	if (VisualConfigAsset && VisualConfigAsset->RotationSource == EMinimapRotationSource::ControlRotation)
	{
		return TrackedPawn->GetControlRotation().Yaw;
	}
	return OBNavigation::ResolveActorNavigationRotation(TrackedPawn).Yaw;
}

float UOBMapWidgetBase::GetTotalStaticRotation() const
{
	const float Alignment = GetAlignmentAngle();
	const float Offset = (VisualConfigAsset) ? VisualConfigAsset->MapRotationOffset : 0.0f;
	return Alignment + Offset;
}

void UOBMapWidgetBase::UpdateMapMaterial(const FOBNavigationMapViewContext& ViewContext)
{
	if (!MapMaterialInstance)
	{
		return;
	}

	// View‑center UV – drives the map scrolling.
	const FLinearColor ViewCenterUVColor(ViewContext.ViewCenterUV.X, ViewContext.ViewCenterUV.Y, 0.0f, 0.0f);
	MapMaterialInstance->SetVectorParameterValue(TEXT("ViewCenterUV"), ViewCenterUVColor);

	// Dynamic player yaw (radians).
	MapMaterialInstance->SetScalarParameterValue(TEXT("PlayerYawRad"), FMath::DegreesToRadians(ViewContext.DynamicMapYaw));

	// Zoom multiplier.
	MapMaterialInstance->SetScalarParameterValue(TEXT("ZoomAmount"), CurrentZoom);

	const FLinearColor ViewUVScaleColor(ViewContext.ViewUVScale.X, ViewContext.ViewUVScale.Y, 0.0f, 0.0f);
	MapMaterialInstance->SetVectorParameterValue(TEXT("ViewUVScale"), ViewUVScaleColor);

	// Static rotation offset (alignment + custom offset) in radians.
	MapMaterialInstance->SetScalarParameterValue(TEXT("MapRotationOffsetRad"), FMath::DegreesToRadians(ViewContext.TotalStaticRotation));
}

void UOBMapWidgetBase::UpdateMapImageViewport(const FOBNavigationMapLayerSpec& CurrentLayer)
{
	if (!MapImage)
	{
		return;
	}

	UCanvasPanel* MarkerCanvas = GetMarkerCanvas();
	if (!MarkerCanvas)
	{
		return;
	}

	const FVector2D CanvasSize = MarkerCanvas->GetCachedGeometry().GetLocalSize();
	const FOBNavigationMapViewport MapViewport = OBNavigation::MapView::CalculateMapViewport(
		CanvasSize,
		CurrentLayer,
		GetNavigationSurface());
	if (!MapViewport.IsValid())
	{
		return;
	}

	if (UCanvasPanelSlot* MapImageSlot = Cast<UCanvasPanelSlot>(MapImage->Slot))
	{
		MapImageSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		MapImageSlot->SetAlignment(FVector2D::ZeroVector);
		MapImageSlot->SetPosition(MapViewport.Origin);
		MapImageSlot->SetSize(MapViewport.Size);
	}
	else if (!bWarnedMapImageNonCanvasSlot && VisualConfigAsset && VisualConfigAsset->bShowDebugMessages)
	{
		bWarnedMapImageNonCanvasSlot = true;
		UE_LOG(LogOBNavigation, Warning,
		       TEXT("[%s::%hs] - MapImage is not in a CanvasPanelSlot; single-texture fallback may stretch. Put MapImage and MapMarkerCanvas in matching CanvasPanel space for aspect-correct rendering."),
		       *GetName(), __FUNCTION__);
	}
}

void UOBMapWidgetBase::UpdateMapTiles(const FOBNavigationMapLayerSpec& CurrentLayer,
                                      const FOBNavigationMapViewContext& ViewContext)
{
	if (!bAppliedTiledLayer || !TileManager)
	{
		return;
	}

	UCanvasPanel* TileCanvas = EnsureTileLayerCanvas();
	if (!TileCanvas)
	{
		return;
	}

	const FVector2D CanvasSize = TileCanvas->GetCachedGeometry().GetLocalSize();
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return;
	}

	const FOBNavigationMapViewport MapViewport = OBNavigation::MapView::CalculateMapViewport(
		CanvasSize,
		CurrentLayer,
		GetNavigationSurface());
	if (!MapViewport.IsValid())
	{
		return;
	}

	TileManager->UpdateActiveTiles(ViewContext, MapViewport.Size, GetNavigationSurface(), GetMinimapMaxLODTileLimit());
	const FOBMapTileRuntimeStats TileStats = TileManager->GetRuntimeStats();
	if (TileStats.State != LastLoggedTileManagerState)
	{
		LastLoggedTileManagerState = TileStats.State;
		if (VisualConfigAsset && VisualConfigAsset->bShowDebugMessages)
		{
			if (TileStats.State == EOBMapTileManagerState::Ready)
			{
				UE_LOG(LogOBNavigation, Log,
				       TEXT("[%s::%hs] - Tile manager ready. Layer='%s' RunId='%s' SourceMap='%s' Definition='%s' TileSet='%s' MaxLOD=%d"),
				       *GetName(), __FUNCTION__, *CurrentLayer.LayerName.ToString(), *TileStats.CaptureRunId,
				       *TileStats.SourceMapName, *TileStats.DefinitionPath, *TileStats.TileSetPath, TileStats.MaxLOD);
				if (const UMinimapTileSetDataAsset* TileSet = TileManager->GetTileSet())
				{
					for (const FMinimapTilePyramidLevel& Level : TileSet->PyramidLevels)
					{
						UE_LOG(LogOBNavigation, Log,
						       TEXT("[%s::%hs] - TileSet level dump. LOD=%d Grid=%dx%d TilePixels=%dx%d LogicalPixels=%dx%d TileCount=%d"),
						       *GetName(), __FUNCTION__, Level.LOD, Level.GridDimensions.X, Level.GridDimensions.Y,
						       Level.TilePixelSize.X, Level.TilePixelSize.Y, Level.LogicalPixelSize.X,
						       Level.LogicalPixelSize.Y, Level.Tiles.Num());

						const int32 PreviewTileCount = FMath::Min(4, Level.Tiles.Num());
						for (int32 TileIndex = 0; TileIndex < PreviewTileCount; ++TileIndex)
						{
							const FMinimapTileRef& TileRef = Level.Tiles[TileIndex];
							UE_LOG(LogOBNavigation, Log,
							       TEXT("[%s::%hs] - TileSet sample. LOD=%d X=%d Y=%d UVMin=%s UVMax=%s WorldBoundsMin=%s WorldBoundsMax=%s Texture='%s'"),
							       *GetName(), __FUNCTION__, TileRef.Coord.LOD, TileRef.Coord.X, TileRef.Coord.Y,
							       *TileRef.UVMin.ToString(), *TileRef.UVMax.ToString(),
							       *TileRef.WorldBounds.Min.ToString(), *TileRef.WorldBounds.Max.ToString(),
							       *TileRef.Texture.ToSoftObjectPath().ToString());
						}
					}
				}
			}
			else if (TileStats.State == EOBMapTileManagerState::Failed)
			{
				UE_LOG(LogOBNavigation, Warning,
				       TEXT("[%s::%hs] - Tile manager failed. Layer='%s' Definition='%s' Reason='%s'"),
				       *GetName(), __FUNCTION__, *CurrentLayer.LayerName.ToString(), *TileStats.DefinitionPath,
				       *TileStats.FailureReason);
			}
		}
	}

	TSet<FString> ActiveTileKeys;
	const bool bShowTileDebug = VisualConfigAsset && VisualConfigAsset->bShowDebugMessages;
	const bool bLogTacticalTilePlacement = bShowTileDebug
		&& GetNavigationSurface() == EOBNavigationSurface::FullMap
		&& (TileStats.ActiveLOD != LastLoggedTacticalTileLOD
			|| !MapViewport.Origin.Equals(LastLoggedTacticalViewportOrigin, 0.5f)
			|| !MapViewport.Size.Equals(LastLoggedTacticalViewportSize, 0.5f));
	const FOBNavigationMapViewContext TileViewContext = [&ViewContext]()
	{
		FOBNavigationMapViewContext Result = ViewContext;
		Result.bClampToCanvas = false;
		return Result;
	}();

	for (const FOBActiveMapTile& ActiveTile : TileManager->GetActiveTiles())
	{
		if (!ActiveTile.Texture)
		{
			continue;
		}

		const FString TileKey = FString::Printf(TEXT("%d:%d:%d"),
		                                        ActiveTile.Coord.LOD, ActiveTile.Coord.X, ActiveTile.Coord.Y);
		ActiveTileKeys.Add(TileKey);

		UImage* TileImage = ActiveTileImages.FindRef(TileKey);
		if (!TileImage)
		{
			TileImage = NewObject<UImage>(this);
			if (!TileImage)
			{
				continue;
			}

			TileImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UCanvasPanelSlot* NewSlot = Cast<UCanvasPanelSlot>(TileCanvas->AddChild(TileImage)))
			{
				NewSlot->SetAlignment(FVector2D::ZeroVector);
				NewSlot->SetZOrder(0);
			}
			TileImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			ActiveTileImages.Add(TileKey, TileImage);
		}

		const FVector2D TileUVMin(
			FMath::Min(ActiveTile.UVMin.X, ActiveTile.UVMax.X),
			FMath::Min(ActiveTile.UVMin.Y, ActiveTile.UVMax.Y));
		const FVector2D TileUVMax(
			FMath::Max(ActiveTile.UVMin.X, ActiveTile.UVMax.X),
			FMath::Max(ActiveTile.UVMin.Y, ActiveTile.UVMax.Y));
		const FVector2D TileUVExtent = TileUVMax - TileUVMin;
		if (TileUVExtent.X <= 0.0f || TileUVExtent.Y <= 0.0f)
		{
			TileImage->SetVisibility(ESlateVisibility::Collapsed);
			UE_LOG(LogOBNavigation, Warning,
			       TEXT("[%s::%hs] - Skipping tile with invalid UV extent. Tile=%s UVMin=%s UVMax=%s Texture='%s'"),
			       *GetName(), __FUNCTION__, *TileKey, *ActiveTile.UVMin.ToString(), *ActiveTile.UVMax.ToString(),
			       *ActiveTile.TextureRef.ToSoftObjectPath().ToString());
			continue;
		}

		const FVector2D TileCenterUV = (TileUVMin + TileUVMax) * 0.5f;
		FOBNavigationCanvasProjection Projection;
		if (!OBNavigation::MapView::ProjectUVToCanvas(TileCenterUV, MapViewport, TileViewContext, Projection))
		{
			TileImage->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		const float SafeZoom = FMath::Max(ViewContext.Zoom, KINDA_SMALL_NUMBER);
		const FVector2D ScaledTileUVExtent = DivideByMapWidgetViewUVScale(TileUVExtent, ViewContext.ViewUVScale);
		const FVector2D TileSize(
			ScaledTileUVExtent.X * MapViewport.Size.X * SafeZoom,
			ScaledTileUVExtent.Y * MapViewport.Size.Y * SafeZoom);
		const FVector2D TileTopLeft = Projection.CanvasPosition - TileSize * 0.5f;

		TileImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		TileImage->SetRenderTransformAngle(ViewContext.GetAppliedRotationDegrees());

		const bool bUseTileMaskMaterial = ShouldMaskTiledMapTiles() && GetTiledMapTileMaterial();
		if (bUseTileMaskMaterial)
		{
			UMaterialInstanceDynamic* TileMID = ActiveTileMaterialInstances.FindRef(TileKey);
			if (!TileMID)
			{
				TileMID = UMaterialInstanceDynamic::Create(GetTiledMapTileMaterial(), this);
				ActiveTileMaterialInstances.Add(TileKey, TileMID);
			}

			if (TileMID)
			{
				const FVector2D TileScreenMin = TileTopLeft / CanvasSize;
				const FVector2D TileScreenMax = (TileTopLeft + TileSize) / CanvasSize;
				TileMID->SetTextureParameterValue(TEXT("TileTexture"), ActiveTile.Texture);
				TileMID->SetVectorParameterValue(TEXT("TileScreenMin"), FLinearColor(TileScreenMin.X, TileScreenMin.Y, 0.0f, 0.0f));
				TileMID->SetVectorParameterValue(TEXT("TileScreenMax"), FLinearColor(TileScreenMax.X, TileScreenMax.Y, 0.0f, 0.0f));
				TileMID->SetScalarParameterValue(TEXT("ShapeAlpha"), 1.0f);
				TileImage->SetBrushFromMaterial(TileMID);
			}
		}
		else
		{
			ActiveTileMaterialInstances.Remove(TileKey);
			TileImage->SetBrushFromTexture(ActiveTile.Texture, false);
			if (ShouldMaskTiledMapTiles() && !bWarnedMissingTiledMapTileMaterial && VisualConfigAsset && VisualConfigAsset->bShowDebugMessages)
			{
				bWarnedMissingTiledMapTileMaterial = true;
				UE_LOG(LogOBNavigation, Warning,
				       TEXT("[%s::%hs] - Circle minimap tiled layer is using direct textures because TiledMapTileMaterial is not configured."),
				       *GetName(), __FUNCTION__);
			}
		}

		if (UCanvasPanelSlot* TileSlot = Cast<UCanvasPanelSlot>(TileImage->Slot))
		{
			TileSlot->SetAlignment(FVector2D::ZeroVector);
			TileSlot->SetPosition(TileTopLeft);
			TileSlot->SetSize(TileSize);
			TileSlot->SetZOrder(0);
		}

		if (bShowTileDebug)
		{
			UTextBlock* TileLabel = ActiveTileCoordinateLabels.FindRef(TileKey);
			if (!TileLabel)
			{
				TileLabel = NewObject<UTextBlock>(this);
				if (TileLabel)
				{
					TileLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
					TileLabel->SetColorAndOpacity(FSlateColor(FLinearColor::Yellow));
					TileLabel->SetShadowColorAndOpacity(FLinearColor::Black);
					TileLabel->SetShadowOffset(FVector2D(1.0f, 1.0f));
					FSlateFontInfo LabelFont = TileLabel->GetFont();
					LabelFont.Size = 12;
					TileLabel->SetFont(LabelFont);
					if (UCanvasPanelSlot* NewLabelSlot = Cast<UCanvasPanelSlot>(TileCanvas->AddChild(TileLabel)))
					{
						NewLabelSlot->SetAlignment(FVector2D::ZeroVector);
						NewLabelSlot->SetZOrder(5);
					}
					ActiveTileCoordinateLabels.Add(TileKey, TileLabel);
				}
			}

			if (TileLabel)
			{
				TileLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
				TileLabel->SetText(FText::Format(
					NSLOCTEXT("OBNavigation", "MapTileCoordinate", "L{0} X{1} Y{2}"),
					FText::AsNumber(ActiveTile.Coord.LOD), FText::AsNumber(ActiveTile.Coord.X),
					FText::AsNumber(ActiveTile.Coord.Y)));
				if (UCanvasPanelSlot* LabelSlot = Cast<UCanvasPanelSlot>(TileLabel->Slot))
				{
					LabelSlot->SetPosition(TileTopLeft + FVector2D(4.0f, 4.0f));
					LabelSlot->SetSize(FVector2D(160.0f, 24.0f));
					LabelSlot->SetZOrder(5);
				}
			}
		}
		else if (UTextBlock* TileLabel = ActiveTileCoordinateLabels.FindRef(TileKey))
		{
			TileLabel->RemoveFromParent();
			ActiveTileCoordinateLabels.Remove(TileKey);
		}

		UE_LOG(LogOBNavigation, VeryVerbose,
		       TEXT("[%s::%hs] - Tile placement. Tile=%s Coord=(LOD=%d X=%d Y=%d) UVMin=%s UVMax=%s CenterUV=%s CanvasCenter=%s TopLeft=%s Size=%s Rotation=%.2f Texture='%s'"),
		       *GetName(), __FUNCTION__, *TileKey, ActiveTile.Coord.LOD, ActiveTile.Coord.X, ActiveTile.Coord.Y,
		       *ActiveTile.UVMin.ToString(), *ActiveTile.UVMax.ToString(), *TileCenterUV.ToString(),
		       *Projection.CanvasPosition.ToString(), *TileTopLeft.ToString(), *TileSize.ToString(),
		       ViewContext.GetAppliedRotationDegrees(), *ActiveTile.TextureRef.ToSoftObjectPath().ToString());

		if (bLogTacticalTilePlacement)
		{
			UE_LOG(LogOBNavigation, Log,
			       TEXT("[%s::%hs] - Tactical active tile. RawCanvas=%s ViewportOrigin=%s ViewportSize=%s LayerOutput=%dx%d Aspect=%.3f LOD=%d X=%d Y=%d UVMin=%s UVMax=%s TopLeft=%s Size=%s Texture='%s'"),
			       *GetName(), __FUNCTION__, *CanvasSize.ToString(), *MapViewport.Origin.ToString(),
			       *MapViewport.Size.ToString(), CurrentLayer.OutputSize.X, CurrentLayer.OutputSize.Y,
			       MapViewport.AspectRatio, ActiveTile.Coord.LOD, ActiveTile.Coord.X, ActiveTile.Coord.Y,
			       *ActiveTile.UVMin.ToString(), *ActiveTile.UVMax.ToString(), *TileTopLeft.ToString(),
			       *TileSize.ToString(), *ActiveTile.TextureRef.ToSoftObjectPath().ToString());
		}
	}

	if (bLogTacticalTilePlacement)
	{
		LastLoggedTacticalTileLOD = TileStats.ActiveLOD;
		LastLoggedTacticalViewportOrigin = MapViewport.Origin;
		LastLoggedTacticalViewportSize = MapViewport.Size;
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1, 0.0f, FColor::Orange,
				FString::Printf(TEXT("Viewport: %.0fx%.0f in %.0fx%.0f"),
					MapViewport.Size.X, MapViewport.Size.Y, CanvasSize.X, CanvasSize.Y));
		}
	}

	TArray<FString> TilesToRemove;
	for (const TPair<FString, TObjectPtr<UImage>>& Pair : ActiveTileImages)
	{
		if (!ActiveTileKeys.Contains(Pair.Key))
		{
			TilesToRemove.Add(Pair.Key);
		}
	}

	for (const FString& TileKey : TilesToRemove)
	{
		if (UImage* TileImage = ActiveTileImages.FindRef(TileKey))
		{
			TileImage->RemoveFromParent();
		}
		ActiveTileImages.Remove(TileKey);
		ActiveTileMaterialInstances.Remove(TileKey);
	}

	TArray<FString> TileLabelsToRemove;
	for (const TPair<FString, TObjectPtr<UTextBlock>>& Pair : ActiveTileCoordinateLabels)
	{
		if (!ActiveTileKeys.Contains(Pair.Key) || !bShowTileDebug)
		{
			TileLabelsToRemove.Add(Pair.Key);
		}
	}

	for (const FString& TileKey : TileLabelsToRemove)
	{
		if (UTextBlock* TileLabel = ActiveTileCoordinateLabels.FindRef(TileKey))
		{
			TileLabel->RemoveFromParent();
		}
		ActiveTileCoordinateLabels.Remove(TileKey);
	}

	const bool bHasLoadedActiveTiles = !TileManager->GetActiveTiles().IsEmpty()
		&& ActiveTileImages.Num() >= TileManager->GetActiveTiles().Num();
	const UMinimapDefinitionDataAsset* Definition = CurrentLayer.PanoramicDefinition.Get();
	if (MapImage && Definition && !Definition->BaseMapTexture.IsNull())
	{
		MapImage->SetVisibility(bHasLoadedActiveTiles ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (VisualConfigAsset && VisualConfigAsset->bShowDebugMessages)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1, 0.0f, FColor::Orange,
				FString::Printf(TEXT("Tiles: LOD %d/%d Active=%d Loaded=%d Cached=%d Run=%s"),
				                TileStats.ActiveLOD, TileStats.MaxLOD, TileStats.ActiveTileCount,
				                TileStats.LoadedTileCount, TileStats.CachedTileCount, *TileStats.CaptureRunId));
		}

		UE_LOG(LogOBNavigation, Verbose,
		       TEXT("[%s::%hs] - Tiled layer='%s' RunId='%s' SourceMap='%s' ActiveLOD=%d ActiveTiles=%d LoadedTiles=%d CachedTiles=%d"),
		       *GetName(), __FUNCTION__, *CurrentLayer.LayerName.ToString(),
		       *TileStats.CaptureRunId, *TileStats.SourceMapName, TileStats.ActiveLOD, TileStats.ActiveTileCount,
		       TileStats.LoadedTileCount, TileStats.CachedTileCount);
	}
}

int32 UOBMapWidgetBase::GetTileBudget() const
{
	return VisualConfigAsset ? FMath::Max(1, VisualConfigAsset->TiledMapTileBudget) : 25;
}

int32 UOBMapWidgetBase::GetMinimapMaxLODTileLimit() const
{
	return VisualConfigAsset ? FMath::Max(0, VisualConfigAsset->MinimapMaxLODTileLimit) : 12;
}

UMaterialInterface* UOBMapWidgetBase::GetTiledMapTileMaterial() const
{
	return VisualConfigAsset ? VisualConfigAsset->TiledMapTileMaterial : nullptr;
}

bool UOBMapWidgetBase::ShouldMaskTiledMapTiles() const
{
	return false;
}

void UOBMapWidgetBase::UpdateMapMarkers(const APawn* TrackedPawn, const FOBNavigationMapLayerSpec& CurrentLayer,
                                        const FOBNavigationMapViewContext& ViewContext,
                                        TSet<FGuid>& OutHandledMarkerIDs)
{
	UCanvasPanel* MarkerCanvas = GetMarkerCanvas();
	if (!MarkerWidgetClass || !MarkerCanvas || !NavSubsystem || !VisualConfigAsset)
	{
		return;
	}

	const FVector2D CanvasSize = MarkerCanvas->GetCachedGeometry().GetLocalSize();
	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return;
	}

	const FOBNavigationMapViewport MapViewport = OBNavigation::MapView::CalculateMapViewport(
		CanvasSize,
		CurrentLayer,
		GetNavigationSurface());
	if (!MapViewport.IsValid())
	{
		return;
	}

	const FVector2D CanvasCenter = MapViewport.GetCenter();
	const TArray<UOBMapMarker*> VisibleMarkers = NavSubsystem->GetVisibleMarkers(GetNavigationSurface());
	for (UOBMapMarker* Marker : VisibleMarkers)
	{
		if (!Marker || !Marker->ConfigAsset || !ShouldShowMarker(Marker))
		{
			continue;
		}

		const bool bIsPlayerMarker = PlayerMarkerID.IsValid() && Marker->MarkerID == PlayerMarkerID;
		if (bIsPlayerMarker && !ShouldShowPlayerMarker())
		{
			continue;
		}

		FVector2D FinalPosition = CanvasCenter;
		float IndicatorAngle = 0.0f;
		bool bIsClampedToEdge = false;

		if (bIsPlayerMarker && ShouldCenterPlayerMarker() && TrackedPawn)
		{
			float TargetYaw = OBNavigation::ResolveActorNavigationRotation(TrackedPawn).Yaw;
			if (VisualConfigAsset && VisualConfigAsset->RotationSource == EMinimapRotationSource::ControlRotation)
			{
				TargetYaw = TrackedPawn->GetControlRotation().Yaw;
			}
			IndicatorAngle = ShouldRotateMap() ? 0.0f : TargetYaw - ViewContext.TotalStaticRotation;
		}
		else
		{
			FVector2D MarkerUV;
			EOBMapProjectionResult MarkerProjectionResult = EOBMapProjectionResult::NoLayer;
			if (!NavSubsystem->WorldToMapUVChecked(CurrentLayer, Marker->WorldLocation, MarkerUV, MarkerProjectionResult))
			{
				continue;
			}

			FOBNavigationCanvasProjection CanvasProjection;
			if (!OBNavigation::MapView::ProjectUVToCanvas(MarkerUV, MapViewport, ViewContext, CanvasProjection))
			{
				continue;
			}

			FinalPosition = CanvasProjection.CanvasPosition;
			bIsClampedToEdge = CanvasProjection.bIsClampedToEdge;
			if (bIsClampedToEdge)
			{
				IndicatorAngle = FMath::RadiansToDegrees(
					FMath::Atan2(CanvasProjection.RotatedPixelOffset.Y, CanvasProjection.RotatedPixelOffset.X));
			}
			else
			{
				IndicatorAngle = ViewContext.bShouldRotateMap
					                 ? Marker->WorldRotation.Yaw - ViewContext.DynamicMapYaw
					                 : Marker->WorldRotation.Yaw - ViewContext.TotalStaticRotation;
			}
		}

		OutHandledMarkerIDs.Add(Marker->MarkerID);

		UOBMapMarkerWidget* MarkerWidget = ActiveMapMarkerWidgets.FindRef(Marker->MarkerID);
		if (!MarkerWidget)
		{
			MarkerWidget = CreateWidget<UOBMapMarkerWidget>(this, MarkerWidgetClass);
			if (!MarkerWidget)
			{
				continue;
			}

			if (UCanvasPanelSlot* NewSlot = Cast<UCanvasPanelSlot>(MarkerCanvas->AddChild(MarkerWidget)))
			{
				NewSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			}

			ActiveMapMarkerWidgets.Add(Marker->MarkerID, MarkerWidget);
			MarkerWidget->InitializeMarker(Marker->ConfigAsset->IdentifierIconTexture,
			                               Marker->ConfigAsset->IndicatorMaterial);
		}

			MarkerWidget->UpdateVisuals(IndicatorAngle, 90.0f, 1.0f);
			const float DistanceMeters = TrackedPawn
				                             ? FVector::Dist(OBNavigation::ResolveActorNavigationLocation(TrackedPawn),
				                                             Marker->WorldLocation) / 100.0f
				                             : 0.0f;
		MarkerWidget->UpdateDistance(DistanceMeters, bIsClampedToEdge);

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget->Slot))
		{
			const FVector2D MarkerSize = Marker->ConfigAsset->Size * GetMarkerScale();
			FVector2D SlotPosition = FinalPosition;

			// Offset non-centered markers so custom indicator pivots stay anchored after rotation.
			if (!(bIsPlayerMarker && ShouldCenterPlayerMarker()))
			{
				const FVector2D PivotOffset = (Marker->ConfigAsset->IndicatorPivot - FVector2D(0.5f, 0.5f)) * MarkerSize;
				const FVector2D RotatedPivotOffset = PivotOffset.GetRotated(IndicatorAngle);
				SlotPosition = FinalPosition - (RotatedPivotOffset - PivotOffset);
			}

			CanvasSlot->SetSize(MarkerSize);
			CanvasSlot->SetPosition(SlotPosition);
			CanvasSlot->SetZOrder(bIsPlayerMarker ? 10 : 1);
		}

		if (GEngine && VisualConfigAsset->bShowDebugMessages)
		{
			const FColor DebugColor = bIsPlayerMarker ? FColor::Magenta : FColor::Green;
			GEngine->AddOnScreenDebugMessage(
				-1, 0.0f, DebugColor,
				FString::Printf(TEXT("Marker [%s]: Final Pos: %s"),
				                *Marker->MarkerID.ToString().Left(8), *FinalPosition.ToString()));
		}
	}
}

void UOBMapWidgetBase::UpdateMapOverlays(const FOBNavigationMapLayerSpec& CurrentLayer,
                                         const FOBNavigationMapViewContext& ViewContext)
{
	if (!GetMarkerCanvas())
	{
		return;
	}

	EnsureOverlayWidget();
	if (!OverlayWidget)
	{
		return;
	}

	TArray<FOBNavigationOverlayElement> VisibleElements;
	const FName CategoryFilter = GetOverlayCategoryFilter();
	const FName TagFilter = GetOverlayTagFilter();
	for (const FOBNavigationOverlayLayer& Layer : CurrentLayer.OverlayLayers)
	{
		if (!Layer.bVisibleByDefault)
		{
			continue;
		}

		for (const FOBNavigationOverlayElement& Element : Layer.Elements)
		{
			if (!Element.bVisibleByDefault)
			{
				continue;
			}
			if (!CategoryFilter.IsNone() && Element.Category != CategoryFilter)
			{
				continue;
			}
			if (!TagFilter.IsNone() && !Element.FilterTags.Contains(TagFilter))
			{
				continue;
			}

			VisibleElements.Add(Element);
		}
	}

	OverlayWidget->SetOverlayContext(CurrentLayer, VisibleElements, ViewContext);
}

void UOBMapWidgetBase::EnsureOverlayWidget()
{
	UCanvasPanel* MarkerCanvas = GetMarkerCanvas();
	if (OverlayWidget || !MarkerCanvas)
	{
		return;
	}

	OverlayWidget = CreateWidget<UOBMapOverlayWidget>(this, UOBMapOverlayWidget::StaticClass());
	if (!OverlayWidget)
	{
		return;
	}

	OverlayWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* OverlaySlot = Cast<UCanvasPanelSlot>(MarkerCanvas->AddChild(OverlayWidget)))
	{
		OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		OverlaySlot->SetOffsets(FMargin(0.0f));
		OverlaySlot->SetAlignment(FVector2D::ZeroVector);
		OverlaySlot->SetZOrder(0);
	}
}

void UOBMapWidgetBase::ClearMarkerWidgets()
{
	for (const auto& Pair : ActiveMapMarkerWidgets)
	{
		if (Pair.Value)
		{
			Pair.Value->RemoveFromParent();
		}
	}
	ActiveMapMarkerWidgets.Reset();
}

void UOBMapWidgetBase::ClearTileWidgets()
{
	for (const TPair<FString, TObjectPtr<UImage>>& Pair : ActiveTileImages)
	{
		if (Pair.Value)
		{
			Pair.Value->RemoveFromParent();
		}
	}
	ActiveTileImages.Reset();
	ActiveTileMaterialInstances.Reset();

	for (const TPair<FString, TObjectPtr<UTextBlock>>& Pair : ActiveTileCoordinateLabels)
	{
		if (Pair.Value)
		{
			Pair.Value->RemoveFromParent();
		}
	}
	ActiveTileCoordinateLabels.Reset();

	if (TileLayerCanvas)
	{
		TileLayerCanvas->RemoveFromParent();
		TileLayerCanvas = nullptr;
	}

	if (TileManager)
	{
		TileManager->Shutdown();
	}

	LastLoggedTileManagerState = EOBMapTileManagerState::Uninitialized;
	LastLoggedTacticalTileLOD = INDEX_NONE;
	LastLoggedTacticalViewportOrigin = FVector2D(-1.0f, -1.0f);
	LastLoggedTacticalViewportSize = FVector2D(-1.0f, -1.0f);
	bWarnedMissingTiledMapTileMaterial = false;
	bWarnedMapImageNonCanvasSlot = false;
}

UCanvasPanel* UOBMapWidgetBase::EnsureTileLayerCanvas()
{
	if (TileLayerCanvas)
	{
		return TileLayerCanvas;
	}

	UCanvasPanel* MarkerCanvas = GetMarkerCanvas();
	if (!MarkerCanvas)
	{
		return nullptr;
	}

	TileLayerCanvas = NewObject<UCanvasPanel>(this);
	if (!TileLayerCanvas)
	{
		return nullptr;
	}

	TileLayerCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	TileLayerCanvas->SetClipping(EWidgetClipping::ClipToBounds);
	if (UCanvasPanelSlot* TileLayerSlot = Cast<UCanvasPanelSlot>(MarkerCanvas->AddChild(TileLayerCanvas)))
	{
		TileLayerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		TileLayerSlot->SetOffsets(FMargin(0.0f));
		TileLayerSlot->SetAlignment(FVector2D::ZeroVector);
		TileLayerSlot->SetZOrder(-30);
	}

	return TileLayerCanvas;
}

void UOBMapWidgetBase::ApplyMapLayer(const FOBNavigationMapLayerSpec& NewLayerSpec)
{
	if (!MapMaterialInstance || !MapImage)
	{
		return;
	}

	const UMinimapDefinitionDataAsset* Definition = NewLayerSpec.PanoramicDefinition.Get();
	if (!Definition && NewLayerSpec.HasPanoramicDefinition())
	{
		Definition = NewLayerSpec.PanoramicDefinition.LoadSynchronous();
	}

	const bool bUseTiledLayer = Definition && Definition->IsTiledDefinition();
	if (AppliedLayerName == NewLayerSpec.LayerName
		&& AppliedPanoramicDefinition == NewLayerSpec.PanoramicDefinition
		&& bAppliedTiledLayer == bUseTiledLayer)
	{
		return;
	}

	AppliedLayerName = NewLayerSpec.LayerName;
	AppliedPanoramicDefinition = NewLayerSpec.PanoramicDefinition;
	bAppliedTiledLayer = bUseTiledLayer;

	ClearTileWidgets();

	if (!Definition)
	{
		MapImage->SetVisibility(ESlateVisibility::Collapsed);
		if (VisualConfigAsset && VisualConfigAsset->bShowDebugMessages)
		{
			UE_LOG(LogOBNavigation, Warning,
			       TEXT("[%s::%hs] - Applied empty map layer. PanoramicDefinition is required. Layer='%s'"),
			       *GetName(), __FUNCTION__, *NewLayerSpec.LayerName.ToString());
		}
		return;
	}

	if (bUseTiledLayer)
	{
		if (!TileManager)
		{
			TileManager = NewObject<UOBMapTileManager>(this);
		}

		if (TileManager)
		{
			TileManager->Initialize(NewLayerSpec, GetTileBudget());
		}

		if (!Definition->BaseMapTexture.IsNull())
		{
			if (UTexture2D* BaseMapTexture = Definition->BaseMapTexture.LoadSynchronous())
			{
				MapMaterialInstance->SetTextureParameterValue("MapTexture", BaseMapTexture);
				MapImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				MapImage->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		else
		{
			MapImage->SetVisibility(ESlateVisibility::Collapsed);
		}

		EnsureTileLayerCanvas();
		if (VisualConfigAsset && VisualConfigAsset->bShowDebugMessages)
		{
			UE_LOG(LogOBNavigation, Log,
			       TEXT("[%s::%hs] - Applied tiled map layer. Layer='%s' Definition='%s' BoundsMin=%s BoundsMax=%s MapRotation=%.2f MaxLOD=%d"),
			       *GetName(), __FUNCTION__, *NewLayerSpec.LayerName.ToString(),
			       *NewLayerSpec.PanoramicDefinition.ToSoftObjectPath().ToString(),
			       *NewLayerSpec.WorldBounds.Min.ToString(), *NewLayerSpec.WorldBounds.Max.ToString(),
			       NewLayerSpec.MapRotationDegrees, TileManager ? TileManager->GetMaxLOD() : INDEX_NONE);
		}
		return;
	}

	if (!Definition->BaseMapTexture.IsNull())
	{
		if (UTexture2D* BaseMapTexture = Definition->BaseMapTexture.LoadSynchronous())
		{
			MapMaterialInstance->SetTextureParameterValue("MapTexture", BaseMapTexture);
			MapImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (VisualConfigAsset && VisualConfigAsset->bShowDebugMessages)
			{
				UE_LOG(LogOBNavigation, Log,
				       TEXT("[%s::%hs] - Applied single-texture Panoramic map layer. Layer='%s' BaseMapTexture='%s' Definition='%s' BoundsMin=%s BoundsMax=%s MapRotation=%.2f"),
				       *GetName(), __FUNCTION__, *NewLayerSpec.LayerName.ToString(), *GetNameSafe(BaseMapTexture),
				       *NewLayerSpec.PanoramicDefinition.ToSoftObjectPath().ToString(),
				       *NewLayerSpec.WorldBounds.Min.ToString(), *NewLayerSpec.WorldBounds.Max.ToString(),
				       NewLayerSpec.MapRotationDegrees);
			}
			return;
		}
	}

	MapImage->SetVisibility(ESlateVisibility::Collapsed);
	if (VisualConfigAsset && VisualConfigAsset->bShowDebugMessages)
	{
		UE_LOG(LogOBNavigation, Error,
		       TEXT("[%s::%hs] - Panoramic map layer has neither TileSet nor loadable BaseMapTexture. Layer='%s' Definition='%s'"),
		       *GetName(), __FUNCTION__, *NewLayerSpec.LayerName.ToString(),
		       *NewLayerSpec.PanoramicDefinition.ToSoftObjectPath().ToString());
	}
}
