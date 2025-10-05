// Fill out your copyright notice in the Description page of Project Settings.

#include "OBMinimapWidget.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "OBNavigationSubsystem.h"
#include "OBMapLayerAsset.h"
#include "Data/OBMinimapConfigAsset.h"
#include "Materials/MaterialInstanceDynamic.h"

void UOBMinimapWidget::InitializeAndStartTracking(UOBMinimapConfigAsset* InConfigAsset)
{
	if (bIsInitializedAndTracking)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Widget is already initialized."), *GetName(), __FUNCTION__);
		return;
	}

	if (!InConfigAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - Initialization failed: Invalid ConfigAsset provided."), *GetName(),
		       __FUNCTION__);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	ConfigAsset = InConfigAsset;

	// --- 1. COPY CONFIG VALUES TO INTERNAL STATE ---
	CurrentMapRotationOffset = ConfigAsset->MapRotationOffset;
	CurrentMinimapShape = ConfigAsset->MinimapShape;

	// --- 2. SETUP VISUAL ASSETS FROM CONFIG ---
	if (MapImage && ConfigAsset->MinimapBackgroundMaterial)
	{
		MinimapMaterialInstance = UMaterialInstanceDynamic::Create(ConfigAsset->MinimapBackgroundMaterial, this);
		MapImage->SetBrushFromMaterial(MinimapMaterialInstance);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - Failed to set up MapImage material."), *GetName(), __FUNCTION__);
	}

	if (PlayerIcon && ConfigAsset->PlayerIconTexture)
	{
		PlayerIcon->SetBrushFromTexture(ConfigAsset->PlayerIconTexture);
	}

	if (CompassRingImage && ConfigAsset->CompassRingTexture)
	{
		CompassRingImage->SetBrushFromTexture(ConfigAsset->CompassRingTexture);
	}

	// --- 3. SETUP SUBSYSTEM & DELEGATES ---
	if (const UGameInstance* GI = GetGameInstance())
	{
		NavSubsystem = GI->GetSubsystem<UOBNavigationSubsystem>();
		if (NavSubsystem)
		{
			NavSubsystem->OnMinimapLayerChanged.AddDynamic(this, &UOBMinimapWidget::OnMinimapLayerChanged);
			// Initial layer setup
			OnMinimapLayerChanged(NavSubsystem->GetCurrentMinimapLayer());
		}
	}

	// --- 4. APPLY INITIAL SETTINGS FROM COPIED STATE ---
	SetMapRotationOffset(CurrentMapRotationOffset);
	SetMinimapShape(CurrentMinimapShape);

	// --- 5. VALIDATE AND START ---
	if (!MinimapMaterialInstance || !NavSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - Initialization failed due to missing subsystem or material instance."),
		       *GetName(), __FUNCTION__);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	bIsInitializedAndTracking = true;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible); // Make it visible
	UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Minimap initialized and tracking started."), *GetName(), __FUNCTION__);
}

void UOBMinimapWidget::SetMapRotationOffset(float NewOffsetYaw)
{
	CurrentMapRotationOffset = NewOffsetYaw;
	if (MinimapMaterialInstance)
	{
		const float TotalStaticRotation = CurrentMapRotationOffset + GetAlignmentAngle();
		MinimapMaterialInstance->SetScalarParameterValue("MapRotationOffsetRad",
		                                                 FMath::DegreesToRadians(TotalStaticRotation));
		if (GetConfig()->bIsCompassEnabled && CompassRingImage)
		{
			CompassRingImage->SetRenderTransformAngle(-TotalStaticRotation);
		}
	}
}

void UOBMinimapWidget::SetMinimapShape(const EMinimapShape NewShape)
{
	CurrentMinimapShape = NewShape;
	if (MinimapMaterialInstance)
	{
		const float ShapeValue = (CurrentMinimapShape == EMinimapShape::Circle) ? 1.0f : 0.0f;
		MinimapMaterialInstance->SetScalarParameterValue("ShapeAlpha", ShapeValue);
	}
}

void UOBMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Only run the tick logic if the widget has been properly initialized.
	if (!bIsInitializedAndTracking)
	{
		return;
	}

	if (!NavSubsystem || !NavSubsystem->GetTrackedPlayerPawn()) { return; }

	const APawn* TrackedPawn = NavSubsystem->GetTrackedPlayerPawn();
	const UOBMapLayerAsset* CurrentLayer = NavSubsystem->GetCurrentMinimapLayer();

	// --- UNIFIED ROTATION LOGIC ---
	const float AlignmentAngle = GetAlignmentAngle();
	const float TotalStaticRotation = CurrentMapRotationOffset + AlignmentAngle;

	// --- PLAYER ICON ---
	const float CharacterWorldYaw = TrackedPawn->GetActorRotation().Yaw;
	const float FinalIconYaw = CharacterWorldYaw + TotalStaticRotation;
	if (PlayerIcon)
	{
		PlayerIcon->SetRenderTransformAngle(FinalIconYaw);
	}

	// --- MINIMAP MATERIAL ---
	if (CurrentLayer && MinimapMaterialInstance)
	{
		if (FVector2D PlayerUV; NavSubsystem->WorldToMapUV(CurrentLayer, TrackedPawn->GetActorLocation(), PlayerUV))
		{
			// Set UV position
			MinimapMaterialInstance->SetVectorParameterValue("PlayerPositionUV",
			                                                 FLinearColor(PlayerUV.X, PlayerUV.Y, 0.0f, 0.0f));

			// Set DYNAMIC rotation (for rotating maps)
			float DynamicMapYaw = 0.0f;
			if (ConfigAsset->bShouldRotateMap)
			{
				switch (ConfigAsset->RotationSource)
				{
				case EMinimapRotationSource::ControlRotation:
					DynamicMapYaw = TrackedPawn->GetControlRotation().Yaw;
					break;
				case EMinimapRotationSource::ActorRotation:
					DynamicMapYaw = TrackedPawn->GetActorRotation().Yaw;
					break;
				}
			}
			MinimapMaterialInstance->SetScalarParameterValue("PlayerYaw", FMath::DegreesToRadians(DynamicMapYaw));

			// Set STATIC rotation (always applied)
			MinimapMaterialInstance->SetScalarParameterValue("MapRotationOffsetRad",
			                                                 FMath::DegreesToRadians(TotalStaticRotation));

			// Set Zoom
			MinimapMaterialInstance->SetScalarParameterValue("Zoom", ConfigAsset->Zoom);

			// --- DEBUG LOGS ---
			if (GEngine && ConfigAsset->bShowDebugMessages)
			{
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan,
				                                 FString::Printf(TEXT("--- MINIMAP DEBUG ---")));
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White,
				                                 FString::Printf(TEXT("Character World Yaw: %.2f"), CharacterWorldYaw));
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White,
				                                 FString::Printf(TEXT("Alignment Angle: %.2f"), AlignmentAngle));
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White,
				                                 FString::Printf(
					                                 TEXT("Map Offset: %.2f"), ConfigAsset->MapRotationOffset));
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow,
				                                 FString::Printf(
					                                 TEXT("=> Total Static Rotation: %.2f"), TotalStaticRotation));
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow,
				                                 FString::Printf(TEXT("=> Final Icon Yaw: %.2f"), FinalIconYaw));
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow,
				                                 FString::Printf(
					                                 TEXT("=> Mat Param [PlayerYaw]: %.2f deg"), DynamicMapYaw));
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow,
				                                 FString::Printf(
					                                 TEXT("=> Mat Param [MapRotationOffset]: %.2f deg"),
					                                 TotalStaticRotation));
			}
		}
	}

	// --- COMPASS LOGIC ---
	if (ConfigAsset->bIsCompassEnabled && CompassMarkerCanvas)
	{
		UpdateCompassMarkers(TrackedPawn, TotalStaticRotation);
	}
}

void UOBMinimapWidget::UpdateCompassMarkers(const APawn* TrackedPawn, float InTotalStaticRotation)
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
		const float PosX = CanvasCenter.X + ConfigAsset->CompassMarkerRadius * FMath::Cos(AngleRad);
		const float PosY = CanvasCenter.Y + ConfigAsset->CompassMarkerRadius * FMath::Sin(AngleRad);

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
	switch (ConfigAsset->MapAlignment)
	{
	case EMapAlignment::Forward_PlusX: return 0.0f;
	case EMapAlignment::Right_PlusY: return 90.0f;
	case EMapAlignment::Backward_MinusX: return 180.0f;
	case EMapAlignment::Left_MinusY: return -90.0f;
	default: return 0.0f;
	}
}
