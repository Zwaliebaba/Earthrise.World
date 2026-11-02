#pragma once

#include "ChunkCoordinate.h"
#include "HudModel.h"
#include "OverlayBox.h"
#include "Session.h"
#include "World.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Earthrise
{

/// What the player did in a frame that a run reads.
struct RunInput
{
  double nowSeconds;
  bool openPressed;          // O, or the question's OPEN · O
  bool shutPressed;          // H, or SHUT · H
  bool nextPressed;          // Space
  std::int32_t quarterTurns; // the camera's moves in the frame
  float zoomNotches;
  bool integrityOn; // the views as shown
  bool passabilityOn;
};

/// What Earthrise runs (ADR-020): the M2 breach run live, the breach lab (M3-G1), or a scenario such as the Siege castle
/// (M4-B4). It owns the world and decides what each tick hands it, how fast the clock runs, and what the HUD shows over
/// its own. It was Scenario until M4-B4, whose Simulation::Scenario took the header's name.
class Run
{
public:
  Run() = default;
  Run(const Run&) = delete;
  Run& operator=(const Run&) = delete;
  Run(Run&&) = delete;
  Run& operator=(Run&&) = delete;
  virtual ~Run() = default;

  /// The world being played, which the lab replaces between items.
  [[nodiscard]] virtual Simulation::World& World() = 0;
  /// Rises each time the world is replaced, so that the client forgets the old one.
  [[nodiscard]] virtual std::size_t Generation() const noexcept = 0;
  /// The courtyard's middle, which BREACH OPEN AT W2 watches.
  [[nodiscard]] virtual Protocol::VoxelPosition Goal() const noexcept = 0;
  /// Where the camera looks when the world is new.
  [[nodiscard]] virtual Protocol::VoxelPosition CameraTarget() const noexcept = 0;

  /// One tick: hands the world the session's queries, its commands where the player may command and the scenario's
  /// own, and ticks it. Returns false when it did not tick.
  virtual bool Tick(Client::Session& _session, double _seconds) = 0;
  /// After the frame's ticks.
  virtual void Frame(const RunInput& _input, Client::Session& _session) = 0;

  [[nodiscard]] virtual double Speed() const noexcept = 0;
  [[nodiscard]] virtual bool Paused() const noexcept = 0;
  [[nodiscard]] virtual bool FireAllowed() const noexcept = 0;
  [[nodiscard]] virtual bool PassabilityForced() const noexcept = 0;
  /// Whether Space is the scenario's this frame, and not the jump to the newest alert.
  [[nodiscard]] virtual bool TakesSpace() const noexcept = 0;
  /// The banner and the objective over the HUD's own, or nothing.
  [[nodiscard]] virtual std::optional<Client::HudMode> Mode() const = 0;
  [[nodiscard]] virtual std::optional<Client::HudObjective> Objective() const = 0;
  /// Boxes the run marks the world with: the lab's mark.
  [[nodiscard]] virtual std::vector<Client::OverlayBox> Marks(const Client::Session& _session) const = 0;
};

} // namespace Earthrise
