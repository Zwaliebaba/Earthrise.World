#include "pch.h"

#include "Siege.h"

#include "BreachRun.h"
#include "Scenario.h"
#include "ServerData.h"
#include "DeckCoverage.h"
#include "SiegeAttacker.h"
#include "Strings.h"

#include <chrono>

namespace Earthrise
{

namespace
{

constexpr std::uint8_t PLAYER = Simulation::BreachRun::PLAYER; // the Siege castle's attacker
constexpr std::string_view SIEGE_SCENARIO = "Siege";
constexpr std::string_view SIEGE_MIRROR_SCENARIO = "SiegeMirror";
constexpr std::string_view GATEHOUSE_TEMPLATE = "T5";

/// _structure's box, both corners in voxels, its far corner one past: its footprint after the turn and its height.
std::pair<Protocol::VoxelPosition, Protocol::VoxelPosition> BoxOf(const Simulation::World& _world, const Simulation::Structure& _structure)
{
  const Simulation::ServerTemplate* shape = _world.Data().FindTemplate(_structure.templateId);
  if (shape == nullptr)
  {
    return {_structure.originVoxels, _structure.originVoxels};
  }
  const bool across = _structure.quarterTurns % 2 == 1;
  const auto width = static_cast<std::int32_t>(across ? shape->depthVoxels : shape->widthVoxels);
  const auto depth = static_cast<std::int32_t>(across ? shape->widthVoxels : shape->depthVoxels);
  const Protocol::VoxelPosition& origin = _structure.originVoxels;
  return {origin,
          {.xVoxels = origin.xVoxels + width,
           .yVoxels = origin.yVoxels + static_cast<std::int32_t>(shape->heightVoxels),
           .zVoxels = origin.zVoxels + depth}};
}

} // namespace

Siege::Siege(const Simulation::World::Desc& _desc, const SiegeOptions& _options)
  : m_world(_desc),
    m_options(_options)
{
  if (!_options.participant.has_value() && !_options.watch)
  {
    throw std::invalid_argument("--siege needs a participant, such as --siege P00, or --watch");
  }
  const std::string_view scenarioId = _options.mirror ? SIEGE_MIRROR_SCENARIO : SIEGE_SCENARIO;
  Simulation::LoadScenario(m_world, scenarioId,
                           {.version = _options.control ? std::optional(Protocol::TrialVersion::Control) : std::nullopt, .armies = true});
  if (_options.watch)
  {
    m_world.SetCommander(PLAYER, std::make_unique<Simulation::SiegeAttacker>(nullptr));
  }
  const std::optional<Simulation::TrialRules>& trial = m_world.Trial();
  const Simulation::Scenario* scenario = m_world.Data().FindScenario(scenarioId);
  if (!trial.has_value() || scenario == nullptr)
  {
    throw std::runtime_error("the Siege scenario holds no trial");
  }
  const Simulation::Structure& keep = m_world.Structures().at(trial->keep);
  const Simulation::ServerTemplate* shape = m_world.Data().FindTemplate(keep.templateId);
  const bool across = keep.quarterTurns % 2 == 1;
  const auto width = static_cast<std::int32_t>(across ? shape->depthVoxels : shape->widthVoxels);
  const auto depth = static_cast<std::int32_t>(across ? shape->widthVoxels : shape->depthVoxels);
  m_keepMiddle = {.xVoxels = keep.originVoxels.xVoxels + (width / 2),
                  .yVoxels = keep.originVoxels.yVoxels,
                  .zVoxels = keep.originVoxels.zVoxels + (depth / 2)};
  // The keep's foot, south of its forebuilding, which the bailey's walls close until a breach.
  m_goal = {.xVoxels = m_keepMiddle.xVoxels, .yVoxels = keep.originVoxels.yVoxels, .zVoxels = keep.originVoxels.zVoxels - 2};
  // The camp's middle, where the camera starts.
  const Protocol::VoxelPosition& low = scenario->trial.campMinVoxels;
  const Protocol::VoxelPosition& high = scenario->trial.campMaxVoxels;
  m_camp = {.xVoxels = (low.xVoxels + high.xVoxels) / 2, .yVoxels = low.yVoxels, .zVoxels = (low.zVoxels + high.zVoxels) / 2};
  m_objective = {.title = std::string(Client::SIEGE_OBJECTIVE_TITLE), .detail = {}, .detailOff = false, .progress = std::nullopt};

  // The trial's log, and its replay from the scenario as it stands before the first tick (ADR-026, decision 6). Its
  // times are UTC, as the lab's are.
  m_recorder.emplace(m_world);
  const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
  const std::string stem =
    Client::SiegeLog::FileStem(_options.participant.value_or("watch"), _options.control, scenarioId, std::format("{:%Y%m%d-%H%M%S}", now));
  std::filesystem::create_directories(_options.logFolder);
  const std::filesystem::path logPath = _options.logFolder / (stem + ".csv");
  m_replayPath = _options.logFolder / (stem + ".errp");
  m_logFile.open(logPath, std::ios::binary);
  if (!m_logFile)
  {
    throw std::runtime_error(std::format("the Siege trial could not write its log, {}", logPath.string()));
  }
  std::string gate;
  for (const auto& [id, structure] : m_world.Structures())
  {
    if (structure.templateId == GATEHOUSE_TEMPLATE && gate.empty())
    {
      gate = structure.name;
    }
  }
  for (const std::string& line : Client::SiegeLog::Head({.participant = _options.participant.value_or("watch"),
                                                         .control = _options.control,
                                                         .buildId = _options.buildId,
                                                         .serverDataHash = _options.serverDataHash,
                                                         .scenario = std::string(scenarioId),
                                                         .nearest = scenario->trial.nearest,
                                                         .weak = scenario->trial.weak,
                                                         .gate = gate,
                                                         .widthPixels = _options.widthPixels,
                                                         .heightPixels = _options.heightPixels,
                                                         .uiScale = _options.uiScale,
                                                         .startTime = std::format("{:%Y-%m-%d %H:%M:%S} UTC", now)}))
  {
    m_logFile << line << '\n';
  }
  m_logFile.flush();
}

Siege::~Siege()
{
  try
  {
    Finish(std::nullopt);
  }
  catch (const std::exception&) // NOLINT(bugprone-empty-catch): a destructor must not throw, and the run is closing
  {
  }
}

Simulation::World& Siege::World()
{
  return m_world;
}

std::size_t Siege::Generation() const noexcept
{
  return 0;
}

Protocol::VoxelPosition Siege::Goal() const noexcept
{
  return m_goal;
}

Protocol::VoxelPosition Siege::CameraTarget() const noexcept
{
  return m_camp;
}

std::int32_t Siege::CameraQuarterTurns() const noexcept
{
  // From the south-west, or a turn back, from the south-east, in the mirror (input 28).
  return m_options.mirror ? -1 : 0;
}

bool Siege::Tick(Client::Session& _session, double _seconds)
{
  if (m_end.has_value())
  {
    return false; // the trial has ended, and the world stands as it ended
  }
  // Each of the player's commands, recorded with its source and its time for the trial's log (M4-I7).
  for (const std::vector<std::byte>& command : m_commands.Take(_session, m_world.TickCount(), _seconds))
  {
    m_world.Submit(PLAYER, command);
  }
  for (const std::vector<std::byte>& query : _session.TakeQueries())
  {
    m_world.Ask(PLAYER, query);
  }
  m_world.Tick();
  if (m_recorder.has_value())
  {
    m_recorder->Record(m_world);
  }
  LogTick();
  WriteRows();
  return true;
}

void Siege::LogTick()
{
  const std::uint32_t tick = m_world.TickCount() - 1; // the tick just run, as its events name it
  const std::uint8_t attacker = m_world.Trial().has_value() ? m_world.Trial()->attacker : PLAYER;
  std::optional<Protocol::TrialOutcome> outcome;
  for (const Simulation::TrialEvent& event : m_world.LastTickEvents())
  {
    switch (event.kind)
    {
    case Simulation::TrialEventKind::ShotFired:
      m_fired[event.other] = {.player = event.player, .engine = event.subject, .target = event.voxel};
      break;
    case Simulation::TrialEventKind::ShotLanded:
    {
      const auto fired = m_fired.find(event.other);
      const Protocol::VoxelPosition target = fired != m_fired.end() ? fired->second.target : Protocol::VoxelPosition{};
      const bool struck = event.amount != 0;
      m_log.Shot(event.tick, event.player, event.subject, target, struck ? std::optional(event.voxel) : std::nullopt,
                 struck ? NameAt(event.voxel) : std::string());
      if (fired != m_fired.end())
      {
        m_fired.erase(fired);
      }
      break;
    }
    case Simulation::TrialEventKind::SectionOpened:
      m_log.Opened(event.tick, NameOf(event.subject));
      m_pending.push_back({.structure = event.subject, .warning = false, .tick = event.tick});
      break;
    case Simulation::TrialEventKind::FigurePassed:
      m_log.Passed(event.tick, NameOf(event.other), event.player == attacker);
      break;
    case Simulation::TrialEventKind::StoneSpent:
    case Simulation::TrialEventKind::TimberSpent:
      m_log.Spent(event.tick, event.player, event.amount, event.kind == Simulation::TrialEventKind::TimberSpent,
                  !m_world.SiegeEngines().contains(event.subject));
      break;
    case Simulation::TrialEventKind::StructureStopped:
      m_log.Stopped(event.tick, NameOf(event.subject));
      break;
    case Simulation::TrialEventKind::StructureWorks:
      m_log.Works(event.tick, NameOf(event.subject));
      break;
    case Simulation::TrialEventKind::UnitDied:
      m_log.Died(event.tick, event.player, static_cast<Protocol::UnitKind>(event.amount));
      break;
    case Simulation::TrialEventKind::Outcome:
      outcome = static_cast<Protocol::TrialOutcome>(event.amount);
      break;
    }
  }
  // A collapse that began its warning this tick, of a structure.
  for (const auto& [id, collapse] : m_world.Collapses())
  {
    if (m_collapsesSeen.insert(id).second && collapse.structure != Protocol::NO_ENTITY)
    {
      m_pending.push_back({.structure = collapse.structure, .warning = true, .tick = tick});
    }
  }
  // The player's commands, each as M4-I7 recorded it.
  const std::span<const Client::CommandRecord> records = m_commands.Records();
  for (; m_commandsLogged < records.size(); ++m_commandsLogged)
  {
    m_log.Command(records[m_commandsLogged]);
  }
  m_log.Advance(tick);
  if (outcome.has_value())
  {
    Finish(outcome);
  }
}

std::string Siege::NameAt(const Protocol::VoxelPosition& _voxels) const
{
  for (const auto& [id, structure] : m_world.Structures())
  {
    const auto [low, high] = BoxOf(m_world, structure);
    if (_voxels.xVoxels >= low.xVoxels && _voxels.xVoxels < high.xVoxels && _voxels.yVoxels >= low.yVoxels &&
        _voxels.yVoxels < high.yVoxels && _voxels.zVoxels >= low.zVoxels && _voxels.zVoxels < high.zVoxels)
    {
      return structure.name.empty() ? structure.templateId : structure.name;
    }
  }
  return {};
}

std::string Siege::NameOf(Protocol::EntityId _structure) const
{
  const auto found = m_world.Structures().find(_structure);
  if (found == m_world.Structures().end())
  {
    return "-";
  }
  return found->second.name.empty() ? found->second.templateId : found->second.name;
}

void Siege::WriteRows()
{
  for (const std::string& row : m_log.TakeRows())
  {
    m_logFile << row << '\n';
  }
  m_logFile.flush();
}

void Siege::Finish(std::optional<Protocol::TrialOutcome> _outcome)
{
  if (m_finished)
  {
    return;
  }
  m_finished = true;
  const std::uint32_t tick = m_world.TickCount() == 0 ? 0 : m_world.TickCount() - 1;
  m_log.End(tick, _outcome);
  WriteRows();
  if (m_recorder.has_value())
  {
    std::ofstream replay(m_replayPath, std::ios::binary);
    const std::span<const std::byte> bytes = m_recorder->Bytes();
    replay.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  }
}

void Siege::Drew(const DrawnFrame& _frame)
{
  const std::uint32_t tick = m_world.TickCount() == 0 ? 0 : m_world.TickCount() - 1;
  const auto coverage = [this, &_frame](Protocol::EntityId _structure)
  {
    const auto found = m_world.Structures().find(_structure);
    if (found == m_world.Structures().end())
    {
      return Client::Coverage{.place = Client::ScreenPlace::Off, .deckSharePercent = 0};
    }
    const auto [low, high] = BoxOf(m_world, found->second);
    return Client::CoverageOf(_frame.viewProjection, _frame.widthPixels, _frame.heightPixels, _frame.topBarPixels, _frame.deckPixels,
                              {static_cast<float>(low.xVoxels), static_cast<float>(low.yVoxels), static_cast<float>(low.zVoxels)},
                              {static_cast<float>(high.xVoxels), static_cast<float>(high.yVoxels), static_cast<float>(high.zVoxels)});
  };
  // Each breach and warning since the last frame, where this frame draws it.
  for (const Pending& pending : std::exchange(m_pending, {}))
  {
    m_log.Onscreen(pending.tick, NameOf(pending.structure), pending.warning, coverage(pending.structure));
  }
  // And each still waiting, framed once its centre lies in the uncovered area.
  for (const std::string& name : m_log.Unframed())
  {
    for (const auto& [id, structure] : m_world.Structures())
    {
      if ((structure.name.empty() ? structure.templateId : structure.name) == name && coverage(id).place == Client::ScreenPlace::Uncovered)
      {
        m_log.Framed(tick, name);
        break;
      }
    }
  }
  WriteRows();
}

void Siege::Frame(const RunInput& _input, Client::Session& _session)
{
  const Protocol::StateUpdate& state = _session.State();
  m_end = Client::SiegeModel::EndBanner(state);
  if (_input.pausePressed && !m_end.has_value())
  {
    m_model.TogglePause();
  }
  m_objective = Client::SiegeModel::Objective(state);
  m_resources = Client::SiegeModel::Resources(state);
}

double Siege::Speed() const noexcept
{
  return m_options.speed;
}

bool Siege::Paused() const noexcept
{
  return m_model.Paused() || m_end.has_value();
}

bool Siege::FireAllowed() const noexcept
{
  return true;
}

bool Siege::SelectionAllowed() const noexcept
{
  return true;
}

bool Siege::PassabilityForced() const noexcept
{
  return false;
}

bool Siege::TakesSpace() const noexcept
{
  return false;
}

std::optional<Client::HudMode> Siege::Mode() const
{
  return std::nullopt;
}

std::optional<Client::HudMode> Siege::Banner() const
{
  return m_end.has_value() ? m_end : m_model.PauseBanner();
}

std::optional<Client::HudObjective> Siege::Objective() const
{
  return m_objective;
}

std::vector<Client::HudResource> Siege::Resources(std::uint32_t /* _tick */, std::uint32_t /* _shotsFired */) const
{
  return m_resources;
}

bool Siege::ShowsFigures() const noexcept
{
  return !m_options.participant.has_value();
}

bool Siege::ShowsDeck() const noexcept
{
  return true;
}

std::vector<Client::OverlayBox> Siege::Marks(const Client::Session& /* _session */) const
{
  return {};
}

const Client::CommandLog& Siege::Commands() const noexcept
{
  return m_commands;
}

} // namespace Earthrise
