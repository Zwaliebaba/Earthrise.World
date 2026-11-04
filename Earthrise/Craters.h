#pragma once

#include "CraterModel.h"
#include "Run.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Earthrise
{

struct CratersOptions
{
  std::string participant;
  std::uint64_t seed; // the item order's
  std::filesystem::path logFolder;
  std::uint64_t buildId;
  std::uint64_t serverDataHash;
};

/// The Siege playtest's crater items (M5-D2; MvpPlan-M5.md, *The session*, step 9; GameDesign.md, M5's values, item 12):
/// `Earthrise --craters <participant> [--seed n]`. The practice item, then C1 to C8 in the participant's order
/// (Client::CraterModel), each on a copy of the lab's enclosure struck by Simulation::StrikeCraterItem, with no engine
/// on the site. Each is set up out of sight, ticking until it is quiet, then shown with the camera on W2 at the lab's
/// framing, the mark on the ground before the damage, and the question WHICH ENGINE MADE THE DAMAGE AT THE MARK?
/// answered by R or T or the banner's buttons, with no time limit. The camera and the views work; selection and FIRE do
/// not. No result is shown. Each answer is a row of Logs\Craters-<participant>-<yyyymmdd-hhmmss>.csv.
class Craters final : public Run
{
public:
  /// Builds the enclosure on a world from Simulation::LabDesc, opens the log and sets up the first item. Throws
  /// std::runtime_error for a log that cannot be written.
  Craters(const Simulation::World::Desc& _desc, const CratersOptions& _options);
  Craters(const Craters&) = delete;
  Craters& operator=(const Craters&) = delete;
  Craters(Craters&&) = delete;
  Craters& operator=(Craters&&) = delete;
  ~Craters() override = default;

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
  [[nodiscard]] std::optional<Client::HudObjective> Objective() const override;
  [[nodiscard]] std::vector<Client::HudResource> Resources(std::uint32_t _tick, std::uint32_t _shotsFired) const override;
  /// Never: a participant answers (M3-H4).
  [[nodiscard]] bool ShowsFigures() const noexcept override;
  [[nodiscard]] bool ShowsDeck() const noexcept override;
  [[nodiscard]] std::vector<Client::OverlayBox> Marks(const Client::Session& _session) const override;

private:
  /// The current item on a fresh copy of the enclosure, struck and not yet ticked.
  void StartItem();

  Simulation::World m_pristine; // the enclosure, built once, as the lab builds its site
  Protocol::EntityId m_target;  // W2
  std::unique_ptr<Simulation::World> m_world;
  std::size_t m_generation = 0;
  Client::CraterModel m_model;
  std::ofstream m_log;
  Protocol::VoxelPosition m_mark{};
  Protocol::VoxelPosition m_cameraTarget{};
  std::uint32_t m_settleTicks = 0;
};

} // namespace Earthrise
