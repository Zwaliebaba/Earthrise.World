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
};

struct HudMode
{
  std::string command; // empty when no targeting mode is on
  std::string banner;
};

struct HudAlert
{
  std::string text;
  std::string sub;                                 // a second line, or empty
  std::optional<Protocol::VoxelPosition> location; // where JUMP and Space put the camera
  bool urgent;
  double expiresSeconds; // on the client's clock; infinity for one that stays until it is replaced
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
};

/// The top bar's resources in the breach lab (M3-E2): TIME m:ss of game time, at _tick, and SHOTS n.
[[nodiscard]] std::vector<HudResource> BreachResources(std::uint32_t _tick, std::uint32_t _shotsFired);

} // namespace Client
