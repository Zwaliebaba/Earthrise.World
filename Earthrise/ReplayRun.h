#pragma once

#include "Replay.h"
#include "Run.h"
#include "SiegeModel.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Earthrise
{

/// `Earthrise --replay <file>` (M4-J2; ADR-026, decision 6): a Siege trial's replay played back on screen a tick at a
/// time, at --speed, under the trial's HUD, its commanders recomputed as it runs (ADR-027, decision 6). The player
/// watches and orders nothing. At its end, or at the first tick whose state hash differs, the clock stops under REPLAY
/// MATCHED · n TICKS or REPLAY DIFFERS AT TICK n.
class ReplayRun final : public Run
{
public:
  /// Reads _file and loads its start against _desc, the scenario _scenarioId's. Throws std::runtime_error when the file
  /// does not read, or when the start is refused, as one from another build or other server data is.
  ReplayRun(const Simulation::World::Desc& _desc, const std::filesystem::path& _file, std::string_view _scenarioId, double _speed);

  /// The scenario a replay was recorded on: the `# scenario:` line of the log beside it, of the same name, or Siege.
  [[nodiscard]] static std::string ScenarioOf(const std::filesystem::path& _file);

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
  [[nodiscard]] bool ShowsFigures() const noexcept override;
  [[nodiscard]] bool ShowsDeck() const noexcept override;
  [[nodiscard]] std::vector<Client::OverlayBox> Marks(const Client::Session& _session) const override;

private:
  std::unique_ptr<Simulation::ReplayPlayer> m_player;
  double m_speed;
  bool m_mirror;
  Protocol::VoxelPosition m_camp{};
  Client::SiegeModel m_model;
  Client::HudObjective m_objective{};
  std::vector<Client::HudResource> m_resources;
  std::optional<Client::HudMode> m_end;
};

} // namespace Earthrise
