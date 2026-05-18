# Minimap Material Setup Guide

> **Purpose**: This document walks you through creating a **custom material** for the OBNavigation minimap that matches the C++ parameters we exposed in `UOBMapWidgetBase::UpdateMapMaterial`.

---

## 1. Overview of the Required Parameters
| Parameter (C++) | Material Input | Type | Description |
|-----------------|----------------|------|-------------|
| `ViewCenterUV`  | **PlayerPosUV** (Custom Node) | `float2` | UV coordinate of the player on the map texture (centre of the view). |
| `PlayerYawRad`  | **PlayerYawRad** | `float` | Dynamic rotation from the player (control or actor rotation). |
| `ZoomAmount`    | **ZoomAmount** | `float` | Zoom multiplier – >1 zooms in, <1 zooms out. |
| `MapRotationOffsetRad` | **MapRotationOffsetRad** | `float` | Static base rotation for the whole map (alignment + custom offset). |
| `Map`           | **Map** (Texture2D) | `Texture2D` | The actual minimap texture (usually a render target or static map atlas). |
| `MapSampler`    | **MapSampler** (Sampler) | `SamplerState` | Sampler for the map texture. |
| `UV` (screen)   | **UV** (float2) | `float2` | Built‑in UV of the widget (0‑1 across the widget). |

> **NOTE**: The material **must** use the exact parameter names shown above – the C++ code sets them with `TEXT("…")`. Mismatched names will cause the material to ignore the values and the minimap will appear static.

---

## 2. Create the Material
1. In the **Content Browser**, right‑click → **Material** → name it e.g. `M_MinimapBackground`.
2. Open the material editor and set **Material Domain** to **User Interface**.
3. Set **Blend Mode** to **Transparent** (or **Masked** if you only need an opaque map). This matches the UI widget expectations.
4. Turn **Two‑Sided** on if you ever use a mirrored map.

---

## 3. Add the Custom Expression Node
1. Drag a **Custom** node onto the graph.
2. In the node's **Details** panel:
   - **Code**: paste the HLSL code you posted (see below).
   - **Inputs**: add the following inputs (order does not matter, but names must match exactly):
     ```
     UV                (float2)
     Map               (Texture2D)
     MapSampler        (SamplerState)
     PlayerPosUV       (float2)
     PlayerYawRad      (float)
     MapRotationOffsetRad (float)
     ZoomAmount        (float)
     ```
   - **Outputs**: set a single output named `Result` of type **float4** (the sampled colour).
3. Connect the **Result** output to the **Base Color** slot of the material node.

### HLSL Code for the Custom Node
```hlsl
// Input parameters
// float2 UV: The default screen texture coordinates of the UI widget (0-1).
// Texture2D Map: The map texture to sample.
// SamplerState MapSampler: The sampler for the texture.
// float2 PlayerPosUV: The player's calculated position on the map texture (U,V space).
// float PlayerYawRad: The DYNAMIC rotation based on the player (0 if map is static).
// float MapRotationOffsetRad: The STATIC base rotation for the entire map view.
// float ZoomAmount: How much to zoom in.

// The center of our screen/widget. All rotations happen around this point.
float2 Center = float2(0.5, 0.5);

// 1. Start with the screen's current UV coordinate.
float2 ScreenPixelUV = UV;

// 2. Offset so the player ends up at the centre.
float2 OffsetFromCenter = ScreenPixelUV - Center;

// 3. Apply zoom (larger ZoomAmount -> smaller offset -> zoom‑in).
OffsetFromCenter /= ZoomAmount;

// 4. Inverse dynamic player rotation.
float PlayerCos = cos(-PlayerYawRad);
float PlayerSin = sin(-PlayerYawRad);
float2x2 PlayerRotationMatrix = float2x2(PlayerCos, -PlayerSin, PlayerSin, PlayerCos);
OffsetFromCenter = mul(OffsetFromCenter, PlayerRotationMatrix);

// 5. Inverse static base rotation.
float MapCos = cos(-MapRotationOffsetRad);
float MapSin = sin(-MapRotationOffsetRad);
float2x2 MapRotationMatrix = float2x2(MapCos, -MapSin, MapSin, MapCos);
OffsetFromCenter = mul(OffsetFromCenter, MapRotationMatrix);

// 6. Final texture coordinate to sample.
float2 FinalUVsToSample = PlayerPosUV + OffsetFromCenter;

// 7. Sample the map texture.
return Map.Sample(MapSampler, FinalUVsToSample);
```

