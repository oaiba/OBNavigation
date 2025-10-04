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
			// Set the STATIC map rotation offset ONE TIME. This uses the new parameter.
			MinimapMaterialInstance->SetScalarParameterValue("MapRotationOffsetRad", FMath::DegreesToRadians(MapRotationOffset));
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
		// Update the STATIC map rotation offset when called. This uses the new parameter.
		MinimapMaterialInstance->SetScalarParameterValue("MapRotationOffsetRad", FMath::DegreesToRadians(MapRotationOffset));
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
		const float PlayerIconYaw = TrackedPawn->GetActorRotation().Yaw;
		PlayerIcon->SetRenderTransformAngle(PlayerIconYaw);
	}

	// --- LOGIC CẬP NHẬT BẢN ĐỒ ---
	if (CurrentLayer)
	{
		if (FVector2D PlayerUV; NavSubsystem->WorldToMapUV(CurrentLayer, TrackedPawn->GetActorLocation(), PlayerUV))
		{
			// Update player's position on the map
			MinimapMaterialInstance->SetVectorParameterValue("PlayerPositionUV", FLinearColor(PlayerUV.X, PlayerUV.Y, 0.0f, 0.0f));

			// --- LOGIC CẬP NHẬT DYNAMIC ROTATION ---
			float DynamicMapYaw = 0.0f; // Default to 0 (no dynamic rotation)

			// If the map is set to rotate dynamically, calculate the yaw.
			if (bShouldRotateMap)
			{
				switch (RotationSource)
				{
				case EMinimapRotationSource::ControlRotation:
					DynamicMapYaw = TrackedPawn->GetControlRotation().Yaw;
					break;
				case EMinimapRotationSource::ActorRotation:
					DynamicMapYaw = TrackedPawn->GetActorRotation().Yaw;
					break;
				}
			}
			
			// Set the DYNAMIC rotation parameter. If map is static, this will always be 0.
			MinimapMaterialInstance->SetScalarParameterValue("PlayerYaw", FMath::DegreesToRadians(DynamicMapYaw));

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