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
		// GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White,
		//                                  FString::Printf(TEXT("Character World Yaw: %.2f"), CharacterWorldYaw));
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
			                               Marker->ConfigAsset->DirectionalIndicatorTexture);
		}

		FVector2D FinalPosition;
		float IndicatorAngle = 0.0f;

		// --- LOGIC VỊ TRÍ VÀ XOAY MỚI ---
		if (Marker->MarkerID == PlayerMarkerID)
		{
			// Người chơi luôn ở giữa
			FinalPosition = CanvasCenter;
			IndicatorAngle = TrackedPawn->GetActorRotation().Yaw + InTotalStaticRotation;
		}
		else
		{
			FVector2D MarkerUV;
			NavSubsystem->WorldToMapUV(CurrentLayer, Marker->WorldLocation, MarkerUV);

			const FVector2D UVDifference = MarkerUV - PlayerUV;
			FVector2D PixelOffset = UVDifference * CanvasSize * ConfigAsset->Zoom;

			// Áp dụng xoay cho offset
			FVector2D RotatedPixelOffset;
			if (ConfigAsset->bShouldRotateMap)
			{
				float DynamicMapYaw = 0.0f;
				// Lấy DynamicMapYaw tương tự như trong NativeTick
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

			// --- LOGIC CLAMPING ---
			if (RotatedPixelOffset.SizeSquared() > FMath::Square(MinimapRadius))
			{
				FVector2D ClampedOffset = RotatedPixelOffset.GetSafeNormal() * MinimapRadius;
				FinalPosition = CanvasCenter + ClampedOffset;
				IndicatorAngle = FMath::RadiansToDegrees(FMath::Atan2(RotatedPixelOffset.Y, RotatedPixelOffset.X));
			}
			else
			{
				FinalPosition = CanvasCenter + RotatedPixelOffset;
				if (Marker->TrackedActor.IsValid())
				{
					IndicatorAngle = Marker->TrackedActor->GetActorRotation().Yaw + InTotalStaticRotation;
				}
				else
				{
					IndicatorAngle = InTotalStaticRotation;
				}
			}
		}

		MarkerWidget->UpdateRotation(IndicatorAngle);

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget->Slot))
		{
			FVector2D SlotPosition;
			if (Marker->MarkerID == PlayerMarkerID)
			{
				// FOR THE PLAYER: The position is simply the center of the canvas.
				SlotPosition = FinalPosition;
			}
			else
			{
				// FOR OTHER MARKERS: We still need pivot compensation, but without the half-size offset.
				const FVector2D MarkerSize = MarkerWidget->GetDesiredSize();
				const FVector2D Pivot = Marker->ConfigAsset->IndicatorPivot;
				const FVector2D PivotOffset = (Pivot - FVector2D(0.5f, 0.5f)) * MarkerSize;
				const FVector2D RotatedPivotOffset = PivotOffset.GetRotated(IndicatorAngle);
				SlotPosition = FinalPosition - (RotatedPivotOffset - PivotOffset);
			}

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
