#pragma once

#include "ChunkCoordinate.h"
#include "EntityId.h"
#include "Hud.h"
#include "HudModel.h"
#include "OverlayBox.h"
#include "Session.h"
#include "StateUpdate.h"

#include <optional>
#include <vector>

namespace Client
{

/// FIRE, the trebuchet's targeting mode (Hud.md §7 and §8; M3-E4). With a trebuchet selected and loaded, F turns it
/// on: the banner, and the attack cursor, read from the aim check it asks the session for the voxel under the pointer
/// whenever that changes or the world does (ADR-020, decision 5). A right click fires at the voxel under the pointer
/// and turns the mode off unless Shift is held, and Esc turns it off. A right click with the trebuchet selected fires
/// with no mode on too (Hud.md §8, *Order*). FIRE is disabled while the trebuchet reloads: F does nothing, and neither
/// does a right click. Every shot goes to the session as a command from a click.
class FireMode
{
public:
  /// One frame's input, as the executable reads it. The right click and the voxel are the world's: the pointer is off
  /// the HUD.
  struct Input
  {
    bool fPressed;
    bool escapePressed;
    bool rightClicked;
    bool shiftHeld;
    std::optional<Protocol::VoxelPosition> pointedVoxels; // the world voxel under the pointer
    bool worldChanged;                                    // a chunk changed since the last frame
  };

  /// Reads one frame. _engine is the selected engine's view, or nullptr when none is selected, which turns the mode
  /// off. Returns whether the mode used Esc, so that it does not also deselect.
  bool Update(const Input& _input, const Protocol::EngineView* _engine, Session& _session);

  [[nodiscard]] bool Active() const noexcept;

  /// Turns the mode off: the banner's CANCEL · ESC.
  void Cancel() noexcept;

  /// The mode banner while the mode is on, or no mode.
  [[nodiscard]] HudMode Mode() const;

  /// Whether _engine can fire now: it is loaded.
  [[nodiscard]] static bool Ready(const Protocol::EngineView& _engine) noexcept;

  /// The attack cursor, while the mode is on and the pointer is over the world: the reading's verdict and distance, in
  /// good for IN RANGE and bad for the rest, or an ellipsis in dim until the first answer comes.
  [[nodiscard]] std::optional<AttackCursor> Cursor(const Session& _session) const;

  /// The strike mark (ADR-021, decision 4): the voxel the shot would strike, in accent when IN RANGE and in bad when
  /// BLOCKED, or nothing.
  [[nodiscard]] std::vector<OverlayBox> StrikeMark(const Session& _session) const;

private:
  [[nodiscard]] const Protocol::AimReading* Reading(const Session& _session) const noexcept;

  bool m_active = false;
  Protocol::EntityId m_engine = Protocol::NO_ENTITY;
  std::optional<Protocol::VoxelPosition> m_pointed;
  std::optional<Protocol::VoxelPosition> m_asked;
};

} // namespace Client
