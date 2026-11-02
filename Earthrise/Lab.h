#pragma once

#include "BreachLab.h"
#include "LabModel.h"
#include "Scenario.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace Earthrise
{

struct LabOptions
{
  std::string participant;
  std::uint64_t seed; // the item order's
  std::filesystem::path logFolder;
  std::uint64_t buildId;
  std::uint64_t serverDataHash;
  std::uint32_t widthPixels;
  std::uint32_t heightPixels;
  float uiScale;
};

/// The breach lab (M3-G1): `Earthrise --lab <participant> [--seed n]`. Two minutes of free fire on the intact site,
/// then the practice item and L01 to L16 in the participant's order (Client::LabModel), each on a fresh copy of the
/// site: the prior shots and the asked ones run through Simulation::LabShots, a tick at a time between frames, so the
/// live world reaches the headless run's ticks and its state hash. The log, one CSV a run in Logs\ beside the
/// executable, has each item's live and headless hashes (MvpPlan-M3.md, *The lab's log*).
class Lab final : public Scenario
{
public:
  /// Builds the site and opens the log. Throws std::runtime_error when the log cannot be written.
  Lab(const Simulation::World::Desc& _desc, const LabOptions& _options);

  [[nodiscard]] Simulation::World& World() override;
  [[nodiscard]] std::size_t Generation() const noexcept override;
  [[nodiscard]] Protocol::VoxelPosition Goal() const noexcept override;
  bool Tick(Client::Session& _session, double _seconds) override;
  void Frame(const ScenarioInput& _input, Client::Session& _session) override;
  [[nodiscard]] double Speed() const noexcept override;
  [[nodiscard]] bool Paused() const noexcept override;
  [[nodiscard]] bool FireAllowed() const noexcept override;
  [[nodiscard]] bool PassabilityForced() const noexcept override;
  [[nodiscard]] bool TakesSpace() const noexcept override;
  [[nodiscard]] std::optional<Client::HudMode> Mode() const override;
  [[nodiscard]] std::optional<Client::HudObjective> Objective() const override;
  [[nodiscard]] std::vector<Client::OverlayBox> Marks(const Client::Session& _session) const override;

private:
  /// The next item's world, a copy of the pristine site, and its prior shots.
  void StartItem();
  void Log(std::string_view _text);
  [[nodiscard]] const Simulation::LabItem& SimulationItem(std::string_view _id) const;

  Simulation::World m_pristine; // the site as built, before any tick
  Simulation::LabSetup m_setup;
  std::unique_ptr<Simulation::World> m_world; // the world played, never null: free fire's, then each item's, each a copy
  std::optional<Simulation::LabShots> m_shots;
  Client::LabModel m_model;
  std::size_t m_generation = 0;
  bool m_itemPending = false; // free fire is over or an item is closed: the next world is built in Frame
  bool m_markAsked = false;
  std::ofstream m_log;
};

} // namespace Earthrise
