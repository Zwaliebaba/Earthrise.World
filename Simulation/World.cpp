#include "pch.h"

#include "World.h"

#include "Formation.h"
#include "Generator.h"
#include "HealthStructureRule.h"
#include "IntegerMath.h"
#include "Integrity.h"
#include "VoxelStructureRule.h"

namespace Simulation
{

FixedPosition CellFloorMiddle(Protocol::VoxelPosition _cellVoxels)
{
  const Core::Fixed half = Core::Fixed::FromRaw(Core::Fixed::ONE_RAW / 2);
  return {.xMeters = Core::Fixed::FromWhole(_cellVoxels.xVoxels) + half,
          .yMeters = Core::Fixed::FromWhole(_cellVoxels.yVoxels),
          .zMeters = Core::Fixed::FromWhole(_cellVoxels.zVoxels) + half};
}

namespace
{

NavigationGrid::Desc NavigationDesc(const World::Desc& _desc)
{
  if (_desc.serverData == nullptr)
  {
    throw std::invalid_argument("a world needs server data");
  }
  return {.widthCells = _desc.widthChunks * Protocol::CHUNK_EDGE_VOXELS,
          .depthCells = _desc.depthChunks * Protocol::CHUNK_EDGE_VOXELS,
          .heightVoxels = _desc.heightChunks * Protocol::CHUNK_EDGE_VOXELS,
          .climbVoxels = 1,
          .rules = &_desc.serverData->Tuning()};
}

/// A template column (_x, _z) turned clockwise seen from above by _turns quarter turns, as the voxel pass turns a batch:
/// one turn takes +z to +x, and the footprint's minimum corner stays at the origin.
std::pair<std::int32_t, std::int32_t> Turned(std::int32_t _x, std::int32_t _z, std::int32_t _width, std::int32_t _depth,
                                             std::uint32_t _turns)
{
  switch (_turns % 4)
  {
  case 1:
    return {_z, _width - 1 - _x};
  case 2:
    return {_width - 1 - _x, _depth - 1 - _z};
  case 3:
    return {_depth - 1 - _z, _x};
  default:
    return {_x, _z};
  }
}

/// Whether a palette index is a structure's as loose material sees it: solid, not ground and not liquid (M2-E7).
bool IsStructureIndex(const MaterialRules& _materials, std::uint8_t _index)
{
  const MaterialRule& rule = _materials.at(_index);
  return _index != Protocol::Chunk::EMPTY && !rule.walkableGround && !rule.liquid;
}

/// Whether a palette index is loose: earth, grass and every rubble (M2-E6).
bool IsLooseIndex(const MaterialRules& _materials, std::uint8_t _index)
{
  const Behavior behavior = _materials.at(_index).behavior;
  return behavior == Behavior::Earth || behavior == Behavior::StoneRubble || behavior == Behavior::WasteRubble;
}

/// How far _at lies outside the run _low to _high, both included: 0 inside it.
std::int32_t Outside(std::int32_t _at, std::int32_t _low, std::int32_t _high)
{
  return std::max({_low - _at, 0, _at - _high});
}

/// How many cells sappers reach a trench from, across the ground: a squad's two ranks (ADR-029, decision 2).
constexpr std::int32_t TRENCH_REACH_CELLS = 2;

/// Whether _cell stands within TRENCH_REACH_CELLS of the box _low to _high across the ground, inside it included.
bool WithinTrenchReach(Protocol::VoxelPosition _low, Protocol::VoxelPosition _high, Protocol::VoxelPosition _cell)
{
  return std::max(Outside(_cell.xVoxels, _low.xVoxels, _high.xVoxels), Outside(_cell.zVoxels, _low.zVoxels, _high.zVoxels)) <=
         TRENCH_REACH_CELLS;
}

} // namespace

World::World(const Desc& _desc)
  : m_desc(_desc),
    m_navigation(NavigationDesc(_desc), _desc.serverData->Materials())
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
  // Reach too, so the first search does not pay for labelling every tile in a tick.
  m_navigation.RebuildAll(m_voxels.Store());
  m_navigation.UpdateReach();
}

void World::Submit(std::uint8_t _player, std::span<const std::byte> _command)
{
  m_pendingCommands.push_back({.player = _player, .bytes = {_command.begin(), _command.end()}});
}

void World::Ask(std::uint8_t _player, std::span<const std::byte> _query)
{
  m_pendingQueries.push_back({.player = _player, .bytes = {_query.begin(), _query.end()}});
  if (m_pendingQueries.size() > MAX_PENDING_QUERIES)
  {
    m_pendingQueries.pop_front();
  }
}

std::vector<std::vector<std::byte>> World::TakeAnswers()
{
  return std::exchange(m_answers, {});
}

bool World::Quiet() const
{
  if (!m_shots.empty() || !m_collapses.empty() || !m_slumpQueue.empty() || !m_searches.empty() || !m_unsearchedEdits.empty() ||
      !m_voxels.TickEdits().empty() || !m_pendingCommands.empty())
  {
    return false;
  }
  // Every figure stands still: in no step, and with no step its squad's plan gives it that it could take now, into a
  // free cell or by a swap (ADR-022).
  const Occupancy occupancy = BuildOccupancy();
  const Plans plans = PlanAll();
  const std::map<Protocol::EntityId, PathSearch::CellIndex> wanted = Wanted(plans);
  for (const auto& [id, place] : plans.places)
  {
    const Unit& unit = m_units.at(id);
    if (unit.reservedCell >= 0 || unit.stepEffort > 0)
    {
      return false;
    }
    const auto step = wanted.find(id);
    if (step != wanted.end() && (Free(occupancy, unit, step->second) || SwapPartner(unit, step->second, occupancy, wanted).has_value()))
    {
      return false;
    }
  }
  return true;
}

