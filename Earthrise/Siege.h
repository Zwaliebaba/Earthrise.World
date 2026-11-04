#pragma once

#include "CommandLog.h"
#include "Replay.h"
#include "ReasonPrompt.h"
#include "Run.h"
#include "SiegeLog.h"
#include "SiegeModel.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
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
  /// --game: the playtest's game, whose version and layout control and mirror already hold (M5-C1), and with it the reason
  /// prompt (M5-C3).
  std::optional<Client::PlaytestGame> playtest;
  double speed;
  // The trial's log and its replay (M4-J2), in this folder, and what its comment lines record.
  std::filesystem::path logFolder;
  std::uint64_t buildId;
  std::uint64_t serverDataHash;
  std::uint32_t widthPixels;
  std::uint32_t heightPixels;
  float uiScale;
};

/// The Siege trial (ADR-026; M4-J1): `Earthrise --siege <participant> [--control] [--mirror]`, or `--siege --watch`.
/// The scenario is loaded, or its mirror, with the defender's commander on, and the player is the attacker. The camera
/// starts on the camp from the south-west, or from the south-east in the mirror, whose sun is mirrored too. The top bar
/// reads the objective, TAKE THE KEEP and then HOLD THE KEEP'S FOOT with the storming clock's progress, and the
/// resources STONE, ARMY and TIME (Client::SiegeModel). Pause or F10 stops the clock and nothing else. Once the trial
/// ends the world stops, under its end banner.
///
/// Each run writes the trial's log, Logs\Siege-<participant>-<voxel|control>-<scenario>-<yyyymmdd-hhmmss>.csv, a row at
/// a time as the ticks and frames make them (Client::SiegeLog; MvpPlan-M4.md, *The trial's log*), and its replay beside
/// it as .errp, at the trial's end and when the run closes (ADR-026, decision 6). A run with --watch and no participant
/// logs as the participant "watch".
class Siege final : public Run
{
public:
  /// Builds the scenario on a world made from Simulation::ScenarioDesc, and opens the log. Throws
  /// std::invalid_argument without a participant and without watch, and std::runtime_error for a scenario that holds no
  /// trial or a log that cannot be written.
  Siege(const Simulation::World::Desc& _desc, const SiegeOptions& _options);
  Siege(const Siege&) = delete;
  Siege& operator=(const Siege&) = delete;
  Siege(Siege&&) = delete;
  Siege& operator=(Siege&&) = delete;
  /// Ends the log and writes the replay, if the trial's end has not.
  ~Siege() override;

  [[nodiscard]] Simulation::World& World() override;
  [[nodiscard]] std::size_t Generation() const noexcept override;
  [[nodiscard]] Protocol::VoxelPosition Goal() const noexcept override;
  [[nodiscard]] Protocol::VoxelPosition CameraTarget() const noexcept override;
  [[nodiscard]] std::int32_t CameraQuarterTurns() const noexcept override;

  bool Tick(Client::Session& _session, double _seconds) override;
  void Frame(const RunInput& _input, Client::Session& _session) override;
  void Drew(const DrawnFrame& _frame) override;

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
  /// A stone in flight, as its ShotFired named it.
  struct Fired
  {
    std::uint8_t player;
    Protocol::EntityId engine;
    Protocol::VoxelPosition target;
  };
  /// A breach or a warning whose place on screen the next frame reads.
  struct Pending
  {
    Protocol::EntityId structure;
    bool warning;
    std::uint32_t tick;
  };

  /// The log's rows from the tick the world has just run at _seconds on the client's clock: its events, its new collapses
  /// and the player's commands; and under --game, a reason prompt for siege work of a kind the game has not yet seen (M5-C3).
  void LogTick(double _seconds);
  /// The name of the structure whose box holds _voxels, or empty.
  [[nodiscard]] std::string NameAt(const Protocol::VoxelPosition& _voxels) const;
  [[nodiscard]] std::string NameOf(Protocol::EntityId _structure) const;
  /// The rows made so far, to the file.
  void WriteRows();
  /// The log's end, its rows and the replay, once.
  void Finish(std::optional<Protocol::TrialOutcome> _outcome);

  Simulation::World m_world;
  SiegeOptions m_options;
  Protocol::VoxelPosition m_keepMiddle{};
  Protocol::VoxelPosition m_goal{};
  Protocol::VoxelPosition m_camp{};
  Client::CommandLog m_commands;
  Client::SiegeModel m_model;
  Client::ReasonPrompt m_prompt; // under --game alone (M5-C3)
  // What the top bar and the banner read, from the last frame's update.
  Client::HudObjective m_objective{};
  std::vector<Client::HudResource> m_resources;
  std::optional<Client::HudMode> m_end;
  // The trial's log and its replay (M4-J2).
  std::optional<Simulation::ReplayRecorder> m_recorder;
  Client::SiegeLog m_log;
  std::ofstream m_logFile;
  std::filesystem::path m_replayPath;
  std::map<Protocol::EntityId, Fired> m_fired;
  std::set<Protocol::EntityId> m_collapsesSeen;
  std::vector<Pending> m_pending;
  std::size_t m_commandsLogged = 0;
  bool m_finished = false;
};

} // namespace Earthrise
