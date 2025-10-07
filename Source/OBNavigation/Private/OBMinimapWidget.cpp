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

	// --- Pass 2: COMPASS MARKERS ---
	if (CompassMarkerCanvas)
	{
		UpdateCompassMarkers(TrackedPawn, TotalStaticRotation, HandledMarkerIDs);
	}

	// --- Pass 3: CLEANUP UNUSED WIDGETS ---
	// Remove any widget from the pool that wasn't handled in either pass
	TArray<FGuid> MarkersToRemove;
	for (const auto& Pair : ActiveMarkerWidgets)
	{
		if (!HandledMarkerIDs.Contains(Pair.Key))
		{
			MarkersToRemove.Add(Pair.Key);
		}
	}

	for (const FGuid& ID : MarkersToRemove)
	{
		if (UOBMapMarkerWidget* WidgetToRemove = ActiveMarkerWidgets.FindRef(ID))
		{
			WidgetToRemove->RemoveFromParent();
		}
		ActiveMarkerWidgets.Remove(ID);
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

void UOBMinimapWidget::UpdateCompassMarkers(const APawn* TrackedPawn, const float InTotalStaticRotation,
                                            TSet<FGuid>& OutHandledMarkerIDs)
{
	const FVector PawnLocation = TrackedPawn->GetActorLocation();
	const FVector2D CanvasCenter = CompassMarkerCanvas->GetCachedGeometry().GetLocalSize() / 2.0f;
	const float CurrentCompassRadius = GetCompassRadius();

	TSet<FGuid> VisibleMarkerIDs;

	for (const UOBMapMarker* Marker : NavSubsystem->GetAllActiveMarkers())
	{
		if (!Marker || !Marker->ConfigAsset || !Marker->ConfigAsset->Visibility.bShowOnCompass)
		{
			continue;
		}
		
		OutHandledMarkerIDs.Add(Marker->MarkerID); // Mark this ID as handled
		VisibleMarkerIDs.Add(Marker->MarkerID);

		const FVector DirToMarkerWorld = (Marker->WorldLocation - PawnLocation).GetSafeNormal2D();
		const float MarkerWorldYaw = FMath::RadiansToDegrees(FMath::Atan2(DirToMarkerWorld.Y, DirToMarkerWorld.X));
		const float MarkerFinalAngle = MarkerWorldYaw + InTotalStaticRotation;

		const float AngleRad = FMath::DegreesToRadians(MarkerFinalAngle);
		const float PosX = CanvasCenter.X + CurrentCompassRadius * FMath::Cos(AngleRad);
		const float PosY = CanvasCenter.Y + CurrentCompassRadius * FMath::Sin(AngleRad);

		UOBMapMarkerWidget* MarkerWidget = ActiveMarkerWidgets.FindRef(Marker->MarkerID);
		if (!MarkerWidget)
		{
			MarkerWidget = CreateWidget<UOBMapMarkerWidget>(this, MarkerWidgetClass);
			if (!MarkerWidget) continue;

			// Add to the correct canvas
			CompassMarkerCanvas->AddChild(MarkerWidget);
			ActiveMarkerWidgets.Add(Marker->MarkerID, MarkerWidget);
		}

		// Ensure it's in the correct canvas
		if (MarkerWidget->GetParent() != CompassMarkerCanvas)
		{
			MarkerWidget->RemoveFromParent();
			CompassMarkerCanvas->AddChild(MarkerWidget);
		}

		MarkerWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

		MarkerWidget->UpdateMarkerVisuals(Marker->ConfigAsset->IdentifierIconTexture, nullptr, 0.0f);		

		// --- LOGIC BÙ TRỪ PIVOT MỚI (CHO LA BÀN) ---
		// Vì indicator trên la bàn không xoay, chúng ta không cần bù trừ phức tạp.
		// Chúng ta chỉ cần định vị widget sao cho điểm pivot của nó nằm trên vòng tròn la bàn.
		const FVector2D MarkerSize = Marker->ConfigAsset->Size;
		const FVector2D Pivot = Marker->ConfigAsset->IndicatorPivot;

		// Calculate the offset from the top-left corner to the pivot point.
		const FVector2D PivotOffsetInPixels = Pivot * MarkerSize;
		const FVector2D CenterPositionOnRing = FVector2D(PosX, PosY); // Vị trí tâm mong muốn

		// The final slot position is the target point on the ring, minus the pivot offset.
		const FVector2D SlotPosition = CenterPositionOnRing - PivotOffsetInPixels;

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget->Slot))
		{
			CanvasSlot->SetPosition(SlotPosition);
		}
	}
}