void World::Tick()
{
  m_tilesRebuilt = false;
  m_touchedStructures.clear();
  m_tickCrushes.clear();
  m_endedCollapses.clear();
  m_tickEvents.clear();
  m_tickRefusals.clear();
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
  AnswerQueries(m_tick);
  RunCommanders();
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

std::vector<Protocol::ChunkSnapshot> World::Publish(std::uint8_t _player)
{
  std::vector<Protocol::ChunkSnapshot> changed = m_voxels.Publish();
  if (!Fogged())
  {
    return changed;
  }
  // Each chunk whose column the player sees, or every one while their map is revealed, unless they hold its snapshot.
  std::map<std::uint64_t, std::uint64_t>& sent = m_chunksSent[_player];
  const bool revealed = Revealed(_player);
  std::vector<Protocol::ChunkSnapshot> snapshots;
  for (Protocol::ChunkSnapshot& snapshot : m_voxels.Store().Snapshots())
  {
    const std::uint64_t key = Protocol::PackChunkKey(snapshot.coordinate);
    const auto last = sent.find(key);
    const std::int32_t x = snapshot.coordinate.xChunks * Protocol::CHUNK_EDGE_VOXELS;
    const std::int32_t z = snapshot.coordinate.zChunks * Protocol::CHUNK_EDGE_VOXELS;
    if ((last != sent.end() && last->second == snapshot.version) ||
        (!revealed && !SeesAny(_player, x, z, x + Protocol::CHUNK_EDGE_VOXELS - 1, z + Protocol::CHUNK_EDGE_VOXELS - 1)))
    {
      continue;
    }
    sent.insert_or_assign(key, snapshot.version);
    snapshots.push_back(std::move(snapshot));
  }
  return snapshots;
}

void World::RecomputeVisibility()
{
  // ADR-025, decision 1: every unit not garrisoned stamps its vision about the cells it stands in, an engine's body its
  // three, and every structure its vision about its footprint, its garrisoned vision while it holds a garrison.
  if (!Fogged())
  {
    m_visibility.clear();
    return;
  }
  for (const auto& [id, player] : m_players)
  {
    m_visibility.try_emplace(id, m_navigation.WidthCells(), m_navigation.DepthCells()).first->second.Clear();
  }
  const std::int32_t machineSide = std::max(1, Data().Tuning().machinesWidthVoxels);
  for (const auto& [id, unit] : m_units)
  {
    const auto visibility = m_visibility.find(unit.owner);
    if (visibility == m_visibility.end() || Garrisoned(unit))
    {
      continue;
    }
    const std::int32_t side = unit.navigationClass == NavigationClass::Machines ? machineSide : 1;
    visibility->second.StampBox(
      unit.cellVoxels, {.xVoxels = unit.cellVoxels.xVoxels + side - 1, .yVoxels = 0, .zVoxels = unit.cellVoxels.zVoxels + side - 1},
      Data().Stats(unit.kind).visionVoxels);
  }
  for (const auto& [id, structure] : m_structures)
  {
    const auto visibility = m_visibility.find(structure.owner);
    const TemplateRules* rules = Data().FindTemplateRules(structure.templateId);
    const std::optional<Box> box = BoxOf(structure);
    if (visibility == m_visibility.end() || rules == nullptr || !box.has_value())
    {
      continue;
    }
    visibility->second.StampBox(
      box->min, {.xVoxels = box->min.xVoxels + box->widthVoxels - 1, .yVoxels = 0, .zVoxels = box->min.zVoxels + box->depthVoxels - 1},
      GarrisonedCount(id) > 0 ? rules->garrisonedVisionVoxels : rules->visionVoxels);
  }
}

bool World::Fogged() const noexcept
{
  return !m_players.empty();
}

bool World::Revealed(std::uint8_t _player) const
{
  const auto player = m_players.find(_player);
  return player != m_players.end() && player->second.revealed &&
         m_tick < static_cast<std::uint32_t>(std::max(0, Data().Tuning().revealTicks));
}

bool World::SeesAny(std::uint8_t _player, std::int32_t _x0, std::int32_t _z0, std::int32_t _x1, std::int32_t _z1) const
{
  const auto visibility = m_visibility.find(_player);
  return visibility != m_visibility.end() && visibility->second.AnyVisible(_x0, _z0, _x1, _z1);
}

bool World::SeesStructure(std::uint8_t _player, const Structure& _structure) const
{
  if (!Fogged() || _structure.owner == _player || Revealed(_player))
  {
    return true;
  }
  const std::optional<Box> box = BoxOf(_structure);
  return box.has_value() && SeesAny(_player, box->min.xVoxels, box->min.zVoxels, box->min.xVoxels + box->widthVoxels - 1,
                                    box->min.zVoxels + box->depthVoxels - 1);
}

Protocol::StructureView World::StructureViewFor(std::uint8_t _player, const Structure& _structure) const
{
  // Its box after its turn: its template's shape, with width and depth swapped by an odd turn.
  Protocol::VoxelPosition extent{};
  if (const ServerTemplate* found = Data().FindTemplate(_structure.templateId); found != nullptr)
  {
    const TemplateShape shape = found->Shape();
    const auto width = static_cast<std::int32_t>(shape.widthVoxels);
    const auto depth = static_cast<std::int32_t>(shape.depthVoxels);
    const bool across = _structure.quarterTurns % 2 == 1;
    extent = {across ? depth : width, static_cast<std::int32_t>(shape.heightVoxels), across ? width : depth};
  }
  // The figures garrisoned in it, which only its owner's update counts (ADR-024, decision 5).
  const TemplateRules* rules = Data().FindTemplateRules(_structure.templateId);
  const std::size_t garrisoned = _structure.owner == _player ? GarrisonedCount(_structure.id) : 0;
  return {.id = _structure.id,
          .owner = _structure.owner,
          .templateId = _structure.templateId,
          .originVoxels = _structure.originVoxels,
          .quarterTurns = _structure.quarterTurns,
          .extentVoxels = extent,
          .name = _structure.name,
          .working = _structure.working,
          .criticalPercent = _structure.criticalPercent,
          .repairLockTicks = _structure.repairLockTicks,
          .gateOpen = _structure.gateOpen,
          .garrisonCount = static_cast<std::uint8_t>(garrisoned),
          .garrisonCapacity = rules == nullptr ? std::uint8_t{0} : rules->garrisonCapacity,
          .hitPoints = std::max(0, _structure.hitPoints), // a control pool below 0 shows as 0 (ADR-028, decision 3)
          .maxHitPoints = Rule().MaxHitPoints(*this, _structure),
          .opened = m_trialProgress.openedSections.contains(_structure.id),
          .lastSeen = false,
          .standingPercent = _structure.standingPercent};
}

bool World::AnswerVisible(std::uint8_t _player, const Protocol::Query& _query) const
{
  // A structure the asker sees, a tile of 32 by 32 cells one of whose cells they see, or a target whose column they see;
  // anything while their map is revealed.
  constexpr std::int32_t TILE_EDGE_CELLS = 32; // TileReachReading's
  if (!Fogged() || Revealed(_player))
  {
    return true;
  }
  if (const auto* integrity = std::get_if<Protocol::SectionIntegrity>(&_query))
  {
    const auto structure = m_structures.find(integrity->structure);
    return structure == m_structures.end() || SeesStructure(_player, structure->second);
  }
  if (const auto* reach = std::get_if<Protocol::TileReach>(&_query))
  {
    const std::int32_t x = reach->tileX * TILE_EDGE_CELLS;
    const std::int32_t z = reach->tileZ * TILE_EDGE_CELLS;
    return SeesAny(_player, x, z, x + TILE_EDGE_CELLS - 1, z + TILE_EDGE_CELLS - 1);
  }
  const Protocol::VoxelPosition target = std::holds_alternative<Protocol::AimCheck>(_query)
                                           ? std::get<Protocol::AimCheck>(_query).targetVoxels
                                           : std::get<Protocol::SquadAim>(_query).targetVoxels;
  return SeesAny(_player, target.xVoxels, target.zVoxels, target.xVoxels, target.zVoxels);
}

Protocol::Reading World::EmptyReading(const Protocol::Query& _query)
{
  if (const auto* integrity = std::get_if<Protocol::SectionIntegrity>(&_query))
  {
    return Protocol::IntegrityReading{.structure = integrity->structure,
                                      .ground = Protocol::GroundKind::Earth,
                                      .canBeUndermined = false,
                                      .holeWidthVoxels = 0,
                                      .holeHeightVoxels = 0,
                                      .holeFallWidthVoxels = 0,
                                      .hasOpening = false,
                                      .openingFloorVoxels = 0,
                                      .openingFloor = Protocol::OpeningFloor::Stone,
                                      .openingClearVoxels = 0,
                                      .routeNow = false,
                                      .gapRunVoxels = 0,
                                      .gapFallRunVoxels = 0,
                                      .falling = false,
                                      .weakMinVoxels = {},
                                      .weakMaxVoxels = {}};
  }
  if (const auto* reach = std::get_if<Protocol::TileReach>(&_query))
  {
    Protocol::TileReachReading reading{.tileX = reach->tileX, .tileZ = reach->tileZ, .usable = {}, .reached = {}, .surfaceVoxels = {}};
    reading.surfaceVoxels.fill(Protocol::NO_SURFACE_VOXELS);
    return reading;
  }
  return Protocol::AimReading{.verdict = Protocol::AimVerdict::OutOfRange, .distanceMeters = 0, .strikes = false, .strikeVoxels = {}};
}

const Protocol::StateUpdate& World::State(std::uint8_t _player) const
{
  static const Protocol::StateUpdate NONE{};
  const auto found = m_states.find(_player);
  return found == m_states.end() ? NONE : found->second;
}

const Protocol::StateUpdate& World::State() const
{
  return State(1);
}

std::span<const TrialEvent> World::LastTickEvents() const noexcept
{
  return m_tickEvents;
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

const NavigationGrid& World::Navigation() const noexcept
{
  return m_navigation;
}

const std::map<std::pair<std::uint32_t, Protocol::EntityId>, PathSearch>& World::Searches() const noexcept
{
  return m_searches;
}

void World::RequestSearch(Squad& _squad)
{
  std::erase_if(m_searches, [&_squad](const auto& _search) { return _search.first.second == _squad.id; });
  if (_squad.units.empty() || !_squad.hasGoal)
  {
    return;
  }
  const Unit& lead = m_units.at(_squad.units.front());
  const std::int32_t width = m_navigation.WidthCells();
  const std::int32_t goalX = std::clamp(_squad.goalVoxels.xVoxels, 0, width - 1);
  const std::int32_t goalZ = std::clamp(_squad.goalVoxels.zVoxels, 0, m_navigation.DepthCells() - 1);
  // From the cell the lead stands in or is stepping into, which its squad holds to until the path arrives.
  const PathSearch::CellIndex start = lead.reservedCell >= 0 ? lead.reservedCell : CellOf(lead.cellVoxels);
  const PathSearch::CellIndex goal = std::int64_t{goalZ} * width + goalX;
  // Reach is known without searching the map (M2-F4): the search goes for the goal, or for the
  // reachable cell nearest it when the goal is out of reach.
  m_navigation.UpdateReach();
  const PathSearch::CellIndex target = m_navigation.NearestReachable(start, goal, _squad.navigationClass);
  m_searches.emplace(std::pair{m_tick, _squad.id}, PathSearch(_squad.id, _squad.navigationClass, start, goal, target, width));
}

std::span<const CrushEvent> World::LastTickCrushes() const noexcept
{
  return m_tickCrushes;
}

std::span<const Protocol::VoxelPosition> World::LastTickEdits() const noexcept
{
  return m_lastTickEdits;
}

Protocol::EntityId World::AddSquad(std::uint8_t _owner, UnitKind _kind, std::span<const Protocol::VoxelPosition> _cellsVoxels)
{
  const UnitStats& stats = Data().Stats(_kind);
  const NavigationClass navigationClass = stats.navigationClass;
  const Protocol::EntityId squadId = NewEntityId();
  Squad squad{.id = squadId,
              .owner = _owner,
              .navigationClass = navigationClass,
              .units = {},
              .hasGoal = false,
              .goalVoxels = {},
              .path = {},
              .shortOfGoal = false,
              .trail = {},
              .garrisonedIn = Protocol::NO_ENTITY,
              .kind = _kind,
              .facing = 0,
              .order = Protocol::SquadOrder::Idle,
              .target = Protocol::NO_ENTITY,
              .trenchMinVoxels = {},
              .trenchMaxVoxels = {},
              .trenchNearSide = 0,
              .digProgress = {}};
  for (const Protocol::VoxelPosition& cell : _cellsVoxels)
  {
    const Protocol::EntityId unitId = NewEntityId();
    m_units.emplace(unitId, Unit{.id = unitId,
                                 .owner = _owner,
                                 .navigationClass = navigationClass,
                                 .squad = squadId,
                                 .position = CellFloorMiddle(cell),
                                 .cellVoxels = cell,
                                 .trailStep = 0,
                                 .stepEffort = 0,
                                 .kind = _kind,
                                 .hitPoints = stats.hitPoints,
                                 .reservedCell = NO_CELL,
                                 .stepFromCell = CellOf(cell),
                                 .waitTicks = 0,
                                 .reloadTicks = 0,
                                 .target = Protocol::NO_ENTITY});
    squad.units.push_back(unitId);
  }
  m_squads.emplace(squadId, std::move(squad));
  return squadId;
}

Protocol::EntityId World::AddSiegeEngine(std::uint8_t _owner, EngineKind _kind, Protocol::VoxelPosition _cellVoxels,
                                         std::uint32_t _quarterTurns, EngineState _state)
{
  const Protocol::EntityId id = NewEntityId();
  const std::array<Protocol::VoxelPosition, 1> body = {_cellVoxels};
  const Protocol::EntityId squad = AddSquad(_owner, _kind == EngineKind::Ram ? UnitKind::Ram : UnitKind::Trebuchet, body);
  m_siegeEngines.emplace(id, SiegeEngine{.id = id,
                                         .owner = _owner,
                                         .kind = _kind,
                                         .position = CellFloorMiddle(_cellVoxels),
                                         .cellVoxels = _cellVoxels,
                                         .reloadTicks = 0,
                                         .quarterTurns = _quarterTurns % 4,
                                         .state = _state,
                                         .squad = squad,
                                         .stateTicksLeft = 0});
  return id;
}

Protocol::EntityId World::AddStructure(std::uint8_t _owner, std::string_view _templateId, Protocol::VoxelPosition _originVoxels,
                                       std::uint32_t _quarterTurns, std::string_view _name, std::int32_t _foundationDepthVoxels)
{
  const ServerTemplate* found = Data().FindTemplate(_templateId);
  if (found == nullptr)
  {
    throw std::invalid_argument("the server data holds no template " + std::string(_templateId));
  }
  const TemplateShape shape = found->Shape();
  const auto width = static_cast<std::int32_t>(shape.widthVoxels);
  const auto depth = static_cast<std::int32_t>(shape.depthVoxels);
  const std::uint32_t turns = _quarterTurns % 4;
  for (std::uint32_t y = 0; y < shape.heightVoxels; ++y)
  {
    for (std::int32_t z = 0; z < depth; ++z)
    {
      for (std::int32_t x = 0; x < width; ++x)
      {
        const std::uint8_t index = shape.At(static_cast<std::uint32_t>(x), y, static_cast<std::uint32_t>(z));
        if (index == Protocol::Chunk::EMPTY)
        {
          continue;
        }
        const auto [turnedX, turnedZ] = Turned(x, z, width, depth, turns);
        m_voxels.Edit({.xVoxels = _originVoxels.xVoxels + turnedX,
                       .yVoxels = _originVoxels.yVoxels + static_cast<std::int32_t>(y),
                       .zVoxels = _originVoxels.zVoxels + turnedZ},
                      index);
        // The foundation course: the bottom layer's building stone, repeated down (ADR-026, decision 1). Timber and
        // the gate it holds stand on the ground.
        if (y == 0 && Data().Materials().at(index).behavior == Behavior::BuildingStone)
        {
          for (std::int32_t below = 1; below <= _foundationDepthVoxels; ++below)
          {
            m_voxels.Edit({.xVoxels = _originVoxels.xVoxels + turnedX,
                           .yVoxels = _originVoxels.yVoxels - below,
                           .zVoxels = _originVoxels.zVoxels + turnedZ},
                          index);
          }
        }
      }
    }
  }
  // Built whole, as a scenario's structures start (ADR-024, decision 1).
  const std::size_t voxels = static_cast<std::size_t>(shape.widthVoxels) * shape.heightVoxels * shape.depthVoxels;
  std::vector<std::uint8_t> mask((voxels + 7) / 8, std::uint8_t{0});
  for (std::size_t voxel = 0; voxel < voxels; ++voxel)
  {
    if (shape.indices[voxel] != Protocol::Chunk::EMPTY)
    {
      mask[voxel / 8] = static_cast<std::uint8_t>(mask[voxel / 8] | (1U << (voxel % 8)));
    }
  }
  const Protocol::EntityId id = NewEntityId();
  Structure& structure = m_structures
                           .emplace(id, Structure{.id = id,
                                                  .owner = _owner,
                                                  .templateId = std::string(_templateId),
                                                  .originVoxels = _originVoxels,
                                                  .quarterTurns = turns,
                                                  .name = std::string(_name),
                                                  .builtMask = std::move(mask),
                                                  .criticalPercent = 100,
                                                  .working = true,
                                                  .repairLockTicks = 0,
                                                  .repairProgress = {},
                                                  .gateOpen = false,
                                                  .hitPoints = 0})
                           .first->second;
  const ServerTemplate& stamped = *found;
  structure.criticalPercent = CriticalShare(structure, HeldByCollapses());
  structure.standingPercent = StandingShare(structure, HeldByCollapses());
  const TemplateRules* rules = Data().FindTemplateRules(stamped.id);
  structure.working = rules == nullptr || structure.criticalPercent >= rules->criticalThresholdPercent;
  return id;
}

const std::map<Protocol::EntityId, Structure>& World::Structures() const noexcept
{
  return m_structures;
}

void World::AddPlayer(const Player& _player)
{
  m_players.insert_or_assign(_player.id, _player);
}

const std::map<std::uint8_t, Player>& World::Players() const noexcept
{
  return m_players;
}

void World::PlaceInGarrison(Protocol::EntityId _squad, Protocol::EntityId _structure)
{
  const auto squad = m_squads.find(_squad);
  const auto structure = m_structures.find(_structure);
  if (squad == m_squads.end() || structure == m_structures.end())
  {
    throw std::invalid_argument("a garrison needs a squad and a structure the world holds");
  }
  squad->second.garrisonedIn = _structure;
  squad->second.hasGoal = false;
  squad->second.path.clear();
  squad->second.trail.clear();
  std::erase_if(m_searches, [_squad](const auto& _search) { return _search.first.second == _squad; });
  for (const Protocol::EntityId unitId : squad->second.units)
  {
    Unit& unit = m_units.at(unitId);
    unit.cellVoxels = structure->second.originVoxels;
    unit.position = CellFloorMiddle(unit.cellVoxels);
    unit.trailStep = OFF_TRAIL;
    unit.stepEffort = 0;
    unit.reservedCell = NO_CELL;
    unit.stepFromCell = CellOf(unit.cellVoxels);
    unit.waitTicks = 0;
  }
}

void World::SetTrial(const TrialRules& _rules)
{
  m_trial = _rules;
  // The control starts every structure with its template's pool (ADR-028, decision 2).
  if (_rules.version == Protocol::TrialVersion::Control)
  {
    for (auto& [id, structure] : m_structures)
    {
      const TemplateRules* rules = Data().FindTemplateRules(structure.templateId);
      structure.hitPoints = rules == nullptr ? 0 : rules->controlHitPoints;
    }
  }
}

const std::optional<TrialRules>& World::Trial() const noexcept
{
  return m_trial;
}

bool World::Garrisoned(const Unit& _unit) const
{
  const auto squad = m_squads.find(_unit.squad);
  return squad != m_squads.end() && squad->second.garrisonedIn != Protocol::NO_ENTITY;
}

void World::Refuse(std::uint8_t _player, const Protocol::Command& _command, Protocol::Refusal _reason)
{
  m_tickRefusals.emplace_back(_player, Protocol::RefusalView{.commandTag = Protocol::CommandTag(_command), .reason = _reason});
}

SiegeEngine* World::EngineOfBody(Protocol::EntityId _squad)
{
  const auto found = std::ranges::find_if(m_siegeEngines, [_squad](const auto& _engine) { return _engine.second.squad == _squad; });
  return found == m_siegeEngines.end() ? nullptr : &found->second;
}

void World::Crush(Unit& _unit, std::int32_t _amount)
{
  const std::int32_t armor = Data().Stats(_unit.kind).armor.at(static_cast<std::size_t>(Protocol::DamageType::Crush));
  _unit.hitPoints -= std::max(1, _amount - armor);
}

void World::RemoveDead()
{
  std::vector<Protocol::EntityId> dead;
  for (const auto& [id, unit] : m_units)
  {
    if (unit.hitPoints <= 0)
    {
      dead.push_back(id);
    }
  }
  for (const Protocol::EntityId id : dead)
  {
    const Unit unit = m_units.at(id);
    m_tickEvents.push_back({.kind = TrialEventKind::UnitDied,
                            .tick = m_tick,
                            .player = unit.owner,
                            .subject = id,
                            .other = unit.squad,
                            .amount = static_cast<std::int32_t>(unit.kind),
                            .voxel = unit.cellVoxels});
    m_units.erase(id);
    const auto squad = m_squads.find(unit.squad);
    if (squad == m_squads.end())
    {
      continue;
    }
    std::erase(squad->second.units, id);
    if (!squad->second.units.empty())
    {
      continue;
    }
    // Its last figure: the squad goes, and an engine with its body.
    std::erase_if(m_siegeEngines, [&unit](const auto& _engine) { return _engine.second.squad == unit.squad; });
    RemoveSquad(unit.squad);
  }
}

void World::FollowBodies()
{
  for (auto& [id, engine] : m_siegeEngines)
  {
    const auto squad = m_squads.find(engine.squad);
    if (squad == m_squads.end() || squad->second.units.empty())
    {
      continue;
    }
    const Unit& body = m_units.at(squad->second.units.front());
    engine.cellVoxels = body.cellVoxels;
    engine.position = body.position;
  }
}

void World::Acquire()
{
  EnterGarrisons();
  // ADR-023, decision 4. A squad attacking a target that is gone turns idle where it stands; one whose target's lead has
  // gone ATTACK_FOLLOW_CELLS from its goal searches again, towards where that lead stands now.
  for (auto& [id, squad] : m_squads)
  {
    if (squad.garrisonedIn != Protocol::NO_ENTITY || squad.units.empty() || squad.order != Protocol::SquadOrder::Attack)
    {
      continue;
    }
    if (m_structures.contains(squad.target))
    {
      continue; // a ram's: step 4 drives and strikes it
    }
    const Squad* target = TargetSquad(squad.target);
    if (target == nullptr || target->garrisonedIn != Protocol::NO_ENTITY || target->units.empty())
    {
      Halt(squad, Protocol::SquadOrder::Idle);
      continue;
    }
    const Protocol::VoxelPosition at = m_units.at(target->units.front()).cellVoxels;
    if (!Holding(id) && (std::abs(at.xVoxels - squad.goalVoxels.xVoxels) > ATTACK_FOLLOW_CELLS ||
                         std::abs(at.zVoxels - squad.goalVoxels.zVoxels) > ATTACK_FOLLOW_CELLS))
    {
      squad.goalVoxels = at;
      RequestSearch(squad);
    }
  }
  // Each figure with an attack takes the nearest enemy figure within its vision, ties to the lower id, or when its squad
  // attacks, the nearest of its target's at any range. A moving squad's figures take none: they strike what they meet
  // (decision 3) and walk on; nor does a laborer set to repair or a sapper set to dig, which keep to their work (ADR-024,
  // decision 3; ADR-029).
  for (auto& [id, unit] : m_units)
  {
    unit.target = Protocol::NO_ENTITY;
    const Squad& squad = m_squads.at(unit.squad);
    if (Garrisoned(unit) || Data().Stats(unit.kind).attack <= 0 || squad.order == Protocol::SquadOrder::Move ||
        squad.order == Protocol::SquadOrder::Repair || squad.order == Protocol::SquadOrder::Trench)
    {
      continue;
    }
    if (squad.order == Protocol::SquadOrder::Attack)
    {
      const Squad* target = TargetSquad(squad.target);
      unit.target = target == nullptr ? Protocol::NO_ENTITY : Nearest(unit, std::numeric_limits<std::int64_t>::max(), target->id);
    }
    else
    {
      unit.target = Nearest(unit, Data().Stats(unit.kind).visionVoxels, Protocol::NO_ENTITY);
    }
  }
  // A moving or storming squad whose lead stands at its trail's end has arrived, and turns idle. One whose lead left its
  // trail to fight walks on from where the lead stands once it fights no more.
  for (auto& [id, squad] : m_squads)
  {
    if (squad.garrisonedIn != Protocol::NO_ENTITY || squad.units.empty() || !squad.hasGoal || Holding(id))
    {
      continue;
    }
    const Unit& lead = m_units.at(squad.units.front());
    const bool walking = squad.order == Protocol::SquadOrder::Move || squad.order == Protocol::SquadOrder::Storm;
    if (lead.reservedCell >= 0 || !(walking || squad.order == Protocol::SquadOrder::Attack))
    {
      continue;
    }
    if (lead.trailStep == OFF_TRAIL)
    {
      if (lead.target == Protocol::NO_ENTITY)
      {
        RequestSearch(squad);
      }
    }
    else if (walking && std::size_t{lead.trailStep} + 1 >= squad.trail.size())
    {
      squad.order = Protocol::SquadOrder::Idle;
    }
  }
}

Protocol::EntityId World::Nearest(const Unit& _unit, std::int64_t _rangeVoxels, Protocol::EntityId _onlySquad) const
{
  // By squared distance across the ground, cell to cell; an engine's body by its cell, its footprint's corner. A ranged
  // figure also takes what its reach at the elevation covers, so that one standing high can shoot as far as it reaches.
  const std::int64_t limit = _rangeVoxels >= 3'037'000'499 ? std::numeric_limits<std::int64_t>::max() : _rangeVoxels * _rangeVoxels;
  const std::int32_t rangeVoxels = Data().Stats(_unit.kind).rangeVoxels;
  Protocol::EntityId nearest = Protocol::NO_ENTITY;
  std::int64_t best = 0;
  for (const auto& [id, other] : m_units)
  {
    if (other.owner == _unit.owner || Garrisoned(other) || (_onlySquad != Protocol::NO_ENTITY && other.squad != _onlySquad))
    {
      continue;
    }
    const std::int64_t dx = std::int64_t{other.cellVoxels.xVoxels} - _unit.cellVoxels.xVoxels;
    const std::int64_t dz = std::int64_t{other.cellVoxels.zVoxels} - _unit.cellVoxels.zVoxels;
    const std::int64_t distance = (dx * dx) + (dz * dz);
    const std::int64_t reach =
      rangeVoxels > 0 ? ArrowRangeVoxels(rangeVoxels, ElevationSteps(_unit.cellVoxels.yVoxels, other.cellVoxels.yVoxels, Data().Tuning()),
                                         Data().Tuning())
                      : 0;
    if ((distance <= limit || distance <= reach * reach) && (nearest == Protocol::NO_ENTITY || distance < best))
    {
      nearest = id;
      best = distance;
    }
  }
  return nearest;
}

bool World::Adjacent(const Unit& _first, const Unit& _second) const
{
  // Two footprints touch when they are at most a cell apart across x and across z: the eight neighbouring cells of a
  // figure, and of each cell of a machine's square (ADR-023, decision 3).
  const auto side = [this](const Unit& _unit)
  { return _unit.navigationClass == NavigationClass::Machines ? std::max(1, Data().Tuning().machinesWidthVoxels) : 1; };
  const Protocol::VoxelPosition& first = _first.cellVoxels;
  const Protocol::VoxelPosition& second = _second.cellVoxels;
  return second.xVoxels <= first.xVoxels + side(_first) && first.xVoxels <= second.xVoxels + side(_second) &&
         second.zVoxels <= first.zVoxels + side(_first) && first.zVoxels <= second.zVoxels + side(_second);
}

void World::Melee()
{
  // ADR-023, decision 3. Reloads run down; then each figure with a melee attack whose reload is due strikes its target
  // when it touches it, or else the nearest enemy figure that touches it, ties to the lower id. Every strike is worked
  // out from the state before any lands, and all land together, so that ids decide nothing about who strikes first.
  for (auto& [id, unit] : m_units)
  {
    if (unit.reloadTicks > 0)
    {
      --unit.reloadTicks;
    }
  }
  struct Strike
  {
    Protocol::EntityId striker;
    Protocol::EntityId struck;
    std::int32_t damage;
  };
  std::vector<Strike> strikes;
  for (const auto& [id, unit] : m_units)
  {
    const UnitStats& stats = Data().Stats(unit.kind);
    if (Garrisoned(unit) || stats.attack <= 0 || stats.rangeVoxels > 0 || unit.reloadTicks > 0)
    {
      continue;
    }
    Protocol::EntityId struck = Protocol::NO_ENTITY;
    if (const auto target = m_units.find(unit.target); target != m_units.end() && Adjacent(unit, target->second))
    {
      struck = unit.target;
    }
    else
    {
      std::int64_t best = std::numeric_limits<std::int64_t>::max();
      for (const auto& [otherId, other] : m_units)
      {
        if (other.owner == unit.owner || Garrisoned(other) || !Adjacent(unit, other))
        {
          continue;
        }
        const std::int64_t dx = std::int64_t{other.cellVoxels.xVoxels} - unit.cellVoxels.xVoxels;
        const std::int64_t dz = std::int64_t{other.cellVoxels.zVoxels} - unit.cellVoxels.zVoxels;
        if ((dx * dx) + (dz * dz) < best)
        {
          best = (dx * dx) + (dz * dz);
          struck = otherId;
        }
      }
    }
    if (struck != Protocol::NO_ENTITY)
    {
      strikes.push_back({.striker = id, .struck = struck, .damage = Damage(stats, Data().Stats(m_units.at(struck).kind), 0)});
    }
  }
  for (const Strike& strike : strikes)
  {
    m_units.at(strike.struck).hitPoints -= strike.damage;
    Unit& striker = m_units.at(strike.striker);
    striker.reloadTicks = Data().Stats(striker.kind).reloadTicks;
  }
}

bool World::CanLoose(const Unit& _unit, const Unit& _foe) const
{
  // In its range across the ground plus a band for each elevation step, and with nothing solid between its loosing point
  // and the aim point above the target's cell (ADR-023, decision 5).
  const Rules& rules = Data().Tuning();
  const std::int64_t reach =
    ArrowRangeVoxels(Data().Stats(_unit.kind).rangeVoxels, ElevationSteps(_unit.cellVoxels.yVoxels, _foe.cellVoxels.yVoxels, rules), rules);
  const std::int64_t dx = std::int64_t{_foe.cellVoxels.xVoxels} - _unit.cellVoxels.xVoxels;
  const std::int64_t dz = std::int64_t{_foe.cellVoxels.zVoxels} - _unit.cellVoxels.zVoxels;
  if ((dx * dx) + (dz * dz) > reach * reach)
  {
    return false;
  }
  const ArrowLine line = AimArrow(_unit.cellVoxels, _foe.cellVoxels, rules);
  return !FirstSolid(line.start, line.end, m_voxels).has_value();
}

void World::Loose()
{
  // After melee, in unit-id order: each figure with a ranged attack whose reload is due looses an arrow at its target
  // when it can. The arrow's damage is fixed now, by the elevation between the two (ADR-023, decisions 2 and 5).
  const Rules& rules = Data().Tuning();
  for (auto& [id, unit] : m_units)
  {
    const UnitStats& stats = Data().Stats(unit.kind);
    if (Garrisoned(unit) || stats.attack <= 0 || stats.rangeVoxels <= 0 || unit.reloadTicks > 0)
    {
      continue;
    }
    const auto foe = m_units.find(unit.target);
    if (foe == m_units.end() || !CanLoose(unit, foe->second))
    {
      continue;
    }
    const std::int32_t steps = ElevationSteps(unit.cellVoxels.yVoxels, foe->second.cellVoxels.yVoxels, rules);
    const Protocol::EntityId arrow = NewEntityId();
    m_shots.emplace(arrow, Shot{.id = arrow,
                                .engine = unit.squad,
                                .owner = unit.owner,
                                .flight = AimArrow(unit.cellVoxels, foe->second.cellVoxels, rules),
                                .ticksFlown = 0,
                                .kind = Protocol::ShotKind::Arrow,
                                .shooter = id,
                                .target = unit.target,
                                .damage = Damage(stats, Data().Stats(foe->second.kind), steps)});
    unit.reloadTicks = stats.reloadTicks;
  }
}

Protocol::EntityId World::StructureHolding(Protocol::VoxelPosition _voxel) const
{
  for (const auto& [id, structure] : m_structures)
  {
    const ServerTemplate* found = Data().FindTemplate(structure.templateId);
    if (found == nullptr)
    {
      continue;
    }
    const bool across = structure.quarterTurns % 2 == 1;
    const auto extentX = static_cast<std::int32_t>(across ? found->depthVoxels : found->widthVoxels);
    const auto extentZ = static_cast<std::int32_t>(across ? found->widthVoxels : found->depthVoxels);
    const Protocol::VoxelPosition& origin = structure.originVoxels;
    if (_voxel.xVoxels >= origin.xVoxels && _voxel.xVoxels < origin.xVoxels + extentX && _voxel.zVoxels >= origin.zVoxels &&
        _voxel.zVoxels < origin.zVoxels + extentZ && _voxel.yVoxels >= origin.yVoxels &&
        _voxel.yVoxels < origin.yVoxels + static_cast<std::int32_t>(found->heightVoxels))
    {
      return id;
    }
  }
  return Protocol::NO_ENTITY;
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
  std::erase_if(m_searches, [_squad](const auto& _search) { return _search.first.second == _squad; });
  m_squads.erase(found);
}

// ── The tick's steps ────────────────────────────────────────────────────────────────────────────

void World::ApplyCommands()
{
  // Step 1: the commands that arrived for this tick, in the order they arrived.
  m_lastTickCommands = std::exchange(m_pendingCommands, {});
  // Then the commanders', given at the end of the last tick, applied as a player's are but kept apart (ADR-027, decision 2).
  m_lastCommanderCommands = std::exchange(m_commanderCommands, {});
  for (const std::vector<SubmittedCommand>* commands : {&m_lastTickCommands, &m_lastCommanderCommands})
  {
    for (const SubmittedCommand& submitted : *commands)
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
}

void World::Apply(std::uint8_t _player, const Protocol::Command& _command)
{
  // A refused command does nothing, and its player's next update names it (ADR-023, decision 10).
  if (const auto* move = std::get_if<Protocol::MoveSquad>(&_command))
  {
    Squad* squad = OrderableSquad(_player, _command, move->squad);
    if (squad == nullptr)
    {
      return;
    }
    // An engine moves as its body's squad, while packed (ADR-023).
    const SiegeEngine* engine = EngineOfBody(move->squad);
    if (engine != nullptr && engine->state != EngineState::Packed)
    {
      Refuse(_player, _command, Protocol::Refusal::NotPacked);
      return;
    }
    squad->order = Protocol::SquadOrder::Move;
    squad->target = Protocol::NO_ENTITY;
    squad->hasGoal = true;
    squad->goalVoxels = move->goalVoxels;
    RequestSearch(*squad);
  }
  else if (const auto* fire = std::get_if<Protocol::FireEngine>(&_command))
  {
    Fire(_player, *fire);
  }
  else if (const auto* attack = std::get_if<Protocol::AttackTarget>(&_command))
  {
    // A garrison of _player's is aimed at another player's squad or engine, and looses at its figures first when they are
    // within its reach, without leaving its building (ADR-024, decision 5, as M4-F2 amends it).
    if (const auto garrison = m_squads.find(attack->squad);
        garrison != m_squads.end() && garrison->second.garrisonedIn != Protocol::NO_ENTITY)
    {
      const Squad* target = TargetSquad(attack->target);
      if (garrison->second.owner != _player)
      {
        Refuse(_player, _command, Protocol::Refusal::NotOwned);
      }
      else if (target == nullptr || target->owner == _player || Data().Stats(garrison->second.kind).rangeVoxels <= 0)
      {
        Refuse(_player, _command, Protocol::Refusal::Invalid);
      }
      else
      {
        garrison->second.target = target->id;
      }
      return;
    }
    // A squad or an engine of another player, by a squad with an attack on units; or, by a ram, a structure of another
    // player's that it can touch (ADR-023, decision 9).
    Squad* squad = OrderableSquad(_player, _command, attack->squad);
    if (squad == nullptr)
    {
      return;
    }
    if (const SiegeEngine* ram = EngineOfBody(attack->squad); ram != nullptr && ram->kind == EngineKind::Ram)
    {
      const auto structure = m_structures.find(attack->target);
      const std::optional<Box> box = structure == m_structures.end() ? std::nullopt : BoxOf(structure->second);
      const std::optional<RamContact> contact = box.has_value() && structure->second.owner != _player
                                                  ? RamContactFor(m_units.at(squad->units.front()), *box, CriticalVoxels(structure->second))
                                                  : std::nullopt;
      if (!contact.has_value())
      {
        Refuse(_player, _command, Protocol::Refusal::Invalid);
        return;
      }
      squad->order = Protocol::SquadOrder::Attack;
      squad->target = attack->target;
      squad->hasGoal = true;
      squad->goalVoxels = contact->cellVoxels;
      RequestSearch(*squad);
      return;
    }
    const Squad* target = TargetSquad(attack->target);
    if (target == nullptr || target->owner == _player || target->garrisonedIn != Protocol::NO_ENTITY || target->units.empty() ||
        Data().Stats(squad->kind).attack <= 0)
    {
      Refuse(_player, _command, Protocol::Refusal::Invalid);
      return;
    }
    squad->order = Protocol::SquadOrder::Attack;
    squad->target = attack->target;
    squad->hasGoal = true;
    squad->goalVoxels = m_units.at(target->units.front()).cellVoxels;
    RequestSearch(*squad);
  }
  else if (const auto* storm = std::get_if<Protocol::StormTo>(&_command))
  {
    Squad* squad = OrderableSquad(_player, _command, storm->squad);
    if (squad == nullptr)
    {
      return;
    }
    if (Data().Stats(squad->kind).attack <= 0)
    {
      Refuse(_player, _command, Protocol::Refusal::Invalid);
      return;
    }
    squad->order = Protocol::SquadOrder::Storm;
    squad->target = Protocol::NO_ENTITY;
    squad->hasGoal = true;
    squad->goalVoxels = storm->goalVoxels;
    RequestSearch(*squad);
  }
  else if (const auto* stop = std::get_if<Protocol::StopSquad>(&_command))
  {
    if (Squad* squad = OrderableSquad(_player, _command, stop->squad); squad != nullptr)
    {
      Halt(*squad, Protocol::SquadOrder::Idle);
    }
  }
  else if (const auto* hold = std::get_if<Protocol::HoldSquad>(&_command))
  {
    if (Squad* squad = OrderableSquad(_player, _command, hold->squad); squad != nullptr)
    {
      Halt(*squad, Protocol::SquadOrder::Hold);
    }
  }
  else if (const auto* split = std::get_if<Protocol::SplitSquad>(&_command))
  {
    Split(_player, *split);
  }
  else if (const auto* merge = std::get_if<Protocol::MergeSquads>(&_command))
  {
    Merge(_player, *merge);
  }
  else if (const auto* gate = std::get_if<Protocol::SetGate>(&_command))
  {
    SetGate(_player, *gate);
  }
  else if (const auto* garrison = std::get_if<Protocol::GarrisonSquad>(&_command))
  {
    Garrison(_player, *garrison);
  }
  else if (const auto* eject = std::get_if<Protocol::EjectGarrison>(&_command))
  {
    Eject(_player, *eject);
  }
  else if (const auto* deploy = std::get_if<Protocol::DeployEngine>(&_command))
  {
    Deploy(_player, *deploy);
  }
  else if (const auto* pack = std::get_if<Protocol::PackEngine>(&_command))
  {
    Pack(_player, *pack);
  }
  else if (const auto* repair = std::get_if<Protocol::RepairStructure>(&_command))
  {
    OrderRepair(_player, *repair);
  }
  else if (const auto* trench = std::get_if<Protocol::DigTrench>(&_command))
  {
    DigTrench(_player, *trench);
  }
  else
  {
    // M4's other commands land with the orders that build them.
    Refuse(_player, _command, Protocol::Refusal::Invalid);
  }
}

Squad* World::OrderableSquad(std::uint8_t _player, const Protocol::Command& _command, Protocol::EntityId _squad)
{
  const auto squad = m_squads.find(_squad);
  if (squad == m_squads.end() || squad->second.garrisonedIn != Protocol::NO_ENTITY)
  {
    Refuse(_player, _command, Protocol::Refusal::Invalid);
    return nullptr;
  }
  if (squad->second.owner != _player)
  {
    Refuse(_player, _command, Protocol::Refusal::NotOwned);
    return nullptr;
  }
  return &squad->second;
}

const Squad* World::TargetSquad(Protocol::EntityId _target) const
{
  if (const auto squad = m_squads.find(_target); squad != m_squads.end())
  {
    return &squad->second;
  }
  if (const auto engine = m_siegeEngines.find(_target); engine != m_siegeEngines.end())
  {
    const auto body = m_squads.find(engine->second.squad);
    return body == m_squads.end() ? nullptr : &body->second;
  }
  return nullptr;
}

void World::Halt(Squad& _squad, Protocol::SquadOrder _order)
{
  // Its figures finish the steps they are in and stand.
  _squad.order = _order;
  _squad.target = Protocol::NO_ENTITY;
  _squad.hasGoal = false;
  _squad.shortOfGoal = false;
  _squad.path.clear();
  _squad.trail.clear();
  std::erase_if(m_searches, [&_squad](const auto& _search) { return _search.first.second == _squad.id; });
  for (const Protocol::EntityId id : _squad.units)
  {
    m_units.at(id).trailStep = OFF_TRAIL;
  }
}

void World::Split(std::uint8_t _player, const Protocol::SplitSquad& _split)
{
  // The highest-id figure leaves as a squad of one, standing where it is, idle (MvpPlan-M4.md, Contracts).
  Squad* squad = OrderableSquad(_player, _split, _split.squad);
  if (squad == nullptr)
  {
    return;
  }
  if (squad->units.size() < 2)
  {
    Refuse(_player, _split, Protocol::Refusal::Invalid);
    return;
  }
  const Protocol::EntityId leaving = squad->units.back();
  squad->units.pop_back();
  const Protocol::EntityId id = NewEntityId();
  Unit& unit = m_units.at(leaving);
  unit.squad = id;
  unit.trailStep = OFF_TRAIL;
  unit.waitTicks = 0;
  m_squads.emplace(id, Squad{.id = id,
                             .owner = squad->owner,
                             .navigationClass = squad->navigationClass,
                             .units = {leaving},
                             .hasGoal = false,
                             .goalVoxels = {},
                             .path = {},
                             .shortOfGoal = false,
                             .trail = {},
                             .garrisonedIn = Protocol::NO_ENTITY,
                             .kind = squad->kind,
                             .facing = squad->facing,
                             .order = Protocol::SquadOrder::Idle,
                             .target = Protocol::NO_ENTITY,
                             .trenchMinVoxels = {},
                             .trenchMaxVoxels = {},
                             .trenchNearSide = 0,
                             .digProgress = {}});
}

void World::Merge(std::uint8_t _player, const Protocol::MergeSquads& _merge)
{
  // The first squad takes figures from the others in turn, the lowest ids first, up to its kind's full size, and a squad
  // left with none goes (MvpPlan-M4.md, Contracts). Every squad is the player's, of one kind, and neither a body nor in a
  // building; the first refuses when it is full already.
  std::vector<Squad*> squads;
  for (const Protocol::EntityId id : _merge.squads)
  {
    Squad* squad = OrderableSquad(_player, _merge, id);
    if (squad == nullptr)
    {
      return;
    }
    if (std::ranges::find(squads, squad) != squads.end() || EngineOfBody(id) != nullptr ||
        (!squads.empty() && squad->kind != squads.front()->kind))
    {
      Refuse(_player, _merge, Protocol::Refusal::Invalid);
      return;
    }
    squads.push_back(squad);
  }
  if (squads.size() < 2)
  {
    Refuse(_player, _merge, Protocol::Refusal::Invalid);
    return;
  }
  Squad& into = *squads.front();
  const std::size_t full = Data().Stats(into.kind).squadFigures;
  if (into.units.size() >= full)
  {
    Refuse(_player, _merge, Protocol::Refusal::Full);
    return;
  }
  std::vector<Protocol::EntityId> emptied;
  for (std::size_t from = 1; from < squads.size() && into.units.size() < full; ++from)
  {
    Squad& giving = *squads[from];
    while (!giving.units.empty() && into.units.size() < full)
    {
      into.units.push_back(giving.units.front());
      m_units.at(giving.units.front()).squad = into.id;
      giving.units.erase(giving.units.begin());
    }
    if (giving.units.empty())
    {
      emptied.push_back(giving.id);
    }
  }
  // Its figures in id order, the lowest its lead, form about the lead again, and walk on when it has a goal.
  std::ranges::sort(into.units);
  for (const Protocol::EntityId id : into.units)
  {
    m_units.at(id).trailStep = OFF_TRAIL;
    m_units.at(id).waitTicks = 0;
  }
  into.trail.clear();
  into.path.clear();
  if (into.hasGoal)
  {
    RequestSearch(into);
  }
  for (const Protocol::EntityId id : emptied)
  {
    RemoveSquad(id);
  }
}

void World::Fire(std::uint8_t _player, const Protocol::FireEngine& _fire)
{
  // Refused, and doing nothing, for an engine that is not the player's, is not deployed or is
  // reloading, for a target out of range (GameDesign.md, M2's values, item 12), and for a player
  // without the shot's stone (ADR-023, decision 10).
  const auto engine = m_siegeEngines.find(_fire.engine);
  if (engine == m_siegeEngines.end() || engine->second.kind != EngineKind::Trebuchet)
  {
    Refuse(_player, _fire, Protocol::Refusal::Invalid);
    return;
  }
  if (engine->second.owner != _player)
  {
    Refuse(_player, _fire, Protocol::Refusal::NotOwned);
    return;
  }
  if (engine->second.state != EngineState::Deployed)
  {
    Refuse(_player, _fire, Protocol::Refusal::NotDeployed);
    return;
  }
  if (engine->second.reloadTicks > 0)
  {
    Refuse(_player, _fire, Protocol::Refusal::Reloading);
    return;
  }
  const std::optional<ShotArc> arc = AimShot(engine->second.cellVoxels, _fire.targetVoxels, Data().Tuning());
  if (!arc.has_value())
  {
    Refuse(_player, _fire, Protocol::Refusal::Invalid);
    return;
  }
  // The stone comes from its player's stockpile; a player the match has no record of, as in M2's and
  // M3's worlds, pays nothing.
  if (const auto player = m_players.find(_player); player != m_players.end())
  {
    const std::int32_t cost = Data().Tuning().shotStoneHundredthsCubicMeters;
    if (player->second.stoneHundredthsCubicMeters < cost)
    {
      Refuse(_player, _fire, Protocol::Refusal::NotEnoughStone);
      return;
    }
    player->second.stoneHundredthsCubicMeters -= cost;
    NoteSpent(false, _player, engine->first, cost, engine->second.cellVoxels);
  }
  const Protocol::EntityId id = NewEntityId();
  m_shots.emplace(id, Shot{.id = id,
                           .engine = engine->first,
                           .owner = _player,
                           .flight = *arc,
                           .ticksFlown = 0,
                           .kind = Protocol::ShotKind::Stone,
                           .shooter = Protocol::NO_ENTITY,
                           .target = Protocol::NO_ENTITY,
                           .damage = 0});
  engine->second.reloadTicks = static_cast<std::uint32_t>(Data().Tuning().shotReloadTicks);
}

void World::Deploy(std::uint8_t _player, const Protocol::DeployEngine& _deploy)
{
  const auto engine = m_siegeEngines.find(_deploy.engine);
  if (engine == m_siegeEngines.end() || engine->second.kind != EngineKind::Trebuchet || _deploy.quarterTurns > 3)
  {
    Refuse(_player, _deploy, Protocol::Refusal::Invalid);
    return;
  }
  if (engine->second.owner != _player)
  {
    Refuse(_player, _deploy, Protocol::Refusal::NotOwned);
    return;
  }
  if (engine->second.state != EngineState::Packed)
  {
    Refuse(_player, _deploy, Protocol::Refusal::NotPacked);
    return;
  }
  // It stops where it stands, finishing the step it is in, and turns to throw the way it is told.
  if (const auto body = m_squads.find(engine->second.squad); body != m_squads.end())
  {
    Halt(body->second, Protocol::SquadOrder::Idle);
  }
  engine->second.quarterTurns = _deploy.quarterTurns;
  const auto ticks = static_cast<std::uint32_t>(std::max(0, Data().Tuning().trebuchetDeployTicks));
  engine->second.state = ticks == 0 ? EngineState::Deployed : EngineState::Deploying;
  engine->second.stateTicksLeft = ticks;
}

void World::Pack(std::uint8_t _player, const Protocol::PackEngine& _pack)
{
  const auto engine = m_siegeEngines.find(_pack.engine);
  if (engine == m_siegeEngines.end() || engine->second.kind != EngineKind::Trebuchet)
  {
    Refuse(_player, _pack, Protocol::Refusal::Invalid);
    return;
  }
  if (engine->second.owner != _player)
  {
    Refuse(_player, _pack, Protocol::Refusal::NotOwned);
    return;
  }
  if (engine->second.state != EngineState::Deployed)
  {
    Refuse(_player, _pack, Protocol::Refusal::NotDeployed);
    return;
  }
  const auto ticks = static_cast<std::uint32_t>(std::max(0, Data().Tuning().trebuchetPackTicks));
  engine->second.state = ticks == 0 ? EngineState::Packed : EngineState::Packing;
  engine->second.stateTicksLeft = ticks;
}

void World::CrushAround(Protocol::VoxelPosition _struck)
{
  // A unit within a cell of the struck voxel across the ground, standing within a voxel of its top; a machine's body
  // also when its footprint holds the struck column (MvpPlan.md, M4, input 8).
  const std::int32_t damage = Data().Tuning().shotCrushDamage;
  const std::int32_t machineSide = std::max(1, Data().Tuning().machinesWidthVoxels);
  for (auto& [id, unit] : m_units)
  {
    if (Garrisoned(unit))
    {
      continue;
    }
    const Protocol::VoxelPosition& cell = unit.cellVoxels;
    const bool beside = std::abs(cell.xVoxels - _struck.xVoxels) <= 1 && std::abs(cell.zVoxels - _struck.zVoxels) <= 1 &&
                        std::abs(cell.yVoxels - (_struck.yVoxels + 1)) <= 1;
    const std::int32_t side = unit.navigationClass == NavigationClass::Machines ? machineSide : 1;
    const bool holds = _struck.xVoxels >= cell.xVoxels && _struck.xVoxels < cell.xVoxels + side && _struck.zVoxels >= cell.zVoxels &&
                       _struck.zVoxels < cell.zVoxels + side;
    if (beside || holds)
    {
      Crush(unit, damage);
    }
  }
}

std::optional<World::Box> World::BoxOf(const Structure& _structure) const
{
  const ServerTemplate* found = Data().FindTemplate(_structure.templateId);
  if (found == nullptr)
  {
    return std::nullopt;
  }
  const bool across = _structure.quarterTurns % 2 == 1;
  return Box{.min = _structure.originVoxels,
             .widthVoxels = static_cast<std::int32_t>(across ? found->depthVoxels : found->widthVoxels),
             .heightVoxels = static_cast<std::int32_t>(found->heightVoxels),
             .depthVoxels = static_cast<std::int32_t>(across ? found->widthVoxels : found->depthVoxels)};
}

std::array<std::pair<std::int32_t, std::int32_t>, 3> World::FrontColumns(Protocol::VoxelPosition _cellVoxels, std::int32_t _facingX,
                                                                         std::int32_t _facingZ) const
{
  const std::int32_t half = std::max(1, Data().Tuning().machinesWidthVoxels) / 2;
  const std::int32_t x = _cellVoxels.xVoxels + half + (_facingX * (half + 1));
  const std::int32_t z = _cellVoxels.zVoxels + half + (_facingZ * (half + 1));
  const std::int32_t acrossX = std::abs(_facingZ);
  const std::int32_t acrossZ = std::abs(_facingX);
  return {{{x, z}, {x - acrossX, z - acrossZ}, {x + acrossX, z + acrossZ}}};
}

std::optional<Protocol::VoxelPosition> World::LowestIn(const Box& _box, std::int32_t _x, std::int32_t _z, std::int32_t _floorVoxels) const
{
  if (_x < _box.min.xVoxels || _x >= _box.min.xVoxels + _box.widthVoxels || _z < _box.min.zVoxels ||
      _z >= _box.min.zVoxels + _box.depthVoxels)
  {
    return std::nullopt;
  }
  for (std::int32_t y = std::max(_floorVoxels, _box.min.yVoxels); y < _box.min.yVoxels + _box.heightVoxels; ++y)
  {
    const Protocol::VoxelPosition voxel{.xVoxels = _x, .yVoxels = y, .zVoxels = _z};
    if (m_voxels.At(voxel) != Protocol::Chunk::EMPTY)
    {
      return voxel;
    }
  }
  return std::nullopt;
}

std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> World::CriticalVoxels(const Structure& _structure) const
{
  std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> critical;
  const ServerTemplate* found = Data().FindTemplate(_structure.templateId);
  if (found == nullptr)
  {
    return critical;
  }
  std::size_t voxel = 0;
  for (std::uint32_t y = 0; y < found->heightVoxels; ++y)
  {
    for (std::uint32_t z = 0; z < found->depthVoxels; ++z)
    {
      for (std::uint32_t x = 0; x < found->widthVoxels; ++x, ++voxel)
      {
        if (found->parts.at(voxel) == ServerTemplate::Part::Critical && found->indices.at(voxel) != Protocol::Chunk::EMPTY)
        {
          const Protocol::VoxelPosition at = TemplateVoxel(_structure, *found, x, y, z);
          critical.emplace(at.xVoxels, at.yVoxels, at.zVoxels);
        }
      }
    }
  }
  return critical;
}

std::optional<World::RamBlow> World::BestBlow(Protocol::VoxelPosition _cellVoxels, const Box& _box,
                                              const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>>& _critical) const
{
  constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> FACINGS_TRIED = {{{0, -1}, {0, 1}, {-1, 0}, {1, 0}}};
  std::optional<RamBlow> best;
  for (const auto& [facingX, facingZ] : FACINGS_TRIED)
  {
    // It touches when a column before its front holds a voxel within its own height above its floor, the machines'
    // headroom; it strikes that column's or a neighbour's lowest voxel, whichever lies lowest, the one before its centre
    // first, so a gate goes from the bottom row up.
    const std::int32_t reach = std::max(1, m_navigation.Class(NavigationClass::Machines).headroomVoxels);
    bool touches = false;
    std::optional<Protocol::VoxelPosition> struck;
    for (const auto& [x, z] : FrontColumns(_cellVoxels, facingX, facingZ))
    {
      const std::optional<Protocol::VoxelPosition> lowest = LowestIn(_box, x, z, _cellVoxels.yVoxels);
      touches = touches || (lowest.has_value() && lowest->yVoxels < _cellVoxels.yVoxels + reach);
      if (lowest.has_value() && (!struck.has_value() || lowest->yVoxels < struck->yVoxels))
      {
        struck = lowest;
      }
    }
    if (!touches || !struck.has_value())
    {
      continue;
    }
    // What it takes of the critical part first, so that a ram at a gatehouse goes for its gate whatever the ladder's
    // sizes (GameDesign.md, M4's phase D, item 3), then what it takes in all.
    const std::vector<Protocol::VoxelPosition> crater = RamCrater(*struck, _box);
    const auto critical =
      static_cast<std::size_t>(std::ranges::count_if(crater, [&_critical](const Protocol::VoxelPosition& _voxel)
                                                     { return _critical.contains({_voxel.xVoxels, _voxel.yVoxels, _voxel.zVoxels}); }));
    if (!best.has_value() || std::pair{critical, crater.size()} > std::pair{best->critical, best->takes})
    {
      best = RamBlow{.facingX = facingX, .facingZ = facingZ, .voxel = *struck, .takes = crater.size(), .critical = critical};
    }
  }
  return best;
}

std::optional<World::RamContact> World::RamContactFor(const Unit& _ram, const Box& _box,
                                                      const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>>& _critical) const
{
  // Every cell within a ram's width of the box, or in it, as a passage is: the one whose blow takes most of the critical
  // part, then most, so that a ram ordered at a gatehouse goes for its gate, then the nearest, ties to the lower index.
  const std::int32_t side = std::max(1, Data().Tuning().machinesWidthVoxels);
  const std::int32_t width = m_navigation.WidthCells();
  const PathSearch::CellIndex from = CellOf(_ram.cellVoxels);
  std::optional<RamContact> best;
  std::pair<std::size_t, std::size_t> bestRank{0, 0};
  std::int64_t bestDistance = std::numeric_limits<std::int64_t>::max();
  for (std::int32_t z = _box.min.zVoxels - side - 1; z <= _box.min.zVoxels + _box.depthVoxels; ++z)
  {
    for (std::int32_t x = _box.min.xVoxels - side - 1; x <= _box.min.xVoxels + _box.widthVoxels; ++x)
    {
      if (x < 0 || z < 0 || x >= width || z >= m_navigation.DepthCells() || !m_navigation.Usable(x, z, NavigationClass::Machines))
      {
        continue;
      }
      const Protocol::VoxelPosition cell{.xVoxels = x, .yVoxels = m_navigation.WalkableHeight(x, z), .zVoxels = z};
      const std::optional<RamBlow> blow = BestBlow(cell, _box, _critical);
      if (!blow.has_value() || blow->takes == 0 || std::pair{blow->critical, blow->takes} < bestRank)
      {
        continue;
      }
      const PathSearch::CellIndex index = CellOf(cell);
      const std::int64_t distance = PathSearch::Octile(from, index, width);
      const std::pair<std::size_t, std::size_t> rank{blow->critical, blow->takes};
      if ((rank > bestRank || distance < bestDistance) && m_navigation.NearestReachable(from, index, NavigationClass::Machines) == index)
      {
        best = RamContact{.cellVoxels = cell, .facingX = blow->facingX, .facingZ = blow->facingZ};
        bestRank = rank;
        bestDistance = distance;
      }
    }
  }
  return best;
}

std::vector<Protocol::VoxelPosition> World::RamCrater(Protocol::VoxelPosition _voxel, const Box& _box) const
{
  // Input 9's ladder (MvpPlan.md, M4): timber takes ramTimberCraterVoxels, earth and rubble ramEarthCraterVoxels, stone
  // ramStoneCraterVoxels when the struck voxel lies within ramStoneReachVoxels of the structure's base and none above,
  // and bedrock and water none. A crater is the struck voxel and those above it in its column, inside the box and of its
  // behaviour, up to its size: a gate one voxel thick loses a column of itself, never the ground under it.
  const Rules& rules = Data().Tuning();
  const Behavior behavior = Data().Materials().at(m_voxels.At(_voxel)).behavior;
  std::int32_t size = 0;
  switch (behavior)
  {
  case Behavior::Timber:
  case Behavior::Leaves:
    size = rules.ramTimberCraterVoxels;
    break;
  case Behavior::Earth:
  case Behavior::StoneRubble:
  case Behavior::WasteRubble:
    size = rules.ramEarthCraterVoxels;
    break;
  case Behavior::BuildingStone:
  case Behavior::CommonRock:
    size = _voxel.yVoxels < _box.min.yVoxels + rules.ramStoneReachVoxels ? rules.ramStoneCraterVoxels : 0;
    break;
  default:
    break;
  }
  std::vector<Protocol::VoxelPosition> crater;
  for (Protocol::VoxelPosition voxel = _voxel;
       std::cmp_less(crater.size(), std::max(0, size)) && voxel.yVoxels < _box.min.yVoxels + _box.heightVoxels; ++voxel.yVoxels)
  {
    if (m_voxels.At(voxel) == Protocol::Chunk::EMPTY || Data().Materials().at(m_voxels.At(voxel)).behavior != behavior)
    {
      break;
    }
    crater.push_back(voxel);
  }
  return crater;
}

void World::RamBlows()
{
  // After the engines' reloads run down: a ram standing where it touches the structure it is ordered at strikes, every
  // ramBlowTicks, the blow that takes most, each blow an edit as a shot's crater is. One that has arrived where its blow
  // takes nothing makes for the place whose blow takes most, or turns idle when there is none (ADR-023, decision 9).
  for (auto& [id, engine] : m_siegeEngines)
  {
    const auto squad = m_squads.find(engine.squad);
    if (engine.kind != EngineKind::Ram || squad == m_squads.end() || squad->second.units.empty() ||
        squad->second.order != Protocol::SquadOrder::Attack || Holding(squad->first))
    {
      continue;
    }
    const auto structure = m_structures.find(squad->second.target);
    const std::optional<Box> box = structure == m_structures.end() ? std::nullopt : BoxOf(structure->second);
    const Unit& ram = m_units.at(squad->second.units.front());
    const std::vector<PathSearch::CellIndex>& trail = squad->second.trail;
    const bool walking = !trail.empty() && ram.trailStep != OFF_TRAIL && std::size_t{ram.trailStep} + 1 < trail.size();
    if (!box.has_value() || ram.reservedCell >= 0 || walking)
    {
      continue;
    }
    const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> critical = CriticalVoxels(structure->second);
    const std::optional<RamBlow> blow = BestBlow(ram.cellVoxels, *box, critical);
    if (!blow.has_value() || blow->takes == 0)
    {
      const std::optional<RamContact> next = RamContactFor(ram, *box, critical);
      if (!next.has_value() || next->cellVoxels == ram.cellVoxels)
      {
        Halt(squad->second, Protocol::SquadOrder::Idle);
        continue;
      }
      squad->second.goalVoxels = next->cellVoxels;
      squad->second.hasGoal = true;
      RequestSearch(squad->second);
      continue;
    }
    if (engine.reloadTicks > 0)
    {
      continue;
    }
    Rule().RamStrikes(*this, structure->first, blow->voxel);
    engine.reloadTicks = static_cast<std::uint32_t>(std::max(0, Data().Tuning().ramBlowTicks));
  }
}

void World::AdvanceJobs()
{
  // Step 2: designations and jobs: the garrisons' healing, repair and trenches (ADR-024, decisions 3 to 5; ADR-029).
  HealGarrisons();
  AdvanceRepairs();
  AdvanceTrenches();
}

void World::MoveUnits()
{
  // Step 3: each figure's target (ADR-023, decision 4); the searches, served in the order they were
  // asked for, then squad id, within pathNodesPerTick nodes, one the budget cuts short going on next
  // tick (M2-F3; ADR-015, decision 5); movement (ADR-022); then melee (ADR-023, decision 3).
  Acquire();
  auto budget = static_cast<std::uint32_t>(std::max(0, Data().Tuning().pathNodesPerTick));
  while (budget > 0 && !m_searches.empty())
  {
    PathSearch& search = m_searches.begin()->second;
    budget -= search.Advance(m_navigation, budget);
    if (!search.Done())
    {
      break;
    }
    const auto squad = m_squads.find(search.Squad());
    if (squad != m_squads.end())
    {
      squad->second.path = search.Path();
      squad->second.shortOfGoal = search.ShortOfGoal();
      FormTrail(squad->second);
    }
    m_searches.erase(m_searches.begin());
  }
  WalkFigures();
  FollowBodies();
  Melee();
  Loose();
  RemoveDead();
}

void World::FormTrail(Squad& _squad)
{
  const std::vector<PathSearch::CellIndex>& path = _squad.path;
  if (path.empty() || _squad.units.empty())
  {
    return;
  }
  // The lead walks the path from where it stands, or the cell it is stepping into, which its search
  // started from. When that is the trail cell it is at, the trail behind it stays, so that the
  // figures following it stay on it, and the path takes the place of what lay ahead.
  Unit& lead = m_units.at(_squad.units.front());
  if (lead.trailStep != OFF_TRAIL && lead.trailStep < _squad.trail.size() && _squad.trail[lead.trailStep] == path.front())
  {
    _squad.trail.resize(std::size_t{lead.trailStep} + 1);
    _squad.trail.insert(_squad.trail.end(), path.begin() + 1, path.end());
    return;
  }
  _squad.trail = path;
  lead.trailStep = 0;
  for (std::size_t place = 1; place < _squad.units.size(); ++place)
  {
    m_units.at(_squad.units[place]).trailStep = OFF_TRAIL;
  }
}

PathSearch::CellIndex World::CellOf(const Protocol::VoxelPosition& _cellVoxels) const noexcept
{
  return (std::int64_t{_cellVoxels.zVoxels} * m_navigation.WidthCells()) + _cellVoxels.xVoxels;
}

std::int64_t World::SpeedOf(const Unit& _unit) const
{
  // Each kind's own speed, from UNIT (ADR-023, decision 1).
  return Data().Stats(_unit.kind).speedMetersPerTick.Raw();
}

std::vector<PathSearch::CellIndex> World::Footprint(const Unit& _unit, PathSearch::CellIndex _cell) const
{
  // A machine occupies the square of cells its class's width covers, its cell the minimum corner (ADR-022, decision 1).
  const std::int32_t width = m_navigation.WidthCells();
  const std::int32_t side = _unit.navigationClass == NavigationClass::Machines ? std::max(1, Data().Tuning().machinesWidthVoxels) : 1;
  std::vector<PathSearch::CellIndex> cells;
  cells.reserve(static_cast<std::size_t>(side) * static_cast<std::size_t>(side));
  for (std::int32_t dz = 0; dz < side; ++dz)
  {
    for (std::int32_t dx = 0; dx < side; ++dx)
    {
      cells.push_back(_cell + (std::int64_t{dz} * width) + dx);
    }
  }
  return cells;
}

World::Occupancy World::BuildOccupancy() const
{
  // Every unit outside a building holds the cells it stands in, which for one in a step is the cell it
  // left until it is halfway, and has reserved the cells it steps into.
  Occupancy occupancy;
  for (const auto& [id, unit] : m_units)
  {
    if (Garrisoned(unit))
    {
      continue;
    }
    for (const PathSearch::CellIndex cell : Footprint(unit, CellOf(unit.cellVoxels)))
    {
      occupancy.occupied.emplace(cell, id);
    }
    if (unit.reservedCell >= 0)
    {
      for (const PathSearch::CellIndex cell : Footprint(unit, unit.reservedCell))
      {
        occupancy.reserved.emplace(cell, id);
      }
    }
  }
  return occupancy;
}

bool World::Free(const Occupancy& _occupancy, const Unit& _unit, PathSearch::CellIndex _cell) const
{
  for (const PathSearch::CellIndex cell : Footprint(_unit, _cell))
  {
    const auto occupied = _occupancy.occupied.find(cell);
    const auto reserved = _occupancy.reserved.find(cell);
    if ((occupied != _occupancy.occupied.end() && occupied->second != _unit.id) ||
        (reserved != _occupancy.reserved.end() && reserved->second != _unit.id))
    {
      return false;
    }
  }
  return true;
}

std::set<PathSearch::CellIndex> World::Flood(PathSearch::CellIndex _start, NavigationClass _class, std::int32_t _radiusCells) const
{
  // The cells a class can reach from _start along its links without going more than _radiusCells from it, across or
  // along: where its figures can take their slots.
  const std::int32_t width = m_navigation.WidthCells();
  const auto startX = static_cast<std::int32_t>(_start % width);
  const auto startZ = static_cast<std::int32_t>(_start / width);
  std::set<PathSearch::CellIndex> reached{_start};
  std::deque<PathSearch::CellIndex> open{_start};
  while (!open.empty())
  {
    const PathSearch::CellIndex cell = open.front();
    open.pop_front();
    const auto x = static_cast<std::int32_t>(cell % width);
    const auto z = static_cast<std::int32_t>(cell / width);
    const std::uint8_t links = m_navigation.Links(x, z, _class);
    for (std::size_t direction = 0; direction < NavigationGrid::NEIGHBORS.size(); ++direction)
    {
      const std::int32_t nextX = x + NavigationGrid::NEIGHBORS.at(direction)[0];
      const std::int32_t nextZ = z + NavigationGrid::NEIGHBORS.at(direction)[1];
      if ((links & (1U << direction)) == 0 || std::abs(nextX - startX) > _radiusCells || std::abs(nextZ - startZ) > _radiusCells)
      {
        continue;
      }
      const PathSearch::CellIndex next = (std::int64_t{nextZ} * width) + nextX;
      if (reached.insert(next).second)
      {
        open.push_back(next);
      }
    }
  }
  return reached;
}

World::SquadPlan World::PlanFor(const Squad& _squad) const
{
  // ADR-022, decisions 4 and 5: each follower's target, its slot about the lead or, where the slot is dropped, the
  // trail. On the way a slot its class cannot use, that it cannot reach from the lead within a few cells, or more than
  // SLOT_TRAIL_REACH_CELLS from the lead's trail is dropped. At the goal every slot takes a cell: when its own will not
  // do, the trail cell its figure would stop at in single file, and when a slot before it holds that, the nearest usable
  // and reachable one no slot before it holds.
  SquadPlan plan{.moving = false, .arrived = false, .targets = std::vector<PathSearch::CellIndex>(_squad.units.size(), NO_CELL)};
  if (_squad.units.empty() || _squad.trail.empty())
  {
    return plan;
  }
  plan.moving = true;
  const std::int32_t width = m_navigation.WidthCells();
  const Unit& lead = m_units.at(_squad.units.front());
  const PathSearch::CellIndex leadCell = CellOf(lead.cellVoxels);
  plan.arrived =
    !Holding(_squad.id) && lead.reservedCell < 0 && lead.trailStep != OFF_TRAIL && std::size_t{lead.trailStep} + 1 >= _squad.trail.size();
  if (_squad.units.size() < 2)
  {
    return plan;
  }
  const UnitStats& stats = Data().Stats(_squad.kind);
  const std::uint8_t ranks = _squad.navigationClass == NavigationClass::Cavalry || stats.squadFigures < 2 ? 1 : 2;
  const std::vector<Protocol::VoxelPosition> slots =
    FormationSlots(lead.cellVoxels, _squad.facing, static_cast<std::uint8_t>(_squad.units.size()), ranks, stats.squadFigures);
  const auto usable = [this, &_squad](const Protocol::VoxelPosition& _slot)
  {
    return _slot.xVoxels >= 0 && _slot.zVoxels >= 0 && _slot.xVoxels < m_navigation.WidthCells() &&
           _slot.zVoxels < m_navigation.DepthCells() && m_navigation.Usable(_slot.xVoxels, _slot.zVoxels, _squad.navigationClass);
  };
  if (plan.arrived)
  {
    const std::set<PathSearch::CellIndex> reach = Flood(leadCell, _squad.navigationClass, ARRIVAL_FLOOD_CELLS);
    std::set<PathSearch::CellIndex> held{leadCell};
    for (std::size_t place = 1; place < slots.size(); ++place)
    {
      const PathSearch::CellIndex slot = CellOf(slots[place]);
      PathSearch::CellIndex chosen = NO_CELL;
      const PathSearch::CellIndex file = lead.trailStep >= place ? _squad.trail[lead.trailStep - place] : NO_CELL;
      if (usable(slots[place]) && reach.contains(slot) && !held.contains(slot))
      {
        chosen = slot;
      }
      else if (file != NO_CELL && reach.contains(file) && !held.contains(file))
      {
        chosen = file;
      }
      else
      {
        std::int64_t best = std::numeric_limits<std::int64_t>::max();
        for (const PathSearch::CellIndex cell : reach)
        {
          const std::int64_t distance = PathSearch::Octile(cell, slot, width);
          if (!held.contains(cell) && distance < best)
          {
            best = distance;
            chosen = cell;
          }
        }
      }
      if (chosen != NO_CELL)
      {
        held.insert(chosen);
      }
      plan.targets[place] = chosen;
    }
    return plan;
  }
  const std::set<PathSearch::CellIndex> reach = Flood(leadCell, _squad.navigationClass, TRAVEL_FLOOD_CELLS);
  const std::size_t leadStep = lead.trailStep == OFF_TRAIL ? 0 : lead.trailStep;
  const std::size_t figures = _squad.units.size();
  const std::size_t windowFirst = leadStep > figures + 2 ? leadStep - figures - 2 : 0;
  const std::size_t windowEnd = std::min(_squad.trail.size(), leadStep + figures + 3);
  for (std::size_t place = 1; place < slots.size(); ++place)
  {
    const PathSearch::CellIndex slot = CellOf(slots[place]);
    bool nearTrail = false;
    for (std::size_t step = windowFirst; step < windowEnd && !nearTrail; ++step)
    {
      const PathSearch::CellIndex cell = _squad.trail[step];
      nearTrail = std::abs((cell % width) - (slot % width)) <= SLOT_TRAIL_REACH_CELLS &&
                  std::abs((cell / width) - (slot / width)) <= SLOT_TRAIL_REACH_CELLS;
    }
    if (usable(slots[place]) && reach.contains(slot) && nearTrail)
    {
      plan.targets[place] = slot;
    }
  }
  return plan;
}

std::optional<World::Step> World::NextStep(const Unit& _unit, const Squad& _squad, std::size_t _place, const SquadPlan& _plan) const
{
  // ADR-023, decision 4: a holding squad never leaves its cells, and a figure engaging an enemy closes on it by greedy
  // steps while they reach it within its vision, and stands once it touches it.
  if (_squad.order == Protocol::SquadOrder::Hold)
  {
    return std::nullopt;
  }
  if (const auto foe = m_units.find(_unit.target); foe != m_units.end())
  {
    // A melee figure engages what it touches, a ranged one what it can loose at (decisions 5 and 6).
    const bool ranged = Data().Stats(_unit.kind).rangeVoxels > 0;
    if (ranged ? CanLoose(_unit, foe->second) : Adjacent(_unit, foe->second))
    {
      return std::nullopt;
    }
    const PathSearch::CellIndex from = CellOf(_unit.cellVoxels);
    const PathSearch::CellIndex there = CellOf(foe->second.cellVoxels);
    if (GreedyReaches(from, _unit.navigationClass, there, Data().Stats(_unit.kind).visionVoxels))
    {
      if (const std::optional<PathSearch::CellIndex> next = Towards(from, _unit.navigationClass, there); next.has_value() && *next != there)
      {
        return Step{.cell = *next, .trailStep = OFF_TRAIL};
      }
    }
  }
  if (!_plan.moving)
  {
    return std::nullopt;
  }
  const std::vector<PathSearch::CellIndex>& trail = _squad.trail;
  const auto alongTrail = [&](std::uint32_t _step) -> std::optional<Step>
  {
    if (std::size_t{_step} + 1 >= trail.size() || !Linked(trail[_step], trail[std::size_t{_step} + 1], _unit.navigationClass))
    {
      return std::nullopt;
    }
    return Step{.cell = trail[std::size_t{_step} + 1], .trailStep = _step + 1};
  };
  if (_place == 0)
  {
    // The lead walks the trail to its end (M2-F5).
    return _unit.trailStep == OFF_TRAIL ? std::nullopt : alongTrail(_unit.trailStep);
  }
  const PathSearch::CellIndex here = CellOf(_unit.cellVoxels);
  const PathSearch::CellIndex target = _plan.targets.at(_place);
  if (target == here)
  {
    return std::nullopt;
  }
  // It makes for its slot when its greedy steps arrive there; a slot they do not reach, behind a wall from it, is
  // dropped as one far off the trail is (ADR-022, decision 4, amended).
  if (target != NO_CELL && GreedyReaches(here, _unit.navigationClass, target))
  {
    if (const std::optional<PathSearch::CellIndex> next = Towards(here, _unit.navigationClass, target); next.has_value())
    {
      return Step{.cell = *next, .trailStep = OFF_TRAIL};
    }
  }
  // Its slot dropped: it follows the trail as in M2, stopping as many cells behind the lead as it stands behind it in
  // the squad, and makes for the trail when it is off it.
  const std::uint32_t leadStep = m_units.at(_squad.units.front()).trailStep;
  const std::uint32_t stop = leadStep == OFF_TRAIL || leadStep < _place ? 0 : leadStep - static_cast<std::uint32_t>(_place);
  std::uint32_t onTrail = _unit.trailStep;
  for (std::size_t step = std::min<std::size_t>(stop + 1, trail.size()); onTrail == OFF_TRAIL && step-- > 0;)
  {
    // A figure that stands on the trail without knowing its place there, as after a new trail is laid, takes it.
    if (trail[step] == here)
    {
      onTrail = static_cast<std::uint32_t>(step);
    }
  }
  if (onTrail != OFF_TRAIL)
  {
    return onTrail < stop ? alongTrail(onTrail) : std::nullopt;
  }
  // Off it: the trail cell up to its stop that its greedy steps reach, the nearest by octile distance, ties to the later.
  const std::int32_t width = m_navigation.WidthCells();
  std::optional<PathSearch::CellIndex> nearest;
  std::int64_t best = std::numeric_limits<std::int64_t>::max();
  for (std::size_t step = 0; step <= stop && step < trail.size(); ++step)
  {
    const std::int64_t distance = PathSearch::Octile(here, trail[step], width);
    if (distance <= best && GreedyReaches(here, _unit.navigationClass, trail[step]))
    {
      best = distance;
      nearest = trail[step];
    }
  }
  if (!nearest.has_value())
  {
    return std::nullopt;
  }
  const std::optional<PathSearch::CellIndex> next = Towards(here, _unit.navigationClass, *nearest);
  return next.has_value() ? std::optional<Step>(Step{.cell = *next, .trailStep = OFF_TRAIL}) : std::nullopt;
}

std::optional<PathSearch::CellIndex> World::Towards(PathSearch::CellIndex _from, NavigationClass _class,
                                                    PathSearch::CellIndex _target) const
{
  // The step among its eight neighbours that most reduces the octile distance to _target, along a link open to its
  // class, ties to the lower link index (ADR-022, decision 4); none when none does.
  const std::int32_t width = m_navigation.WidthCells();
  std::int64_t best = PathSearch::Octile(_from, _target, width);
  std::optional<PathSearch::CellIndex> step;
  const std::uint8_t links = m_navigation.Links(static_cast<std::int32_t>(_from % width), static_cast<std::int32_t>(_from / width), _class);
  for (std::size_t direction = 0; direction < NavigationGrid::NEIGHBORS.size(); ++direction)
  {
    if ((links & (1U << direction)) == 0)
    {
      continue;
    }
    const PathSearch::CellIndex next =
      _from + (std::int64_t{NavigationGrid::NEIGHBORS.at(direction)[1]} * width) + NavigationGrid::NEIGHBORS.at(direction)[0];
    const std::int64_t distance = PathSearch::Octile(next, _target, width);
    if (distance < best)
    {
      best = distance;
      step = next;
    }
  }
  return step;
}

bool World::GreedyReaches(PathSearch::CellIndex _from, NavigationClass _class, PathSearch::CellIndex _target, std::int32_t _steps) const
{
  // Whether greedy steps from _from arrive at _target within _steps. Each step brings it nearer, so they cannot circle;
  // they stop against a wall between the two.
  const std::int32_t width = m_navigation.WidthCells();
  if (std::abs((_from % width) - (_target % width)) > _steps || std::abs((_from / width) - (_target / width)) > _steps)
  {
    return false;
  }
  PathSearch::CellIndex cell = _from;
  for (std::int32_t step = 0; step < _steps && cell != _target; ++step)
  {
    const std::optional<PathSearch::CellIndex> next = Towards(cell, _class, _target);
    if (!next.has_value())
    {
      return false;
    }
    cell = *next;
  }
  return cell == _target;
}

World::Plans World::PlanAll() const
{
  Plans plans;
  for (const auto& [id, squad] : m_squads)
  {
    if (squad.garrisonedIn != Protocol::NO_ENTITY)
    {
      continue;
    }
    plans.squads.emplace(id, PlanFor(squad));
    for (std::size_t place = 0; place < squad.units.size(); ++place)
    {
      plans.places.emplace(squad.units[place], place);
    }
  }
  return plans;
}

std::map<Protocol::EntityId, PathSearch::CellIndex> World::Wanted(const Plans& _plans) const
{
  std::map<Protocol::EntityId, PathSearch::CellIndex> wanted;
  for (const auto& [id, place] : _plans.places)
  {
    const Unit& unit = m_units.at(id);
    if (unit.reservedCell >= 0 || Holding(unit.squad))
    {
      continue;
    }
    if (const std::optional<Step> step = NextStep(unit, m_squads.at(unit.squad), place, _plans.squads.at(unit.squad)); step.has_value())
    {
      wanted.emplace(id, step->cell);
    }
  }
  return wanted;
}

bool World::Holding(Protocol::EntityId _squad) const
{
  return std::ranges::any_of(m_searches, [_squad](const auto& _search) { return _search.first.second == _squad; });
}

void World::StartStep(Unit& _unit, Squad& _squad, std::size_t _place, const Step& _step, Occupancy& _occupancy)
{
  // A step is reserved before it is taken (ADR-022, decision 2).
  const std::int32_t width = m_navigation.WidthCells();
  const PathSearch::CellIndex from = CellOf(_unit.cellVoxels);
  _unit.stepFromCell = from;
  _unit.reservedCell = _step.cell;
  _unit.stepEffort = 0;
  _unit.waitTicks = 0;
  // Its trail step is the trail cell it stands in or steps towards, as M2's figures' were.
  _unit.trailStep = _step.trailStep;
  for (const PathSearch::CellIndex cell : Footprint(_unit, _step.cell))
  {
    _occupancy.reserved.insert_or_assign(cell, _unit.id);
  }
  if (_place == 0)
  {
    // A squad faces the way its lead last stepped.
    const auto dx = static_cast<std::int32_t>((_step.cell % width) - (from % width));
    const auto dz = static_cast<std::int32_t>((_step.cell / width) - (from / width));
    _squad.facing = FacingOf(std::clamp(dx, -1, 1), std::clamp(dz, -1, 1));
  }
}

void World::AdvanceStep(Unit& _unit, const Squad& _squad, std::int64_t& _budget, Occupancy& _occupancy)
{
  const std::int64_t need = StepEffort(_unit.stepFromCell, _unit.reservedCell, _unit.navigationClass);
  const std::int64_t before = _unit.stepEffort;
  const std::int64_t spent = std::min(_budget, std::max<std::int64_t>(need - before, 0));
  _unit.stepEffort += spent;
  _budget -= spent;
  // It holds the cell it left until it is halfway, and the one it steps into after.
  if (2 * before < need && 2 * _unit.stepEffort >= need)
  {
    for (const PathSearch::CellIndex cell : Footprint(_unit, _unit.stepFromCell))
    {
      const auto held = _occupancy.occupied.find(cell);
      if (held != _occupancy.occupied.end() && held->second == _unit.id)
      {
        _occupancy.occupied.erase(held);
      }
    }
    for (const PathSearch::CellIndex cell : Footprint(_unit, _unit.reservedCell))
    {
      _occupancy.occupied.insert_or_assign(cell, _unit.id);
    }
  }
  if (_unit.stepEffort < need)
  {
    Place(_unit);
    return;
  }
  // Arrived: the reservation is spent, and a figure that stepped onto its squad's trail follows it from there.
  for (const PathSearch::CellIndex cell : Footprint(_unit, _unit.reservedCell))
  {
    const auto reserved = _occupancy.reserved.find(cell);
    if (reserved != _occupancy.reserved.end() && reserved->second == _unit.id)
    {
      _occupancy.reserved.erase(reserved);
    }
  }
  const PathSearch::CellIndex arrived = _unit.reservedCell;
  _unit.reservedCell = NO_CELL;
  _unit.stepFromCell = arrived;
  _unit.stepEffort = 0;
  if (_unit.trailStep == OFF_TRAIL && !_squad.trail.empty())
  {
    // The trail may pass a cell twice: it takes the latest pass at or behind its lead.
    const std::uint32_t leadStep = m_units.at(_squad.units.front()).trailStep;
    const std::size_t last = std::min<std::size_t>(leadStep == OFF_TRAIL ? _squad.trail.size() - 1 : leadStep, _squad.trail.size() - 1);
    for (std::size_t step = last + 1; step-- > 0;)
    {
      if (_squad.trail[step] == arrived)
      {
        _unit.trailStep = static_cast<std::uint32_t>(step);
        break;
      }
    }
  }
  Place(_unit);
}

void World::WalkFigures()
{
  // ADR-022: one figure a cell. In unit-id order each figure takes the step its squad's plan gives it when the cell is
  // free, two friendly figures that want each other's cells swap, and one that cannot waits, spending nothing; a
  // figure that waits waitBeforeSearchTicks in a row has its squad search again. A machine has right of way over its own
  // side's figures: none of them steps into a cell its next step needs, and one standing there steps aside (decision 3,
  // amended).
  Occupancy occupancy = BuildOccupancy();
  const Plans plans = PlanAll();
  std::map<Protocol::EntityId, PathSearch::CellIndex> wanted = Wanted(plans);
  const std::set<std::pair<std::uint8_t, PathSearch::CellIndex>> rightOfWay = RightOfWay(wanted);
  std::set<Protocol::EntityId> walked;
  for (auto& [id, unit] : m_units)
  {
    const auto place = plans.places.find(id);
    if (place == plans.places.end())
    {
      continue;
    }
    Squad& squad = m_squads.at(unit.squad);
    const bool holding = Holding(unit.squad);
    std::int64_t budget = SpeedOf(unit);
    bool waited = false;
    while (budget > 0)
    {
      if (unit.reservedCell < 0)
      {
        // It stands at a cell: a squad still searching starts no step (M2-F3).
        if (holding)
        {
          break;
        }
        const std::optional<Step> step = NextStep(unit, squad, place->second, plans.squads.at(unit.squad));
        if (!step.has_value())
        {
          break;
        }
        const bool yields = unit.navigationClass != NavigationClass::Machines && rightOfWay.contains({unit.owner, step->cell});
        if (!yields && Free(occupancy, unit, step->cell))
        {
          StartStep(unit, squad, place->second, *step, occupancy);
        }
        else if (const std::optional<Protocol::EntityId> partner = yields ? std::nullopt : SwapPartner(unit, step->cell, occupancy, wanted);
                 partner.has_value())
        {
          // ADR-022, decision 3: both start the step together, each with this tick's effort.
          Unit& other = m_units.at(*partner);
          Squad& otherSquad = m_squads.at(other.squad);
          const PathSearch::CellIndex back = CellOf(unit.cellVoxels);
          const bool otherAlong = other.trailStep != OFF_TRAIL && std::size_t{other.trailStep} + 1 < otherSquad.trail.size() &&
                                  otherSquad.trail[std::size_t{other.trailStep} + 1] == back;
          const Step otherStep{.cell = back, .trailStep = otherAlong ? other.trailStep + 1 : OFF_TRAIL};
          StartStep(unit, squad, place->second, *step, occupancy);
          StartStep(other, otherSquad, plans.places.at(*partner), otherStep, occupancy);
          wanted.erase(*partner);
          if (walked.contains(*partner))
          {
            std::int64_t otherBudget = SpeedOf(other);
            AdvanceStep(other, otherSquad, otherBudget, occupancy);
          }
        }
        else
        {
          if (unit.navigationClass == NavigationClass::Machines)
          {
            GiveWay(unit, step->cell, plans, occupancy, wanted, walked);
          }
          waited = true;
          break;
        }
      }
      AdvanceStep(unit, squad, budget, occupancy);
    }
    walked.insert(id);
    // It counts the ticks it has waited in a row, unless it waits on purpose, engaging an enemy.
    unit.waitTicks = waited && unit.target == Protocol::NO_ENTITY ? unit.waitTicks + 1 : 0;
  }
  // A squad blocked for long searches again from its lead's cell (ADR-022, decision 6).
  const auto patience = static_cast<std::uint32_t>(std::max(1, Data().Tuning().waitBeforeSearchTicks));
  for (auto& [id, squad] : m_squads)
  {
    const bool blocked =
      std::ranges::any_of(squad.units, [this, patience](Protocol::EntityId _unit) { return m_units.at(_unit).waitTicks >= patience; });
    if (blocked && squad.hasGoal)
    {
      for (const Protocol::EntityId unit : squad.units)
      {
        m_units.at(unit).waitTicks = 0;
      }
      RequestSearch(squad);
    }
    TrimTrail(squad);
  }
}

std::set<std::pair<std::uint8_t, PathSearch::CellIndex>>
World::RightOfWay(const std::map<Protocol::EntityId, PathSearch::CellIndex>& _wanted) const
{
  std::set<std::pair<std::uint8_t, PathSearch::CellIndex>> cells;
  for (const auto& [id, cell] : _wanted)
  {
    const Unit& unit = m_units.at(id);
    if (unit.navigationClass == NavigationClass::Machines)
    {
      for (const PathSearch::CellIndex needed : Footprint(unit, cell))
      {
        cells.emplace(unit.owner, needed);
      }
    }
  }
  return cells;
}

void World::GiveWay(const Unit& _machine, PathSearch::CellIndex _cell, const Plans& _plans, Occupancy& _occupancy,
                    std::map<Protocol::EntityId, PathSearch::CellIndex>& _wanted, const std::set<Protocol::EntityId>& _walked)
{
  // ADR-022, decision 3, amended: a machine never swaps, and a re-search does not see figures (decision 6), so a figure of
  // its own side standing still in a cell its step needs, an idle squad's in its lane, would stop it for good. Such a
  // figure, of a squad neither holding nor searching, steps to a free neighbouring cell its class may step into, outside
  // the machine's cells and the cells of its next three steps when it can, outside its cells and its step's otherwise:
  // the first such in link order. It then makes for its slot or its trail as any figure off it does.
  const std::int32_t width = m_navigation.WidthCells();
  std::set<PathSearch::CellIndex> machine;
  for (const PathSearch::CellIndex cell : Footprint(_machine, CellOf(_machine.cellVoxels)))
  {
    machine.insert(cell);
  }
  for (const PathSearch::CellIndex cell : Footprint(_machine, _cell))
  {
    machine.insert(cell);
  }
  std::set<PathSearch::CellIndex> ahead = machine;
  const Squad& machineSquad = m_squads.at(_machine.squad);
  if (_machine.trailStep != OFF_TRAIL)
  {
    constexpr std::size_t AHEAD_STEPS = 3;
    for (std::size_t step = std::size_t{_machine.trailStep} + 1;
         step < machineSquad.trail.size() && step <= std::size_t{_machine.trailStep} + AHEAD_STEPS; ++step)
    {
      for (const PathSearch::CellIndex cell : Footprint(_machine, machineSquad.trail[step]))
      {
        ahead.insert(cell);
      }
    }
  }
  for (const PathSearch::CellIndex needed : Footprint(_machine, _cell))
  {
    const auto occupant = _occupancy.occupied.find(needed);
    if (occupant == _occupancy.occupied.end() || occupant->second == _machine.id)
    {
      continue;
    }
    Unit& figure = m_units.at(occupant->second);
    const auto place = _plans.places.find(figure.id);
    if (figure.owner != _machine.owner || figure.navigationClass == NavigationClass::Machines || figure.reservedCell >= 0 ||
        place == _plans.places.end() || Holding(figure.squad))
    {
      continue;
    }
    Squad& squad = m_squads.at(figure.squad);
    if (squad.order == Protocol::SquadOrder::Hold)
    {
      continue;
    }
    const PathSearch::CellIndex from = CellOf(figure.cellVoxels);
    const std::uint8_t links =
      m_navigation.Links(static_cast<std::int32_t>(from % width), static_cast<std::int32_t>(from / width), figure.navigationClass);
    std::optional<PathSearch::CellIndex> aside;
    for (const std::set<PathSearch::CellIndex>* clear : {&ahead, &machine})
    {
      for (std::size_t direction = 0; direction < NavigationGrid::NEIGHBORS.size() && !aside.has_value(); ++direction)
      {
        const PathSearch::CellIndex next =
          from + (std::int64_t{NavigationGrid::NEIGHBORS.at(direction)[1]} * width) + NavigationGrid::NEIGHBORS.at(direction)[0];
        if ((links & (1U << direction)) != 0 && !clear->contains(next) && Free(_occupancy, figure, next))
        {
          aside = next;
        }
      }
      if (aside.has_value())
      {
        break;
      }
    }
    if (!aside.has_value())
    {
      continue;
    }
    StartStep(figure, squad, place->second, Step{.cell = *aside, .trailStep = OFF_TRAIL}, _occupancy);
    _wanted.erase(figure.id);
    if (_walked.contains(figure.id))
    {
      std::int64_t budget = SpeedOf(figure);
      AdvanceStep(figure, squad, budget, _occupancy);
    }
  }
}

std::optional<Protocol::EntityId> World::SwapPartner(const Unit& _unit, PathSearch::CellIndex _cell, const Occupancy& _occupancy,
                                                     const std::map<Protocol::EntityId, PathSearch::CellIndex>& _wanted) const
{
  // The figure standing still in _cell, of _unit's owner and a cell's width, that wants _unit's cell (ADR-022, decision
  // 3); or, when _unit leads its squad, any of its squad standing there, which gives way to it; or one standing idle
  // there, which wants no cell (decision 3, amended).
  const auto occupant = _occupancy.occupied.find(_cell);
  if (occupant == _occupancy.occupied.end() || occupant->second == _unit.id || _unit.navigationClass == NavigationClass::Machines)
  {
    return std::nullopt;
  }
  const Unit& other = m_units.at(occupant->second);
  const auto wants = _wanted.find(other.id);
  if (other.owner != _unit.owner || other.reservedCell >= 0 || other.navigationClass == NavigationClass::Machines ||
      _occupancy.reserved.contains(_cell))
  {
    return std::nullopt;
  }
  const bool mutual = wants != _wanted.end() && wants->second == CellOf(_unit.cellVoxels);
  // A squad never blocks its own lead, whatever its figures want, and the lead only goes on along its trail, so the two
  // never trade back. Between followers only the mutual swap: two that each gave way to the other would trade cells
  // for ever, and a ring of them each waiting on the next waits until the lead moves.
  const bool givesWay = other.squad == _unit.squad && m_squads.at(_unit.squad).units.front() == _unit.id;
  // A figure of another squad that wants no cell, fights nothing and is at no work or hold, of a squad not searching,
  // trades cells with one whose step needs its own: an arrived squad's rear left standing in a lane would otherwise stop
  // the squads behind it for good, since it wants nothing to swap for and a squad's new search does not see it. Wanting
  // nothing, it never trades back, and it stays idle in the cell it is given. Within a squad a follower at its stop waits
  // for the lead, as before, so that none passes the one ahead.
  const Protocol::SquadOrder order = m_squads.at(other.squad).order;
  const bool walksOrIdles = order == Protocol::SquadOrder::Idle || order == Protocol::SquadOrder::Move ||
                            order == Protocol::SquadOrder::Storm || order == Protocol::SquadOrder::Attack;
  const bool idle =
    other.squad != _unit.squad && wants == _wanted.end() && other.target == Protocol::NO_ENTITY && walksOrIdles && !Holding(other.squad);
  return mutual || givesWay || idle ? std::optional<Protocol::EntityId>(other.id) : std::nullopt;
}

void World::TrimTrail(Squad& _squad)
{
  // The cells behind every figure on the trail are walked, but the lead keeps TRAIL_BEHIND_CELLS behind it, and as many
  // as its squad has figures, for those that leave their slots to follow it.
  if (_squad.trail.empty() || _squad.units.empty())
  {
    return;
  }
  const std::uint32_t leadStep = m_units.at(_squad.units.front()).trailStep;
  if (leadStep == OFF_TRAIL)
  {
    return;
  }
  const std::uint32_t behind = std::max<std::uint32_t>(TRAIL_BEHIND_CELLS, static_cast<std::uint32_t>(_squad.units.size()));
  std::uint32_t keep = leadStep > behind ? leadStep - behind : 0;
  for (const Protocol::EntityId id : _squad.units)
  {
    const Unit& unit = m_units.at(id);
    if (unit.trailStep != OFF_TRAIL)
    {
      keep = std::min(keep, unit.reservedCell >= 0 && unit.trailStep > 0 ? unit.trailStep - 1 : unit.trailStep);
    }
  }
  if (keep == 0 || keep >= _squad.trail.size())
  {
    return;
  }
  _squad.trail.erase(_squad.trail.begin(), _squad.trail.begin() + static_cast<std::ptrdiff_t>(keep));
  for (const Protocol::EntityId id : _squad.units)
  {
    Unit& unit = m_units.at(id);
    if (unit.trailStep != OFF_TRAIL)
    {
      unit.trailStep -= keep;
    }
  }
}

void World::Place(Unit& _unit) const
{
  const std::int32_t width = m_navigation.WidthCells();
  const PathSearch::CellIndex to = _unit.reservedCell >= 0 ? _unit.reservedCell : CellOf(_unit.cellVoxels);
  const PathSearch::CellIndex from = _unit.reservedCell >= 0 ? _unit.stepFromCell : to;
  const std::int64_t need = _unit.reservedCell >= 0 ? std::max<std::int64_t>(StepEffort(from, to, _unit.navigationClass), 1) : 1;
  const std::int64_t effort = _unit.reservedCell >= 0 ? _unit.stepEffort : 0;
  // Between the two cells' middles in proportion to the effort spent. It stands in the cell it left
  // until it is halfway, and at that cell's height (ADR-015, decision 6).
  const auto along = [effort, need](std::int64_t _fromCell, std::int64_t _toCell)
  {
    const std::int64_t half = Core::Fixed::ONE_RAW / 2;
    const std::int64_t start = _fromCell * Core::Fixed::ONE_RAW + half;
    const std::int64_t finish = _toCell * Core::Fixed::ONE_RAW + half;
    return Core::Fixed::FromRaw(static_cast<std::int32_t>(start + Core::FloorDivide((finish - start) * effort, need)));
  };
  const PathSearch::CellIndex cell = 2 * effort < need ? from : to;
  const auto cellX = static_cast<std::int32_t>(cell % width);
  const auto cellZ = static_cast<std::int32_t>(cell / width);
  const std::int16_t height = m_navigation.WalkableHeight(cellX, cellZ);
  const std::int32_t heightVoxels = height == NavigationGrid::NOT_WALKABLE ? _unit.cellVoxels.yVoxels : height;
  _unit.cellVoxels = {.xVoxels = cellX, .yVoxels = heightVoxels, .zVoxels = cellZ};
  _unit.position = {.xMeters = along(from % width, to % width),
                    .yMeters = Core::Fixed::FromWhole(heightVoxels),
                    .zMeters = along(from / width, to / width)};
}

std::int64_t World::StepEffort(PathSearch::CellIndex _from, PathSearch::CellIndex _to, NavigationClass _class) const
{
  // A diagonal step is sqrt(2) m: floor(sqrt(2) * 65536), in Fixed's raw meters.
  constexpr std::int64_t DIAGONAL_RAW = 92681;
  constexpr std::int64_t TWO_SQUARED_METERS_RAW = std::int64_t{2} * Core::Fixed::ONE_RAW * Core::Fixed::ONE_RAW;
  static_assert(DIAGONAL_RAW * DIAGONAL_RAW <= TWO_SQUARED_METERS_RAW && (DIAGONAL_RAW + 1) * (DIAGONAL_RAW + 1) > TWO_SQUARED_METERS_RAW);
  if (_from == _to)
  {
    return 0;
  }
  const std::int32_t width = m_navigation.WidthCells();
  const bool diagonal = _from % width != _to % width && _from / width != _to / width;
  const std::int64_t length = diagonal ? DIAGONAL_RAW : Core::Fixed::ONE_RAW;
  return length * m_navigation.Factor(static_cast<std::int32_t>(_to % width), static_cast<std::int32_t>(_to / width), _class);
}

bool World::Linked(PathSearch::CellIndex _from, PathSearch::CellIndex _to, NavigationClass _class) const
{
  if (_from == _to)
  {
    return true;
  }
  const std::int32_t width = m_navigation.WidthCells();
  const auto fromX = static_cast<std::int32_t>(_from % width);
  const auto fromZ = static_cast<std::int32_t>(_from / width);
  const std::int32_t dx = static_cast<std::int32_t>(_to % width) - fromX;
  const std::int32_t dz = static_cast<std::int32_t>(_to / width) - fromZ;
  for (std::size_t direction = 0; direction < NavigationGrid::NEIGHBORS.size(); ++direction)
  {
    if (NavigationGrid::NEIGHBORS.at(direction)[0] == dx && NavigationGrid::NEIGHBORS.at(direction)[1] == dz)
    {
      return (m_navigation.Links(fromX, fromZ, _class) & (1U << direction)) != 0;
    }
  }
  return false;
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
    // A deploy or a pack runs down and ends in its state (MvpPlan.md, M4, input 17).
    if (engine.stateTicksLeft > 0)
    {
      --engine.stateTicksLeft;
      if (engine.stateTicksLeft == 0)
      {
        engine.state = engine.state == EngineState::Packing ? EngineState::Packed : EngineState::Deployed;
      }
    }
  }
  RamBlows();
  LooseFromGarrisons();
  PassageAttacks();
  std::vector<Protocol::EntityId> landed;
  for (auto& [id, shot] : m_shots)
  {
    if (const auto* line = std::get_if<ArrowLine>(&shot.flight))
    {
      // An arrow stops at the first solid voxel its step crosses and does nothing; one that reaches its end hits its
      // target when the target stands within arrowHitReachCells of the cell it was aimed at, and otherwise falls
      // harmlessly. It passes every other unit (ADR-023, decision 5).
      const bool stopped =
        FirstSolid(ArrowPositionAt(*line, shot.ticksFlown), ArrowPositionAt(*line, shot.ticksFlown + 1), m_voxels).has_value();
      const bool arrives = ArrowArrives(*line, shot.ticksFlown);
      ++shot.ticksFlown;
      if (!stopped && arrives)
      {
        const std::int32_t reach = Data().Tuning().arrowHitReachCells;
        const auto target = m_units.find(shot.target);
        if (target != m_units.end() && !Garrisoned(target->second) &&
            std::abs(target->second.cellVoxels.xVoxels - line->end.xMeters.FloorWhole()) <= reach &&
            std::abs(target->second.cellVoxels.zVoxels - line->end.zMeters.FloorWhole()) <= reach)
        {
          target->second.hitPoints -= shot.damage;
        }
      }
      if (stopped || arrives)
      {
        landed.push_back(id);
      }
      continue;
    }
    const ShotArc& arc = std::get<ShotArc>(shot.flight);
    const ShotStep step = StepShot(arc, shot.ticksFlown, m_voxels, WidthVoxels(), DepthVoxels());
    ++shot.ticksFlown;
    const std::optional<Protocol::VoxelPosition>& hit = step.hit;
    if (hit.has_value())
    {
      const bool alongX = std::abs(arc.acrossXMeters.Raw()) >= std::abs(arc.acrossZMeters.Raw());
      const auto sign = [](Core::Fixed _value) { return _value.Raw() > 0 ? 1 : (_value.Raw() < 0 ? -1 : 0); };
      const ShotHit& struck = m_tickHits.emplace_back(ShotHit{
        .shot = id, .voxel = *hit, .behindX = alongX ? sign(arc.acrossXMeters) : 0, .behindZ = alongX ? 0 : sign(arc.acrossZMeters)});
      // Its crush (ADR-023, decision 7), then what it does to the structures and the terrain, by the trial's rule
      // (ADR-028, decision 1).
      Rule().ShotStrikes(*this, struck);
      landed.push_back(id);
    }
    else if (step.left)
    {
      landed.push_back(id);
    }
  }
  for (const Protocol::EntityId id : landed)
  {
    m_shots.erase(id);
  }
  RemoveDead();
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
  // One collapse for each unsupported piece, given ids in piece order. In the control the search is for terrain alone:
  // a piece holding a structure's voxel stands while the structure has hit points (ADR-028, decision 2).
  for (Piece piece : found.pieces)
  {
    std::erase_if(piece, [&claimed](const Protocol::VoxelPosition& _voxel)
                  { return claimed.contains({_voxel.xVoxels, _voxel.yVoxels, _voxel.zVoxels}); });
    if (piece.empty() ||
        (!Rule().CollapsesStructures() && std::ranges::any_of(piece, [this](const Protocol::VoxelPosition& _voxel)
                                                              { return StructureHolding(_voxel) != Protocol::NO_ENTITY; })))
    {
      continue;
    }
    const Protocol::EntityId id = NewEntityId();
    CollapsePlan plan = PlanCollapse(piece, edits, m_voxels, Data());
    const Protocol::EntityId structure = StructureHolding(piece.front());
    m_collapses.emplace(id, Collapse{.id = id,
                                     .piece = std::move(piece),
                                     .plan = std::move(plan),
                                     .ticksElapsed = 0,
                                     .struck = {},
                                     .structure = structure,
                                     .fallen = {}});
  }
}

void World::AdvanceCollapses()
{
  // Step 6: each collapse in id order counts its warning down, during which no voxel of its piece
  // moves, then falls (M2-E4). Its edits are searched in the next tick's step 5, so a cascade starts
  // a tick later (ADR-004, decision 3).
  std::vector<Protocol::EntityId> ended;
  for (auto& [id, collapse] : m_collapses)
  {
    ++collapse.ticksElapsed;
    if (collapse.ticksElapsed > collapse.plan.warningTicks && Fall(collapse))
    {
      ended.push_back(id);
      m_endedCollapses.push_back(ViewOf(collapse));
    }
  }
  for (const Protocol::EntityId id : ended)
  {
    m_collapses.erase(id);
  }
  RemoveDead();
}

bool World::Fall(Collapse& _collapse)
{
  const CollapsePlan& plan = _collapse.plan;
  const std::uint32_t fallTick = _collapse.ticksElapsed - plan.warningTicks; // from 1
  if (fallTick == 1)
  {
    // The piece leaves the grid on the fall's first tick, and every unit standing in the strip is hit,
    // once (items 5 and 8). What it was is kept for the client's debris (M3-B2).
    for (const Protocol::VoxelPosition& voxel : _collapse.piece)
    {
      _collapse.fallen.push_back({.position = voxel, .paletteIndex = m_voxels.At(voxel)});
      m_voxels.Edit(voxel, Protocol::Chunk::EMPTY);
    }
    std::set<std::pair<std::int32_t, std::int32_t>> strip;
    for (const Protocol::VoxelPosition& column : plan.strip)
    {
      strip.insert({column.xVoxels, column.zVoxels});
    }
    for (const auto& [unitId, unit] : m_units)
    {
      if (strip.contains({unit.cellVoxels.xVoxels, unit.cellVoxels.zVoxels}) && !Garrisoned(unit))
      {
        Strike(_collapse, unitId);
      }
    }
  }
  // The landing arrives in equal parts over the fall's ticks, nearest the foot first.
  const std::uint64_t total = plan.landing.size();
  const std::uint64_t fallTicks = std::max<std::uint32_t>(plan.fallTicks, 1);
  const std::uint64_t from = total * std::min<std::uint64_t>(fallTick - 1, fallTicks) / fallTicks;
  const std::uint64_t to = total * std::min<std::uint64_t>(fallTick, fallTicks) / fallTicks;
  std::map<std::pair<std::int32_t, std::int32_t>, std::int32_t> filled; // column, highest voxel landed
  for (std::uint64_t index = from; index < to; ++index)
  {
    const PlacedVoxel& placed = plan.landing[static_cast<std::size_t>(index)];
    m_voxels.Edit(placed.position, placed.paletteIndex);
    const auto [entry, added] = filled.emplace(std::pair{placed.position.xVoxels, placed.position.zVoxels}, placed.position.yVoxels);
    entry->second = std::max(entry->second, placed.position.yVoxels);
  }
  // A unit whose cell the landing reaches is hit too, if the piece's leaving did not hit it already;
  // step 9 lifts it (ADR-015, decision 7).
  for (const auto& [unitId, unit] : m_units)
  {
    const auto column = filled.find({unit.cellVoxels.xVoxels, unit.cellVoxels.zVoxels});
    if (column != filled.end() && column->second >= unit.cellVoxels.yVoxels && !Garrisoned(unit))
    {
      Strike(_collapse, unitId);
    }
  }
  return fallTick >= fallTicks;
}

void World::Strike(Collapse& _collapse, Protocol::EntityId _unit)
{
  const auto at = std::ranges::lower_bound(_collapse.struck, _unit);
  if (at != _collapse.struck.end() && *at == _unit)
  {
    return;
  }
  _collapse.struck.insert(at, _unit);
  m_tickCrushes.push_back({.unit = _unit, .collapse = _collapse.id, .dropVoxels = 0});
  Crush(m_units.at(_unit), Data().Tuning().collapseCrushDamage);
}

void World::SettleLoose()
{
  // Step 7: slump and loose material near structures (M2-E6, E7; GameDesign.md, M2's values, item 9).
  // Every column the tick's edits touched, and its four neighbours, joins the queue; then, in
  // column-index order, a column whose top voxel is loose and stands more than a voxel above a
  // neighbour's top sends it to the lowest such neighbour, ties north, east, south, west, and so does
  // one resting on a structure, or standing within looseRestBelowTopVoxels of a structure's top
  // beside it, up to slumpVoxelsPerTick voxels a tick. What waits is saved.
  for (const Protocol::VoxelPosition& voxel : m_voxels.TickEdits())
  {
    QueueSlump(voxel.xVoxels, voxel.zVoxels);
  }
  std::int32_t budget = Data().Tuning().slumpVoxelsPerTick;
  while (budget > 0 && !m_slumpQueue.empty())
  {
    const std::int64_t column = *m_slumpQueue.begin();
    m_slumpQueue.erase(m_slumpQueue.begin());
    const auto x = static_cast<std::int32_t>(column % WidthVoxels());
    const auto z = static_cast<std::int32_t>(column / WidthVoxels());
    const std::optional<std::pair<std::int32_t, std::int32_t>> to = Slump(x, z);
    if (to.has_value())
    {
      --budget;
      QueueSlump(x, z);
      QueueSlump(to->first, to->second);
    }
  }
}

std::int32_t World::WidthVoxels() const noexcept
{
  return m_desc.widthChunks * Protocol::CHUNK_EDGE_VOXELS;
}

std::int32_t World::DepthVoxels() const noexcept
{
  return m_desc.depthChunks * Protocol::CHUNK_EDGE_VOXELS;
}

std::int32_t World::HeightVoxels() const noexcept
{
  return m_desc.heightChunks * Protocol::CHUNK_EDGE_VOXELS;
}

std::int32_t World::TopOf(std::int32_t _x, std::int32_t _z) const
{
  const MaterialRules& materials = Data().Materials();
  for (std::int32_t y = HeightVoxels() - 1; y >= 0; --y)
  {
    const std::uint8_t index = m_voxels.At({.xVoxels = _x, .yVoxels = y, .zVoxels = _z});
    if (index != Protocol::Chunk::EMPTY && !materials.at(index).liquid)
    {
      return y;
    }
  }
  return -1;
}

void World::QueueSlump(std::int32_t _x, std::int32_t _z)
{
  constexpr std::array<std::array<std::int32_t, 2>, 5> AROUND = {{{0, 0}, {0, 1}, {1, 0}, {0, -1}, {-1, 0}}};
  for (const auto& [dx, dz] : AROUND)
  {
    const std::int32_t x = _x + dx;
    const std::int32_t z = _z + dz;
    if (x >= 0 && x < WidthVoxels() && z >= 0 && z < DepthVoxels())
    {
      m_slumpQueue.insert(std::int64_t{z} * WidthVoxels() + x);
    }
  }
}

std::optional<std::pair<std::int32_t, std::int32_t>> World::Slump(std::int32_t _x, std::int32_t _z)
{
  constexpr std::uint8_t EARTH = 2; // a grass voxel that moves lands as earth: only the generator places grass
  constexpr std::uint8_t GRASS = 1;
  const MaterialRules& materials = Data().Materials();
  const std::int32_t rest = Data().Tuning().looseRestBelowTopVoxels;
  const auto isLoose = [&materials](std::uint8_t _index)
  {
    const Behavior behavior = materials.at(_index).behavior;
    return behavior == Behavior::Earth || behavior == Behavior::StoneRubble || behavior == Behavior::WasteRubble;
  };
  const auto isStructure = [&materials](std::uint8_t _index)
  {
    const MaterialRule& rule = materials.at(_index);
    return _index != Protocol::Chunk::EMPTY && !rule.walkableGround && !rule.liquid;
  };
  const auto at = [this](std::int32_t _atX, std::int32_t _atY, std::int32_t _atZ)
  { return m_voxels.At({.xVoxels = _atX, .yVoxels = _atY, .zVoxels = _atZ}); };

  const std::int32_t top = TopOf(_x, _z);
  if (top >= 0 && isStructure(at(_x, top, _z)))
  {
    return SlumpFromUnder(_x, _z); // ADR-029, decision 3: only towards what a trench opened
  }
  if (top < 0 || !isLoose(at(_x, top, _z)))
  {
    return std::nullopt;
  }
  const std::uint8_t index = at(_x, top, _z);

  // A neighbour, as loose material sees it: its top; whether a structure tops it, and how high; and
  // whether it is grounded, its loose voxels resting on ground and not on a structure. Loose
  // material only ever comes to rest on a grounded column (M2-E7).
  struct Side
  {
    bool inside = false;
    std::int32_t top = -1;
    bool structureTop = false;
    bool grounded = false;
  };
  const auto sideAt = [&](std::int32_t _sideX, std::int32_t _sideZ)
  {
    Side side;
    if (_sideX < 0 || _sideX >= WidthVoxels() || _sideZ < 0 || _sideZ >= DepthVoxels())
    {
      return side;
    }
    side.inside = true;
    side.top = TopOf(_sideX, _sideZ);
    side.structureTop = side.top >= 0 && isStructure(at(_sideX, side.top, _sideZ));
    // A column a trench opened offers the gap under its structure, as loose material sees it (ADR-029, decision 3).
    bool under = false;
    if (side.structureTop && Opened(_sideX, _sideZ) && LooseHeight(_sideX, _sideZ, &under) >= 0 && under)
    {
      side.top = LooseHeight(_sideX, _sideZ, &under);
      side.structureTop = false;
      side.grounded = !isStructure(at(_sideX, side.top, _sideZ));
      return side;
    }
    std::int32_t y = side.top;
    while (y >= 0 && isLoose(at(_sideX, y, _sideZ)))
    {
      --y;
    }
    side.grounded = y < 0 || !isStructure(at(_sideX, y, _sideZ));
    return side;
  };
  // North, east, south, west: +z, +x, -z, -x.
  constexpr std::array<std::array<std::int32_t, 2>, 4> SIDES = {{{0, 1}, {1, 0}, {0, -1}, {-1, 0}}};
  std::array<Side, 4> sides{};
  for (std::size_t side = 0; side < SIDES.size(); ++side)
  {
    sides.at(side) = sideAt(_x + SIDES.at(side)[0], _z + SIDES.at(side)[1]);
  }
  // The lowest grounded side below _below, ties north, east, south, west.
  const auto lowestBelow = [&](std::int32_t _below) -> std::optional<std::size_t>
  {
    std::optional<std::size_t> lowest;
    for (std::size_t side = 0; side < SIDES.size(); ++side)
    {
      const Side& candidate = sides.at(side);
      if (candidate.inside && candidate.grounded && candidate.top < _below &&
          (!lowest.has_value() || candidate.top < sides.at(*lowest).top))
      {
        lowest = side;
      }
    }
    return lowest;
  };

  // Slump: to the lowest side more than a voxel below (M2-E6).
  std::optional<std::size_t> to = lowestBelow(top - 1);
  // Off a structure it rests on, to any lower side (M2-E7).
  if (!to.has_value() && isStructure(at(_x, top - 1, _z)))
  {
    to = lowestBelow(top);
  }
  // Away from a structure beside it whose top it stands within looseRestBelowTopVoxels of, to the side
  // opposite, if that is lower and the voxel would not land as near another structure's top there.
  for (std::size_t side = 0; side < SIDES.size() && !to.has_value(); ++side)
  {
    const std::size_t opposite = (side + 2) % SIDES.size();
    const Side& away = sides.at(opposite);
    if (!sides.at(side).structureTop || top <= sides.at(side).top - rest || !away.inside || !away.grounded || away.top >= top)
    {
      continue;
    }
    const std::int32_t awayX = _x + SIDES.at(opposite)[0];
    const std::int32_t awayZ = _z + SIDES.at(opposite)[1];
    bool nearAnother = false;
    for (const auto& [dx, dz] : SIDES)
    {
      const Side beyond = sideAt(awayX + dx, awayZ + dz);
      nearAnother = nearAnother || (beyond.structureTop && away.top + 1 > beyond.top - rest);
    }
    if (!nearAnother)
    {
      to = opposite;
    }
  }
  if (!to.has_value())
  {
    return std::nullopt;
  }
  const std::int32_t x = _x + SIDES.at(*to)[0];
  const std::int32_t z = _z + SIDES.at(*to)[1];
  m_voxels.Edit({.xVoxels = _x, .yVoxels = top, .zVoxels = _z}, Protocol::Chunk::EMPTY);
  m_voxels.Edit({.xVoxels = x, .yVoxels = sides.at(*to).top + 1, .zVoxels = z}, index == GRASS ? EARTH : index);
  return std::pair{x, z};
}

void World::RebuildNavigation()
{
  // Step 8: every navigation tile the tick's edits touched, and those of the gates its commands opened or shut, none
  // deferred (ADR-015, decision 4; ADR-024, decision 6).
  std::vector<Protocol::VoxelPosition> touched(m_voxels.TickEdits().begin(), m_voxels.TickEdits().end());
  touched.insert(touched.end(), m_gateTouches.begin(), m_gateTouches.end());
  m_gateTouches.clear();
  RebuildTiles(m_navigation.TilesTouchedBy(touched));
}

void World::RebuildTiles(const std::vector<std::pair<std::int32_t, std::int32_t>>& _tiles)
{
  for (const auto& [tileX, tileZ] : _tiles)
  {
    m_navigation.RebuildTile(m_voxels.Store(), tileX, tileZ);
  }
  if (_tiles.empty())
  {
    return;
  }
  // Reach too, so the queries of step 12 and whoever reads navigation between ticks find it current
  // (M3-B5). It labels only the tiles the rebuild changed, as the next search would have.
  m_navigation.UpdateReach();
  m_tilesRebuilt = true;
  // Then, in squad-id order, the squads to search again in the next tick's step 3 (ADR-015,
  // decision 5): each short of its goal, each whose path crosses a rebuilt tile, and each whose
  // search is not done, since the cells it has closed may have changed under it.
  const std::set<std::pair<std::int32_t, std::int32_t>> rebuilt(_tiles.begin(), _tiles.end());
  const std::int32_t width = m_navigation.WidthCells();
  for (auto& [id, squad] : m_squads)
  {
    if (!squad.hasGoal)
    {
      continue;
    }
    const bool searching = std::ranges::any_of(m_searches, [id](const auto& _search) { return _search.first.second == id; });
    // What is left to walk: the trail once the squad has one, else the path.
    const std::vector<PathSearch::CellIndex>& ahead = squad.trail.empty() ? squad.path : squad.trail;
    const bool crosses =
      std::ranges::any_of(ahead,
                          [&rebuilt, width](PathSearch::CellIndex _cell)
                          {
                            return rebuilt.contains({static_cast<std::int32_t>(_cell % width) / NavigationGrid::TILE_EDGE_CELLS,
                                                     static_cast<std::int32_t>(_cell / width) / NavigationGrid::TILE_EDGE_CELLS});
                          });
    if (squad.shortOfGoal || searching || crosses)
    {
      RequestSearch(squad);
    }
  }
}

void World::SettleUnits()
{
  // Step 9: units whose cells changed drop, rise or move, in unit-id order (M2-F6; ADR-015, decision
  // 7). A drop over 3 m, three of the 1 m world's voxels, raises a crush event carrying it.
  constexpr std::int32_t CRUSHING_DROP_VOXELS = 3;
  std::set<Protocol::EntityId> displaced;
  Occupancy occupancy = BuildOccupancy();
  for (auto& [id, unit] : m_units)
  {
    if (Garrisoned(unit))
    {
      continue; // in no cell (ADR-024, decision 5)
    }
    const std::int32_t x = unit.cellVoxels.xVoxels;
    const std::int32_t z = unit.cellVoxels.zVoxels;
    const std::int16_t height = m_navigation.WalkableHeight(x, z);
    if (height == NavigationGrid::NOT_WALKABLE || !m_navigation.Usable(x, z, unit.navigationClass))
    {
      // No longer walkable: to the nearest cell its class can use that no other unit holds, ties to the lower index
      // (ADR-022, *Consequences*). Its squad forms again from where its figures stand.
      if (MoveToFreeCell(unit, occupancy))
      {
        displaced.insert(unit.squad);
      }
      continue;
    }
    if (height == unit.cellVoxels.yVoxels)
    {
      continue;
    }
    // Ground gone, it drops; its cell filled, it is lifted, and the collapse that filled it has hit
    // it in step 6.
    if (unit.cellVoxels.yVoxels - height > CRUSHING_DROP_VOXELS)
    {
      // Crush for each metre beyond 3 (ADR-023, decision 8).
      const std::int32_t drop = unit.cellVoxels.yVoxels - height;
      m_tickCrushes.push_back({.unit = id, .collapse = Protocol::NO_ENTITY, .dropVoxels = drop});
      Crush(unit, Data().Tuning().dropCrushDamagePerVoxel * (drop - CRUSHING_DROP_VOXELS));
    }
    unit.cellVoxels.yVoxels = height;
    unit.position.yMeters = Core::Fixed::FromWhole(height);
  }
  // Two units lifted into one cell are moved apart, the higher id to the nearest free cell (ADR-022, *Consequences*).
  std::map<PathSearch::CellIndex, Protocol::EntityId> standing;
  for (auto& [id, unit] : m_units)
  {
    if (Garrisoned(unit))
    {
      continue;
    }
    const std::vector<PathSearch::CellIndex> cells = Footprint(unit, CellOf(unit.cellVoxels));
    const bool shared = std::ranges::any_of(cells, [&standing](PathSearch::CellIndex _cell) { return standing.contains(_cell); });
    if (shared && MoveToFreeCell(unit, occupancy))
    {
      displaced.insert(unit.squad);
    }
    for (const PathSearch::CellIndex cell : Footprint(unit, CellOf(unit.cellVoxels)))
    {
      standing.emplace(cell, id);
    }
  }
  for (const Protocol::EntityId squadId : displaced)
  {
    const auto squad = m_squads.find(squadId);
    if (squad == m_squads.end())
    {
      continue;
    }
    squad->second.trail.clear();
    for (const Protocol::EntityId unitId : squad->second.units)
    {
      Unit& unit = m_units.at(unitId);
      unit.trailStep = OFF_TRAIL;
      unit.stepEffort = 0;
      unit.reservedCell = NO_CELL;
      unit.stepFromCell = CellOf(unit.cellVoxels);
      unit.position = CellFloorMiddle(unit.cellVoxels);
    }
    RequestSearch(squad->second);
  }
  RemoveDead();
  FollowBodies();
}

bool World::MoveToFreeCell(Unit& _unit, Occupancy& _occupancy)
{
  // The nearest cell by octile distance, ties to the lower index, that the unit's class can use and no other unit holds
  // or has reserved, within a widening square about it.
  constexpr std::int32_t SEARCH_CELLS = 24;
  const std::int32_t width = m_navigation.WidthCells();
  const PathSearch::CellIndex here = CellOf(_unit.cellVoxels);
  std::optional<PathSearch::CellIndex> best;
  std::int64_t bestDistance = std::numeric_limits<std::int64_t>::max();
  for (std::int32_t ring = 0; ring <= SEARCH_CELLS && !best.has_value(); ++ring)
  {
    for (std::int32_t dz = -ring; dz <= ring; ++dz)
    {
      for (std::int32_t dx = -ring; dx <= ring; ++dx)
      {
        if (std::max(std::abs(dx), std::abs(dz)) != ring)
        {
          continue;
        }
        const std::int32_t x = _unit.cellVoxels.xVoxels + dx;
        const std::int32_t z = _unit.cellVoxels.zVoxels + dz;
        if (x < 0 || z < 0 || x >= width || z >= m_navigation.DepthCells() || !m_navigation.Usable(x, z, _unit.navigationClass))
        {
          continue;
        }
        const PathSearch::CellIndex cell = (std::int64_t{z} * width) + x;
        const std::int64_t distance = PathSearch::Octile(here, cell, width);
        if (Free(_occupancy, _unit, cell) && (distance < bestDistance || (distance == bestDistance && best.has_value() && cell < *best)))
        {
          best = cell;
          bestDistance = distance;
        }
      }
    }
  }
  if (!best.has_value())
  {
    return false;
  }
  for (auto held = _occupancy.occupied.begin(); held != _occupancy.occupied.end();)
  {
    held = held->second == _unit.id ? _occupancy.occupied.erase(held) : std::next(held);
  }
  for (auto held = _occupancy.reserved.begin(); held != _occupancy.reserved.end();)
  {
    held = held->second == _unit.id ? _occupancy.reserved.erase(held) : std::next(held);
  }
  const auto x = static_cast<std::int32_t>(*best % width);
  const auto z = static_cast<std::int32_t>(*best / width);
  _unit.cellVoxels = {.xVoxels = x, .yVoxels = m_navigation.WalkableHeight(x, z), .zVoxels = z};
  _unit.position = CellFloorMiddle(_unit.cellVoxels);
  _unit.reservedCell = NO_CELL;
  _unit.stepFromCell = *best;
  _unit.stepEffort = 0;
  for (const PathSearch::CellIndex cell : Footprint(_unit, *best))
  {
    _occupancy.occupied.insert_or_assign(cell, _unit.id);
  }
  return true;
}

void World::CheckRules()
{
  // Step 10: the structures' parts and whether each works (ADR-024, decision 2), then the trial's rules and its events
  // (ADR-026, decisions 4 and 5).
  UpdateStructures();
  CheckTrial();
}

bool World::IsSection(const Structure& _structure) const
{
  const ServerTemplate* found = Data().FindTemplate(_structure.templateId);
  return found != nullptr && found->criticalPart.empty();
}

void World::CheckTrial()
{
  // ADR-026, decision 4; MvpPlan.md, M4, inputs 15 and 16. Once decided, the outcome stands and the clock is stopped.
  if (!m_trial.has_value())
  {
    return;
  }
  const TrialRules& rules = *m_trial;
  TrialProgress& progress = m_trialProgress;
  FindOpenedSections();
  TrackPassing();
  progress.contested = false;
  if (progress.outcome != Protocol::TrialOutcome::Running)
  {
    return;
  }
  const auto keep = m_structures.find(rules.keep);
  const std::optional<Box> box = keep == m_structures.end() ? std::nullopt : BoxOf(keep->second);
  // The keep's foot: the cells within ringCells of its footprint, outside it. A soldier is a figure that is no engine's
  // body; a garrisoned figure stands in no cell.
  const auto inRing = [this, &box, &rules](const Unit& _unit)
  {
    if (!box.has_value() || Garrisoned(_unit))
    {
      return false;
    }
    const Protocol::VoxelPosition& cell = _unit.cellVoxels;
    const std::int32_t distance = std::max(Outside(cell.xVoxels, box->min.xVoxels, box->min.xVoxels + box->widthVoxels - 1),
                                           Outside(cell.zVoxels, box->min.zVoxels, box->min.zVoxels + box->depthVoxels - 1));
    return distance >= 1 && distance <= rules.ringCells;
  };
  bool attackers = false;
  bool defenders = false;
  bool army = false;
  for (const auto& [id, unit] : m_units)
  {
    const bool soldier = unit.kind != Protocol::UnitKind::Ram && unit.kind != Protocol::UnitKind::Trebuchet;
    army = army || unit.owner == rules.attacker;
    attackers = attackers || (unit.owner == rules.attacker && soldier && inRing(unit));
    defenders = defenders || (unit.owner == rules.defender && inRing(unit));
  }
  // The clock resets while the keep works, so when repair brings it back to working, and when the ring holds no attacker's
  // soldier; it pauses while both sides stand there, and otherwise counts. A voxel or a hit point put back in a keep that
  // still does not work leaves it as it is (the owner, 2026-11-02).
  const bool keepWorking = keep == m_structures.end() || keep->second.working;
  if (keepWorking || !attackers)
  {
    progress.holdTicks = 0;
  }
  else if (defenders)
  {
    progress.contested = true;
  }
  else
  {
    ++progress.holdTicks;
  }
  if (progress.holdTicks >= rules.holdTicks)
  {
    progress.outcome = Protocol::TrialOutcome::Won;
  }
  else if (!army)
  {
    progress.outcome = Protocol::TrialOutcome::LostArmy;
  }
  else if (m_tick + 1 >= rules.timeLimitTicks)
  {
    progress.outcome = Protocol::TrialOutcome::LostToClock;
  }
  if (progress.outcome != Protocol::TrialOutcome::Running)
  {
    progress.endTick = m_tick + 1;
    m_tickEvents.push_back({.kind = TrialEventKind::Outcome,
                            .tick = m_tick,
                            .player = rules.attacker,
                            .subject = rules.keep,
                            .other = Protocol::NO_ENTITY,
                            .amount = static_cast<std::int32_t>(progress.outcome),
                            .voxel = {}});
  }
}

void World::FindOpenedSections()
{
  // ADR-026, decision 5: a column of the footprint foot can use and reach from the camp, linked to a cell beside the
  // footprint on the section's inner side, the side of its centre line nearer its owner's keep. Reach changes only with
  // navigation, so only a tick that rebuilt a tile looks.
  if (!m_trial.has_value() || !m_tilesRebuilt)
  {
    return;
  }
  const TrialRules& rules = *m_trial;
  std::optional<PathSearch::CellIndex> camp;
  for (std::int32_t z = rules.campMinVoxels.zVoxels; z <= rules.campMaxVoxels.zVoxels && !camp.has_value(); ++z)
  {
    for (std::int32_t x = rules.campMinVoxels.xVoxels; x <= rules.campMaxVoxels.xVoxels && !camp.has_value(); ++x)
    {
      if (m_navigation.Usable(x, z, NavigationClass::Foot))
      {
        camp = CellOf({.xVoxels = x, .yVoxels = 0, .zVoxels = z});
      }
    }
  }
  if (!camp.has_value())
  {
    return;
  }
  for (const auto& [id, structure] : m_structures)
  {
    const std::optional<Box> box = BoxOf(structure);
    if (m_trialProgress.openedSections.contains(id) || !IsSection(structure) || !box.has_value())
    {
      continue;
    }
    const std::optional<std::pair<std::int64_t, std::int64_t>> keep = KeepCenterTwice(structure.owner);
    std::optional<Protocol::VoxelPosition> through;
    for (std::int32_t z = box->min.zVoxels; z < box->min.zVoxels + box->depthVoxels && !through.has_value(); ++z)
    {
      for (std::int32_t x = box->min.xVoxels; x < box->min.xVoxels + box->widthVoxels && !through.has_value(); ++x)
      {
        const PathSearch::CellIndex column = CellOf({.xVoxels = x, .yVoxels = 0, .zVoxels = z});
        if (!m_navigation.Usable(x, z, NavigationClass::Foot) ||
            m_navigation.NearestReachable(*camp, column, NavigationClass::Foot) != column)
        {
          continue;
        }
        for (std::int32_t innerZ = box->min.zVoxels - 1; innerZ <= box->min.zVoxels + box->depthVoxels && !through.has_value(); ++innerZ)
        {
          for (std::int32_t innerX = box->min.xVoxels - 1; innerX <= box->min.xVoxels + box->widthVoxels && !through.has_value(); ++innerX)
          {
            const bool beside = innerX < box->min.xVoxels || innerX >= box->min.xVoxels + box->widthVoxels || innerZ < box->min.zVoxels ||
                                innerZ >= box->min.zVoxels + box->depthVoxels;
            const PathSearch::CellIndex inner = CellOf({.xVoxels = innerX, .yVoxels = 0, .zVoxels = innerZ});
            if (beside && OnOwnersSide(*box, keep, innerX, innerZ) && m_navigation.Usable(innerX, innerZ, NavigationClass::Foot) &&
                m_navigation.NearestReachable(column, inner, NavigationClass::Foot) == inner)
            {
              through = Protocol::VoxelPosition{.xVoxels = x, .yVoxels = m_navigation.WalkableHeight(x, z), .zVoxels = z};
            }
          }
        }
      }
    }
    if (through.has_value())
    {
      m_trialProgress.openedSections.insert(id);
      m_tickEvents.push_back({.kind = TrialEventKind::SectionOpened,
                              .tick = m_tick,
                              .player = structure.owner,
                              .subject = id,
                              .other = Protocol::NO_ENTITY,
                              .amount = 0,
                              .voxel = *through});
    }
  }
}

void World::TrackPassing()
{
  // ADR-026, decision 5: a figure that comes into a section's footprint from one side of its centre line is counted when
  // it leaves it on the other, inwards as 1 and outwards as -1; one leaving the way it came is not. Which side it came in
  // by is the half of the footprint it is first found in.
  std::map<Protocol::EntityId, std::pair<Protocol::EntityId, bool>>& passing = m_trialProgress.passing;
  std::erase_if(passing, [this](const auto& _entry) { return !m_units.contains(_entry.first) || Garrisoned(m_units.at(_entry.first)); });
  std::vector<std::pair<Protocol::EntityId, Box>> sections;
  for (const auto& [id, structure] : m_structures)
  {
    if (const std::optional<Box> box = BoxOf(structure); IsSection(structure) && box.has_value())
    {
      sections.emplace_back(id, *box);
    }
  }
  for (const auto& [id, unit] : m_units)
  {
    if (Garrisoned(unit))
    {
      continue;
    }
    const Protocol::VoxelPosition& cell = unit.cellVoxels;
    const auto holds = [&cell](const Box& _box)
    {
      return cell.xVoxels >= _box.min.xVoxels && cell.xVoxels < _box.min.xVoxels + _box.widthVoxels && cell.zVoxels >= _box.min.zVoxels &&
             cell.zVoxels < _box.min.zVoxels + _box.depthVoxels;
    };
    const auto in = std::ranges::find_if(sections, [&holds](const auto& _section) { return holds(_section.second); });
    const auto tracked = passing.find(id);
    if (tracked != passing.end() && (in == sections.end() || in->first != tracked->second.first))
    {
      const Structure& section = m_structures.at(tracked->second.first);
      const std::optional<Box> box = BoxOf(section);
      const bool inner = box.has_value() && OnOwnersSide(*box, KeepCenterTwice(section.owner), cell.xVoxels, cell.zVoxels);
      if (inner != tracked->second.second)
      {
        m_tickEvents.push_back({.kind = TrialEventKind::FigurePassed,
                                .tick = m_tick,
                                .player = unit.owner,
                                .subject = id,
                                .other = section.id,
                                .amount = inner ? 1 : -1,
                                .voxel = cell});
      }
      passing.erase(tracked);
    }
    if (in != sections.end() && !passing.contains(id))
    {
      const Structure& section = m_structures.at(in->first);
      passing.emplace(id, std::pair{in->first, OnOwnersSide(in->second, KeepCenterTwice(section.owner), cell.xVoxels, cell.zVoxels)});
    }
  }
}

Protocol::VoxelPosition World::TemplateVoxel(const Structure& _structure, const ServerTemplate& _template, std::uint32_t _x,
                                             std::uint32_t _y, std::uint32_t _z) const
{
  const auto [x, z] = Turned(static_cast<std::int32_t>(_x), static_cast<std::int32_t>(_z), static_cast<std::int32_t>(_template.widthVoxels),
                             static_cast<std::int32_t>(_template.depthVoxels), _structure.quarterTurns);
  return {.xVoxels = _structure.originVoxels.xVoxels + x,
          .yVoxels = _structure.originVoxels.yVoxels + static_cast<std::int32_t>(_y),
          .zVoxels = _structure.originVoxels.zVoxels + z};
}

std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> World::HeldByCollapses() const
{
  std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> held;
  for (const auto& [id, collapse] : m_collapses)
  {
    for (const Protocol::VoxelPosition& voxel : collapse.piece)
    {
      held.emplace(voxel.xVoxels, voxel.yVoxels, voxel.zVoxels);
    }
  }
  return held;
}

std::uint8_t World::CriticalShare(const Structure& _structure,
                                  const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>>& _held) const
{
  const ServerTemplate* found = Data().FindTemplate(_structure.templateId);
  if (found == nullptr || found->criticalPart.empty())
  {
    return 100;
  }
  std::int64_t built = 0;
  std::int64_t standing = 0;
  std::size_t voxel = 0;
  for (std::uint32_t y = 0; y < found->heightVoxels; ++y)
  {
    for (std::uint32_t z = 0; z < found->depthVoxels; ++z)
    {
      for (std::uint32_t x = 0; x < found->widthVoxels; ++x, ++voxel)
      {
        const bool isBuilt = voxel / 8 < _structure.builtMask.size() && (_structure.builtMask[voxel / 8] & (1U << (voxel % 8))) != 0;
        if (found->parts.at(voxel) != ServerTemplate::Part::Critical || !isBuilt)
        {
          continue;
        }
        ++built;
        const Protocol::VoxelPosition at = TemplateVoxel(_structure, *found, x, y, z);
        if (m_voxels.At(at) == found->indices.at(voxel) && !_held.contains({at.xVoxels, at.yVoxels, at.zVoxels}))
        {
          ++standing;
        }
      }
    }
  }
  return built == 0 ? std::uint8_t{0} : static_cast<std::uint8_t>((100 * standing) / built);
}

std::uint8_t World::StandingShare(const Structure& _structure,
                                  const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>>& _held) const
{
  const ServerTemplate* found = Data().FindTemplate(_structure.templateId);
  if (found == nullptr)
  {
    return 100;
  }
  std::int64_t built = 0;
  std::int64_t standing = 0;
  std::size_t voxel = 0;
  for (std::uint32_t y = 0; y < found->heightVoxels; ++y)
  {
    for (std::uint32_t z = 0; z < found->depthVoxels; ++z)
    {
      for (std::uint32_t x = 0; x < found->widthVoxels; ++x, ++voxel)
      {
        if (voxel / 8 >= _structure.builtMask.size() || (_structure.builtMask[voxel / 8] & (1U << (voxel % 8))) == 0)
        {
          continue;
        }
        ++built;
        const Protocol::VoxelPosition at = TemplateVoxel(_structure, *found, x, y, z);
        if (m_voxels.At(at) == found->indices.at(voxel) && !_held.contains({at.xVoxels, at.yVoxels, at.zVoxels}))
        {
          ++standing;
        }
      }
    }
  }
  return built == 0 ? std::uint8_t{100} : static_cast<std::uint8_t>((100 * standing) / built);
}

void World::SetCommander(std::uint8_t _player, std::unique_ptr<Commander> _commander)
{
  if (_commander == nullptr)
  {
    m_commanders.erase(_player);
    return;
  }
  m_commanders.insert_or_assign(_player, CommanderSlot(std::move(_commander)));
}

std::span<const SubmittedCommand> World::LastTickCommanderCommands() const noexcept
{
  return m_lastCommanderCommands;
}

void World::RunCommanders()
{
  // Step 13 (ADR-027, decisions 2 to 4): every commanderDecisionTicks, offset by its player so that decisions spread, each
  // commander reads its player's view within commanderWorkPerTick units and gives commands for the next step 1.
  const auto every = static_cast<std::uint32_t>(std::max(1, Data().Tuning().commanderDecisionTicks));
  for (const auto& [player, slot] : m_commanders)
  {
    if ((m_tick + player) % every != 0)
    {
      continue;
    }
    CommanderView view(*this, player, Data().Tuning().commanderWorkPerTick);
    std::vector<Protocol::Command> commands;
    slot.Get().Decide(view, commands);
    for (const Protocol::Command& command : commands)
    {
      Core::ByteWriter writer;
      Protocol::Encode(command, writer);
      m_commanderCommands.push_back({.player = player, .bytes = writer.TakeBytes()});
    }
  }
}

const StructureRule& World::Rule() const noexcept
{
  if (m_trial.has_value() && m_trial->version == Protocol::TrialVersion::Control)
  {
    return HealthStructureRule::Instance();
  }
  return VoxelStructureRule::Instance();
}

void World::VoxelCrater(const ShotHit& _hit, bool _keepStructures)
{
  // Its crater as edits to empty (M2-E2); a strike or a crater that touches a structure locks its repair (ADR-024,
  // decision 4).
  std::vector<Protocol::VoxelPosition> crater = CraterVoxels(
    _hit.voxel, _hit.behindX, _hit.behindZ, [this](Protocol::VoxelPosition _voxel) { return m_voxels.At(_voxel); }, Data().Materials(),
    Data().Tuning());
  for (const Protocol::VoxelPosition& voxel : crater)
  {
    if (!_keepStructures || StructureHolding(voxel) == Protocol::NO_ENTITY)
    {
      m_voxels.Edit(voxel, Protocol::Chunk::EMPTY);
    }
  }
  crater.push_back(_hit.voxel);
  LockRepairs(crater);
}

void World::VoxelRamBlow(Structure& _structure, Protocol::VoxelPosition _voxel)
{
  const std::optional<Box> box = BoxOf(_structure);
  if (box.has_value())
  {
    for (const Protocol::VoxelPosition& voxel : RamCrater(_voxel, *box))
    {
      m_voxels.Edit(voxel, Protocol::Chunk::EMPTY);
    }
  }
  // A blow on a structure locks its repair (ADR-024, decision 4).
  _structure.repairLockTicks = LockTicksFor(_structure);
}

void World::DigVoxel(Protocol::VoxelPosition _voxel)
{
  const Rules& rules = Data().Tuning();
  m_voxels.Edit(_voxel, Protocol::Chunk::EMPTY);
  m_openedColumns.insert((std::int64_t{_voxel.zVoxels} * WidthVoxels()) + _voxel.xVoxels);
  // A trench cell within repairLockReachVoxels of a footprint locks its repair (ADR-024, decision 4).
  for (auto& [structureId, structure] : m_structures)
  {
    const std::optional<Box> box = BoxOf(structure);
    if (box.has_value() &&
        std::max(Outside(_voxel.xVoxels, box->min.xVoxels, box->min.xVoxels + box->widthVoxels - 1),
                 Outside(_voxel.zVoxels, box->min.zVoxels, box->min.zVoxels + box->depthVoxels - 1)) <= rules.repairLockReachVoxels)
    {
      structure.repairLockTicks = LockTicksFor(structure);
    }
  }
}

void World::VoxelRepair(Structure& _structure)
{
  // The next voxel to put back, if any, in its own material, timber for timber and stone for the rest, from its owner's
  // stockpile; a player the match has no record of pays nothing, as for a shot.
  const std::optional<std::pair<Protocol::VoxelPosition, std::uint8_t>> next = NextRepair(_structure);
  if (!next.has_value())
  {
    return;
  }
  const bool timber = Data().Materials().at(next->second).behavior == Behavior::Timber;
  if (!CanPay(_structure.owner, timber))
  {
    return;
  }
  _structure.repairProgress += Data().Tuning().repairCubicMetersPerTick;
  if (_structure.repairProgress < Core::Fixed::FromRaw(Core::Fixed::ONE_RAW))
  {
    return;
  }
  _structure.repairProgress -= Core::Fixed::FromRaw(Core::Fixed::ONE_RAW);
  m_voxels.Edit(next->first, next->second);
  Pay(_structure.owner, timber, _structure.id, next->first);
}

bool World::TimberTemplate(const Structure& _structure) const
{
  const ServerTemplate* found = Data().FindTemplate(_structure.templateId);
  std::size_t timber = 0;
  std::size_t built = 0;
  if (found != nullptr)
  {
    for (const std::uint8_t index : found->indices)
    {
      built += index != Protocol::Chunk::EMPTY ? 1 : 0;
      timber += Data().Materials().at(index).behavior == Behavior::Timber ? 1 : 0;
    }
  }
  return 2 * timber > built;
}

bool World::CanPay(std::uint8_t _player, bool _timber) const
{
  constexpr std::int32_t CUBIC_METER_HUNDREDTHS = 100;
  const auto player = m_players.find(_player);
  return player == m_players.end() ||
         (_timber ? player->second.timberHundredthsCubicMeters : player->second.stoneHundredthsCubicMeters) >= CUBIC_METER_HUNDREDTHS;
}

void World::Pay(std::uint8_t _player, bool _timber, Protocol::EntityId _subject, Protocol::VoxelPosition _voxel)
{
  constexpr std::int32_t CUBIC_METER_HUNDREDTHS = 100;
  if (const auto player = m_players.find(_player); player != m_players.end())
  {
    (_timber ? player->second.timberHundredthsCubicMeters : player->second.stoneHundredthsCubicMeters) -= CUBIC_METER_HUNDREDTHS;
    NoteSpent(_timber, _player, _subject, CUBIC_METER_HUNDREDTHS, _voxel);
  }
}

void World::TouchStructure(Protocol::EntityId _structure)
{
  m_touchedStructures.insert(_structure);
}

void World::FallWhole(const Structure& _structure)
{
  const ServerTemplate* found = Data().FindTemplate(_structure.templateId);
  if (found == nullptr)
  {
    return;
  }
  Piece piece;
  std::size_t voxel = 0;
  for (std::uint32_t y = 0; y < found->heightVoxels; ++y)
  {
    for (std::uint32_t z = 0; z < found->depthVoxels; ++z)
    {
      for (std::uint32_t x = 0; x < found->widthVoxels; ++x, ++voxel)
      {
        const Protocol::VoxelPosition at = TemplateVoxel(_structure, *found, x, y, z);
        if (found->indices.at(voxel) != Protocol::Chunk::EMPTY && m_voxels.At(at) != Protocol::Chunk::EMPTY)
        {
          piece.push_back(at);
        }
      }
    }
  }
  if (piece.empty())
  {
    return;
  }
  const Protocol::EntityId id = NewEntityId();
  CollapsePlan plan = PlanCollapse(piece, piece, m_voxels, Data());
  m_collapses.emplace(id, Collapse{.id = id,
                                   .piece = std::move(piece),
                                   .plan = std::move(plan),
                                   .ticksElapsed = 0,
                                   .struck = {},
                                   .structure = _structure.id,
                                   .fallen = {}});
}

void World::SetWorking(Structure& _structure, bool _working)
{
  const bool working = _working;
  if (working != _structure.working)
  {
    _structure.working = working;
    // A gate that stops working shuts, for good until it is repaired above its threshold (ADR-024, decision 2).
    if (!working && _structure.gateOpen)
    {
      SwingGate(_structure, false, true);
    }
    // And its garrison is put out (decision 5).
    if (!working)
    {
      PutOut(_structure);
    }
    m_tickEvents.push_back({.kind = working ? TrialEventKind::StructureWorks : TrialEventKind::StructureStopped,
                            .tick = m_tick,
                            .player = _structure.owner,
                            .subject = _structure.id,
                            .other = Protocol::NO_ENTITY,
                            .amount = _structure.criticalPercent,
                            .voxel = _structure.originVoxels});
  }
}

void World::Garrison(std::uint8_t _player, const Protocol::GarrisonSquad& _garrison)
{
  // A squad of _player's, not an engine's body, into a building of theirs that works and has room for it (ADR-024,
  // decision 5): it walks to the footprint and goes in when its lead reaches it.
  Squad* squad = OrderableSquad(_player, _garrison, _garrison.squad);
  if (squad == nullptr)
  {
    return;
  }
  const auto structure = m_structures.find(_garrison.structure);
  const TemplateRules* rules = structure == m_structures.end() ? nullptr : Data().FindTemplateRules(structure->second.templateId);
  if (structure == m_structures.end() || rules == nullptr || rules->garrisonCapacity == 0 || EngineOfBody(_garrison.squad) != nullptr)
  {
    Refuse(_player, _garrison, Protocol::Refusal::Invalid);
    return;
  }
  if (structure->second.owner != _player)
  {
    Refuse(_player, _garrison, Protocol::Refusal::NotOwned);
    return;
  }
  if (!structure->second.working)
  {
    Refuse(_player, _garrison, Protocol::Refusal::NotWorking);
    return;
  }
  if (GarrisonedCount(_garrison.structure) + squad->units.size() > rules->garrisonCapacity)
  {
    Refuse(_player, _garrison, Protocol::Refusal::Full);
    return;
  }
  // A squad moving with a structure for its target is on its way to garrison it.
  const std::optional<Box> box = BoxOf(structure->second);
  squad->order = Protocol::SquadOrder::Move;
  squad->target = _garrison.structure;
  squad->hasGoal = true;
  squad->goalVoxels = box.has_value() ? Protocol::VoxelPosition{.xVoxels = box->min.xVoxels + (box->widthVoxels / 2),
                                                                .yVoxels = box->min.yVoxels,
                                                                .zVoxels = box->min.zVoxels + (box->depthVoxels / 2)}
                                      : structure->second.originVoxels;
  RequestSearch(*squad);
}

void World::Eject(std::uint8_t _player, const Protocol::EjectGarrison& _eject)
{
  const auto structure = m_structures.find(_eject.structure);
  if (structure == m_structures.end())
  {
    Refuse(_player, _eject, Protocol::Refusal::Invalid);
    return;
  }
  if (structure->second.owner != _player)
  {
    Refuse(_player, _eject, Protocol::Refusal::NotOwned);
    return;
  }
  PutOut(structure->second);
}

std::size_t World::GarrisonedCount(Protocol::EntityId _structure) const
{
  std::size_t count = 0;
  for (const auto& [id, squad] : m_squads)
  {
    count += squad.garrisonedIn == _structure ? squad.units.size() : 0;
  }
  return count;
}

void World::EnterGarrisons()
{
  std::vector<Protocol::EntityId> entering;
  for (auto& [id, squad] : m_squads)
  {
    if (squad.garrisonedIn != Protocol::NO_ENTITY || squad.order != Protocol::SquadOrder::Move || squad.units.empty() ||
        !m_structures.contains(squad.target) || Holding(id))
    {
      continue;
    }
    const Structure& structure = m_structures.at(squad.target);
    const Unit& lead = m_units.at(squad.units.front());
    const std::optional<Box> box = BoxOf(structure);
    if (!box.has_value() || lead.reservedCell >= 0)
    {
      continue;
    }
    const Protocol::VoxelPosition& at = lead.cellVoxels;
    const bool reached = at.xVoxels >= box->min.xVoxels - 1 && at.xVoxels <= box->min.xVoxels + box->widthVoxels &&
                         at.zVoxels >= box->min.zVoxels - 1 && at.zVoxels <= box->min.zVoxels + box->depthVoxels;
    if (!reached)
    {
      continue;
    }
    const TemplateRules* rules = Data().FindTemplateRules(structure.templateId);
    if (!structure.working || rules == nullptr || GarrisonedCount(structure.id) + squad.units.size() > rules->garrisonCapacity)
    {
      Halt(squad, Protocol::SquadOrder::Idle);
      continue;
    }
    entering.push_back(id);
  }
  for (const Protocol::EntityId id : entering)
  {
    Squad& squad = m_squads.at(id);
    const Protocol::EntityId building = squad.target;
    PlaceInGarrison(id, building);
    squad.order = Protocol::SquadOrder::Idle;
    squad.target = Protocol::NO_ENTITY;
    for (const Protocol::EntityId unit : squad.units)
    {
      m_units.at(unit).target = Protocol::NO_ENTITY;
    }
  }
}

std::optional<std::pair<std::int64_t, std::int64_t>> World::KeepCenterTwice(std::uint8_t _owner) const
{
  const auto center = [this](const Structure& _structure) -> std::optional<std::pair<std::int64_t, std::int64_t>>
  {
    const std::optional<Box> box = BoxOf(_structure);
    if (!box.has_value())
    {
      return std::nullopt;
    }
    return std::pair{(2 * std::int64_t{box->min.xVoxels}) + box->widthVoxels - 1,
                     (2 * std::int64_t{box->min.zVoxels}) + box->depthVoxels - 1};
  };
  if (m_trial.has_value() && m_trial->defender == _owner)
  {
    if (const auto keep = m_structures.find(m_trial->keep); keep != m_structures.end())
    {
      return center(keep->second);
    }
  }
  for (const auto& [id, structure] : m_structures)
  {
    if (structure.owner == _owner && structure.templateId == "T2")
    {
      return center(structure);
    }
  }
  return std::nullopt;
}

void World::PutOut(const Structure& _structure)
{
  const std::optional<Box> box = BoxOf(_structure);
  std::vector<Protocol::EntityId> squads;
  for (const auto& [id, squad] : m_squads)
  {
    if (squad.garrisonedIn == _structure.id)
    {
      squads.push_back(id);
    }
  }
  if (squads.empty() || !box.has_value())
  {
    return;
  }
  // The cells a falling piece will cover are no place to stand.
  std::set<std::pair<std::int32_t, std::int32_t>> strips;
  for (const auto& [id, collapse] : m_collapses)
  {
    for (const Protocol::VoxelPosition& cell : collapse.plan.strip)
    {
      strips.emplace(cell.xVoxels, cell.zVoxels);
    }
  }
  const std::optional<std::pair<std::int64_t, std::int64_t>> keep = KeepCenterTwice(_structure.owner);
  Occupancy occupancy = BuildOccupancy();
  // Rings about the box, nearest first, each in index order: the owner's side's cells first, then the rest.
  constexpr std::int32_t PUT_OUT_RINGS = 12;
  std::vector<Protocol::VoxelPosition> ownSide;
  std::vector<Protocol::VoxelPosition> otherSide;
  for (std::int32_t ring = 1; ring <= PUT_OUT_RINGS; ++ring)
  {
    for (std::int32_t z = box->min.zVoxels - ring; z < box->min.zVoxels + box->depthVoxels + ring; ++z)
    {
      for (std::int32_t x = box->min.xVoxels - ring; x < box->min.xVoxels + box->widthVoxels + ring; ++x)
      {
        const bool onRing = x == box->min.xVoxels - ring || x == box->min.xVoxels + box->widthVoxels + ring - 1 ||
                            z == box->min.zVoxels - ring || z == box->min.zVoxels + box->depthVoxels + ring - 1;
        if (!onRing || !m_navigation.Usable(x, z, NavigationClass::Foot) || strips.contains({x, z}))
        {
          continue;
        }
        const Protocol::VoxelPosition cell{.xVoxels = x, .yVoxels = m_navigation.WalkableHeight(x, z), .zVoxels = z};
        (OnOwnersSide(*box, keep, x, z) ? ownSide : otherSide).push_back(cell);
      }
    }
  }
  ownSide.insert(ownSide.end(), otherSide.begin(), otherSide.end());
  std::size_t next = 0;
  for (const Protocol::EntityId id : squads)
  {
    Squad& squad = m_squads.at(id);
    squad.garrisonedIn = Protocol::NO_ENTITY;
    squad.order = Protocol::SquadOrder::Idle;
    squad.target = Protocol::NO_ENTITY;
    for (const Protocol::EntityId unitId : squad.units)
    {
      Unit& unit = m_units.at(unitId);
      while (next < ownSide.size() && !Free(occupancy, unit, CellOf(ownSide[next])))
      {
        ++next;
      }
      // With no cell left, it stays at the building's origin, as it stood inside.
      if (next < ownSide.size())
      {
        unit.cellVoxels = ownSide[next++];
      }
      unit.position = CellFloorMiddle(unit.cellVoxels);
      unit.stepFromCell = CellOf(unit.cellVoxels);
      unit.reservedCell = NO_CELL;
      unit.trailStep = OFF_TRAIL;
      unit.stepEffort = 0;
      for (const PathSearch::CellIndex cell : Footprint(unit, CellOf(unit.cellVoxels)))
      {
        occupancy.occupied.emplace(cell, unitId);
      }
    }
  }
}

Protocol::VoxelPosition World::LoosingCell(const Box& _box, Protocol::VoxelPosition _targetCellVoxels, std::int32_t _standVoxels) const
{
  const auto outside = [](std::int32_t _target, std::int32_t _low, std::int32_t _extent)
  {
    if (_target < _low)
    {
      return _low - 1;
    }
    if (_target >= _low + _extent)
    {
      return _low + _extent;
    }
    return _target;
  };
  return {.xVoxels = outside(_targetCellVoxels.xVoxels, _box.min.xVoxels, _box.widthVoxels),
          .yVoxels = _box.min.yVoxels + _standVoxels,
          .zVoxels = outside(_targetCellVoxels.zVoxels, _box.min.zVoxels, _box.depthVoxels)};
}

void World::LooseFromGarrisons()
{
  // Each ranged figure in a building looses, when its reload is due, at the nearest enemy figure it reaches from the cell
  // outside the building's face nearest that enemy, at the stand height, its elevation counted from there, with nothing
  // solid on the line; ties to the lower id (ADR-024, decision 5; ADR-023, decision 5).
  const Rules& rules = Data().Tuning();
  for (auto& [id, unit] : m_units)
  {
    const UnitStats& stats = Data().Stats(unit.kind);
    if (!Garrisoned(unit) || stats.attack <= 0 || stats.rangeVoxels <= 0 || unit.reloadTicks > 0)
    {
      continue;
    }
    const Structure& building = m_structures.at(m_squads.at(unit.squad).garrisonedIn);
    const TemplateRules* templateRules = Data().FindTemplateRules(building.templateId);
    const std::optional<Box> box = BoxOf(building);
    if (templateRules == nullptr || !box.has_value())
    {
      continue;
    }
    Protocol::EntityId nearest = Protocol::NO_ENTITY;
    std::int64_t best = 0;
    Protocol::VoxelPosition from{};
    // Its squad's target's figures first, when it reaches one; else the nearest enemy it reaches (M4-F2).
    const Protocol::EntityId aimed = m_squads.at(unit.squad).target;
    const bool aimedInReach =
      aimed != Protocol::NO_ENTITY &&
      std::ranges::any_of(m_units,
                          [&](const auto& _entry)
                          {
                            const Unit& other = _entry.second;
                            if (other.squad != aimed || Garrisoned(other))
                            {
                              return false;
                            }
                            const Protocol::VoxelPosition loosing = LoosingCell(*box, other.cellVoxels, templateRules->standHeightVoxels);
                            const std::int64_t dx = std::int64_t{other.cellVoxels.xVoxels} - loosing.xVoxels;
                            const std::int64_t dz = std::int64_t{other.cellVoxels.zVoxels} - loosing.zVoxels;
                            const std::int64_t reach =
                              ArrowRangeVoxels(stats.rangeVoxels, ElevationSteps(loosing.yVoxels, other.cellVoxels.yVoxels, rules), rules);
                            return (dx * dx) + (dz * dz) <= reach * reach &&
                                   !FirstSolid(AimArrow(loosing, other.cellVoxels, rules).start,
                                               AimArrow(loosing, other.cellVoxels, rules).end, m_voxels)
                                      .has_value();
                          });
    for (const auto& [otherId, other] : m_units)
    {
      if (other.owner == unit.owner || Garrisoned(other) || (aimedInReach && other.squad != aimed))
      {
        continue;
      }
      const Protocol::VoxelPosition loosing = LoosingCell(*box, other.cellVoxels, templateRules->standHeightVoxels);
      const std::int64_t dx = std::int64_t{other.cellVoxels.xVoxels} - loosing.xVoxels;
      const std::int64_t dz = std::int64_t{other.cellVoxels.zVoxels} - loosing.zVoxels;
      const std::int64_t distance = (dx * dx) + (dz * dz);
      const std::int64_t reach =
        ArrowRangeVoxels(stats.rangeVoxels, ElevationSteps(loosing.yVoxels, other.cellVoxels.yVoxels, rules), rules);
      if (distance > reach * reach || (nearest != Protocol::NO_ENTITY && distance >= best))
      {
        continue;
      }
      const ArrowLine line = AimArrow(loosing, other.cellVoxels, rules);
      if (!FirstSolid(line.start, line.end, m_voxels).has_value())
      {
        nearest = otherId;
        best = distance;
        from = loosing;
      }
    }
    if (nearest == Protocol::NO_ENTITY)
    {
      continue;
    }
    const Unit& foe = m_units.at(nearest);
    const Protocol::EntityId arrow = NewEntityId();
    m_shots.emplace(arrow,
                    Shot{.id = arrow,
                         .engine = unit.squad,
                         .owner = unit.owner,
                         .flight = AimArrow(from, foe.cellVoxels, rules),
                         .ticksFlown = 0,
                         .kind = Protocol::ShotKind::Arrow,
                         .shooter = id,
                         .target = nearest,
                         .damage = Damage(stats, Data().Stats(foe.kind), ElevationSteps(from.yVoxels, foe.cellVoxels.yVoxels, rules))});
    unit.reloadTicks = stats.reloadTicks;
  }
}

void World::HealGarrisons()
{
  const std::int32_t every = Data().Tuning().garrisonHealTicksPerHitPoint;
  if (every <= 0 || m_tick % static_cast<std::uint32_t>(every) != 0)
  {
    return;
  }
  for (auto& [id, unit] : m_units)
  {
    if (Garrisoned(unit))
    {
      unit.hitPoints = std::min(unit.hitPoints + 1, Data().Stats(unit.kind).hitPoints);
    }
  }
}

bool World::HasGate(const Structure& _structure) const
{
  const ServerTemplate* found = Data().FindTemplate(_structure.templateId);
  return found != nullptr && found->criticalPart == "Gate";
}

std::vector<Protocol::VoxelPosition> World::GateVoxels(const Structure& _structure) const
{
  std::vector<Protocol::VoxelPosition> voxels;
  const ServerTemplate* found = Data().FindTemplate(_structure.templateId);
  if (found == nullptr || !HasGate(_structure))
  {
    return voxels;
  }
  std::size_t voxel = 0;
  for (std::uint32_t y = 0; y < found->heightVoxels; ++y)
  {
    for (std::uint32_t z = 0; z < found->depthVoxels; ++z)
    {
      for (std::uint32_t x = 0; x < found->widthVoxels; ++x, ++voxel)
      {
        if (found->parts.at(voxel) == ServerTemplate::Part::Critical && found->indices.at(voxel) != Protocol::Chunk::EMPTY)
        {
          voxels.push_back(TemplateVoxel(_structure, *found, x, y, z));
        }
      }
    }
  }
  return voxels;
}

void World::SetGate(std::uint8_t _player, const Protocol::SetGate& _gate)
{
  // Its owner opens or shuts a gate that works; one that does not stays shut (ADR-024, decision 6).
  const auto structure = m_structures.find(_gate.structure);
  if (structure == m_structures.end() || !HasGate(structure->second))
  {
    Refuse(_player, _gate, Protocol::Refusal::Invalid);
    return;
  }
  if (structure->second.owner != _player)
  {
    Refuse(_player, _gate, Protocol::Refusal::NotOwned);
    return;
  }
  if (_gate.open && !structure->second.working)
  {
    Refuse(_player, _gate, Protocol::Refusal::NotWorking);
    return;
  }
  SwingGate(structure->second, _gate.open, false);
}

void World::SwingGate(Structure& _structure, bool _open, bool _now)
{
  if (_structure.gateOpen == _open)
  {
    return;
  }
  _structure.gateOpen = _open;
  // Every open gate's voxels are air to navigation, for every unit (MvpPlan.md, M4, input 13); shots and craters still
  // meet them, since the world holds them.
  std::vector<Protocol::VoxelPosition> air;
  for (const auto& [id, structure] : m_structures)
  {
    if (structure.gateOpen)
    {
      const std::vector<Protocol::VoxelPosition> gate = GateVoxels(structure);
      air.insert(air.end(), gate.begin(), gate.end());
    }
  }
  m_navigation.SetAir(air);
  const std::vector<Protocol::VoxelPosition> swung = GateVoxels(_structure);
  if (_now)
  {
    RebuildTiles(m_navigation.TilesTouchedBy(swung));
  }
  else
  {
    m_gateTouches.insert(m_gateTouches.end(), swung.begin(), swung.end());
  }
}

std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> World::PassageVoxels(const Structure& _structure) const
{
  std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> passage;
  const ServerTemplate* found = Data().FindTemplate(_structure.templateId);
  if (found == nullptr || !HasGate(_structure))
  {
    return passage;
  }
  const auto width = static_cast<std::int32_t>(found->widthVoxels);
  const auto height = static_cast<std::int32_t>(found->heightVoxels);
  const auto depth = static_cast<std::int32_t>(found->depthVoxels);
  // Called only inside the template's bounds, so no coordinate is negative.
  const std::size_t rowVoxels = found->widthVoxels;
  const std::size_t layerRows = found->depthVoxels;
  const auto index = [rowVoxels, layerRows](std::int32_t _x, std::int32_t _y, std::int32_t _z)
  { return static_cast<std::size_t>(_x) + (rowVoxels * (static_cast<std::size_t>(_z) + (layerRows * static_cast<std::size_t>(_y)))); };
  const auto inside = [width, height, depth](std::int32_t _x, std::int32_t _y, std::int32_t _z)
  { return _x >= 0 && _x < width && _y >= 0 && _y < height && _z >= 0 && _z < depth; };
  constexpr std::array<std::array<std::int32_t, 3>, 6> FACES = {{{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};
  // A fill through the template's empty voxels, face to face, from the gate's.
  std::vector<std::uint8_t> seen(found->indices.size(), 0);
  std::vector<std::array<std::int32_t, 3>> open;
  const auto reach = [&](std::int32_t _x, std::int32_t _y, std::int32_t _z)
  {
    if (inside(_x, _y, _z) && seen.at(index(_x, _y, _z)) == 0 && found->indices.at(index(_x, _y, _z)) == Protocol::Chunk::EMPTY)
    {
      seen.at(index(_x, _y, _z)) = 1;
      open.push_back({_x, _y, _z});
    }
  };
  for (std::int32_t y = 0; y < height; ++y)
  {
    for (std::int32_t z = 0; z < depth; ++z)
    {
      for (std::int32_t x = 0; x < width; ++x)
      {
        if (found->parts.at(index(x, y, z)) == ServerTemplate::Part::Critical &&
            found->indices.at(index(x, y, z)) != Protocol::Chunk::EMPTY)
        {
          for (const auto& [dx, dy, dz] : FACES)
          {
            reach(x + dx, y + dy, z + dz);
          }
        }
      }
    }
  }
  while (!open.empty())
  {
    const auto [x, y, z] = open.back();
    open.pop_back();
    const Protocol::VoxelPosition at =
      TemplateVoxel(_structure, *found, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), static_cast<std::uint32_t>(z));
    passage.emplace(at.xVoxels, at.yVoxels, at.zVoxels);
    for (const auto& [dx, dy, dz] : FACES)
    {
      reach(x + dx, y + dy, z + dz);
    }
  }
  return passage;
}

void World::PassageAttacks()
{
  // While a gatehouse works and holds at least one garrisoned figure, every passageAttackTicks it deals
  // passageAttackDamage of crush to every unit standing in its passage, the ram included, whose roof is pierce armor and
  // does not stop it (ADR-024, decision 6; MvpPlan.md, M4, input 13). A unit is in the passage when a cell of its
  // footprint is, at the height it stands at.
  const Rules& rules = Data().Tuning();
  if (rules.passageAttackTicks <= 0 || m_tick % static_cast<std::uint32_t>(rules.passageAttackTicks) != 0)
  {
    return;
  }
  const std::int32_t machineSide = std::max(1, rules.machinesWidthVoxels);
  for (const auto& [id, structure] : m_structures)
  {
    if (!structure.working || GarrisonedCount(id) == 0)
    {
      continue;
    }
    const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> passage = PassageVoxels(structure);
    for (auto& [unitId, unit] : m_units)
    {
      if (passage.empty() || Garrisoned(unit))
      {
        continue;
      }
      const std::int32_t side = unit.navigationClass == NavigationClass::Machines ? machineSide : 1;
      bool inPassage = false;
      for (std::int32_t dz = 0; dz < side && !inPassage; ++dz)
      {
        for (std::int32_t dx = 0; dx < side && !inPassage; ++dx)
        {
          inPassage = passage.contains({unit.cellVoxels.xVoxels + dx, unit.cellVoxels.yVoxels, unit.cellVoxels.zVoxels + dz});
        }
      }
      if (inPassage)
      {
        Crush(unit, rules.passageAttackDamage);
      }
    }
  }
}

bool World::OnOwnersSide(const Box& _box, const std::optional<std::pair<std::int64_t, std::int64_t>>& _keepTwice, std::int32_t _x,
                         std::int32_t _z) noexcept
{
  // The side of the footprint's centre line nearer the keep (ADR-024, decisions 3 and 5; ADR-026, decision 5): the line
  // runs along its longer side, so a wall's is the line between its faces; a square footprint's runs across the keep's
  // larger offset. In doubled coordinates, so that the centre is whole. Every side of the keep itself is its owner's.
  const std::int64_t centerX = (2 * std::int64_t{_box.min.xVoxels}) + _box.widthVoxels - 1;
  const std::int64_t centerZ = (2 * std::int64_t{_box.min.zVoxels}) + _box.depthVoxels - 1;
  if (!_keepTwice.has_value() || (_keepTwice->first == centerX && _keepTwice->second == centerZ))
  {
    return true;
  }
  const std::int64_t keepX = _keepTwice->first - centerX;
  const std::int64_t keepZ = _keepTwice->second - centerZ;
  const bool alongX = _box.widthVoxels != _box.depthVoxels ? _box.widthVoxels > _box.depthVoxels : std::abs(keepZ) >= std::abs(keepX);
  return alongX ? (((2 * std::int64_t{_z}) - centerZ) * keepZ) > 0 : (((2 * std::int64_t{_x}) - centerX) * keepX) > 0;
}

void World::OrderRepair(std::uint8_t _player, const Protocol::RepairStructure& _repair)
{
  // A laborer of _player's, a squad of one, at a structure of theirs (ADR-024, decision 3; MvpPlan.md, M4, input 11).
  Squad* squad = OrderableSquad(_player, _repair, _repair.laborer);
  if (squad == nullptr)
  {
    return;
  }
  const auto structure = m_structures.find(_repair.structure);
  if (squad->kind != Protocol::UnitKind::Laborer || squad->units.empty() || structure == m_structures.end())
  {
    Refuse(_player, _repair, Protocol::Refusal::Invalid);
    return;
  }
  if (structure->second.owner != _player)
  {
    Refuse(_player, _repair, Protocol::Refusal::NotOwned);
    return;
  }
  const std::optional<Box> box = BoxOf(structure->second);
  if (!box.has_value())
  {
    Refuse(_player, _repair, Protocol::Refusal::Invalid);
    return;
  }
  // It walks to the cell nearest it that it may repair from and foot can use, ring by ring about the footprint, each ring
  // in index order, ties to the first; with none, the order is refused.
  const Protocol::VoxelPosition at = m_units.at(squad->units.front()).cellVoxels;
  const std::int32_t reach = std::max(1, Data().Tuning().repairReachVoxels);
  const std::optional<std::pair<std::int64_t, std::int64_t>> keep = KeepCenterTwice(_player);
  std::optional<Protocol::VoxelPosition> stand;
  std::int64_t best = 0;
  for (std::int32_t ring = 1; ring <= reach; ++ring)
  {
    for (std::int32_t z = box->min.zVoxels - ring; z < box->min.zVoxels + box->depthVoxels + ring; ++z)
    {
      for (std::int32_t x = box->min.xVoxels - ring; x < box->min.xVoxels + box->widthVoxels + ring; ++x)
      {
        const bool onRing = x == box->min.xVoxels - ring || x == box->min.xVoxels + box->widthVoxels + ring - 1 ||
                            z == box->min.zVoxels - ring || z == box->min.zVoxels + box->depthVoxels + ring - 1;
        if (!onRing || !m_navigation.Usable(x, z, NavigationClass::Foot) || !OnOwnersSide(*box, keep, x, z))
        {
          continue;
        }
        const std::int64_t dx = std::int64_t{x} - at.xVoxels;
        const std::int64_t dz = std::int64_t{z} - at.zVoxels;
        const std::int64_t distance = (dx * dx) + (dz * dz);
        if (!stand.has_value() || distance < best)
        {
          stand = Protocol::VoxelPosition{.xVoxels = x, .yVoxels = m_navigation.WalkableHeight(x, z), .zVoxels = z};
          best = distance;
        }
      }
    }
  }
  if (!stand.has_value())
  {
    Refuse(_player, _repair, Protocol::Refusal::Invalid);
    return;
  }
  squad->order = Protocol::SquadOrder::Repair;
  squad->target = _repair.structure;
  squad->hasGoal = true;
  squad->goalVoxels = *stand;
  RequestSearch(*squad);
}

bool World::AtRepairStand(const Structure& _structure, const Box& _box, Protocol::VoxelPosition _cell) const
{
  const auto outside = [](std::int32_t _at, std::int32_t _low, std::int32_t _extent)
  { return std::max({_low - _at, 0, _at - (_low + _extent - 1)}); };
  const std::int32_t distance =
    std::max(outside(_cell.xVoxels, _box.min.xVoxels, _box.widthVoxels), outside(_cell.zVoxels, _box.min.zVoxels, _box.depthVoxels));
  return distance >= 1 && distance <= Data().Tuning().repairReachVoxels &&
         OnOwnersSide(_box, KeepCenterTwice(_structure.owner), _cell.xVoxels, _cell.zVoxels);
}

std::optional<std::pair<Protocol::VoxelPosition, std::uint8_t>> World::NextRepair(const Structure& _structure) const
{
  const ServerTemplate* found = Data().FindTemplate(_structure.templateId);
  const std::optional<Box> box = BoxOf(_structure);
  if (found == nullptr || !box.has_value())
  {
    return std::nullopt;
  }
  // A cell a unit stands in, up to its headroom, takes nothing back: it would bury the unit.
  const auto occupied = [this](Protocol::VoxelPosition _at)
  {
    return std::ranges::any_of(m_units,
                               [this, _at](const auto& _entry)
                               {
                                 const Unit& unit = _entry.second;
                                 const NavigationGrid::ClassRule& rule = m_navigation.Class(unit.navigationClass);
                                 const std::int32_t side = std::max(1, rule.widthVoxels);
                                 return !Garrisoned(unit) && _at.xVoxels >= unit.cellVoxels.xVoxels &&
                                        _at.xVoxels < unit.cellVoxels.xVoxels + side && _at.zVoxels >= unit.cellVoxels.zVoxels &&
                                        _at.zVoxels < unit.cellVoxels.zVoxels + side && _at.yVoxels >= unit.cellVoxels.yVoxels &&
                                        _at.yVoxels < unit.cellVoxels.yVoxels + std::max(1, rule.headroomVoxels);
                               });
  };
  const auto solid = [this](std::int32_t _x, std::int32_t _y, std::int32_t _z)
  { return m_voxels.At({.xVoxels = _x, .yVoxels = _y, .zVoxels = _z}) != Protocol::Chunk::EMPTY; };
  const std::int64_t centerX = (2 * std::int64_t{box->min.xVoxels}) + box->widthVoxels - 1;
  const std::int64_t centerZ = (2 * std::int64_t{box->min.zVoxels}) + box->depthVoxels - 1;
  const std::size_t layerVoxels = std::size_t{found->widthVoxels} * found->depthVoxels;
  std::optional<std::pair<Protocol::VoxelPosition, std::uint8_t>> best;
  std::tuple<std::uint32_t, std::int64_t, std::uint32_t, std::uint32_t> bestKey{};
  std::size_t voxel = 0;
  for (std::uint32_t y = 0; y < found->heightVoxels; ++y)
  {
    for (std::uint32_t z = 0; z < found->depthVoxels; ++z)
    {
      for (std::uint32_t x = 0; x < found->widthVoxels; ++x, ++voxel)
      {
        const std::uint8_t index = found->indices.at(voxel);
        const std::size_t byte = voxel / 8;
        const bool built = byte < _structure.builtMask.size() && ((_structure.builtMask[byte] >> (voxel % 8)) & 1U) != 0;
        if (index == Protocol::Chunk::EMPTY || !built)
        {
          continue;
        }
        const Protocol::VoxelPosition at = TemplateVoxel(_structure, *found, x, y, z);
        if (solid(at.xVoxels, at.yVoxels, at.zVoxels) || occupied(at))
        {
          continue;
        }
        // It stands on what is below it, or, where the template has nothing below it, an arch's or a lintel's underside,
        // beside what holds it across the layer; the support search still judges it once it is back (ADR-004).
        bool supported = solid(at.xVoxels, at.yVoxels - 1, at.zVoxels);
        if (!supported && y > 0 && found->indices.at(voxel - layerVoxels) == Protocol::Chunk::EMPTY)
        {
          supported = solid(at.xVoxels - 1, at.yVoxels, at.zVoxels) || solid(at.xVoxels + 1, at.yVoxels, at.zVoxels) ||
                      solid(at.xVoxels, at.yVoxels, at.zVoxels - 1) || solid(at.xVoxels, at.yVoxels, at.zVoxels + 1);
        }
        if (!supported)
        {
          continue;
        }
        // Lowest first, then nearest the footprint's centre; ties a column through the template's depth at a time, along
        // its x, each from its front, a wall's outer face, so that a crater a shot cut through it goes back whole before
        // the next is begun, and one shot can take it again (ADR-024, decision 3, as amended by M4-D5).
        const std::int64_t dx = (2 * std::int64_t{at.xVoxels}) - centerX;
        const std::int64_t dz = (2 * std::int64_t{at.zVoxels}) - centerZ;
        const std::tuple<std::uint32_t, std::int64_t, std::uint32_t, std::uint32_t> key{y, (dx * dx) + (dz * dz), x,
                                                                                        found->depthVoxels - 1 - z};
        if (!best.has_value() || key < bestKey)
        {
          best = std::pair{at, index};
          bestKey = key;
        }
      }
    }
  }
  return best;
}

void World::AdvanceRepairs()
{
  // ADR-024, decisions 3 and 4; MvpPlan.md, M4, input 11. A laborer set to repair a structure that is gone turns idle.
  for (auto& [id, squad] : m_squads)
  {
    if (squad.order == Protocol::SquadOrder::Repair && !m_structures.contains(squad.target))
    {
      Halt(squad, Protocol::SquadOrder::Idle);
    }
  }
  for (auto& [id, structure] : m_structures)
  {
    if (structure.repairLockTicks > 0)
    {
      --structure.repairLockTicks;
      continue;
    }
    const std::optional<Box> box = BoxOf(structure);
    if (!box.has_value())
    {
      continue;
    }
    // However many laborers are at it, one is enough, and the rate is the section's.
    const Box& area = *box;
    const bool tended = std::ranges::any_of(m_squads,
                                            [this, &structure, &area](const auto& _entry)
                                            {
                                              const Squad& squad = _entry.second;
                                              return squad.owner == structure.owner && squad.kind == Protocol::UnitKind::Laborer &&
                                                     squad.order == Protocol::SquadOrder::Repair && squad.target == structure.id &&
                                                     !squad.units.empty() &&
                                                     AtRepairStand(structure, area, m_units.at(squad.units.front()).cellVoxels);
                                            });
    if (tended)
    {
      Rule().Repair(*this, structure);
    }
  }
}

void World::LockRepairs(std::span<const Protocol::VoxelPosition> _voxels)
{
  for (auto& [id, structure] : m_structures)
  {
    const std::optional<Box> box = BoxOf(structure);
    if (!box.has_value())
    {
      continue;
    }
    const Box& area = *box;
    const auto inside = [&area](const Protocol::VoxelPosition& _voxel)
    {
      return _voxel.xVoxels >= area.min.xVoxels && _voxel.xVoxels < area.min.xVoxels + area.widthVoxels &&
             _voxel.yVoxels >= area.min.yVoxels && _voxel.yVoxels < area.min.yVoxels + area.heightVoxels &&
             _voxel.zVoxels >= area.min.zVoxels && _voxel.zVoxels < area.min.zVoxels + area.depthVoxels;
    };
    if (std::ranges::any_of(_voxels, inside))
    {
      structure.repairLockTicks = LockTicksFor(structure);
    }
  }
}

std::uint32_t World::LockTicksFor(const Structure& _structure) const
{
  const ServerTemplate* found = Data().FindTemplate(_structure.templateId);
  const Rules& rules = Data().Tuning();
  return static_cast<std::uint32_t>(
    std::max(0, found != nullptr && !found->criticalPart.empty() ? rules.repairLockBuildingTicks : rules.repairLockTicks));
}

void World::NoteSpent(bool _timber, std::uint8_t _player, Protocol::EntityId _subject, std::int32_t _amount, Protocol::VoxelPosition _voxel)
{
  m_tickEvents.push_back({.kind = _timber ? TrialEventKind::TimberSpent : TrialEventKind::StoneSpent,
                          .tick = m_tick,
                          .player = _player,
                          .subject = _subject,
                          .other = Protocol::NO_ENTITY,
                          .amount = _amount,
                          .voxel = _voxel});
}

void World::DigTrench(std::uint8_t _player, const Protocol::DigTrench& _trench)
{
  // A squad of _player's sappers, at a box inside the world (ADR-029, decision 1; MvpPlan.md, M4, input 12).
  Squad* squad = OrderableSquad(_player, _trench, _trench.squad);
  if (squad == nullptr)
  {
    return;
  }
  const Protocol::VoxelPosition& low = _trench.minVoxels;
  const Protocol::VoxelPosition& high = _trench.maxVoxels;
  const bool inWorld = low.xVoxels >= 0 && low.yVoxels >= 0 && low.zVoxels >= 0 && high.xVoxels < WidthVoxels() &&
                       high.yVoxels < HeightVoxels() && high.zVoxels < DepthVoxels();
  if (squad->kind != Protocol::UnitKind::Sapper || squad->units.empty() || low.xVoxels > high.xVoxels || low.yVoxels > high.yVoxels ||
      low.zVoxels > high.zVoxels || !inWorld)
  {
    Refuse(_player, _trench, Protocol::Refusal::Invalid);
    return;
  }
  // Its ramp is left at the end of the box's long axis nearer the squad's lead, x for a square box, the lower end on a tie.
  const Protocol::VoxelPosition at = m_units.at(squad->units.front()).cellVoxels;
  const bool alongX = high.xVoxels - low.xVoxels >= high.zVoxels - low.zVoxels;
  const std::uint8_t nearSide = alongX ? (std::abs(at.xVoxels - low.xVoxels) <= std::abs(at.xVoxels - high.xVoxels) ? 0 : 1)
                                       : (std::abs(at.zVoxels - low.zVoxels) <= std::abs(at.zVoxels - high.zVoxels) ? 2 : 3);
  // A lead within reach digs where it stands; one farther walks to the nearest cell foot can use beside the box, outside
  // it, ring by ring, each ring in index order; with none, the order is refused.
  std::optional<Protocol::VoxelPosition> stand;
  if (!WithinTrenchReach(low, high, at))
  {
    std::int64_t best = 0;
    for (std::int32_t ring = 1; ring <= TRENCH_REACH_CELLS && !stand.has_value(); ++ring)
    {
      for (std::int32_t z = low.zVoxels - ring; z <= high.zVoxels + ring; ++z)
      {
        for (std::int32_t x = low.xVoxels - ring; x <= high.xVoxels + ring; ++x)
        {
          const bool onRing = x == low.xVoxels - ring || x == high.xVoxels + ring || z == low.zVoxels - ring || z == high.zVoxels + ring;
          if (!onRing || !m_navigation.Usable(x, z, NavigationClass::Foot))
          {
            continue;
          }
          const std::int64_t dx = std::int64_t{x} - at.xVoxels;
          const std::int64_t dz = std::int64_t{z} - at.zVoxels;
          if (!stand.has_value() || (dx * dx) + (dz * dz) < best)
          {
            stand = Protocol::VoxelPosition{.xVoxels = x, .yVoxels = m_navigation.WalkableHeight(x, z), .zVoxels = z};
            best = (dx * dx) + (dz * dz);
          }
        }
      }
    }
    if (!stand.has_value())
    {
      Refuse(_player, _trench, Protocol::Refusal::Invalid);
      return;
    }
  }
  Halt(*squad, Protocol::SquadOrder::Trench);
  squad->trenchMinVoxels = low;
  squad->trenchMaxVoxels = high;
  squad->trenchNearSide = nearSide;
  squad->digProgress = {};
  if (stand.has_value())
  {
    squad->hasGoal = true;
    squad->goalVoxels = *stand;
    RequestSearch(*squad);
  }
}

bool World::AtTrench(const Squad& _squad, Protocol::VoxelPosition _cell) noexcept
{
  return WithinTrenchReach(_squad.trenchMinVoxels, _squad.trenchMaxVoxels, _cell);
}

std::optional<Protocol::VoxelPosition> World::NextDig(const Squad& _squad) const
{
  // Layer k of the box, counted from its top, is dug only in the columns k or more from the ramp's end along its long axis,
  // so the floor at that end rises a voxel a cell, as foot climb (GameDesign.md, M2's values, item 10).
  const MaterialRules& materials = Data().Materials();
  const Protocol::VoxelPosition& low = _squad.trenchMinVoxels;
  const Protocol::VoxelPosition& high = _squad.trenchMaxVoxels;
  const auto fromRamp = [&_squad, &low, &high](std::int32_t _x, std::int32_t _z)
  {
    switch (_squad.trenchNearSide)
    {
    case 0:
      return _x - low.xVoxels;
    case 1:
      return high.xVoxels - _x;
    case 2:
      return _z - low.zVoxels;
    default:
      return high.zVoxels - _z;
    }
  };
  for (std::int32_t y = high.yVoxels; y >= low.yVoxels; --y)
  {
    std::optional<Protocol::VoxelPosition> best;
    std::int32_t bestFromRamp = -1;
    for (std::int32_t z = low.zVoxels; z <= high.zVoxels; ++z)
    {
      for (std::int32_t x = low.xVoxels; x <= high.xVoxels; ++x)
      {
        const Protocol::VoxelPosition voxel{.xVoxels = x, .yVoxels = y, .zVoxels = z};
        const std::int32_t ramp = fromRamp(x, z);
        // Open digging, never tunnelling: nothing stands on it (GameDescription.md, Undermining).
        if (high.yVoxels - y > ramp || !IsLooseIndex(materials, m_voxels.At(voxel)) ||
            m_voxels.At({.xVoxels = x, .yVoxels = y + 1, .zVoxels = z}) != Protocol::Chunk::EMPTY || ramp <= bestFromRamp)
        {
          continue;
        }
        best = voxel;
        bestFromRamp = ramp;
      }
    }
    if (best.has_value())
    {
      return best;
    }
  }
  return std::nullopt;
}

void World::AdvanceTrenches()
{
  // ADR-029, decision 2; MvpPlan.md, M4, input 12. Each sapper at the box adds its kind's rate; the spoil leaves play.
  for (auto& [id, squad] : m_squads)
  {
    if (squad.order != Protocol::SquadOrder::Trench || squad.garrisonedIn != Protocol::NO_ENTITY)
    {
      continue;
    }
    Core::Fixed rate{};
    for (const Protocol::EntityId unitId : squad.units)
    {
      const Unit& unit = m_units.at(unitId);
      if (AtTrench(squad, unit.cellVoxels))
      {
        rate += Data().Stats(unit.kind).digCubicMetersPerTick;
      }
    }
    std::optional<Protocol::VoxelPosition> next = rate.Raw() > 0 ? NextDig(squad) : std::nullopt;
    if (!next.has_value())
    {
      continue;
    }
    squad.digProgress += rate;
    while (next.has_value() && squad.digProgress >= Core::Fixed::FromRaw(Core::Fixed::ONE_RAW))
    {
      squad.digProgress -= Core::Fixed::FromRaw(Core::Fixed::ONE_RAW);
      Rule().TrenchDug(*this, *next);
      next = NextDig(squad);
    }
  }
}

bool World::Opened(std::int32_t _x, std::int32_t _z) const
{
  return m_openedColumns.contains((std::int64_t{_z} * WidthVoxels()) + _x);
}

std::int32_t World::LooseHeight(std::int32_t _x, std::int32_t _z, bool* _under) const
{
  const MaterialRules& materials = Data().Materials();
  const auto at = [this, _x, _z](std::int32_t _y) { return m_voxels.At({.xVoxels = _x, .yVoxels = _y, .zVoxels = _z}); };
  std::int32_t y = TopOf(_x, _z);
  *_under = y >= 0 && IsStructureIndex(materials, at(y));
  if (!*_under)
  {
    return y;
  }
  while (y >= 0 && IsStructureIndex(materials, at(y)))
  {
    --y;
  }
  if (y < 0 || at(y) != Protocol::Chunk::EMPTY)
  {
    return -1;
  }
  while (y >= 0 && at(y) == Protocol::Chunk::EMPTY)
  {
    --y;
  }
  return y;
}

std::optional<std::pair<std::int32_t, std::int32_t>> World::SlumpFromUnder(std::int32_t _x, std::int32_t _z)
{
  constexpr std::uint8_t EARTH = 2; // a grass voxel that moves lands as earth, as in Slump
  constexpr std::uint8_t GRASS = 1;
  const MaterialRules& materials = Data().Materials();
  const auto at = [this](std::int32_t _atX, std::int32_t _atY, std::int32_t _atZ)
  { return m_voxels.At({.xVoxels = _atX, .yVoxels = _atY, .zVoxels = _atZ}); };
  // The loose voxel the structure rests on, or the one below the gap under it.
  std::int32_t from = TopOf(_x, _z);
  while (from >= 0 && IsStructureIndex(materials, at(_x, from, _z)))
  {
    --from;
  }
  while (from >= 0 && at(_x, from, _z) == Protocol::Chunk::EMPTY)
  {
    --from;
  }
  if (from < 0 || !IsLooseIndex(materials, at(_x, from, _z)))
  {
    return std::nullopt;
  }
  constexpr std::array<std::array<std::int32_t, 2>, 4> SIDES = {{{0, 1}, {1, 0}, {0, -1}, {-1, 0}}};
  std::optional<std::pair<std::int32_t, std::int32_t>> to;
  std::int32_t lowest = 0;
  for (const auto& [dx, dz] : SIDES)
  {
    const std::int32_t sideX = _x + dx;
    const std::int32_t sideZ = _z + dz;
    if (sideX < 0 || sideX >= WidthVoxels() || sideZ < 0 || sideZ >= DepthVoxels() || !Opened(sideX, sideZ))
    {
      continue;
    }
    bool under = false;
    const std::int32_t height = LooseHeight(sideX, sideZ, &under);
    const bool lands = !(under && height < 0) && (height < 0 || !IsStructureIndex(materials, at(sideX, height, sideZ)));
    if (lands && height < from - 1 && (!to.has_value() || height < lowest))
    {
      to = std::pair{sideX, sideZ};
      lowest = height;
    }
  }
  if (!to.has_value())
  {
    return std::nullopt;
  }
  const std::uint8_t index = at(_x, from, _z);
  m_voxels.Edit({.xVoxels = _x, .yVoxels = from, .zVoxels = _z}, Protocol::Chunk::EMPTY);
  m_voxels.Edit({.xVoxels = to->first, .yVoxels = lowest + 1, .zVoxels = to->second}, index == GRASS ? EARTH : index);
  m_openedColumns.insert((std::int64_t{_z} * WidthVoxels()) + _x);
  return to;
}

void World::RestoreGates()
{
  std::vector<Protocol::VoxelPosition> air;
  for (const auto& [id, structure] : m_structures)
  {
    if (structure.gateOpen)
    {
      const std::vector<Protocol::VoxelPosition> gate = GateVoxels(structure);
      air.insert(air.end(), gate.begin(), gate.end());
    }
  }
  if (air.empty())
  {
    return;
  }
  m_navigation.SetAir(air);
  for (const auto& [tileX, tileZ] : m_navigation.TilesTouchedBy(air))
  {
    m_navigation.RebuildTile(m_voxels.Store(), tileX, tileZ);
  }
  m_navigation.UpdateReach();
}

void World::UpdateStructures()
{
  // Only a structure the tick touched can have changed, so a quiet tick costs nothing (ADR-024, decision 2): one with an
  // edit in its box, or whose collapse started or ended this tick.
  std::set<Protocol::EntityId> touched;
  for (const auto& [id, collapse] : m_collapses)
  {
    if (collapse.ticksElapsed == 1 && collapse.structure != Protocol::NO_ENTITY)
    {
      touched.insert(collapse.structure);
    }
  }
  for (const Protocol::CollapseView& ended : m_endedCollapses)
  {
    if (ended.structure != Protocol::NO_ENTITY)
    {
      touched.insert(ended.structure);
    }
  }
  // Every structure whose box holds an edit, where boxes overlap too, as a section's and a tower's may.
  std::vector<std::pair<Protocol::EntityId, Box>> boxes;
  for (const auto& [id, structure] : m_structures)
  {
    if (const std::optional<Box> box = BoxOf(structure); box.has_value())
    {
      boxes.emplace_back(id, *box);
    }
  }
  for (const Protocol::VoxelPosition& voxel : m_voxels.TickEdits())
  {
    for (const auto& [id, box] : boxes)
    {
      if (voxel.xVoxels >= box.min.xVoxels && voxel.xVoxels < box.min.xVoxels + box.widthVoxels && voxel.zVoxels >= box.min.zVoxels &&
          voxel.zVoxels < box.min.zVoxels + box.depthVoxels && voxel.yVoxels >= box.min.yVoxels &&
          voxel.yVoxels < box.min.yVoxels + box.heightVoxels)
      {
        touched.insert(id);
      }
    }
  }
  // And every structure whose pool the control's rule changed this tick.
  touched.insert(m_touchedStructures.begin(), m_touchedStructures.end());
  if (touched.empty())
  {
    return;
  }
  const std::set<std::tuple<std::int32_t, std::int32_t, std::int32_t>> held = HeldByCollapses();
  for (const Protocol::EntityId id : touched)
  {
    if (const auto structure = m_structures.find(id); structure != m_structures.end())
    {
      Rule().UpdateWorking(*this, structure->second, held);
    }
  }
}

void World::PublishState()
{
  // Step 11: each player's visibility, then the state update for each, filtered by what they see (ADR-002, decision 4;
  // ADR-020, decision 3; ADR-025), and for player 1 in a world with no players, unfiltered, as M3's was. Publish(player)
  // hands each the chunks they see.
  RecomputeVisibility();
  // Each player's sight of the others' structures is remembered as they stand, for when they are out of it (ADR-025,
  // decision 2); one the world no longer holds is forgotten.
  for (const auto& [id, player] : m_players)
  {
    std::map<Protocol::EntityId, Protocol::StructureView>& seen = m_lastSeen[id];
    std::erase_if(seen, [this](const auto& _entry) { return !m_structures.contains(_entry.first); });
    for (const auto& [structureId, structure] : m_structures)
    {
      if (structure.owner != id && SeesStructure(id, structure))
      {
        Protocol::StructureView view = StructureViewFor(id, structure);
        view.lastSeen = true;
        seen.insert_or_assign(structureId, std::move(view));
      }
    }
  }
  m_states.clear();
  if (m_players.empty())
  {
    m_states.emplace(std::uint8_t{1}, StateFor(1));
  }
  for (const auto& [id, player] : m_players)
  {
    m_states.emplace(id, StateFor(id));
  }
}

Protocol::StateUpdate World::StateFor(std::uint8_t _player) const
{
  Protocol::StateUpdate state{.tick = m_tick,
                              .structures = {},
                              .engines = {},
                              .squads = {},
                              .units = {},
                              .shots = {},
                              .collapses = {},
                              .hits = {},
                              .players = {},
                              .player = _player,
                              .refusals = {},
                              .trial = {},
                              .widthCells = m_navigation.WidthCells(),
                              .depthCells = m_navigation.DepthCells(),
                              .visibleCells = {}};
  // Each structure as the player sees it: their own and those they see now as they stand, the others they have seen as
  // they last saw them (ADR-025, decision 2).
  for (const auto& [id, structure] : m_structures)
  {
    if (!Fogged() || SeesStructure(_player, structure))
    {
      state.structures.push_back(StructureViewFor(_player, structure));
      continue;
    }
    if (const auto seen = m_lastSeen.find(_player); seen != m_lastSeen.end())
    {
      if (const auto last = seen->second.find(id); last != seen->second.end())
      {
        state.structures.push_back(last->second);
      }
    }
  }
  // Another player's engine, unit or shot is in the update while a cell it stands in is seen, never garrisoned and never
  // while the map is merely revealed; a squad with its figures in sight, and without its goal or target.
  const std::int32_t machineSide = std::max(1, Data().Tuning().machinesWidthVoxels);
  const auto seen = [this, _player](Protocol::VoxelPosition _cell, std::int32_t _side)
  { return !Fogged() || SeesAny(_player, _cell.xVoxels, _cell.zVoxels, _cell.xVoxels + _side - 1, _cell.zVoxels + _side - 1); };
  const auto unitSeen = [this, _player, &seen, machineSide](const Unit& _unit)
  {
    return !Garrisoned(_unit) &&
           (_unit.owner == _player || seen(_unit.cellVoxels, _unit.navigationClass == NavigationClass::Machines ? machineSide : 1));
  };
  // An engine's hit points are its body's.
  const auto bodyOf = [this](const SiegeEngine& _engine) -> const Unit*
  {
    const auto squad = m_squads.find(_engine.squad);
    return squad == m_squads.end() || squad->second.units.empty() ? nullptr : &m_units.at(squad->second.units.front());
  };
  for (const auto& [id, engine] : m_siegeEngines)
  {
    if (engine.owner != _player && !seen(engine.cellVoxels, machineSide))
    {
      continue;
    }
    state.engines.push_back({.id = id,
                             .owner = engine.owner,
                             .kind = engine.kind,
                             .cellVoxels = engine.cellVoxels,
                             .quarterTurns = engine.quarterTurns,
                             .reloadTicks = engine.reloadTicks,
                             .state = engine.state,
                             .squad = engine.squad,
                             .stateTicksLeft = engine.stateTicksLeft,
                             .position = engine.position,
                             .hitPoints = bodyOf(engine) == nullptr ? 0 : bodyOf(engine)->hitPoints,
                             .maxHitPoints = Data().Stats(engine.kind == EngineKind::Ram ? UnitKind::Ram : UnitKind::Trebuchet).hitPoints});
  }
  for (const auto& [id, squad] : m_squads)
  {
    const bool own = squad.owner == _player || !Fogged();
    std::vector<Protocol::EntityId> units;
    for (const Protocol::EntityId unit : squad.units)
    {
      if (own || unitSeen(m_units.at(unit)))
      {
        units.push_back(unit);
      }
    }
    if (!own && units.empty())
    {
      continue;
    }
    state.squads.push_back({.id = id,
                            .owner = squad.owner,
                            .units = std::move(units),
                            .hasGoal = own && squad.hasGoal,
                            .goalVoxels = own ? squad.goalVoxels : Protocol::VoxelPosition{},
                            .shortOfGoal = own && squad.shortOfGoal,
                            .garrisonedIn = squad.garrisonedIn,
                            .kind = squad.kind,
                            .order = squad.garrisonedIn != Protocol::NO_ENTITY ? Protocol::SquadOrder::Garrisoned : squad.order,
                            .fullFigures = Data().Stats(squad.kind).squadFigures,
                            .target = own ? squad.target : Protocol::NO_ENTITY});
  }
  for (const auto& [id, unit] : m_units)
  {
    if (!unitSeen(unit))
    {
      continue;
    }
    state.units.push_back({.id = id,
                           .squad = unit.squad,
                           .owner = unit.owner,
                           .kind = unit.kind,
                           .position = unit.position,
                           .cellVoxels = unit.cellVoxels,
                           .hitPoints = unit.hitPoints,
                           .maxHitPoints = Data().Stats(unit.kind).hitPoints});
  }
  for (const auto& [id, shot] : m_shots)
  {
    const FixedPosition position = std::holds_alternative<ArrowLine>(shot.flight)
                                     ? ArrowPositionAt(std::get<ArrowLine>(shot.flight), shot.ticksFlown)
                                     : ShotPositionAt(std::get<ShotArc>(shot.flight), shot.ticksFlown);
    const Protocol::VoxelPosition cell{.xVoxels = position.xMeters.FloorWhole(), .yVoxels = 0, .zVoxels = position.zMeters.FloorWhole()};
    if (shot.owner != _player && !seen(cell, 1))
    {
      continue;
    }
    state.shots.push_back({.id = id, .engine = shot.engine, .position = position, .kind = shot.kind, .owner = shot.owner});
  }
  // The collapses still running and those whose fall ended this tick, in id order, each while a column of its piece or
  // its strip is seen.
  const auto collapseSeen = [&seen](const Protocol::CollapseView& _view)
  {
    const auto columnSeen = [&seen](const Protocol::VoxelPosition& _voxel) { return seen(_voxel, 1); };
    return std::ranges::any_of(_view.piece, columnSeen) || std::ranges::any_of(_view.strip, columnSeen);
  };
  for (const auto& [id, collapse] : m_collapses)
  {
    Protocol::CollapseView view = ViewOf(collapse);
    if (collapseSeen(view))
    {
      state.collapses.push_back(std::move(view));
    }
  }
  std::ranges::copy_if(m_endedCollapses, std::back_inserter(state.collapses), collapseSeen);
  std::ranges::sort(state.collapses, {}, &Protocol::CollapseView::id);
  for (const ShotHit& hit : m_tickHits)
  {
    if (seen(hit.voxel, 1))
    {
      state.hits.push_back({.shot = hit.shot, .voxel = hit.voxel});
    }
  }
  if (const auto visibility = m_visibility.find(_player); visibility != m_visibility.end())
  {
    state.visibleCells.assign(visibility->second.Bits().begin(), visibility->second.Bits().end());
  }
  // Every player, with the stockpiles of the update's player alone.
  for (const auto& [id, player] : m_players)
  {
    const bool own = id == _player;
    state.players.push_back({.player = id,
                             .side = player.side,
                             .stoneHundredthsCubicMeters = own ? player.stoneHundredthsCubicMeters : 0,
                             .timberHundredthsCubicMeters = own ? player.timberHundredthsCubicMeters : 0});
  }
  for (const auto& [player, refusal] : m_tickRefusals)
  {
    if (player == _player)
    {
      state.refusals.push_back(refusal);
    }
  }
  if (m_trial.has_value())
  {
    // The ticks left once this one has run; the clock stops at the outcome.
    const TrialProgress& progress = m_trialProgress;
    const std::uint32_t ran = progress.outcome == Protocol::TrialOutcome::Running ? m_tick + 1 : progress.endTick;
    const auto keep = m_structures.find(m_trial->keep);
    state.trial = {.version = m_trial->version,
                   .outcome = progress.outcome,
                   .ticksLeft = m_trial->timeLimitTicks > ran ? m_trial->timeLimitTicks - ran : 0,
                   .keep = m_trial->keep,
                   .keepWorking = keep != m_structures.end() && keep->second.working,
                   .holdTicks = progress.holdTicks,
                   .holdTicksNeeded = m_trial->holdTicks,
                   .holdContested = progress.contested};
  }
  return state;
}

Protocol::CollapseView World::ViewOf(const Collapse& _collapse) const
{
  const CollapsePlan& plan = _collapse.plan;
  const std::uint32_t warned = std::min(_collapse.ticksElapsed, plan.warningTicks);
  const std::uint32_t fallen = _collapse.ticksElapsed > plan.warningTicks ? _collapse.ticksElapsed - plan.warningTicks : 0;
  const std::uint64_t fallTicks = std::max<std::uint32_t>(plan.fallTicks, 1);
  // As Fall lands them: in equal parts over the fall's ticks.
  const std::uint64_t landed = plan.landing.size() * std::min<std::uint64_t>(fallen, fallTicks) / fallTicks;
  return {.id = _collapse.id,
          .structure = _collapse.structure,
          .warningTicksLeft = plan.warningTicks - warned,
          .fallTicksLeft = fallen >= plan.fallTicks ? 0 : plan.fallTicks - fallen,
          .piece = _collapse.piece,
          .strip = plan.strip,
          .landing = plan.landing,
          .landedVoxels = static_cast<std::uint32_t>(landed),
          .fallen = _collapse.fallen};
}

void World::AnswerBetweenTicks()
{
  // The state is the end of the last tick ticked, m_tick - 1, or the world as built before any.
  AnswerQueries(m_tick == 0 ? 0 : m_tick - 1);
}

void World::AnswerQueries(std::uint32_t _tick)
{
  // Step 12: the queries, oldest first, on the tick's state, within queryWorkPerTick units of work
  // (ADR-009, decision 5; ADR-020, decision 6). A query that does not fit what is left waits for the
  // next tick, unless nothing has been answered yet this tick, so that one dearer than the whole
  // budget cannot block the queue for good.
  const std::int32_t budget = Data().Tuning().queryWorkPerTick;
  std::int32_t left = budget;
  while (!m_pendingQueries.empty())
  {
    const SubmittedCommand& asked = m_pendingQueries.front();
    Core::ByteReader reader(asked.bytes);
    const Protocol::QueryRequest request = Protocol::DecodeQueryRequest(reader);
    if (!reader.AtEnd())
    {
      throw std::invalid_argument("a query's bytes run on past the query");
    }
    const std::int32_t work = std::holds_alternative<Protocol::SectionIntegrity>(request.query)
                                ? SECTION_INTEGRITY_WORK
                                : (std::holds_alternative<Protocol::TileReach>(request.query) ? TILE_REACH_WORK : AIM_CHECK_WORK);
    if (work > left && left < budget)
    {
      break;
    }
    Core::ByteWriter writer;
    // An answer about what the asker does not see says so, and reads nothing (ADR-025, decision 6).
    const bool visible = AnswerVisible(asked.player, request.query);
    Protocol::Encode(Protocol::Answer{.queryId = request.id,
                                      .tick = _tick,
                                      .reading = visible ? Answer(asked.player, request.query) : EmptyReading(request.query),
                                      .notVisible = !visible},
                     writer);
    m_answers.push_back(writer.TakeBytes());
    left -= work;
    m_pendingQueries.pop_front();
    if (left <= 0)
    {
      break;
    }
  }
}

Protocol::Reading World::Answer(std::uint8_t _player, const Protocol::Query& _query) const
{
  // Answers pass the state update's filter first, in AnswerQueries (ADR-025, decision 6).
  if (const auto* integrity = std::get_if<Protocol::SectionIntegrity>(&_query))
  {
    if (m_structures.contains(integrity->structure))
    {
      return ReadIntegrity(*this, integrity->structure);
    }
    // A structure the world no longer holds reads empty: the client asks about what it last saw.
    return EmptyReading(_query);
  }
  if (const auto* reach = std::get_if<Protocol::TileReach>(&_query))
  {
    return TileReachReadingFor(*reach);
  }
  if (const auto* squadAim = std::get_if<Protocol::SquadAim>(&_query))
  {
    return SquadAimReadingFor(_player, *squadAim);
  }
  return AimReadingFor(_player, std::get<Protocol::AimCheck>(_query));
}

Protocol::AimReading World::SquadAimReadingFor(std::uint8_t _player, const Protocol::SquadAim& _aim) const
{
  // Hud.md §8's attack cursor for a ranged squad, from its lead's loosing point as its arrow would fly (M4-C4): OUT OF
  // RANGE beyond its reach at the elevation, BLOCKED when the line meets a solid voxel, and otherwise IN RANGE.
  Protocol::AimReading reading{.verdict = Protocol::AimVerdict::OutOfRange, .distanceMeters = 0, .strikes = false, .strikeVoxels = {}};
  const auto squad = m_squads.find(_aim.squad);
  if (squad == m_squads.end() || squad->second.owner != _player || squad->second.garrisonedIn != Protocol::NO_ENTITY ||
      squad->second.units.empty() || Data().Stats(squad->second.kind).rangeVoxels <= 0)
  {
    return reading;
  }
  const Rules& rules = Data().Tuning();
  const Protocol::VoxelPosition from = m_units.at(squad->second.units.front()).cellVoxels;
  const std::int64_t acrossX = std::int64_t{_aim.targetVoxels.xVoxels} - from.xVoxels;
  const std::int64_t acrossZ = std::int64_t{_aim.targetVoxels.zVoxels} - from.zVoxels;
  const std::int64_t acrossSquared = (acrossX * acrossX) + (acrossZ * acrossZ);
  reading.distanceMeters = static_cast<std::int32_t>(Core::IntegerSquareRoot(static_cast<std::uint64_t>(acrossSquared)));
  const std::int64_t reach =
    ArrowRangeVoxels(Data().Stats(squad->second.kind).rangeVoxels, ElevationSteps(from.yVoxels, _aim.targetVoxels.yVoxels, rules), rules);
  if (acrossSquared > reach * reach)
  {
    return reading;
  }
  const ArrowLine line = AimArrow(from, _aim.targetVoxels, rules);
  if (const std::optional<Protocol::VoxelPosition> strike = FirstSolid(line.start, line.end, m_voxels); strike.has_value())
  {
    reading.verdict = Protocol::AimVerdict::Blocked;
    reading.strikes = true;
    reading.strikeVoxels = *strike;
    return reading;
  }
  reading.verdict = Protocol::AimVerdict::InRange;
  return reading;
}

Protocol::TileReachReading World::TileReachReadingFor(const Protocol::TileReach& _reach) const
{
  // The tile's reach from the start cell, from the components step 8 brought up to date (M3-B6). A
  // tile or a start off the grid reads empty, as a structure the world no longer holds does: the
  // client asks about what is on its screen.
  Protocol::TileReachReading reading{.tileX = _reach.tileX, .tileZ = _reach.tileZ, .usable = {}, .reached = {}, .surfaceVoxels = {}};
  reading.surfaceVoxels.fill(Protocol::NO_SURFACE_VOXELS);
  const std::int32_t width = m_navigation.WidthCells();
  const Protocol::VoxelPosition start = _reach.startCellVoxels;
  const bool inside = start.xVoxels >= 0 && start.zVoxels >= 0 && start.xVoxels < width && start.zVoxels < m_navigation.DepthCells() &&
                      _reach.tileX >= 0 && _reach.tileZ >= 0 && _reach.tileX < m_navigation.TilesAcross() &&
                      _reach.tileZ < m_navigation.TilesDeep();
  if (!inside)
  {
    return reading;
  }
  const NavigationGrid::TileReach reach =
    m_navigation.ReachInTile(std::int64_t{start.zVoxels} * width + start.xVoxels, _reach.tileX, _reach.tileZ, _reach.navigationClass);
  for (std::size_t bit = 0; bit < reach.usable.size(); ++bit)
  {
    const auto mask = static_cast<std::uint8_t>(1U << (bit % 8));
    if (reach.usable.test(bit))
    {
      reading.usable.at(bit / 8) |= mask;
    }
    if (reach.reached.test(bit))
    {
      reading.reached.at(bit / 8) |= mask;
    }
  }
  // Each column's surface, navigation's own, where the passability view draws the cell (M3-F3).
  constexpr std::int32_t EDGE = NavigationGrid::TILE_EDGE_CELLS;
  for (std::int32_t z = 0; z < EDGE; ++z)
  {
    for (std::int32_t x = 0; x < EDGE; ++x)
    {
      const std::int16_t height = m_navigation.WalkableHeight((_reach.tileX * EDGE) + x, (_reach.tileZ * EDGE) + z);
      const std::int32_t cell = (z * EDGE) + x;
      reading.surfaceVoxels.at(static_cast<std::size_t>(cell)) =
        height == NavigationGrid::NOT_WALKABLE ? Protocol::NO_SURFACE_VOXELS : height;
    }
  }
  return reading;
}

Protocol::AimReading World::AimReadingFor(std::uint8_t _player, const Protocol::AimCheck& _aim) const
{
  // Hud.md §8's attack cursor, from the shot's own arc (M3-B4): TOO CLOSE and OUT OF RANGE from the
  // range AimShot keeps, and otherwise where the arc strikes, IN RANGE when that is the target or one
  // of its six face neighbours and BLOCKED when it is anything else, or nothing.
  Protocol::AimReading reading{.verdict = Protocol::AimVerdict::OutOfRange, .distanceMeters = 0, .strikes = false, .strikeVoxels = {}};
  const auto engine = m_siegeEngines.find(_aim.engine);
  if (engine == m_siegeEngines.end() || engine->second.owner != _player)
  {
    return reading;
  }
  const Protocol::VoxelPosition from = engine->second.cellVoxels;
  const std::int64_t acrossX = std::int64_t{_aim.targetVoxels.xVoxels} - from.xVoxels;
  const std::int64_t acrossZ = std::int64_t{_aim.targetVoxels.zVoxels} - from.zVoxels;
  const std::int64_t acrossSquared = acrossX * acrossX + acrossZ * acrossZ;
  reading.distanceMeters = static_cast<std::int32_t>(Core::IntegerSquareRoot(static_cast<std::uint64_t>(acrossSquared)));
  const std::int64_t nearest = Data().Tuning().shotMinRangeVoxels;
  if (acrossSquared < nearest * nearest)
  {
    reading.verdict = Protocol::AimVerdict::TooClose;
    return reading;
  }
  const std::optional<ShotArc> arc = AimShot(from, _aim.targetVoxels, Data().Tuning());
  if (!arc.has_value())
  {
    return reading; // beyond its range, or too far below the launch to reach at 45 degrees
  }
  const std::optional<Protocol::VoxelPosition> strike = TraceShot(*arc, m_voxels, WidthVoxels(), DepthVoxels());
  reading.verdict = Protocol::AimVerdict::Blocked;
  if (strike.has_value())
  {
    reading.strikes = true;
    reading.strikeVoxels = *strike;
    const std::int32_t apart = std::abs(strike->xVoxels - _aim.targetVoxels.xVoxels) +
                               std::abs(strike->yVoxels - _aim.targetVoxels.yVoxels) +
                               std::abs(strike->zVoxels - _aim.targetVoxels.zVoxels);
    if (apart <= 1)
    {
      reading.verdict = Protocol::AimVerdict::InRange;
    }
  }
  return reading;
}

} // namespace Simulation
