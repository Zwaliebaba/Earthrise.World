#pragma once

#include "Run.h"
#include "SiegeModel.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Earthrise
{

/// The Siege playtest's practice ground (M5-C2; MvpPlan-M5.md, *The session*, step 3; GameDesign.md, M5's values,
/// item 7): `Earthrise --practice [<id>]`. The Practice scenario is loaded: the attacker's army as it stands in the Siege
/// camp, on open ground with nothing to attack, no castle and no defender. Layout A's HUD and every command card are
/// the trial's, so a participant learns each command before their first game. No trial runs, so there is no clock and no
/// outcome, and the facilitator closes the window to end it. The top bar reads PRACTICE · NOTHING HERE COUNTS, with
/// STONE and ARMY. Pause or F10 stops the clock, as in the trial. It writes no log and no replay; the id, when given,
/// names nothing.
class Practice final : public Run
{
public:
  /// Builds the Practice scenario on a world made from Simulation::ScenarioDesc. Throws std::runtime_error for a
  /// practice ground that holds a trial, or none of the player's units.
  Practice(const Simulation::World::Desc& _desc, double _speed);
  Practice(const Practice&) = delete;
  Practice& operator=(const Practice&) = delete;
  Practice(Practice&&) = delete;
  Practice& operator=(Practice&&) = delete;
  ~Practice() override = default;

  [[nodiscard]] Simulation::World& World() override;
  [[nodiscard]] std::size_t Generation() const noexcept override;
  [[nodiscard]] Protocol::VoxelPosition Goal() const noexcept override;
  [[nodiscard]] Protocol::VoxelPosition CameraTarget() const noexcept override;

  bool Tick(Client::Session& _session, double _seconds) override;
  void Frame(const RunInput& _input, Client::Session& _session) override;

  [[nodiscard]] double Speed() const noexcept override;
  [[nodiscard]] bool Paused() const noexcept override;
  [[nodiscard]] bool FireAllowed() const noexcept override;
  [[nodiscard]] bool SelectionAllowed() const noexcept override;
  [[nodiscard]] bool PassabilityForced() const noexcept override;
  [[nodiscard]] bool TakesSpace() const noexcept override;
  [[nodiscard]] std::optional<Client::HudMode> Mode() const override;
  [[nodiscard]] std::optional<Client::HudMode> Banner() const override;
  [[nodiscard]] std::optional<Client::HudObjective> Objective() const override;
  [[nodiscard]] std::vector<Client::HudResource> Resources(std::uint32_t _tick, std::uint32_t _shotsFired) const override;
  /// Never: a participant practises (M3-H4).
  [[nodiscard]] bool ShowsFigures() const noexcept override;
  [[nodiscard]] bool ShowsDeck() const noexcept override;
  [[nodiscard]] std::vector<Client::OverlayBox> Marks(const Client::Session& _session) const override;

private:
  Simulation::World m_world;
  double m_speed;
  Protocol::VoxelPosition m_camp{};
  Client::SiegeModel m_model; // its pause alone
  std::vector<Client::HudResource> m_resources;
};

} // namespace Earthrise
