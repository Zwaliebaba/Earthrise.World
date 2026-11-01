#pragma once

#include "HudModel.h"
#include "ScreenQuad.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Client
{

/// A rectangle of the target in whole pixels, from its top-left corner.
struct PixelRect
{
  std::int32_t xPixels;
  std::int32_t yPixels;
  std::int32_t widthPixels;
  std::int32_t heightPixels;

  [[nodiscard]] bool Contains(std::int32_t _xPixels, std::int32_t _yPixels) const noexcept;
  /// Whether this lies wholly inside _outer.
  [[nodiscard]] bool Inside(const PixelRect& _outer) const noexcept;
  [[nodiscard]] bool Overlaps(const PixelRect& _other) const noexcept;
};

/// What a click on the HUD does.
enum class HudControl : std::uint8_t
{
  TurnLeft,
  TurnRight,
  ZoomOut,
  ZoomIn,
  Cancel, // the mode banner's CANCEL · ESC
  Jump    // an alert's JUMP
};

/// A control's hit target.
struct HudTarget
{
  HudControl control;
  std::size_t alert; // for Jump, the alert's index in HudModel::alerts
  PixelRect rect;
};

/// The attack cursor (Hud.md §8): a ring around the pointer, and its reading beside it, both in the reading's colour.
struct AttackCursor
{
  std::string label;
  std::uint32_t srgb;
};

/// A string the HUD drew: the cell it takes, and the element it has to fit in.
struct HudText
{
  PixelRect cell;
  PixelRect element;
};

/// The HUD's first pieces (Hud.md §3; M3-E2), laid out from the model in logical pixels at the UI scale and drawn as
/// interface quads on whole pixels: the top bar, with the resources on the left, the objective in the middle of the
/// room left between them and the camera buttons ↺ ↻ - + on the right; the mode banner, with CANCEL · ESC; and the
/// alerts, newest first. It keeps what it laid out, so the next frame's clicks land on what was drawn, and a click
/// anywhere on it is the HUD's, never the world's.
class Hud
{
public:
  /// The camera buttons are drawn this size, and each hit target is HIT_LOGICAL_PIXELS square around its button, so
  /// the targets abut and never overlap (Hud.md §2, *Hit targets*; GameDesign.md, M3's values, item 8).
  static constexpr float BUTTON_LOGICAL_PIXELS = 36.0F;
  static constexpr float HIT_LOGICAL_PIXELS = 44.0F;

  /// Lays _model out over a target _widthPixels wide at _uiScale and appends its quads, in drawing order.
  void Build(const HudModel& _model, std::uint32_t _widthPixels, float _uiScale, std::vector<ScreenQuad>& _quads);

  /// Appends the attack cursor around the pointer at (_xPixels, _yPixels): the ring centred on it and the reading on a
  /// panel to its right. It covers nothing: a click through it is the world's.
  void LayCursor(const AttackCursor& _cursor, std::int32_t _xPixels, std::int32_t _yPixels, float _uiScale,
                 std::vector<ScreenQuad>& _quads);

  /// The control under the pixel, as last built, or nothing.
  [[nodiscard]] std::optional<HudTarget> HitTest(std::int32_t _xPixels, std::int32_t _yPixels) const noexcept;

  /// Whether the HUD, as last built, covers the pixel, so that a click there is the HUD's.
  [[nodiscard]] bool Covers(std::int32_t _xPixels, std::int32_t _yPixels) const noexcept;

  [[nodiscard]] std::span<const HudTarget> Targets() const noexcept;
  [[nodiscard]] std::span<const HudText> Texts() const noexcept;

private:
  std::vector<PixelRect> m_panels;
  std::vector<HudTarget> m_targets;
  std::vector<HudText> m_texts;
};

} // namespace Client
