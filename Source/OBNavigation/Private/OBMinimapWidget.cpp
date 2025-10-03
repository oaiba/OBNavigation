// Fill out your copyright notice in the Description page of Project Settings.


#include "OBMinimapWidget.h"

#include "OBMapLayerAsset.h"
#include "OBNavigationSubsystem.h"
#include "Components/Image.h"

void UOBMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// --- BƯỚC 1: Khởi tạo các thành phần UI và Material trước ---
	if (MapImage)
	{
		// Create a dynamic material instance from the material assigned to the image in the editor.
		// This MUST be done before we try to use it.
		MinimapMaterialInstance = MapImage->GetDynamicMaterial();
		if (!MinimapMaterialInstance)
		{
			UE_LOG(LogTemp, Error,
				   TEXT("[%s::%hs] - MapImage does not have a material assigned or it cannot be made dynamic."),
				   *GetName(), __FUNCTION__);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - MapImage is not bound in the widget blueprint!"), *GetName(), __FUNCTION__);
	}


	// --- BƯỚC 2: Lấy Subsystem và gán Delegate ---
	if (const UGameInstance* GI = GetGameInstance())
	{
		NavSubsystem = GI->GetSubsystem<UOBNavigationSubsystem>();
	}

	if (NavSubsystem)
	{
		// Bind to the delegate to react to future map changes
		NavSubsystem->OnMinimapLayerChanged.AddDynamic(this, &UOBMinimapWidget::OnMinimapLayerChanged);

		// --- BƯỚC 3: Thực hiện thiết lập ban đầu SAU KHI mọi thứ đã sẵn sàng ---
		// Now that MinimapMaterialInstance is valid, we can safely call this to set the initial texture.
		OnMinimapLayerChanged(NavSubsystem->GetCurrentMinimapLayer());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - OBNavigationSubsystem is not valid!"), *GetName(), __FUNCTION__);
		if (MapImage) MapImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	// If anything failed, hide the widget to prevent errors in Tick
	if (!MinimapMaterialInstance || !NavSubsystem)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UOBMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Ensure everything is valid before updating
	if (!NavSubsystem || !MinimapMaterialInstance || !NavSubsystem->GetTrackedPlayerPawn())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - Skipping minimap update due to invalid subsystem, material, or tracked pawn."), *GetName(), __FUNCTION__);
		return;
	}

	const APawn* TrackedPawn = NavSubsystem->GetTrackedPlayerPawn();
	const UOBMapLayerAsset* CurrentLayer = NavSubsystem->GetCurrentMinimapLayer();

	// Calculate the player's current UV position on the map
	if (FVector2D PlayerUV; CurrentLayer && NavSubsystem->WorldToMapUV(CurrentLayer, TrackedPawn->GetActorLocation(),
																	   PlayerUV))
	{
		// Update material parameters
		MinimapMaterialInstance->SetVectorParameterValue("PlayerPositionUV", FLinearColor(PlayerUV.X, PlayerUV.Y, 0));

		// Use PlayerController's ControlRotation for camera orientation
		const FRotator ControlRotation = TrackedPawn->GetControlRotation();
		// Works for character even if not directly PlayerController
		MinimapMaterialInstance->SetScalarParameterValue("PlayerYaw", FMath::DegreesToRadians(ControlRotation.Yaw));
		MinimapMaterialInstance->SetScalarParameterValue("Zoom", Zoom); // Ensure zoom is always set
	}

	if (PlayerIcon)
	{
		// The player icon itself does not move, but its rotation should match the player's view
		// This rotation is local to the widget, so use GetControlRotation().Yaw
		PlayerIcon->SetRenderTransformAngle(TrackedPawn->GetControlRotation().Yaw);
	}
}

// ReSharper disable once CppMemberFunctionMayBeConst
// ReSharper disable once CppParameterMayBeConstPtrOrRef
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
		MinimapMaterialInstance->SetScalarParameterValue("Zoom", Zoom);
		MapImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		// No valid map, hide the minimap image
		MapImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}