void UOBMinimapWidget::UpdateMinimapMarkers(const APawn* TrackedPawn, const float InTotalStaticRotation,
                                            TSet<FGuid>& OutHandledMarkerIDs)
{
	if (!MarkerWidgetClass || !NavSubsystem || !ConfigAsset) return;

	if (GEngine && ConfigAsset->bShowDebugMessages)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 0.0f, FColor::Red,
			FString::Printf(
				TEXT("UpdateMinimapMarkers: Found %d total markers in Subsystem."),
				NavSubsystem->GetAllActiveMarkers().Num())
		);
	}

	const UOBMapLayerAsset* CurrentLayer = NavSubsystem->GetCurrentMinimapLayer();

	if (GEngine && ConfigAsset->bShowDebugMessages)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 0.0f, FColor::Orange,
			FString::Printf(TEXT("Current Map Layer: %s"), CurrentLayer ? *CurrentLayer->GetName() : TEXT("NULL"))
		);
	}

	if (!CurrentLayer) return;

	const FVector2D CanvasSize = MinimapMarkerCanvas->GetCachedGeometry().GetLocalSize();
	const FVector2D CanvasCenter = CanvasSize / 2.0f;
	TSet<FGuid> VisibleMarkerIDs;

	FVector2D PlayerUV;
	if (!NavSubsystem->WorldToMapUV(CurrentLayer, TrackedPawn->GetActorLocation(), PlayerUV))
	{
		return;
	}

	for (const UOBMapMarker* Marker : NavSubsystem->GetAllActiveMarkers())
	{
		if (!Marker || !Marker->ConfigAsset || !Marker->ConfigAsset->Visibility.bShowOnMinimap)
		{
			if (GEngine && ConfigAsset->bShowDebugMessages)
			{
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red,
				                                 FString::Printf(
					                                 TEXT(
						                                 "Marker [%s] SKIPPED: Visibility Filter does not include Minimap."),
					                                 *Marker->MarkerID.ToString().Left(8)));
			}
			continue;
		}
		OutHandledMarkerIDs.Add(Marker->MarkerID); // Mark this ID as handled

		VisibleMarkerIDs.Add(Marker->MarkerID);

		FVector2D FinalPosition;

		if (Marker->MarkerID == PlayerMarkerID)
		{
			FinalPosition = CanvasCenter;
			if (GEngine && ConfigAsset->bShowDebugMessages)
			{
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red,
				                                 FString::Printf(
					                                 TEXT("Player Marker: %s"), *Marker->MarkerID.ToString().Left(8)));
			}
		}
		else
		{
			FVector2D MarkerUV;
			if (!NavSubsystem->WorldToMapUV(CurrentLayer, Marker->WorldLocation, MarkerUV))
			{
				if (GEngine && ConfigAsset->bShowDebugMessages)
				{
					GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red,
					                                 FString::Printf(
						                                 TEXT(
							                                 "Marker [%s] SKIPPED: WorldToMapUV returned false (Out of Bounds?). WorldLoc: %s"),
						                                 *Marker->MarkerID.ToString().Left(8),
						                                 *Marker->WorldLocation.ToString()));
				}
				continue;
			}

			const FVector2D UVDifference = MarkerUV - PlayerUV;
			const FVector2D PixelOffset = UVDifference * CanvasSize * ConfigAsset->Zoom;

			// Phép xoay chỉ nên áp dụng khi bản đồ tĩnh, vì khi bản đồ động, hệ quy chiếu đã xoay theo người chơi.
			FVector2D RotatedPixelOffset;
			if (ConfigAsset->bShouldRotateMap)
			{
				// Khi bản đồ xoay, các marker cũng cần xoay tương đối với nó.
				// Phép xoay này bù lại phép xoay ngược của material.
				float DynamicMapYaw = 0.0f;
				switch (ConfigAsset->RotationSource)
				{
				case EMinimapRotationSource::ControlRotation: DynamicMapYaw = TrackedPawn->GetControlRotation().Yaw;
					break;
				case EMinimapRotationSource::ActorRotation: DynamicMapYaw = TrackedPawn->GetActorRotation().Yaw;
					break;
				}
				// Chúng ta không xoay offset theo TotalStaticRotation vì material đã làm điều đó
				RotatedPixelOffset = PixelOffset.GetRotated(DynamicMapYaw);
			}
			else
			{
				// Khi bản đồ tĩnh, chúng ta xoay offset theo góc tĩnh của bản đồ.
				RotatedPixelOffset = PixelOffset.GetRotated(InTotalStaticRotation);
			}

			FinalPosition = CanvasCenter + RotatedPixelOffset;
		}

		// --- Cập nhật Widget ---
		UOBMapMarkerWidget* MarkerWidget = ActiveMarkerWidgets.FindRef(Marker->MarkerID);
		if (!MarkerWidget)
		{
			MarkerWidget = CreateWidget<UOBMapMarkerWidget>(this, MarkerWidgetClass);
			if (MarkerWidget)
			{
				if (GEngine && ConfigAsset->bShowDebugMessages)
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange,
					                                 FString::Printf(
						                                 TEXT("CREATED new Minimap Widget for Marker [%s]"),
						                                 *Marker->MarkerID.ToString().Left(8)));
				}
				MinimapMarkerCanvas->AddChild(MarkerWidget);
				ActiveMarkerWidgets.Add(Marker->MarkerID, MarkerWidget);
			}
			else
			{
				continue;
			}
		}

		if (MarkerWidget->GetParent() != MinimapMarkerCanvas)
		{
			MarkerWidget->RemoveFromParent();
			MinimapMarkerCanvas->AddChild(MarkerWidget);
		}

		MarkerWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

		float IndicatorAngle = 0.0f;
		if (Marker->TrackedActor.IsValid())
		{
			IndicatorAngle = Marker->TrackedActor->GetActorRotation().Yaw + InTotalStaticRotation;
		}

		MarkerWidget->UpdateMarkerVisuals(
			Marker->ConfigAsset->IdentifierIconTexture,
			Marker->ConfigAsset->DirectionalIndicatorTexture,
			IndicatorAngle);

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget->Slot))
		{
			CanvasSlot->SetPosition(FinalPosition - (Marker->ConfigAsset->Size / 2.0f));
			CanvasSlot->SetZOrder(Marker->MarkerID == PlayerMarkerID ? 10 : 1);
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

float UOBMinimapWidget::GetCompassRadius() const
{
	if (!ConfigAsset || !MapImage)
	{
		return 100.0f; // Return a default fallback value
	}

	// The minimap is assumed to be a square or circle.
	// We take the width of the MapImage as the base size.
	const float MinimapSize = MapImage->GetCachedGeometry().GetLocalSize().X;

	// The radius of the minimap itself is half its size.
	const float MinimapRadius = MinimapSize / 2.0f;

	// The compass radius is the minimap radius plus the desired padding.
	return MinimapRadius + ConfigAsset->CompassPadding;
}
