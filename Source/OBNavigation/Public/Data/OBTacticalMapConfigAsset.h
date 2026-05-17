#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OBTacticalMapConfigAsset.generated.h"

/**
 * Configuration for the full-screen tactical map view.
 */
UCLASS(BlueprintType)
class OBNAVIGATION_API UOBTacticalMapConfigAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Initial tactical map zoom multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation|TacticalMap", meta = (ClampMin = "0.01"))
	float InitialZoom = 1.0f;

	/** Minimum tactical map zoom multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation|TacticalMap", meta = (ClampMin = "0.01"))
	float MinZoom = 0.25f;

	/** Maximum tactical map zoom multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation|TacticalMap", meta = (ClampMin = "0.01"))
	float MaxZoom = 8.0f;

	/** Zoom delta applied for each AddZoomInput step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation|TacticalMap", meta = (ClampMin = "0.01"))
	float ZoomStep = 0.25f;

	/** Slate units per second used by continuous pan input. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation|TacticalMap", meta = (ClampMin = "0.0"))
	float PanSpeed = 600.0f;

	/** Keeps the view center inside the active map layer UV bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation|TacticalMap")
	bool bClampViewToMapBounds = true;

	/** Shows the tracked player's marker on the tactical map when its marker config also allows FullMap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation|TacticalMap")
	bool bShowPlayerMarker = false;

	/** Shows the compass ring widget if the tactical map Blueprint includes one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation|TacticalMap")
	bool bShowCompassRing = false;

	/** Rotates the tactical map with the tracked player using the minimap rotation source. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation|TacticalMap")
	bool bRotateWithPlayer = false;

	/** Multiplies marker config sizes on the tactical map. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OBNavigation|TacticalMap", meta = (ClampMin = "0.01"))
	float MarkerScale = 1.0f;
};
