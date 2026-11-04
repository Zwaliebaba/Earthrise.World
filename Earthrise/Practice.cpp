#include "pch.h"

#include "Practice.h"

#include "BreachRun.h"
#include "Scenario.h"
#include "ServerData.h"
#include "Strings.h"

namespace Earthrise
{

namespace
{

constexpr std::uint8_t PLAYER = Simulation::BreachRun::PLAYER; // the Siege camp's attacker, whose army the ground holds
constexpr std::string_view PRACTICE_SCENARIO = "Practice";
/// STONE and ARMY of the trial's resources, without its TIME, since no trial runs (Client::SiegeModel::Resources).
constexpr std::size_t PRACTICE_RESOURCES = 2;

} // namespace

Practice::Practice(const Simulation::World::Desc& _desc, double _speed)
  : m_world(_desc),
    m_speed(_speed)
{
  Simulation::LoadScenario(m_world, PRACTICE_SCENARIO);
  const Simulation::Scenario* scenario = m_world.Data().FindScenario(PRACTICE_SCENARIO);
  if (scenario == nullptr || scenario->trial.has_value() || m_world.Trial().has_value())
  {
    throw std::runtime_error("the practice ground holds a trial, or is not in the server data");
  }
  // The camera starts on the middle of the player's army, which stands as the Siege camp's does.
  std::int64_t sumX = 0;
  std::int64_t sumZ = 0;
  std::int64_t cells = 0;
  for (const Simulation::ScenarioSquad& squad : scenario->squads)
  {
    for (const Protocol::VoxelPosition& cell : squad.cellsVoxels)
    {
      sumX += cell.xVoxels;
      sumZ += cell.zVoxels;
      ++cells;
    }
  }
  if (cells == 0)
  {
    throw std::runtime_error("the practice ground holds none of the player's units");
  }
  m_camp = {.xVoxels = static_cast<std::int32_t>(sumX / cells),
            .yVoxels = scenario->groundVoxels,
            .zVoxels = static_cast<std::int32_t>(sumZ / cells)};
}

Simulation::World& Practice::World()
{
  return m_world;
}

std::size_t Practice::Generation() const noexcept
{
  return 0;
}

Protocol::VoxelPosition Practice::Goal() const noexcept
{
  return m_camp;
}

Protocol::VoxelPosition Practice::CameraTarget() const noexcept
{
  return m_camp;
}

bool Practice::Tick(Client::Session& _session, double /* _seconds */)
{
  for (const std::vector<std::byte>& command : _session.TakeCommands())
  {
    m_world.Submit(PLAYER, command);
  }
  for (const std::vector<std::byte>& query : _session.TakeQueries())
  {
    m_world.Ask(PLAYER, query);
  }
  m_world.Tick();
  return true;
}

void Practice::Frame(const RunInput& _input, Client::Session& _session)
{
  if (_input.pausePressed)
  {
    m_model.TogglePause();
  }
  std::vector<Client::HudResource> resources = Client::SiegeModel::Resources(_session.State());
  resources.resize(std::min(resources.size(), PRACTICE_RESOURCES));
  m_resources = std::move(resources);
}

double Practice::Speed() const noexcept
{
  return m_speed;
}

bool Practice::Paused() const noexcept
{
  return m_model.Paused();
}

bool Practice::FireAllowed() const noexcept
{
  return true;
}

bool Practice::SelectionAllowed() const noexcept
{
  return true;
}

bool Practice::PassabilityForced() const noexcept
{
  return false;
}

bool Practice::TakesSpace() const noexcept
{
  return false;
}

std::optional<Client::HudMode> Practice::Mode() const
{
  return std::nullopt;
}

std::optional<Client::HudMode> Practice::Banner() const
{
  return m_model.PauseBanner();
}

std::optional<Client::HudObjective> Practice::Objective() const
{
  return Client::HudObjective{.title = std::string(Client::PRACTICE_OBJECTIVE_TITLE),
                              .detail = std::string(Client::PRACTICE_OBJECTIVE_DETAIL)};
}

std::vector<Client::HudResource> Practice::Resources(std::uint32_t /* _tick */, std::uint32_t /* _shotsFired */) const
{
  return m_resources;
}

bool Practice::ShowsFigures() const noexcept
{
  return false;
}

bool Practice::ShowsDeck() const noexcept
{
  return true;
}

std::vector<Client::OverlayBox> Practice::Marks(const Client::Session& /* _session */) const
{
  return {};
}

} // namespace Earthrise
