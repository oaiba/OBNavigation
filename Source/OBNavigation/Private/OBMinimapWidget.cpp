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

			// --- LOGIC MỚI: TÌM KIẾM MARKER ID CỦA NGƯỜI CHƠI ---
			if (APawn* TrackedPawn = NavSubsystem->GetTrackedPlayerPawn())
			{
				// The marker should have already been registered by the OBNavigationComponent.
				// We just need to find its ID.
				PlayerMarkerID = NavSubsystem->GetMarkerIDForActor(TrackedPawn);
				if (!PlayerMarkerID.IsValid())
				{
					UE_LOG(LogTemp, Warning,
					       TEXT(
						       "[%s::%hs] - Could not find a pre-registered marker for the tracked player pawn. The player marker might not be shown correctly."
					       ), *GetName(), __FUNCTION__);
				}
			}
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

void UOBMinimapWidget::SetMapRotationOffset(const float NewOffsetYaw)
{
	CurrentMapRotationOffset = NewOffsetYaw;
	if (MinimapMaterialInstance)
	{
		const float TotalStaticRotation = CurrentMapRotationOffset + GetAlignmentAngle();
		MinimapMaterialInstance->SetScalarParameterValue("MapRotationOffsetRad",
		                                                 FMath::DegreesToRadians(TotalStaticRotation));
		if (CompassRingImage)
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

	if (!bIsInitializedAndTracking || !ConfigAsset) return;
	if (!NavSubsystem || !NavSubsystem->GetTrackedPlayerPawn()) return;

	const APawn* TrackedPawn = NavSubsystem->GetTrackedPlayerPawn();
	const UOBMapLayerAsset* CurrentLayer = NavSubsystem->GetCurrentMinimapLayer();
	const float AlignmentAngle = GetAlignmentAngle();
	const float TotalStaticRotation = CurrentMapRotationOffset + AlignmentAngle;
	const float CharacterWorldYaw = TrackedPawn->GetActorRotation().Yaw; // Tính một lần ở đây
	float DynamicMapYaw = 0.0f;

	// --- MINIMAP MATERIAL LOGIC ---
	if (CurrentLayer && MinimapMaterialInstance)
	{
		if (FVector2D PlayerUV; NavSubsystem->WorldToMapUV(CurrentLayer, TrackedPawn->GetActorLocation(), PlayerUV))
		{
			MinimapMaterialInstance->SetVectorParameterValue("PlayerPositionUV",
			                                                 FLinearColor(PlayerUV.X, PlayerUV.Y, 0.0f, 0.0f));

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
			MinimapMaterialInstance->SetScalarParameterValue("Zoom", ConfigAsset->Zoom);

			// Set STATIC rotation (always applied) - DÒNG NÀY BỊ THIẾU TRONG CODE CŨ CỦA BẠN
			MinimapMaterialInstance->SetScalarParameterValue("MapRotationOffsetRad",
			                                                 FMath::DegreesToRadians(TotalStaticRotation));
		}
	}
	TSet<FGuid> HandledMarkerIDs; // Keep track of markers processed in this frame

	// --- Pass 1: MINIMAP MARKERS ---
	if (MinimapMarkerCanvas)
	{
		UpdateMinimapMarkers(TrackedPawn, TotalStaticRotation, HandledMarkerIDs);
	}

	// --- Pass 3: CLEANUP UNUSED WIDGETS ---
	// Remove any widget from the pool that wasn't handled in either pass
	TArray<FGuid> MarkersToRemove;
	for (const auto& Pair : ActiveMinimapMarkerWidgets)
	{
		if (!HandledMarkerIDs.Contains(Pair.Key))
		{
			MarkersToRemove.Add(Pair.Key);
		}
	}

	for (const FGuid& ID : MarkersToRemove)
	{
		if (UOBMapMarkerWidget* WidgetToRemove = ActiveMinimapMarkerWidgets.FindRef(ID))
		{
			WidgetToRemove->RemoveFromParent();
		}
		ActiveMinimapMarkerWidgets.Remove(ID);
	}

	// --- DEBUG LOGS (Sửa lại) ---
	if (GEngine && ConfigAsset->bShowDebugMessages)
	{
		const float FinalIconYaw = CharacterWorldYaw + TotalStaticRotation;

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

void UOBMinimapWidget::UpdateMinimapMarkers(const APawn* TrackedPawn, const float InTotalStaticRotation,
                                            TSet<FGuid>& OutHandledMarkerIDs)
{
	if (!MarkerWidgetClass || !NavSubsystem || !ConfigAsset) return;

	const UOBMapLayerAsset* CurrentLayer = NavSubsystem->GetCurrentMinimapLayer();
	if (!CurrentLayer) return;

	const FVector2D CanvasSize = MinimapMarkerCanvas->GetCachedGeometry().GetLocalSize();
	const FVector2D CanvasCenter = CanvasSize / 2.0f;
	// Bán kính của minimap (giả sử nó vừa vặn với canvas)
	const float MinimapRadius = FMath::Min(CanvasCenter.X, CanvasCenter.Y);

	FVector2D PlayerUV;
	NavSubsystem->WorldToMapUV(CurrentLayer, TrackedPawn->GetActorLocation(), PlayerUV);

	for (UOBMapMarker* Marker : NavSubsystem->GetAllActiveMarkers())
	{
		// SỬA LẠI ĐIỀU KIỆN LỌC: BÂY GIỜ CHỈ CẦN LỌC MINIMAP
		if (!Marker || !Marker->ConfigAsset || !Marker->ConfigAsset->Visibility.bShowOnMinimap)
		{
			continue;
		}

		OutHandledMarkerIDs.Add(Marker->MarkerID);

		// --- LOGIC TẠO/LẤY WIDGET (giữ nguyên từ trước) ---
		UOBMapMarkerWidget* MarkerWidget = ActiveMinimapMarkerWidgets.FindRef(Marker->MarkerID);
		if (!MarkerWidget)
		{
			MarkerWidget = CreateWidget<UOBMapMarkerWidget>(this, MarkerWidgetClass);
			if (!MarkerWidget) continue;
			// Thêm widget vào canvas

			// --- FIX PART 1: FORCE CENTER ALIGNMENT ---
			// Set the alignment to (0.5, 0.5) to ensure the widget's pivot/anchor is its center.
			// This makes positioning logic consistent and independent of Blueprint settings.
			if (auto* NewSlot = Cast<UCanvasPanelSlot>(MinimapMarkerCanvas->AddChild(MarkerWidget)))
			{
				NewSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			}
			ActiveMinimapMarkerWidgets.Add(Marker->MarkerID, MarkerWidget);

			MarkerWidget->InitializeMarker(Marker->ConfigAsset->IdentifierIconTexture,
			                               Marker->ConfigAsset->IndicatorMaterial);
		}

		// --- START: REPLACEMENT LOGIC FOR POSITION AND ROTATION ---
		FVector2D FinalPosition;
		float IndicatorAngle = 0.0f;

		// This block now correctly handles all rotation cases based on map type
		if (Marker->MarkerID == PlayerMarkerID)
		{
			// The player is always in the center.
			FinalPosition = CanvasCenter;

			if (ConfigAsset->bShouldRotateMap)
			{
				// On a rotating map, the player's icon should always point "up".
				IndicatorAngle = 0.0f;
			}
			else
			{
				// On a static map, the player's icon must show its true world orientation,
				// compensating for the map's static rotation.
				// We SUBTRACT the static rotation, not add it.
				IndicatorAngle = TrackedPawn->GetActorRotation().Yaw - InTotalStaticRotation;
			}
		}
		else
		{
			// Logic for all other markers (NPCs, objectives, etc.)
			FVector2D MarkerUV;
			NavSubsystem->WorldToMapUV(CurrentLayer, Marker->WorldLocation, MarkerUV);

			const FVector2D UVDifference = MarkerUV - PlayerUV;
			const FVector2D PixelOffset = UVDifference * CanvasSize * ConfigAsset->Zoom;
			FVector2D RotatedPixelOffset;

			// This part correctly calculates the marker's position on the canvas
			if (ConfigAsset->bShouldRotateMap)
			{
				float DynamicMapYaw = 0.0f;
				switch (ConfigAsset->RotationSource)
				{
				case EMinimapRotationSource::ControlRotation:
					DynamicMapYaw = TrackedPawn->GetControlRotation().Yaw;
					break;
				case EMinimapRotationSource::ActorRotation:
					DynamicMapYaw = TrackedPawn->GetActorRotation().Yaw;
					break;
				}
				RotatedPixelOffset = PixelOffset.GetRotated(-(InTotalStaticRotation + DynamicMapYaw));
			}
			else
			{
				RotatedPixelOffset = PixelOffset.GetRotated(-InTotalStaticRotation);
			}

			// --- Clamping and Angle Calculation Logic ---
			if (RotatedPixelOffset.SizeSquared() > FMath::Square(MinimapRadius))
			{
				// CASE 1: The marker is clamped to the edge of the minimap.
				// Its indicator should point from the center towards its off-screen location.
				// This existing logic is correct and remains unchanged.
				const FVector2D ClampedOffset = RotatedPixelOffset.GetSafeNormal() * MinimapRadius;
				FinalPosition = CanvasCenter + ClampedOffset;
				IndicatorAngle = FMath::RadiansToDegrees(FMath::Atan2(RotatedPixelOffset.Y, RotatedPixelOffset.X));
			}
			else
			{
				// CASE 2: The marker is visible inside the minimap.
				FinalPosition = CanvasCenter + RotatedPixelOffset;

				float ActorWorldYaw = 0.0f; // Default for static markers (points to World North +X)
				if (Marker->TrackedActor.IsValid())
				{
					ActorWorldYaw = Marker->TrackedActor->GetActorRotation().Yaw;
				}

				if (ConfigAsset->bShouldRotateMap)
				{
					// On a rotating map, the marker's angle is relative to the player's view.
					float PlayerYaw = 0.0f;
					switch (ConfigAsset->RotationSource)
					{
					case EMinimapRotationSource::ControlRotation: PlayerYaw = TrackedPawn->GetControlRotation().Yaw;
						break;
					case EMinimapRotationSource::ActorRotation: PlayerYaw = TrackedPawn->GetActorRotation().Yaw;
						break;
					}
					IndicatorAngle = ActorWorldYaw - PlayerYaw;
				}
				else
				{
					// On a static map, the marker shows its true world orientation,
					// compensating for the map's static rotation.
					IndicatorAngle = ActorWorldYaw - InTotalStaticRotation;
				}
			}
		}
		// --- END: REPLACEMENT LOGIC ---

		MarkerWidget->UpdateVisuals(IndicatorAngle, 90.0f, 1.0f);

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget->Slot))
		{
			// --- FIX STARTS HERE ---

			// 1. Use the size defined in the config asset, not the widget's desired size.
			// This ensures the pivot calculations are based on our intended dimensions.
			const FVector2D MarkerSize = Marker->ConfigAsset->Size;
			FVector2D SlotPosition;

			if (Marker->MarkerID == PlayerMarkerID)
			{
				SlotPosition = FinalPosition;
			}
			else
			{
				// Pivot compensation logic now correctly uses the config size.
				const FVector2D Pivot = Marker->ConfigAsset->IndicatorPivot;
				const FVector2D PivotOffset = (Pivot - FVector2D(0.5f, 0.5f)) * MarkerSize;
				const FVector2D RotatedPivotOffset = PivotOffset.GetRotated(IndicatorAngle);
				SlotPosition = FinalPosition - (RotatedPivotOffset - PivotOffset);
			}

			// 2. Explicitly set the size of the widget in the canvas panel.
			// This overrides any incorrect default layout size from the Blueprint and fixes the distortion.
			CanvasSlot->SetSize(MarkerSize);
			CanvasSlot->SetPosition(SlotPosition);
			CanvasSlot->SetZOrder(Marker->MarkerID == PlayerMarkerID ? 10 : 1);

			// --- FIX ENDS HERE ---
		}

		if (GEngine && ConfigAsset->bShowDebugMessages)
		{
			const FColor DebugColor = (Marker->MarkerID == PlayerMarkerID) ? FColor::Magenta : FColor::Green;
			GEngine->AddOnScreenDebugMessage(
				-1, 0.0f, DebugColor,
				FString::Printf(TEXT("Marker [%s]: Final Pos: %s"),
				                *Marker->MarkerID.ToString().Left(8),
				                *FinalPosition.ToString()
				)
			);
		}

		if (GEngine && ConfigAsset->bShowDebugMessages && Marker->MarkerID == PlayerMarkerID)
		{
			GEngine->AddOnScreenDebugMessage(
				-1, 0.0f, FColor::White,
				FString::Printf(TEXT("Player Marker Widget Visibility: %d"), MarkerWidget->GetVisibility())
			);
		}
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
