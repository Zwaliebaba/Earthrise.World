#include "pch.h"

#include "Siege.h"

#include "BreachRun.h"
#include "Scenario.h"
#include "ServerData.h"
#include "SiegeAttacker.h"
#include "Strings.h"

namespace Earthrise
{

namespace
{

constexpr std::uint8_t PLAYER = Simulation::BreachRun::PLAYER; // the Siege castle's attacker
constexpr std::string_view SIEGE_SCENARIO = "Siege";
constexpr std::string_view SIEGE_MIRROR_SCENARIO = "SiegeMirror";

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
  return true;
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
