#pragma once

#include <DirectXMath.h>

#include "ChunkCoordinate.h"
#include "EntityId.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Client
{

// The HUD model (Hud.md §10), the part M3 builds: plain fields, built each frame from the session, which Client::Hud
// lays out and draws.

struct HudResource
{
  std::string key;
  std::string value;
  bool warn; // the value in bad
};

struct HudObjective
{
  std::string title;
  std::string detail;
  bool detailOff = false; // the detail in off text: FIRE while the trebuchet reloads (M3-E4)
};

/// What a click on the HUD does.
enum class HudControl : std::uint8_t
{
  TurnLeft,
  TurnRight,
  ZoomOut,
  ZoomIn,
  Cancel,     // the mode banner's CANCEL · ESC
  Jump,       // an alert's JUMP
  AnswerOpen, // the lab's question's OPEN · O (M3-G1)
  AnswerShut, // and its SHUT · S
  CardSlot,   // a command card's cell, by its index (M4-I3)
  GroupSlot,  // a control group's slot on the strip, by its index, 0 to 4 (M4-I4)
  SetGroup,   // the strip's SET: the next slot clicked takes the selection
  Minimap     // the minimap's well (M4-I5): a press or a drag moves the camera, and a right click orders the selection
};

/// A button that follows the mode banner's text.
struct HudModeButton
{
  HudControl control;
  std::string label;
};

struct HudMode
{
  std::string command; // empty when no mode is on
  std::string banner;
  std::vector<HudModeButton> buttons; // CANCEL · ESC for FIRE, OPEN · O and SHUT · H for the lab's question
  std::string hint = {};              // after the buttons, in dim: the question's F1 F2 VIEWS (M3-H4)
};

struct HudAlert
{
  std::string text;
  std::string sub;                                 // a second line, or empty
  std::optional<Protocol::VoxelPosition> location; // where JUMP and Space put the camera
  bool urgent;
  double expiresSeconds; // on the client's clock; infinity for one that stays until it is replaced
};

/// The selection's chip in the top bar, after the resources (M3-H3): its name, and FIRE's key, or RELOADING in off text.
struct HudSelection
{
  std::string name;
  std::string detail;
  bool detailOff; // the detail in off text, while the trebuchet reloads
};

/// A row of the readout: its key in dim and its value in its colour, both in accent while it flashes, and a bar under
/// it when it has a fill.
struct HudReadoutRow
{
  std::string key;
  std::string value;
  std::uint32_t srgb;
  bool flashing;             // it changed within Overlays::FLASH_SECONDS
  std::optional<float> fill; // HOLE's and GAP's width over the width at which the stone falls, 1 and over full
};

/// The integrity view's readout of the focused section (M3-H1), docked in the bottom-left corner.
struct HudReadout
{
  std::string title; // the section's name
  std::vector<HudReadoutRow> rows;
};

/// A row of the selection panel: KEY in dim and its value in its colour, on one line (Hud.md §5).
struct HudStatRow
{
  std::string key;
  std::string value;
  std::uint32_t srgb;
};

/// What stands for a thing in the selection panel: its figure's portrait, a cell of the image atlas (Portraits.h), or,
/// for a structure, its icon glyph as UTF-8.
struct HudPortrait
{
  std::optional<std::uint32_t> cell;
  std::string icon;
};

/// One type's tile when several are selected (Hud.md §5): its portrait, its squad count in accent, its figures, their
/// average hit points as a bar, its short label, and the active type's accent border.
struct HudPanelTile
{
  HudPortrait portrait;
  std::string count;
  std::string figures;
  std::optional<float> hit; // average hit points over their most
  std::string label;
  bool active;
};

/// One cell of the command card (Hud.md §7; M4-I3): its hotkey top left in accent, its icon glyph in the middle, its
/// label along the bottom; off text when disabled, an accent border on active-bg while its mode is on. An empty slot
/// has no command.
struct HudCommandSlot
{
  std::string command; // empty for an empty slot
  std::string hotkey;
  std::string label;
  std::string icon; // UTF-8
  bool enabled = false;
  bool active = false;
};

/// The command card for the selection's active kind (MvpPlan-M4.md, *Contracts*).
struct HudCommandCard
{
  std::string kind;
  std::uint8_t page = 0;
  std::array<HudCommandSlot, 12> slots;
};

/// A dot on the minimap (Hud.md §6; M4-I5), in pixels from its well's top-left corner: a squad's 3 px, a structure's 7.
struct HudDot
{
  DirectX::XMFLOAT2 positionPixels;
  std::uint8_t team; // 0 the attacker's red, 1 the defender's blue
  bool structure;
  bool selected; // in #ffe2a8
};

/// An alert's ping on the minimap, a square in accent, or bad when urgent.
struct HudPing
{
  DirectX::XMFLOAT2 positionPixels;
  bool urgent;
  double expiresSeconds;
};

/// The minimap's overlay over its base, which the image atlas holds (Portraits.h).
struct HudMinimap
{
  bool baseDirty = false; // the base was built again this frame, and the renderer uploads it
  bool hasBase = false;   // a base was built and the atlas holds it
  std::vector<HudDot> dots;
  std::array<DirectX::XMFLOAT2, 4> viewQuad{}; // the screen's corners on the ground, in pixels from the well's corner
  std::vector<HudPing> pings;
};

/// A structure's line in the world while the integrity view is on (ADR-021; input 23, as the owner amended it for M4-I6),
/// anchored above its box's middle as projected this frame: the line, and its name alone for where the line would
/// crowd another. The HUD lays them in order, so the first are kept whole.
struct HudWorldLabel
{
  std::string line;
  std::string name;
  DirectX::XMFLOAT2 anchorPixels;
  std::uint32_t srgb; // bad while it falls, else the text colour
  bool selected;      // laid first, and always whole
};

/// The selection panel in the deck (Hud.md §5; M4-I2), built from the selection and the update each frame.
struct HudPanel
{
  enum class Shows : std::uint8_t
  {
    Nothing,
    One,
    Several
  };

  Shows shows = Shows::Nothing;
  // One squad, laborer, engine or structure.
  HudPortrait portrait;
  std::string name;
  bool enemy = false;
  std::string figures;          // n / 6 for a squad, empty for one figure and for a structure
  std::optional<float> hit;     // the figures' average hit points over their most, for things with hit points
  std::string hitText;          // hp / max
  std::vector<HudStatRow> rows; // up to three
  // Several.
  std::string title;
  std::vector<HudPanelTile> tiles; // one per type, in Hud.md's order
};

struct HudModel
{
  std::vector<HudResource> resources;
  HudObjective objective;
  HudMode mode;
  std::vector<HudAlert> alerts;                            // newest first, at most HudAlerts::SHOWN
  Protocol::EntityId selectedEngine = Protocol::NO_ENTITY; // M4 brings squads
  bool integrityView = true;                               // F1, on by default
  bool passabilityView = false;                            // F2
  std::optional<HudReadout> readout = std::nullopt;        // while F1 is on and the focused section has a reading
  std::optional<HudSelection> selection = std::nullopt;    // while an engine is selected
  bool deck = false;                       // layout A's deck and the control groups' strip (Hud.md §3; M4-I1), which the M3 runs leave out
  HudPanel panel = {};                     // drawn in the deck's selection panel
  HudCommandCard commandCard = {};         // drawn in the deck's card (M4-I3)
  std::optional<std::string> tooltip = {}; // over the card, at right 8 and bottom 212: the hovered slot's (Hud.md §7)
  std::array<std::string, 9> groupLabels = {}; // each control group's slot after its number, empty for an empty group (M4-I4)
  bool settingGroup = false;                   // SET is on: the next slot clicked takes the selection
  HudMinimap minimap = {};                     // in the deck's minimap well (M4-I5)
  std::vector<HudWorldLabel> worldLabels = {}; // the structures' lines in the world, while F1 is on (M4-I6)
};

/// The top bar's resources in the breach lab (M3-E2): TIME m:ss of game time, at _tick, and SHOTS n.
[[nodiscard]] std::vector<HudResource> BreachResources(std::uint32_t _tick, std::uint32_t _shotsFired);

} // namespace Client
