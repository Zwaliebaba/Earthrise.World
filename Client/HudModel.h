#pragma once

#include "ChunkCoordinate.h"
#include "EntityId.h"

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
  AnswerShut  // and its SHUT · S
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
};

/// The top bar's resources in the breach lab (M3-E2): TIME m:ss of game time, at _tick, and SHOTS n.
[[nodiscard]] std::vector<HudResource> BreachResources(std::uint32_t _tick, std::uint32_t _shotsFired);

} // namespace Client
