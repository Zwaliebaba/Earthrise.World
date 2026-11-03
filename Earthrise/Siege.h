#pragma once

#include "CommandLog.h"
#include "Run.h"
#include "SiegeModel.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Earthrise
{

struct SiegeOptions
{
  std::optional<std::string> participant; // needed unless watch
  bool control;                           // --control: the control version (ADR-028)
  bool mirror;                            // --mirror: SiegeMirror, its weak section on the west (input 28)
  bool watch;                             // --watch: the scripted attacker plays the player, for the owner's checks and J4
  double speed;
};

/// The Siege trial (ADR-026; M4-J1): `Earthrise --siege <participant> [--control] [--mirror]`, or `--siege --watch`.
/// The scenario is loaded, or its mirror, with the defender's commander on, and the player is the attacker. The camera
/// starts on the camp from the south-west, or from the south-east in the mirror, whose sun is mirrored too. The top bar
/// reads the objective, TAKE THE KEEP and then HOLD THE KEEP'S FOOT with the storming clock's progress, and the
/// resources STONE, ARMY and TIME (Client::SiegeModel). Pause or F10 stops the clock and nothing else. Once the trial
/// ends the world stops, under its end banner.
class Siege final : public Run
{
public:
  /// Builds the scenario on a world made from Simulation::ScenarioDesc. Throws std::invalid_argument without a
  /// participant and without watch, and std::runtime_error for a scenario that holds no trial.
  Siege(const Simulation::World::Desc& _desc, const SiegeOptions& _options);

  [[nodiscard]] Simulation::World& World() override;
  [[nodiscard]] std::size_t Generation() const noexcept override;
  [[nodiscard]] Protocol::VoxelPosition Goal() const noexcept override;
  [[nodiscard]] Protocol::VoxelPosition CameraTarget() const noexcept override;
  [[nodiscard]] std::int32_t CameraQuarterTurns() const noexcept override;

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
  /// Never to a participant (M3-H4); with --watch, to the owner.
  [[nodiscard]] bool ShowsFigures() const noexcept override;
  [[nodiscard]] bool ShowsDeck() const noexcept override;
  [[nodiscard]] std::vector<Client::OverlayBox> Marks(const Client::Session& _session) const override;

  /// Every command the player issued, with its source and time, for the trial's log (M4-I7; J2 writes it).
  [[nodiscard]] const Client::CommandLog& Commands() const noexcept;

private:
  Simulation::World m_world;
  SiegeOptions m_options;
  Protocol::VoxelPosition m_keepMiddle{};
  Protocol::VoxelPosition m_goal{};
  Protocol::VoxelPosition m_camp{};
  Client::CommandLog m_commands;
  Client::SiegeModel m_model;
  // What the top bar and the banner read, from the last frame's update.
  Client::HudObjective m_objective{};
  std::vector<Client::HudResource> m_resources;
  std::optional<Client::HudMode> m_end;
};

} // namespace Earthrise
