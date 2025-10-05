// Fill out your copyright notice in the Description page of Project Settings.

#include "OBMinimapWidget.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "OBNavigationSubsystem.h"
#include "OBMapLayerAsset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetMathLibrary.h"

void UOBMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Create Dynamic Material Instance for the map
	if (MapImage)
	{
		MinimapMaterialInstance = MapImage->GetDynamicMaterial();
	}

	// Get Subsystem and bind delegates
	if (const UGameInstance* GI = GetGameInstance())
	{
		NavSubsystem = GI->GetSubsystem<UOBNavigationSubsystem>();
		if (NavSubsystem)
		{
			NavSubsystem->OnMinimapLayerChanged.AddDynamic(this, &UOBMinimapWidget::OnMinimapLayerChanged);
			OnMinimapLayerChanged(NavSubsystem->GetCurrentMinimapLayer());
		}
	}

	// --- INITIAL STATIC ROTATION SETUP ---
	if (MinimapMaterialInstance)
	{
		const float TotalStaticRotation = MapRotationOffset + GetAlignmentAngle();

		// Apply static rotation to the map material
		MinimapMaterialInstance->SetScalarParameterValue("MapRotationOffsetRad",
		                                                 FMath::DegreesToRadians(TotalStaticRotation));

		// Apply the same static rotation to the compass ring image
		if (bIsCompassEnabled && CompassRingImage)
		{
			CompassRingImage->SetRenderTransformAngle(TotalStaticRotation);
		}
	}

	// Final check to hide widget if anything failed
	if (!MinimapMaterialInstance || !NavSubsystem)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UOBMinimapWidget::SetMapRotationOffset(float NewOffsetYaw)
{
	MapRotationOffset = NewOffsetYaw;

	if (MinimapMaterialInstance)
	{
		const float TotalStaticRotation = MapRotationOffset + GetAlignmentAngle();

		// Update static rotation for both map and compass ring
		MinimapMaterialInstance->SetScalarParameterValue("MapRotationOffsetRad",
		                                                 FMath::DegreesToRadians(TotalStaticRotation));

		if (bIsCompassEnabled && CompassRingImage)
		{
			CompassRingImage->SetRenderTransformAngle(TotalStaticRotation);
		}
	}
}

void UOBMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!NavSubsystem || !NavSubsystem->GetTrackedPlayerPawn())
	{
		return;
	}

	const APawn* TrackedPawn = NavSubsystem->GetTrackedPlayerPawn();
	const UOBMapLayerAsset* CurrentLayer = NavSubsystem->GetCurrentMinimapLayer();
	const float TotalStaticRotation = MapRotationOffset + GetAlignmentAngle();

	// --- MINIMAP LOGIC ---
	if (CurrentLayer && MinimapMaterialInstance)
	{
		if (FVector2D PlayerUV; NavSubsystem->WorldToMapUV(CurrentLayer, TrackedPawn->GetActorLocation(), PlayerUV))
		{
			MinimapMaterialInstance->SetVectorParameterValue("PlayerPositionUV",
			                                                 FLinearColor(PlayerUV.X, PlayerUV.Y, 0.0f, 0.0f));

			if (bShouldRotateMap)
			{
				float DynamicMapYaw = 0.0f;
				switch (RotationSource)
				{
				case EMinimapRotationSource::ControlRotation:
					DynamicMapYaw = TrackedPawn->GetControlRotation().Yaw;
					break;
				case EMinimapRotationSource::ActorRotation:
					DynamicMapYaw = TrackedPawn->GetActorRotation().Yaw;
					break;
				}
				const float FinalDynamicRotation = DynamicMapYaw + TotalStaticRotation;
				MinimapMaterialInstance->SetScalarParameterValue("PlayerYaw",
				                                                 FMath::DegreesToRadians(FinalDynamicRotation));
			}

			MinimapMaterialInstance->SetScalarParameterValue("Zoom", Zoom);
		}
	}

	// --- PLAYER ICON LOGIC ---
	if (PlayerIcon)
	{
		const float FinalIconYaw = TrackedPawn->GetActorRotation().Yaw + TotalStaticRotation;
		PlayerIcon->SetRenderTransformAngle(FinalIconYaw);
	}

	// --- COMPASS LOGIC ---
	if (bIsCompassEnabled && CompassMarkerCanvas)
	{
		UpdateCompass(TrackedPawn, TotalStaticRotation);
	}
}

