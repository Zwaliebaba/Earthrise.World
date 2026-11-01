#include "pch.h"

#include "World.h"

namespace Simulation
{

FixedPosition CellFloorMiddle(Protocol::VoxelPosition _cellVoxels)
{
  const Core::Fixed half = Core::Fixed::FromRaw(Core::Fixed::ONE_RAW / 2);
  return {.xMeters = Core::Fixed::FromWhole(_cellVoxels.xVoxels) + half,
          .yMeters = Core::Fixed::FromWhole(_cellVoxels.yVoxels),
          .zMeters = Core::Fixed::FromWhole(_cellVoxels.zVoxels) + half};
}

World::World(const Desc& _desc)
  : m_desc(_desc)
{
  if (m_desc.serverData == nullptr || !m_desc.generate || !m_desc.surfaceVoxels || m_desc.widthChunks <= 0 || m_desc.heightChunks <= 0 ||
      m_desc.depthChunks <= 0)
  {
    throw std::invalid_argument("a world needs server data, a generator, a surface and at least one chunk on each axis");
  }
  ChunkStore& base = m_voxels.Base();
  for (std::int32_t y = 0; y < m_desc.heightChunks; ++y)
  {
    for (std::int32_t z = 0; z < m_desc.depthChunks; ++z)
    {
      for (std::int32_t x = 0; x < m_desc.widthChunks; ++x)
      {
        const Protocol::ChunkCoordinate coordinate{.xChunks = x, .yChunks = y, .zChunks = z};
        const auto chunk = std::make_unique<Protocol::Chunk>();
        m_desc.generate(coordinate, *chunk);
        base.SetChunk(coordinate, *chunk);
      }
    }
  }
}

void World::Submit(std::uint8_t _player, std::span<const std::byte> _command)
{
  m_pendingCommands.push_back({.player = _player, .bytes = {_command.begin(), _command.end()}});
}

void World::Tick()
{
  ApplyCommands();
  AdvanceJobs();
  MoveUnits();
  AdvanceShots();
  SearchSupport();
  AdvanceCollapses();
  SettleLoose();
  RebuildNavigation();
  SettleUnits();
  CheckRules();
  PublishState();
  AnswerQueries();
  m_voxels.EndTick();
  ++m_tick;
}

std::uint32_t World::TickCount() const noexcept
{
  return m_tick;
}

std::span<const SubmittedCommand> World::LastTickCommands() const noexcept
{
  return m_lastTickCommands;
}

Protocol::EntityId World::NewEntityId() noexcept
{
  return m_nextEntityId++;
}

const VoxelWorld& World::Voxels() const noexcept
{
  return m_voxels;
}

VoxelWorld& World::Voxels() noexcept
{
  return m_voxels;
}

std::vector<Protocol::ChunkSnapshot> World::Publish()
{
  return m_voxels.Publish();
}

const World::Desc& World::Description() const noexcept
{
  return m_desc;
}

const ServerData& World::Data() const noexcept
{
  return *m_desc.serverData;
}

const std::map<Protocol::EntityId, Squad>& World::Squads() const noexcept
{
  return m_squads;
}

const std::map<Protocol::EntityId, Unit>& World::Units() const noexcept
{
  return m_units;
}

const std::map<Protocol::EntityId, SiegeEngine>& World::SiegeEngines() const noexcept
{
  return m_siegeEngines;
}

Protocol::EntityId World::AddSquad(std::uint8_t _owner, NavigationClass _class, std::span<const Protocol::VoxelPosition> _cellsVoxels)
{
  const Protocol::EntityId squadId = NewEntityId();
  Squad squad{.id = squadId, .owner = _owner, .navigationClass = _class, .units = {}, .hasGoal = false, .goalVoxels = {}};
  for (const Protocol::VoxelPosition& cell : _cellsVoxels)
  {
    const Protocol::EntityId unitId = NewEntityId();
    m_units.emplace(
      unitId,
      Unit{
        .id = unitId, .owner = _owner, .navigationClass = _class, .squad = squadId, .position = CellFloorMiddle(cell), .cellVoxels = cell});
    squad.units.push_back(unitId);
  }
  m_squads.emplace(squadId, std::move(squad));
  return squadId;
}

Protocol::EntityId World::AddSiegeEngine(std::uint8_t _owner, EngineKind _kind, Protocol::VoxelPosition _cellVoxels)
{
  const Protocol::EntityId id = NewEntityId();
  m_siegeEngines.emplace(
    id, SiegeEngine{.id = id, .owner = _owner, .kind = _kind, .position = CellFloorMiddle(_cellVoxels), .cellVoxels = _cellVoxels});
  return id;
}

void World::RemoveSquad(Protocol::EntityId _squad)
{
  const auto found = m_squads.find(_squad);
  if (found == m_squads.end())
  {
    return;
  }
  for (const Protocol::EntityId unit : found->second.units)
  {
    m_units.erase(unit);
  }
  m_squads.erase(found);
}

// ── The tick's steps ────────────────────────────────────────────────────────────────────────────

void World::ApplyCommands()
{
  // Step 1: the commands that arrived for this tick, in the order they arrived.
  m_lastTickCommands = std::exchange(m_pendingCommands, {});
  for (const SubmittedCommand& submitted : m_lastTickCommands)
  {
    Core::ByteReader reader(submitted.bytes);
    const Protocol::Command command = Protocol::DecodeCommand(reader);
    if (!reader.AtEnd())
    {
      throw std::invalid_argument("a command's bytes run on past the command");
    }
    Apply(submitted.player, command);
  }
}

void World::Apply(std::uint8_t _player, const Protocol::Command& _command)
{
  if (const auto* move = std::get_if<Protocol::MoveSquad>(&_command))
  {
    // A command for a squad that does not exist, or is not the player's, is refused and does nothing.
    const auto squad = m_squads.find(move->squad);
    if (squad != m_squads.end() && squad->second.owner == _player)
    {
      squad->second.hasGoal = true;
      squad->second.goalVoxels = move->goalVoxels;
    }
  }
  // FireEngine is decoded, and its shot is M2-E1's.
}

void World::AdvanceJobs()
{
  // Step 2: designations and jobs, which M2 has none of.
}

void World::MoveUnits()
{
  // Step 3: searches for the squads that need a path, then movement along paths (M2-F3, F5).
}

void World::AdvanceShots()
{
  // Step 4: shots through the voxel grid, and their craters (M2-E1, E2).
}

void World::SearchSupport()
{
  // Step 5: the support search over the edits not yet searched (M2-E3).
}

void World::AdvanceCollapses()
{
  // Step 6: warnings, falls and the rubble they leave (M2-E3 to E5).
}

void World::SettleLoose()
{
  // Step 7: slump and loose material, within the tick's budget (M2-E6, E7).
}

void World::RebuildNavigation()
{
  // Step 8: every navigation tile the tick's edits touched (M2-F1 to F4).
}

void World::SettleUnits()
{
  // Step 9: units whose cells changed drop, rise or move (M2-F6).
}

void World::CheckRules()
{
  // Step 10: the win and loss rules, which M2 has none of.
}

void World::PublishState()
{
  // Step 11: state updates filtered by what each player sees. M2 is headless; Publish() hands the
  // chunks to whoever asks.
}

void World::AnswerQueries()
{
  // Step 12: queries within the tick's budget (ADR-009), which M3 sets.
}

} // namespace Simulation