---

## 4. Wire the Material to the Widget
1. In the **Widget Blueprint** (e.g. `WBP_Minimap`), select the **Image** that displays the background (`MapImage`).
2. In the **Details** panel, set **Brush → Material** to the material you just created (`M_MinimapBackground`).
3. The widget’s C++ code (`UOBMapWidgetBase::InitializeMapWidget`) will create a **Dynamic Material Instance** from this material and store it in `MapMaterialInstance`.
4. At runtime the following parameters are set each tick:
   ```cpp
   // View‑center UV (player’s world position on the map)
   MapMaterialInstance->SetVectorParameterValue(TEXT("ViewCenterUV"), ViewCenterUVColor);

   // Dynamic yaw (radians, based on config)
   MapMaterialInstance->SetScalarParameterValue(TEXT("PlayerYawRad"), ...);

   // Zoom amount
   MapMaterialInstance->SetScalarParameterValue(TEXT("ZoomAmount"), CurrentZoom);

   // Static rotation offset (radians)
   MapMaterialInstance->SetScalarParameterValue(TEXT("MapRotationOffsetRad"), ...);
   ```

> **IMPORTANT**: The **parameter names** used here (`ViewCenterUV`, `PlayerYawRad`, `ZoomAmount`, `MapRotationOffsetRad`) must match the **input names** you defined in the Custom node. If you rename any of them, update both the material and the C++ code accordingly.

---

## 5. Verify the Setup
1. **Play the game** in the editor.
2. Open the **Output Log** – you should see lines like:
   ```
   [WBP_Minimap::NativeTick] - Trace #N … ViewCenterUV= X=0.439 Y=0.510 …
   ```
3. The minimap background should **scroll** under the player marker as you move the pawn.
4. Rotate the camera (if using `ControlRotation`) – the map should rotate smoothly.
5. Adjust **ZoomAmount** in the `UOBMinimapConfigAsset` – you’ll see the map zoom in/out instantly.

If the map stays static:
- Confirm the material instance is **created** (`MapMaterialInstance` is non‑null). Add a `UE_LOG` after `CreateDynamicMaterialInstance` if needed.
- Check the **parameter names** inside the material (hover over the Custom node inputs). They must be exactly `ViewCenterUV`, `PlayerYawRad`, `ZoomAmount`, `MapRotationOffsetRad`.
- Verify that the **Map texture** is correctly assigned to the `Map` input (use a simple test texture first).

---

## 6. Optional – Adding a Debug Overlay
If you want to visualise the player’s UV location on the map texture:
1. Add a **Scalar Parameter** called `DebugShowMarker` (default `0`).
2. In the Custom node, after `float2 FinalUVsToSample = …;` you can do:
   ```hlsl
   if (DebugShowMarker > 0.5)
   {
       // Draw a small red dot at the player position for debugging.
       if (distance(FinalUVsToSample, PlayerPosUV) < 0.005)
           return float4(1,0,0,1); // red dot
   }
   ```
3. Expose this parameter from C++ if you need to toggle it at runtime.

---

## 7. Summary Checklist
- [ ] Create material `M_MinimapBackground` (UI domain, Transparent blend). 
- [ ] Add a **Custom** node with the HLSL code above. 
- [ ] Define inputs: `UV`, `Map`, `MapSampler`, `PlayerPosUV`, `PlayerYawRad`, `MapRotationOffsetRad`, `ZoomAmount`. 
- [ ] Output -> **Base Color** (float4). 
- [ ] Assign the material to the widget’s `MapImage`. 
- [ ] Ensure C++ `UpdateMapMaterial` sets the four parameters with the exact names. 
- [ ] Test in‑game: player marker stays centred, background scrolls, rotation & zoom work. 

---

*Enjoy a smooth, professional minimap that feels premium and fully dynamic!*
