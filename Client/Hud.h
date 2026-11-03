#pragma once

#include "HudModel.h"
#include "ScreenQuad.h"

#include <array>
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

/// Where layout A's deck and the control groups' strip were laid (Hud.md §3; M4-I1), in the target's pixels.
struct HudDeck
{
  static constexpr std::size_t CARD_CELLS = 12; // 4 columns, 3 rows
  static constexpr std::size_t GROUP_SLOTS = 6; // groups 1 to 5, then SET

  PixelRect deck;                            // the whole of it, its border along its top included
  PixelRect minimap;                         // the minimap's well, left
  PixelRect selection;                       // the selection's panel, centre, the rest
  PixelRect card;                            // the command card's grid, right
  std::array<PixelRect, CARD_CELLS> cells;   // row by row from the top-left
  std::array<PixelRect, GROUP_SLOTS> groups; // the strip above the deck
};

/// A string the HUD drew: the cell it takes, and the element it has to fit in.
struct HudText
{
  PixelRect cell;
  PixelRect element;
};

/// The HUD (Hud.md §3; M3-E2), laid out from the model in logical pixels at the UI scale and drawn as interface quads
/// on whole pixels: the top bar, with the resources on the left, the objective in the middle of the screen and the
/// camera buttons ↺ ↻ - + on the right; the mode banner, with CANCEL · ESC; layout A's deck along the bottom, with the
/// minimap's well, the selection's panel and the command card's grid, and the control groups' strip above it (M4-I1),
/// when the model has one; the integrity readout above the deck, or in the bottom-left corner without one (M3-H1); and
/// the alerts, newest first. It keeps what it laid out, so the next frame's clicks
/// land on what was drawn, and a click anywhere on it is the HUD's, never the world's.
class Hud
{
public:
  /// The camera buttons are drawn this size, and each hit target is HIT_LOGICAL_PIXELS square around its button, so
  /// the targets abut and never overlap (Hud.md §2, *Hit targets*; GameDesign.md, M3's values, item 8).
  static constexpr float BUTTON_LOGICAL_PIXELS = 36.0F;
  static constexpr float HIT_LOGICAL_PIXELS = 44.0F;

  /// The readout takes at most this much of the screen's height, laying out at a smaller UI scale where it would take
  /// more: at the default framing W2's foot is drawn 55 % of the way down a 1280 x 720 screen (M3-H1).
  static constexpr float READOUT_HEIGHT_FRACTION = 0.3F;

  /// Layout A's deck is this tall, fixed to the bottom (Hud.md §3): the camera frames the area above it.
  static constexpr float DECK_LOGICAL_PIXELS = 204.0F;

  /// Lays _model out over a target _widthPixels by _heightPixels at _uiScale and appends its quads, in drawing order. An
  /// alert that would meet the readout is left out, with those older than it.
  void Build(const HudModel& _model, std::uint32_t _widthPixels, std::uint32_t _heightPixels, float _uiScale,
             std::vector<ScreenQuad>& _quads);

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
  /// Where the readout was laid, as last built, or nothing.
  [[nodiscard]] std::optional<PixelRect> Readout() const noexcept;
  /// Where the deck and the strip were laid, as last built, or nothing when the model had no deck.
  [[nodiscard]] std::optional<HudDeck> Deck() const noexcept;
  /// Every panel as last built: the top bar, the deck, the strip, the banner, the readout and the alerts.
  [[nodiscard]] std::span<const PixelRect> Panels() const noexcept;

private:
  std::vector<PixelRect> m_panels;
  std::vector<HudTarget> m_targets;
  std::vector<HudText> m_texts;
  std::optional<PixelRect> m_readout;
  std::optional<HudDeck> m_deck;
};

} // namespace Client
