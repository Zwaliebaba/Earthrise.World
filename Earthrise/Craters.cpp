#include "pch.h"

#include "Craters.h"

#include "BreachLab.h"
#include "BreachRun.h"
#include "CraterLab.h"
#include "Look.h"

#include <chrono>

namespace Earthrise
{

namespace
{

constexpr std::uint8_t PLAYER = Simulation::BreachRun::PLAYER;
/// An item is set up, out of sight, until its world is quiet or this many ticks have run: 10 s of game time.
constexpr std::uint32_t SETTLE_TICKS = 200;

/// Simulation's crater items as the client's model sees them: each id and its key.
std::vector<Client::CraterItemSpec> Specs()
{
  std::vector<Client::CraterItemSpec> specs;
  specs.reserve(Simulation::CraterItems().size());
  for (const Simulation::CraterItem& item : Simulation::CraterItems())
  {
    specs.push_back({.id = item.id, .keyRam = item.engine == Simulation::CraterEngine::Ram});
  }
  return specs;
}

const Simulation::CraterItem& SimulationItem(std::string_view _id)
{
  const std::span<const Simulation::CraterItem> items = Simulation::CraterItems();
  const auto found = std::ranges::find(items, _id, &Simulation::CraterItem::id);
  if (found == items.end())
  {
    throw std::runtime_error(std::format("no crater item {}", _id));
  }
  return *found;
}

} // namespace

Craters::Craters(const Simulation::World::Desc& _desc, const CratersOptions& _options)
  : m_pristine(_desc),
    m_target(Simulation::BuildLabEnclosure(m_pristine)),
    m_model(Specs(), _options.seed)
{
  // Logs\Craters-<participant>-<yyyymmdd-hhmmss>.csv, LF-terminated and UTF-8, its times in UTC, as the lab's.
  const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
  std::filesystem::create_directories(_options.logFolder);
  const std::filesystem::path path = _options.logFolder / std::format("Craters-{}-{:%Y%m%d-%H%M%S}.csv", _options.participant, now);
  m_log.open(path, std::ios::binary);
  if (!m_log)
  {
    throw std::runtime_error(std::format("the crater items could not write their log, {}", path.string()));
  }
  m_log << Client::CraterModel::LogHeader({.participant = _options.participant,
                                           .buildId = _options.buildId,
                                           .serverDataHash = _options.serverDataHash,
                                           .itemsHash = Simulation::CraterItemsHash(),
                                           .startTime = std::format("{:%Y-%m-%d %H:%M:%S} UTC", now)});
  m_log.flush();
  StartItem();
}

void Craters::StartItem()
{
  const Client::CraterItemSpec* item = m_model.Current();
  if (item == nullptr)
  {
    return;
  }
  m_world = std::make_unique<Simulation::World>(m_pristine);
  const Simulation::CraterSite site = Simulation::StrikeCraterItem(*m_world, m_target, SimulationItem(item->id));
  // The mark on the ground before the first hit, which it does not hide; the camera on W2 as the lab frames it.
  m_mark = {.xVoxels = site.markVoxels.xVoxels, .yVoxels = site.markVoxels.yVoxels - 1, .zVoxels = site.markVoxels.zVoxels - 1};
  m_cameraTarget = {.xVoxels = (Simulation::BreachRun::WALL_WEST_VOXELS + Simulation::BreachRun::WALL_EAST_VOXELS + 1) / 2,
                    .yVoxels = Simulation::BreachRun::GROUND_VOXELS,
                    .zVoxels = Simulation::BreachRun::WALL_SOUTH_VOXELS};
  m_settleTicks = 0;
  ++m_generation;
}

Simulation::World& Craters::World()
{
  return *m_world;
}

std::size_t Craters::Generation() const noexcept
{
  return m_generation;
}

Protocol::VoxelPosition Craters::Goal() const noexcept
{
  return m_cameraTarget;
}

Protocol::VoxelPosition Craters::CameraTarget() const noexcept
{
  return m_cameraTarget;
}

bool Craters::Tick(Client::Session& _session, double _seconds)
{
  if (Paused())
  {
    return false;
  }
  // No command is taken: selection and FIRE are off. The views' queries are answered.
  static_cast<void>(_session.TakeCommands());
  for (const std::vector<std::byte>& query : _session.TakeQueries())
  {
    m_world->Ask(PLAYER, query);
  }
  m_world->Tick();
  ++m_settleTicks;
  if (m_world->Quiet() || m_settleTicks >= SETTLE_TICKS)
  {
    m_model.Ready(_seconds);
  }
  return true;
}

void Craters::Frame(const RunInput& _input, Client::Session& /* _session */)
{
  if (!m_model.Asking())
  {
    return;
  }
  std::optional<Client::CraterAnswer> answer;
  if (_input.ramPressed)
  {
    answer = Client::CraterAnswer::Ram;
  }
  else if (_input.trebuchetPressed)
  {
    answer = Client::CraterAnswer::Trebuchet;
  }
  if (!answer.has_value())
  {
    return;
  }
  if (const std::optional<Client::CraterItemRow> row = m_model.Answer(*answer, _input.nowSeconds); row.has_value())
  {
    m_log << Client::CraterModel::ItemRowText(*row);
    m_log.flush();
    StartItem();
  }
}

double Craters::Speed() const noexcept
{
  return 1.0;
}

bool Craters::Paused() const noexcept
{
  return m_model.Asking() || m_model.Finished();
}

bool Craters::FireAllowed() const noexcept
{
  return false;
}

bool Craters::SelectionAllowed() const noexcept
{
  return false;
}

bool Craters::PassabilityForced() const noexcept
{
  return false;
}

bool Craters::TakesSpace() const noexcept
{
  return false;
}

std::optional<Client::HudMode> Craters::Mode() const
{
  return m_model.Mode();
}

std::optional<Client::HudObjective> Craters::Objective() const
{
  return m_model.Objective();
}

std::vector<Client::HudResource> Craters::Resources(std::uint32_t /* _tick */, std::uint32_t /* _shotsFired */) const
{
  return {};
}

bool Craters::ShowsFigures() const noexcept
{
  return false;
}

bool Craters::ShowsDeck() const noexcept
{
  return false;
}

std::vector<Client::OverlayBox> Craters::Marks(const Client::Session& /* _session */) const
{
  if (!m_model.Asking())
  {
    return {};
  }
  return {{.cellVoxels = m_mark, .paletteIndex = Client::OVERLAY_ACCENT_INDEX}};
}

} // namespace Earthrise
