#include "pch.h"

#include "Lab.h"

#include "BreachRun.h"
#include "ByteReader.h"
#include "ByteWriter.h"
#include "Command.h"
#include "Look.h"
#include "Query.h"
#include "Save.h"

#include <chrono>

namespace Earthrise
{

namespace
{

constexpr std::uint8_t PLAYER = Simulation::BreachRun::PLAYER;

/// The items as the client shows them, in Simulation's order: P0 first, then L01 to L16.
std::vector<Client::LabItemSpec> Specs()
{
  std::vector<Client::LabItemSpec> specs;
  for (const Simulation::LabItem& item : Simulation::LabItems())
  {
    specs.push_back({.id = item.id, .shotsAsked = item.shotsAsked, .markVoxels = item.markVoxels});
  }
  return specs;
}

/// A command as the log writes it: FireEngine x y z, or MoveSquad x y z.
std::string Describe(const std::vector<std::byte>& _bytes)
{
  Core::ByteReader reader(_bytes);
  const Protocol::Command command = Protocol::DecodeCommand(reader);
  if (const auto* fire = std::get_if<Protocol::FireEngine>(&command))
  {
    return std::format("FireEngine {} {} {}", fire->targetVoxels.xVoxels, fire->targetVoxels.yVoxels, fire->targetVoxels.zVoxels);
  }
  if (const auto* move = std::get_if<Protocol::MoveSquad>(&command))
  {
    return std::format("MoveSquad {} {} {}", move->goalVoxels.xVoxels, move->goalVoxels.yVoxels, move->goalVoxels.zVoxels);
  }
  return "Unknown";
}

} // namespace

Lab::Lab(const Simulation::World::Desc& _desc, const LabOptions& _options)
  : m_pristine(_desc),
    m_setup(Simulation::BuildLabSite(m_pristine)),
    m_model(Specs(), _options.seed)
{
  m_world = std::make_unique<Simulation::World>(m_pristine);
  // Logs\BreachLab-<participant>-<yyyymmdd-hhmmss>.csv, LF-terminated and UTF-8, its times in UTC.
  const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
  std::filesystem::create_directories(_options.logFolder);
  const std::filesystem::path path = _options.logFolder / std::format("BreachLab-{}-{:%Y%m%d-%H%M%S}.csv", _options.participant, now);
  m_log.open(path, std::ios::binary);
  if (!m_log)
  {
    throw std::runtime_error(std::format("the breach lab could not write its log, {}", path.string()));
  }
  Log(Client::LabModel::LogHeader({.participant = _options.participant,
                                   .buildId = _options.buildId,
                                   .serverDataHash = _options.serverDataHash,
                                   .itemsHash = Simulation::LabItemsHash(),
                                   .widthPixels = _options.widthPixels,
                                   .heightPixels = _options.heightPixels,
                                   .uiScale = _options.uiScale,
                                   .startTime = std::format("{:%Y-%m-%d %H:%M:%S} UTC", now)}));
}

Simulation::World& Lab::World()
{
  return *m_world;
}

std::size_t Lab::Generation() const noexcept
{
  return m_generation;
}

Protocol::VoxelPosition Lab::Goal() const noexcept
{
  return m_setup.goalVoxels;
}

Protocol::VoxelPosition Lab::CameraTarget() const noexcept
{
  // W2, the target, from the default azimuth and view height (GameDesign.md, M3's values, item 14).
  return {.xVoxels = m_setup.goalVoxels.xVoxels,
          .yVoxels = Simulation::BreachRun::GROUND_VOXELS,
          .zVoxels = Simulation::BreachRun::WALL_SOUTH_VOXELS};
}

bool Lab::Tick(Client::Session& _session, double _seconds)
{
  if (Paused())
  {
    return false;
  }
  Simulation::World& world = *m_world;
  const Client::LabPhase phase = m_model.Phase();
  const Client::LabItemSpec* item = m_model.Current();
  // While an item's shots run, the mark's aim check each tick, so that the last answer is the world's at the question.
  if (item != nullptr)
  {
    _session.Ask(Protocol::AimCheck{.engine = m_setup.engine, .targetVoxels = item->markVoxels});
  }
  for (const std::vector<std::byte>& query : _session.TakeQueries())
  {
    world.Ask(PLAYER, query);
  }
  const std::vector<std::vector<std::byte>> commands = _session.TakeCommands();
  if (phase == Client::LabPhase::FreeFire)
  {
    // The player's commands, each logged with its source, and the squad's order, as the breach run's script gives it.
    const std::vector<std::string>& sources = _session.TakenSources();
    for (std::size_t index = 0; index < commands.size(); ++index)
    {
      world.Submit(PLAYER, commands.at(index));
      Log(Client::LabModel::CommandRowText(world.TickCount(), _seconds, Describe(commands.at(index)),
                                           index < sources.size() ? std::string_view(sources.at(index)) : std::string_view("key")));
    }
    if (world.TickCount() == Simulation::LAB_ORDER_TICK)
    {
      Core::ByteWriter writer;
      Protocol::Encode(Protocol::MoveSquad{.squad = m_setup.squad, .goalVoxels = m_setup.goalVoxels}, writer);
      world.Submit(PLAYER, writer.Bytes());
    }
    world.Tick();
    m_model.FreeFireTicked(world.TickCount());
    m_itemPending = m_model.Phase() != Client::LabPhase::FreeFire;
    return true;
  }
  // In an item FIRE is the lab's alone: the player's commands are dropped.
  if (!m_shots.has_value())
  {
    return false;
  }
  m_shots->Advance(world);
  if (m_shots->Done())
  {
    if (phase == Client::LabPhase::Watching)
    {
      m_model.PriorShotsDone(_seconds);
    }
    else if (phase == Client::LabPhase::Shooting)
    {
      m_model.AskedShotsDone(Simulation::CourtyardOpen(world, m_setup));
    }
  }
  return true;
}

void Lab::Frame(const RunInput& _input, Client::Session& /* _session */)
{
  switch (m_model.Phase())
  {
  case Client::LabPhase::Question:
  {
    // The question is up and the clock paused: the views' queries are answered between ticks (ADR-020, decision 6,
    // amended), the camera's moves counted, and the answer taken.
    m_world->AnswerBetweenTicks();
    m_model.NoteCamera(_input.quarterTurns, _input.zoomNotches);
    if (_input.openPressed || _input.shutPressed)
    {
      const Client::LabAnswer answer = _input.openPressed ? Client::LabAnswer::Open : Client::LabAnswer::Shut;
      const Client::LabItemSpec* item = m_model.Current();
      if (item != nullptr && m_model.Answer(answer, _input.nowSeconds, _input.integrityOn, _input.passabilityOn))
      {
        const std::vector<Protocol::VoxelPosition> asked(item->shotsAsked, item->markVoxels);
        m_shots.emplace(m_setup, asked);
      }
    }
    break;
  }
  case Client::LabPhase::Result:
  {
    m_world->AnswerBetweenTicks();
    const Client::LabItemSpec* current = m_model.Current();
    if (_input.nextPressed && current != nullptr)
    {
      // The headless run of the same item, on a fresh copy of the site, gives the key and the hash the live one must
      // match (MvpPlan-M3.md, *The lab's log*).
      const Simulation::LabItem& item = SimulationItem(current->id);
      Simulation::World headless(m_pristine);
      const Simulation::LabOutcome outcome = Simulation::ResolveLabItem(headless, m_setup, item);
      const std::optional<Client::LabItemRow> row = m_model.Next(Simulation::StateHash(*m_world), outcome.stateHash, outcome.open);
      if (row.has_value())
      {
        Log(Client::LabModel::ItemRowText(*row));
      }
      m_itemPending = m_model.Phase() == Client::LabPhase::Watching;
    }
    break;
  }
  case Client::LabPhase::FreeFire:
  case Client::LabPhase::Watching:
  case Client::LabPhase::Shooting:
  case Client::LabPhase::Finished:
    break;
  }
  if (m_itemPending)
  {
    StartItem();
  }
}

void Lab::StartItem()
{
  const Client::LabItemSpec* item = m_model.Current();
  m_itemPending = false;
  if (item == nullptr)
  {
    return;
  }
  m_world = std::make_unique<Simulation::World>(m_pristine);
  m_shots.emplace(m_setup, SimulationItem(item->id).priorAimVoxels);
  ++m_generation;
}

const Simulation::LabItem& Lab::SimulationItem(std::string_view _id) const
{
  const std::span<const Simulation::LabItem> items = Simulation::LabItems();
  const auto found = std::ranges::find(items, _id, &Simulation::LabItem::id);
  if (found == items.end())
  {
    throw std::logic_error(std::format("the lab has no item {}", _id));
  }
  return *found;
}

void Lab::Log(std::string_view _text)
{
  m_log.write(_text.data(), static_cast<std::streamsize>(_text.size()));
  m_log.flush();
}

double Lab::Speed() const noexcept
{
  return m_model.Speed();
}

bool Lab::Paused() const noexcept
{
  return m_itemPending || m_model.Paused();
}

bool Lab::FireAllowed() const noexcept
{
  return m_model.FireAllowed();
}

bool Lab::PassabilityForced() const noexcept
{
  return m_model.PassabilityForced();
}

bool Lab::TakesSpace() const noexcept
{
  return m_model.Phase() == Client::LabPhase::Result;
}

std::optional<Client::HudMode> Lab::Mode() const
{
  return m_model.Mode(m_world->TickCount());
}

std::optional<Client::HudObjective> Lab::Objective() const
{
  return m_model.Objective();
}

std::vector<Client::OverlayBox> Lab::Marks(const Client::Session& _session) const
{
  // The mark in accent at the aim voxel, and the voxel the arc strikes beside it in the text's colour.
  std::vector<Client::OverlayBox> marks;
  const Client::LabItemSpec* item = m_model.Current();
  if (!m_model.MarkShown() || item == nullptr)
  {
    return marks;
  }
  marks.push_back({.cellVoxels = item->markVoxels, .paletteIndex = Client::OVERLAY_ACCENT_INDEX});
  const Protocol::AimReading* reading = _session.Aim(m_setup.engine, item->markVoxels);
  if (reading != nullptr && reading->strikes && !(reading->strikeVoxels == item->markVoxels))
  {
    marks.push_back({.cellVoxels = reading->strikeVoxels, .paletteIndex = Client::OVERLAY_TEXT_INDEX});
  }
  return marks;
}

} // namespace Earthrise
