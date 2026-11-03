#pragma once

#include <DirectXMath.h>

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
  bool engineSelected; // an engine is selected at the frame's end (M3-H3)
  bool pausePressed;   // Pause or F10 (MvpPlan.md, M4, input 25)
};

/// What a frame drew the world with, and the HUD's bands over it, after the frame's ticks (M4-J2).
struct DrawnFrame
{
  DirectX::XMFLOAT4X4 viewProjection;
  std::uint32_t widthPixels;
  std::uint32_t heightPixels;
  float topBarPixels;
  float deckPixels; // 0 without the deck
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
  /// And how many quarter turns clockwise of the south-west it stands: the mirrored castle's is a turn back (M4-J1).
  [[nodiscard]] virtual std::int32_t CameraQuarterTurns() const noexcept
  {
    return 0;
  }

  /// One tick: hands the world the session's queries, its commands where the player may command and the scenario's
  /// own, and ticks it. Returns false when it did not tick.
  virtual bool Tick(Client::Session& _session, double _seconds) = 0;
  /// After the frame's ticks.
  virtual void Frame(const RunInput& _input, Client::Session& _session) = 0;
  /// With what the frame draws the world: where the trial's log reads the breaches' places on screen (M4-J2).
  virtual void Drew(const DrawnFrame& /* _frame */) {}

  [[nodiscard]] virtual double Speed() const noexcept = 0;
  [[nodiscard]] virtual bool Paused() const noexcept = 0;
  [[nodiscard]] virtual bool FireAllowed() const noexcept = 0;
  /// Whether a click selects an engine (M3-H3): never in the lab's items.
  [[nodiscard]] virtual bool SelectionAllowed() const noexcept = 0;
  [[nodiscard]] virtual bool PassabilityForced() const noexcept = 0;
  /// Whether Space is the scenario's this frame, and not the jump to the newest alert.
  [[nodiscard]] virtual bool TakesSpace() const noexcept = 0;
  /// The banner and the objective over the HUD's own, or nothing.
  [[nodiscard]] virtual std::optional<Client::HudMode> Mode() const = 0;
  [[nodiscard]] virtual std::optional<Client::HudObjective> Objective() const = 0;
  /// A banner over every mode's, FIRE's and the card's: the Siege trial's end and its pause (M4-J1).
  [[nodiscard]] virtual std::optional<Client::HudMode> Banner() const
  {
    return std::nullopt;
  }
  /// The top bar's resources, from the client's _tick and the player's _shotsFired as the client counts them.
  [[nodiscard]] virtual std::vector<Client::HudResource> Resources(std::uint32_t _tick, std::uint32_t _shotsFired) const = 0;
  /// Whether the frame's figures are drawn in the bottom-left corner as well as in the title bar: never to a
  /// participant (M3-H4).
  [[nodiscard]] virtual bool ShowsFigures() const noexcept = 0;
  /// Whether the HUD has layout A's deck and the control groups' strip, with the UI scale capped to fit it and the
  /// camera framing the area above it (M4-I1): the Siege castle's runs; M3's lab and breach run keep M3's HUD.
  [[nodiscard]] virtual bool ShowsDeck() const noexcept = 0;
  /// Boxes the run marks the world with: the lab's mark.
  [[nodiscard]] virtual std::vector<Client::OverlayBox> Marks(const Client::Session& _session) const = 0;
};

} // namespace Earthrise
