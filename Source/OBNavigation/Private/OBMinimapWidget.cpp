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

	// --- MINIMAP MARKER LOGIC ---
	if (MinimapMarkerCanvas)
	{
		UpdateMinimapMarkers(TrackedPawn, TotalStaticRotation);
	}

	// --- COMPASS LOGIC ---
	if (ConfigAsset->bIsCompassEnabled && CompassMarkerCanvas)
	{
		UpdateCompassMarkers(TrackedPawn, TotalStaticRotation);
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

void UOBMinimapWidget::UpdateCompassMarkers(const APawn* TrackedPawn, float InTotalStaticRotation)
{
	const FVector PawnLocation = TrackedPawn->GetActorLocation();
	const FVector2D CanvasCenter = CompassMarkerCanvas->GetCachedGeometry().GetLocalSize() / 2.0f;

	TSet<FGuid> VisibleMarkerIDs;

	for (const UOBMapMarker* Marker : NavSubsystem->GetAllActiveMarkers())
	{
		if (!Marker || !Marker->ConfigAsset || !Marker->ConfigAsset->Visibility.bShowOnCompass)
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

		UOBMapMarkerWidget* MarkerWidget = ActiveCompassMarkerWidgets.FindRef(Marker->MarkerID);
		if (!MarkerWidget)
		{
			// SỬA ĐỔI: Tạo đúng loại widget
			MarkerWidget = CreateWidget<UOBMapMarkerWidget>(this, MarkerWidgetClass);
			if (MarkerWidget)
			{
				CompassMarkerCanvas->AddChild(MarkerWidget);
				ActiveCompassMarkerWidgets.Add(Marker->MarkerID, MarkerWidget);
			}
			else
			{
				// Nếu CreateWidget thất bại, log lỗi và bỏ qua marker này
				UE_LOG(LogTemp, Error,
				       TEXT("[%s::%hs] - Failed to create MarkerWidget! MarkerWidgetClass might be unset."), *GetName(),
				       __FUNCTION__);
				continue; // Đi đến marker tiếp theo trong vòng lặp
			}
		}

		const FVector2D CenterPositionOnRing = FVector2D(PosX, PosY); // Vị trí tâm mong muốn
		// For compass, the position IS the indicator, so the indicator itself doesn't rotate.
		constexpr float IndicatorAngle = 0.0f; // No rotation for the indicator texture itself.

		MarkerWidget->UpdateMarkerVisuals(
			Marker->ConfigAsset->IdentifierIconTexture,
			Marker->ConfigAsset->DirectionalIndicatorTexture,
			IndicatorAngle
		);

		// --- LOGIC BÙ TRỪ PIVOT MỚI (CHO LA BÀN) ---
		// Vì indicator trên la bàn không xoay, chúng ta không cần bù trừ phức tạp.
		// Chúng ta chỉ cần định vị widget sao cho điểm pivot của nó nằm trên vòng tròn la bàn.
		const FVector2D MarkerSize = Marker->ConfigAsset->Size;
		const FVector2D Pivot = Marker->ConfigAsset->IndicatorPivot;

		// Calculate the offset from the top-left corner to the pivot point.
		const FVector2D PivotOffsetInPixels = Pivot * MarkerSize;

		// The final slot position is the target point on the ring, minus the pivot offset.
		const FVector2D SlotPosition = CenterPositionOnRing - PivotOffsetInPixels;

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget->Slot))
		{
			CanvasSlot->SetPosition(SlotPosition);
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

void UOBMinimapWidget::UpdateMinimapMarkers(const APawn* TrackedPawn, const float InTotalStaticRotation)
{
	if (!MarkerWidgetClass || !NavSubsystem || !ConfigAsset) return;

	// --- THÊM LOG KIỂM TRA ---
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

	// --- THÊM LOG KIỂM TRA ---
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
	// Lấy UV của người chơi một lần duy nhất
	if (!NavSubsystem->WorldToMapUV(CurrentLayer, TrackedPawn->GetActorLocation(), PlayerUV))
	{
		// Không thể tiếp tục nếu không có vị trí của người chơi
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


		VisibleMarkerIDs.Add(Marker->MarkerID);

		FVector2D FinalPosition;

		// --- LOGIC SỬA LỖI VÀ DEBUG ---
		if (Marker->MarkerID == PlayerMarkerID)
		{
			// TRƯỜNG HỢP ĐẶC BIỆT: Marker của người chơi LUÔN LUÔN ở giữa.
			FinalPosition = CanvasCenter;
		}
		else
		{
			// TRƯỜNG HỢP THÔNG THƯỜNG: Tính toán vị trí cho các marker khác
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
		UOBMapMarkerWidget* MarkerWidget = ActiveMinimapMarkerWidgets.FindRef(Marker->MarkerID);
		if (!MarkerWidget)
		{
			MarkerWidget = CreateWidget<UOBMapMarkerWidget>(this, MarkerWidgetClass);
			if (MarkerWidget)
			{
				// --- LOG TẠO MỚI ---
				if (GEngine && ConfigAsset->bShowDebugMessages)
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange,
					                                 FString::Printf(
						                                 TEXT("CREATED new Minimap Widget for Marker [%s]"),
						                                 *Marker->MarkerID.ToString().Left(8)));
				}
				MinimapMarkerCanvas->AddChild(MarkerWidget);
				ActiveMinimapMarkerWidgets.Add(Marker->MarkerID, MarkerWidget);
			}
			else
			{
				continue; // Bỏ qua nếu không tạo được widget
			}
		}

		float IndicatorAngle = 0.0f;
		if (Marker->TrackedActor.IsValid())
		{
			IndicatorAngle = Marker->TrackedActor->GetActorRotation().Yaw + InTotalStaticRotation;
		}

		MarkerWidget->UpdateMarkerVisuals(
			Marker->ConfigAsset->IdentifierIconTexture,
			Marker->ConfigAsset->DirectionalIndicatorTexture,
			IndicatorAngle
		);

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget->Slot))
		{
			CanvasSlot->SetPosition(FinalPosition - (Marker->ConfigAsset->Size / 2.0f));
			CanvasSlot->SetZOrder(Marker->MarkerID == PlayerMarkerID ? 10 : 1);
		}

		// --- DEBUG LOG CHO TỪNG MARKER ---
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

	// --- LOGIC DỌN DẸP POOL VỚI DEBUG ---
	TArray<FGuid> MarkersToRemove;
	for (const auto& Pair : ActiveMinimapMarkerWidgets)
	{
		if (!VisibleMarkerIDs.Contains(Pair.Key))
		{
			MarkersToRemove.Add(Pair.Key);
		}
	}

	for (const FGuid& ID : MarkersToRemove)
	{
		if (UOBMapMarkerWidget* WidgetToRemove = ActiveMinimapMarkerWidgets.FindRef(ID))
		{
			// --- LOG XÓA ---
			if (GEngine && ConfigAsset->bShowDebugMessages)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
				                                 FString::Printf(
					                                 TEXT("REMOVING Minimap Widget for Marker [%s]"),
					                                 *ID.ToString().Left(8)));
			}
			WidgetToRemove->RemoveFromParent();
		}
		ActiveMinimapMarkerWidgets.Remove(ID);
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
