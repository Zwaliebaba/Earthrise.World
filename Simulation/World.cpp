#include "pch.h"

#include "World.h"

#include "Generator.h"

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
  const std::span<const Protocol::VoxelPosition> edits = m_voxels.TickEdits();
  m_lastTickEdits.assign(edits.begin(), edits.end());
  // The edits after step 5, steps 6 and 7's, wait for the next tick's search.
  m_unsearchedEdits.assign(edits.begin() + static_cast<std::ptrdiff_t>(std::min(m_searchedThrough, edits.size())), edits.end());
  m_searchedThrough = 0;
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

const std::map<Protocol::EntityId, Shot>& World::Shots() const noexcept
{
  return m_shots;
}

const std::map<Protocol::EntityId, Collapse>& World::Collapses() const noexcept
{
  return m_collapses;
}

std::span<const ShotHit> World::LastTickHits() const noexcept
{
  return m_tickHits;
}

std::span<const Protocol::VoxelPosition> World::LastTickEdits() const noexcept
{
  return m_lastTickEdits;
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
    id, SiegeEngine{
          .id = id, .owner = _owner, .kind = _kind, .position = CellFloorMiddle(_cellVoxels), .cellVoxels = _cellVoxels, .reloadTicks = 0});
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
  else if (const auto* fire = std::get_if<Protocol::FireEngine>(&_command))
  {
    Fire(_player, *fire);
  }
}

void World::Fire(std::uint8_t _player, const Protocol::FireEngine& _fire)
{
  // Refused, and doing nothing, for an engine that is not the player's or is reloading, and for a
  // target out of range (GameDesign.md, M2's values, item 12).
  const auto engine = m_siegeEngines.find(_fire.engine);
  if (engine == m_siegeEngines.end() || engine->second.owner != _player || engine->second.reloadTicks > 0)
  {
    return;
  }
  const std::optional<ShotArc> arc = AimShot(engine->second.cellVoxels, _fire.targetVoxels, Data().Tuning());
  if (!arc.has_value())
  {
    return;
  }
  const Protocol::EntityId id = NewEntityId();
  m_shots.emplace(id, Shot{.id = id, .engine = engine->first, .owner = _player, .arc = *arc, .ticksFlown = 0});
  engine->second.reloadTicks = static_cast<std::uint32_t>(Data().Tuning().shotReloadTicks);
}

bool World::OutsideWorld(const FixedPosition& _point) const noexcept
{
  const std::int32_t x = _point.xMeters.FloorWhole();
  const std::int32_t z = _point.zMeters.FloorWhole();
  return _point.yMeters.Raw() < 0 || x < 0 || z < 0 || x >= m_desc.widthChunks * Protocol::CHUNK_EDGE_VOXELS ||
         z >= m_desc.depthChunks * Protocol::CHUNK_EDGE_VOXELS;
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
  // Step 4: engines reload, then each shot, in id order, moves along its arc through every voxel its
  // step crosses, and stops at the first solid one: its hit (M2-E1).
  m_tickHits.clear();
  for (auto& [id, engine] : m_siegeEngines)
  {
    if (engine.reloadTicks > 0)
    {
      --engine.reloadTicks;
    }
  }
  std::vector<Protocol::EntityId> landed;
  for (auto& [id, shot] : m_shots)
  {
    const FixedPosition from = ShotPositionAt(shot.arc, shot.ticksFlown);
    ++shot.ticksFlown;
    const FixedPosition to = ShotPositionAt(shot.arc, shot.ticksFlown);
    const std::optional<Protocol::VoxelPosition> hit = TraverseVoxels(
      from, to, [this](Protocol::VoxelPosition _voxel) { return _voxel.yVoxels >= 0 && m_voxels.At(_voxel) != Protocol::Chunk::EMPTY; });
    if (hit.has_value())
    {
      const bool alongX = std::abs(shot.arc.acrossXMeters.Raw()) >= std::abs(shot.arc.acrossZMeters.Raw());
      const auto sign = [](Core::Fixed _value) { return _value.Raw() > 0 ? 1 : (_value.Raw() < 0 ? -1 : 0); };
      const ShotHit& struck = m_tickHits.emplace_back(ShotHit{.shot = id,
                                                              .voxel = *hit,
                                                              .behindX = alongX ? sign(shot.arc.acrossXMeters) : 0,
                                                              .behindZ = alongX ? 0 : sign(shot.arc.acrossZMeters)});
      // Its crater, as edits to empty (M2-E2).
      for (const Protocol::VoxelPosition& voxel : CraterVoxels(
             struck.voxel, struck.behindX, struck.behindZ, [this](Protocol::VoxelPosition _voxel) { return m_voxels.At(_voxel); },
             Data().Materials(), Data().Tuning()))
      {
        m_voxels.Edit(voxel, Protocol::Chunk::EMPTY);
      }
      landed.push_back(id);
    }
    else if (OutsideWorld(to))
    {
      landed.push_back(id);
    }
  }
  for (const Protocol::EntityId id : landed)
  {
    m_shots.erase(id);
  }
}

void World::SearchSupport()
{
  // Step 5: the support search over the edits not yet searched: the last tick's steps 6 and 7, and
  // this tick's so far, those made before it between ticks included (M2-E3; ADR-004, decision 2).
  std::vector<Protocol::VoxelPosition> edits = std::exchange(m_unsearchedEdits, {});
  const std::span<const Protocol::VoxelPosition> tick = m_voxels.TickEdits();
  edits.insert(edits.end(), tick.begin(), tick.end());
  m_searchedThrough = tick.size();
  if (edits.empty())
  {
    return;
  }
  const SupportRules rules{.materials = &Data().Materials(),
                           .anchorDepthVoxels = ANCHOR_DEPTH_VOXELS,
                           .surfaceVoxels = m_desc.surfaceVoxels,
                           .heightVoxels = m_desc.heightChunks * Protocol::CHUNK_EDGE_VOXELS};
  const SupportSearchResult found = FindUnsupported(m_voxels.Store(), rules, edits);
  // A piece still warning is still in the grid, and a search near it finds it again: its voxels are
  // claimed, and only what is new collapses.
  std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> claimed;
  for (const auto& [id, collapse] : m_collapses)
  {
    if (collapse.ticksElapsed <= collapse.plan.warningTicks)
    {
      for (const Protocol::VoxelPosition& voxel : collapse.piece)
      {
        claimed.insert({voxel.xVoxels, voxel.yVoxels, voxel.zVoxels});
      }
    }
  }
  // One collapse for each unsupported piece, given ids in piece order.
  for (Piece piece : found.pieces)
  {
    std::erase_if(piece, [&claimed](const Protocol::VoxelPosition& _voxel)
                  { return claimed.contains({_voxel.xVoxels, _voxel.yVoxels, _voxel.zVoxels}); });
    if (piece.empty())
    {
      continue;
    }
    const Protocol::EntityId id = NewEntityId();
    CollapsePlan plan = PlanCollapse(piece, edits, m_voxels, Data());
    m_collapses.emplace(id, Collapse{.id = id, .piece = std::move(piece), .plan = std::move(plan), .ticksElapsed = 0});
  }
}

void World::AdvanceCollapses()
{
  // Step 6: each collapse in id order counts its warning down; no voxel of a piece moves during it.
  for (auto& [id, collapse] : m_collapses)
  {
    ++collapse.ticksElapsed;
  }
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