void UOBMinimapWidget::UpdateCompass(const APawn* TrackedPawn, float InTotalStaticRotation)
{
	const FVector PawnLocation = TrackedPawn->GetActorLocation();
	const FVector2D CanvasCenter = CompassMarkerCanvas->GetCachedGeometry().GetLocalSize() / 2.0f;

	TSet<FGuid> VisibleMarkerIDs;

	for (const UOBMapMarker* Marker : NavSubsystem->GetAllActiveMarkers())
	{
		if (!Marker || !Marker->ConfigAsset || !(Marker->ConfigAsset->VisibilityFilter & static_cast<uint8>(
			EOBMarkerVisibility::Compass)))
		{
			continue;
		}
		
		VisibleMarkerIDs.Add(Marker->MarkerID);

		const FVector DirToMarkerWorld = (Marker->WorldLocation - PawnLocation).GetSafeNormal2D();
		const float MarkerWorldYaw = FMath::RadiansToDegrees(FMath::Atan2(DirToMarkerWorld.Y, DirToMarkerWorld.X));
		const float MarkerFinalAngle = MarkerWorldYaw + InTotalStaticRotation;

		const float AngleRad = FMath::DegreesToRadians(MarkerFinalAngle);
		const float PosX = CanvasCenter.X + CompassMarkerRadius * FMath::Cos(AngleRad);
		const float PosY = CanvasCenter.Y + CompassMarkerRadius * FMath::Sin(AngleRad);

		UImage* MarkerIcon = ActiveCompassMarkerWidgets.FindRef(Marker->MarkerID);
		if (!MarkerIcon)
		{
			MarkerIcon = NewObject<UImage>(CompassMarkerCanvas);
			MarkerIcon->SetBrushFromTexture(Marker->ConfigAsset->IconTexture);
			MarkerIcon->SetDesiredSizeOverride(Marker->ConfigAsset->Size);
			CompassMarkerCanvas->AddChild(MarkerIcon);
			ActiveCompassMarkerWidgets.Add(Marker->MarkerID, MarkerIcon);
		}

		MarkerIcon->SetVisibility(ESlateVisibility::HitTestInvisible);

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerIcon->Slot))
		{
			CanvasSlot->SetPosition(FVector2D(PosX - (Marker->ConfigAsset->Size.X / 2.0f),
			                                  PosY - (Marker->ConfigAsset->Size.Y / 2.0f)));
		}
	}

	// Remove widgets for markers that no longer exist
	TArray<FGuid> MarkersToRemove;
	for (const auto& Pair : ActiveCompassMarkerWidgets)
	{
		if (!VisibleMarkerIDs.Contains(Pair.Key))
		{
			Pair.Value->RemoveFromParent();
			MarkersToRemove.Add(Pair.Key);
		}
	}
	for (const FGuid& ID : MarkersToRemove)
	{
		ActiveCompassMarkerWidgets.Remove(ID);
	}
}


void UOBMinimapWidget::OnMinimapLayerChanged(UOBMapLayerAsset* NewLayer)
{
	if (!MinimapMaterialInstance || !MapImage)
	{
		return;
	}

	if (NewLayer && NewLayer->MapTexture)
	{
		MinimapMaterialInstance->SetTextureParameterValue("MapTexture", NewLayer->MapTexture);
		MapImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		MapImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}

float UOBMinimapWidget::GetAlignmentAngle() const
{
	switch (MapAlignment)
	{
	case EMapAlignment::Forward_PlusX: return 0.0f;
	case EMapAlignment::Right_PlusY: return 90.0f;
	case EMapAlignment::Backward_MinusX: return 180.0f;
	case EMapAlignment::Left_MinusY: return -90.0f;
	default: return 0.0f;
	}
}
