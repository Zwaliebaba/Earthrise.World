#pragma once

#include <array>
#include <cstdint>

namespace Client
{

// The look the client draws the world with (RendererLook.md), taken from the renderer spike (M3-C2).

/// The palette entries the two sides' faction colour is drawn in, beyond the materials' (RendererLook.md: red
/// #b23a2e, blue #2f5d93).
inline constexpr std::uint8_t ATTACKER_COLOR_INDEX = 240;
inline constexpr std::uint8_t DEFENDER_COLOR_INDEX = 241;
inline constexpr std::uint32_t ATTACKER_COLOR_SRGB = 0xb23a2e;
inline constexpr std::uint32_t DEFENDER_COLOR_SRGB = 0x2f5d93;

/// The palette entry fords are drawn in: water one world voxel deep over a solid floor draws lighter, so fords show
/// (RendererLook.md §3, index 9). Its colour is water's with its HSL lightness raised by FORD_LIGHTNESS_SCALE, which
/// RendererLook.md proposes as 15% and leaves to the spike to tune.
inline constexpr std::uint8_t FORD_COLOR_INDEX = 242;
inline constexpr float FORD_LIGHTNESS_SCALE = 1.15F;
inline constexpr std::uint8_t WATER_INDEX = 9;

// The Golden preset (RendererLook.md §6): its clear colour #d6ccba in linear values, because the view that is cleared
// is sRGB and encodes on write, its sun direction, towards the light, and its lights. The direction is the
// prototype's (-90, 75, 55) with z negated, because the prototype's +z is south and the world's is north (ADR-017):
// the sun stands to the south-west. The intensities are three.js r160's physical ones: r160 divides its Lambert term
// by pi and no longer scales its lights by pi (WebGLRenderer.useLegacyLights is false), so they are divided by pi
// where they are used, as RendererLook.md expected.
inline constexpr std::array<float, 4> CLEAR_COLOR = {0.6724F, 0.6038F, 0.4910F, 1.0F};
inline constexpr std::array<float, 3> SUN_TOWARDS = {-90.0F, 75.0F, -55.0F};
inline constexpr std::uint32_t SUN_SRGB = 0xffd8a6;
inline constexpr float SUN_INTENSITY = 2.7F;
inline constexpr std::uint32_t SKY_SRGB = 0xd4dbe6;
inline constexpr std::uint32_t GROUND_SRGB = 0x5a4a38;
inline constexpr float HEMISPHERE_INTENSITY = 1.0F;

// The sun's shadow maps (ADR-011, decision 9; RendererLook.md §6): the world's, drawn when it changes, and the
// models', drawn each frame, both over the whole world with a margin. Over the 128 m scene a static-map texel is about
// 5 cm. Under the Golden sun, 35 degrees up, ground's depth in the sun's view changes by about 1.4 texels' worth a
// texel, so the filter's 1.5-texel reach spans about 10 cm of depth. A hit is moved 10 cm out along its normal before
// it reads the maps, which puts it 17 cm above the ground along the sun's ray, and its depth gets no further bias.
// These are first values, tuned against check 11 (ADR-011, *Left open*).
inline constexpr std::uint32_t STATIC_SHADOW_TEXELS = 4096;
inline constexpr std::uint32_t DYNAMIC_SHADOW_TEXELS = 2048;
inline constexpr float SHADOW_MARGIN_METERS = 2.0F;
inline constexpr float SHADOW_NORMAL_OFFSET_METERS = 0.1F;
inline constexpr float SHADOW_DEPTH_BIAS = 0.0F;

/// A model voxel's edge (RendererLook.md §1), for how large one is on screen: units are drawn as impostors once it is
/// smaller than about 3 px (§7).
inline constexpr float MODEL_VOXEL_METERS = 0.5F;

// The camera of RendererLook.md §7: orthographic at 34 degrees, standing to the south-west, turning in quarter turns
// eased over 0.3 s, from 12 m to 260 m of view height, 84 m by default. Azimuth is measured from +z (north) towards +x
// (east), so the south-west is -135 degrees. Keys pan the view by PAN_VIEWS_PER_SECOND view heights a second.
inline constexpr float ELEVATION_DEGREES = 34.0F;
inline constexpr float AZIMUTH_DEGREES = -135.0F;
inline constexpr float DEFAULT_VIEW_HEIGHT_METERS = 84.0F;
inline constexpr float MIN_VIEW_HEIGHT_METERS = 12.0F;
inline constexpr float MAX_VIEW_HEIGHT_METERS = 260.0F;
inline constexpr float ROTATION_SECONDS = 0.3F;
inline constexpr float PAN_VIEWS_PER_SECOND = 0.8F;
/// The top bar's height in logical pixels (Hud.md §3): the camera frames the area below it.
inline constexpr float TOP_BAR_LOGICAL_PIXELS = 44.0F;

// Hud.md §2's colour tokens, as sRGB 0xrrggbb. Every panel is PANEL_SRGB at PANEL_ALPHA.
inline constexpr std::uint32_t PANEL_SRGB = 0x151310;
inline constexpr float PANEL_ALPHA = 0.92F;
inline constexpr std::uint32_t BUTTON_SRGB = 0x2a2621; // panel-deep, for buttons
inline constexpr std::uint32_t LINE_SRGB = 0x3a342c;
inline constexpr std::uint32_t TEXT_SRGB = 0xf3ede2;
inline constexpr std::uint32_t DIM_SRGB = 0xa69c8a;
inline constexpr std::uint32_t SUB_SRGB = 0xd8d0c2;
inline constexpr std::uint32_t ACCENT_SRGB = 0xe8b86a;
inline constexpr std::uint32_t ACTIVE_BACKGROUND_SRGB = 0x4a3d22;
inline constexpr std::uint32_t BAD_SRGB = 0xd2584a;
inline constexpr std::uint32_t GOOD_SRGB = 0x9cc26a;
inline constexpr std::uint32_t OFF_SRGB = 0x6a6258;

// The trebuchet's poses (GameDesign.md, M3's values, item 16; MvpAssets.md, *Poses, projectiles, gates and debris*):
// S3c, cocked, at S3's placement moved 5 m along the model's own -x before its turn, as arithmetic on the files after
// the bake's trim and mirror; and S3, released, for RELEASED_POSE_TICKS after it fires.
inline constexpr float COCKED_POSE_OFFSET_X_METERS = -5.0F;
inline constexpr std::uint32_t RELEASED_POSE_TICKS = 20;

} // namespace Client
