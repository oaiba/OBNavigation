// Fill out your copyright notice in the Description page of Project Settings.

#include "OBMinimapWidget.h"
#include "Components/Image.h"
#include "OBNavigationSubsystem.h"
#include "OBMapLayerAsset.h"
#include "Materials/MaterialInstanceDynamic.h"

void UOBMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Create Dynamic Material Instance first
	if (MapImage)
	{
		MinimapMaterialInstance = MapImage->GetDynamicMaterial();
		if (MinimapMaterialInstance)
		{
			// The total static rotation is the sum of the alignment and the manual offset.
			const float TotalStaticRotation = MapRotationOffset + GetAlignmentAngle();
			MinimapMaterialInstance->SetScalarParameterValue("MapRotationOffsetRad", FMath::DegreesToRadians(TotalStaticRotation));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - Failed to create Dynamic Material Instance for MapImage."), *GetName(), __FUNCTION__);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - MapImage is not bound in the widget blueprint!"), *GetName(), __FUNCTION__);
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
		MinimapMaterialInstance->SetScalarParameterValue("MapRotationOffsetRad", FMath::DegreesToRadians(TotalStaticRotation));
	}
}

void UOBMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!NavSubsystem || !MinimapMaterialInstance || !NavSubsystem->GetTrackedPlayerPawn())
	{
		// This log is commented out to prevent spam, but can be re-enabled for debugging.
		// UE_LOG_ONCE(LogTemp, Warning, TEXT("[%s::%hs] - Skipping minimap update due to invalid subsystem, material, or tracked pawn."), *GetName(), __FUNCTION__);
		return;
	}

	const APawn* TrackedPawn = NavSubsystem->GetTrackedPlayerPawn();
	const UOBMapLayerAsset* CurrentLayer = NavSubsystem->GetCurrentMinimapLayer();

	// --- LOGIC CẬP NHẬT ICON NGƯỜI CHƠI ---
	// The player icon ALWAYS rotates based on the character's forward direction (ActorRotation).
	if (PlayerIcon)
	{
		const float PlayerActorYaw = TrackedPawn->GetActorRotation().Yaw;
		// The final icon angle is the actor's world yaw PLUS the map's base alignment rotation.
		const float FinalIconYaw = PlayerActorYaw + GetAlignmentAngle();
		PlayerIcon->SetRenderTransformAngle(FinalIconYaw);
	}

	// --- LOGIC CẬP NHẬT BẢN ĐỒ ---
	if (CurrentLayer)
	{
		if (FVector2D PlayerUV; NavSubsystem->WorldToMapUV(CurrentLayer, TrackedPawn->GetActorLocation(), PlayerUV))
		{
			// Update player's position on the map
			MinimapMaterialInstance->SetVectorParameterValue("PlayerPositionUV", FLinearColor(PlayerUV.X, PlayerUV.Y, 0.0f, 0.0f));


			// If the map is set to rotate dynamically, calculate the yaw.
			if (bShouldRotateMap)
			{
				// --- LOGIC CẬP NHẬT DYNAMIC ROTATION ---
				float DynamicMapYaw = 0.0f; // Default to 0 (no dynamic rotation)

				switch (RotationSource)
				{
				case EMinimapRotationSource::ControlRotation:
					DynamicMapYaw = TrackedPawn->GetControlRotation().Yaw;
					break;
				case EMinimapRotationSource::ActorRotation:
					DynamicMapYaw = TrackedPawn->GetActorRotation().Yaw;
					break;
				}
				
				// The final dynamic rotation also needs to be aligned with the map's base.
				const float FinalDynamicRotation = DynamicMapYaw + GetAlignmentAngle();
				MinimapMaterialInstance->SetScalarParameterValue("PlayerYaw", FMath::DegreesToRadians(FinalDynamicRotation));
			}

			// Always update zoom
			MinimapMaterialInstance->SetScalarParameterValue("Zoom", Zoom);
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
		// A new map is active, update the texture in our material
		MinimapMaterialInstance->SetTextureParameterValue("MapTexture", NewLayer->MapTexture);
		MapImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		// No valid map, hide the minimap image
		MapImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}

float UOBMinimapWidget::GetAlignmentAngle() const
{
	switch (MapAlignment)
	{
	case EMapAlignment::Forward_PlusX:   return 0.0f;
	case EMapAlignment::Right_PlusY:     return 90.0f;
	case EMapAlignment::Backward_MinusX: return 180.0f;
	case EMapAlignment::Left_MinusY:     return -90.0f;
	default:                             return 0.0f;
	}
}
