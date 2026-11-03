#include "pch.h"

#include "ReplayRun.h"

#include "BreachRun.h"
#include "Scenario.h"
#include "ServerData.h"
#include "Strings.h"

#include <fstream>

namespace Earthrise
{

namespace
{

constexpr std::uint8_t PLAYER = Simulation::BreachRun::PLAYER; // the Siege castle's attacker
constexpr std::string_view SIEGE_SCENARIO = "Siege";
constexpr std::string_view SIEGE_MIRROR_SCENARIO = "SiegeMirror";
constexpr std::string_view SCENARIO_LINE = "# scenario: ";

std::vector<std::byte> ReadFile(const std::filesystem::path& _file)
{
  std::ifstream input(_file, std::ios::binary);
  if (!input)
  {
    throw std::runtime_error(std::format("the replay {} could not be read", _file.string()));
  }
  input.seekg(0, std::ios::end);
  std::vector<std::byte> bytes(static_cast<std::size_t>(input.tellg()));
  input.seekg(0, std::ios::beg);
  input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return bytes;
}

} // namespace

ReplayRun::ReplayRun(const Simulation::World::Desc& _desc, const std::filesystem::path& _file, std::string_view _scenarioId, double _speed)
  : m_player(std::make_unique<Simulation::ReplayPlayer>(ReadFile(_file), _desc)),
    m_speed(_speed),
    m_mirror(_scenarioId == SIEGE_MIRROR_SCENARIO)
{
  if (const Simulation::Scenario* scenario = m_player->Played().Data().FindScenario(_scenarioId); scenario != nullptr)
  {
    const Protocol::VoxelPosition& low = scenario->trial.campMinVoxels;
    const Protocol::VoxelPosition& high = scenario->trial.campMaxVoxels;
    m_camp = {.xVoxels = (low.xVoxels + high.xVoxels) / 2, .yVoxels = low.yVoxels, .zVoxels = (low.zVoxels + high.zVoxels) / 2};
  }
  m_objective = {.title = std::string(Client::SIEGE_OBJECTIVE_TITLE), .detail = {}, .detailOff = false, .progress = std::nullopt};
}

std::string ReplayRun::ScenarioOf(const std::filesystem::path& _file)
{
  std::filesystem::path log = _file;
  log.replace_extension(".csv");
  std::ifstream input(log, std::ios::binary);
  std::string line;
  while (std::getline(input, line) && line.starts_with('#'))
  {
    if (line.starts_with(SCENARIO_LINE))
    {
      return line.substr(SCENARIO_LINE.size());
    }
  }
  return std::string(SIEGE_SCENARIO);
}

Simulation::World& ReplayRun::World()
{
  return m_player->Played();
}

std::size_t ReplayRun::Generation() const noexcept
{
  return 0;
}

Protocol::VoxelPosition ReplayRun::Goal() const noexcept
{
  return m_camp;
}

Protocol::VoxelPosition ReplayRun::CameraTarget() const noexcept
{
  return m_camp;
}

std::int32_t ReplayRun::CameraQuarterTurns() const noexcept
{
  return m_mirror ? -1 : 0;
}

bool ReplayRun::Tick(Client::Session& _session, double /* _seconds */)
{
  // The player orders nothing in a replay; the views still ask.
  static_cast<void>(_session.TakeCommands());
  for (const std::vector<std::byte>& query : _session.TakeQueries())
  {
    m_player->Played().Ask(PLAYER, query);
  }
  return m_player->Step();
}

void ReplayRun::Frame(const RunInput& _input, Client::Session& _session)
{
  const Protocol::StateUpdate& state = _session.State();
  if (m_player->Done())
  {
    const Simulation::ReplayResult result = m_player->Result();
    m_end = Client::HudMode{.command = "END",
                            .banner = result.matched ? std::format(Client::REPLAY_MATCHED_BANNER, result.ticksReplayed)
                                                     : std::format(Client::REPLAY_DIFFERS_BANNER, result.firstDifferingTick.value_or(0)),
                            .buttons = {}};
  }
  else if (_input.pausePressed)
  {
    m_model.TogglePause();
  }
  m_objective = Client::SiegeModel::Objective(state);
  m_resources = Client::SiegeModel::Resources(state);
}

double ReplayRun::Speed() const noexcept
{
  return m_speed;
}

bool ReplayRun::Paused() const noexcept
{
  return m_model.Paused() || m_end.has_value();
}

bool ReplayRun::FireAllowed() const noexcept
{
  return false;
}

bool ReplayRun::SelectionAllowed() const noexcept
{
  return true;
}

bool ReplayRun::PassabilityForced() const noexcept
{
  return false;
}

bool ReplayRun::TakesSpace() const noexcept
{
  return false;
}

std::optional<Client::HudMode> ReplayRun::Mode() const
{
  return std::nullopt;
}

std::optional<Client::HudMode> ReplayRun::Banner() const
{
  return m_end.has_value() ? m_end : m_model.PauseBanner();
}

std::optional<Client::HudObjective> ReplayRun::Objective() const
{
  return m_objective;
}

std::vector<Client::HudResource> ReplayRun::Resources(std::uint32_t /* _tick */, std::uint32_t /* _shotsFired */) const
{
  return m_resources;
}

bool ReplayRun::ShowsFigures() const noexcept
{
  return true;
}

bool ReplayRun::ShowsDeck() const noexcept
{
  return true;
}

std::vector<Client::OverlayBox> ReplayRun::Marks(const Client::Session& /* _session */) const
{
  return {};
}

} // namespace Earthrise
