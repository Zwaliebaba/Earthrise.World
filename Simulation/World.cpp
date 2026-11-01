#include "pch.h"

#include "World.h"

#include "Generator.h"
#include "IntegerMath.h"

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

bool World::Quiet() const noexcept
{
  if (!m_shots.empty() || !m_collapses.empty() || !m_slumpQueue.empty() || !m_searches.empty() || !m_unsearchedEdits.empty() ||
      !m_voxels.TickEdits().empty() || !m_pendingCommands.empty())
  {
    return false;
  }
  // A unit stands still at its place in its squad's file: in no step, and at the cell it stops in.
  for (const auto& [id, squad] : m_squads)
  {
    for (std::size_t place = 0; place < squad.units.size(); ++place)
    {
      const auto unit = m_units.find(squad.units[place]);
      if (unit == m_units.end() || squad.trail.empty())
      {
        continue;
      }
      const std::size_t end = squad.trail.size() - 1 - std::min(place, squad.trail.size() - 1);
      if (unit->second.stepEffort > 0 || unit->second.trailStep < end)
      {
        return false;
      }
    }
  }
  return true;
}

void World::Tick()
{
  m_tickCrushes.clear();
  m_endedCollapses.clear();
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

const Protocol::StateUpdate& World::State() const noexcept
{
  return m_state;
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
  // From the trail cell the lead stands in or is stepping into, which its squad holds to until the
  // path arrives; or, before the squad has a trail, from the lead's own cell.
  const PathSearch::CellIndex start = lead.trailStep < _squad.trail.size()
                                        ? _squad.trail[lead.trailStep]
                                        : std::int64_t{lead.cellVoxels.zVoxels} * width + lead.cellVoxels.xVoxels;
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

Protocol::EntityId World::AddSquad(std::uint8_t _owner, NavigationClass _class, std::span<const Protocol::VoxelPosition> _cellsVoxels,
                                   UnitKind _kind)
{
  const Protocol::EntityId squadId = NewEntityId();
  Squad squad{.id = squadId,
              .owner = _owner,
              .navigationClass = _class,
              .units = {},
              .hasGoal = false,
              .goalVoxels = {},
              .path = {},
              .shortOfGoal = false,
              .trail = {}};
  for (const Protocol::VoxelPosition& cell : _cellsVoxels)
  {
    const Protocol::EntityId unitId = NewEntityId();
    m_units.emplace(unitId, Unit{.id = unitId,
                                 .owner = _owner,
                                 .navigationClass = _class,
                                 .squad = squadId,
                                 .position = CellFloorMiddle(cell),
                                 .cellVoxels = cell,
                                 .trailStep = 0,
                                 .stepEffort = 0,
                                 .kind = _kind});
    squad.units.push_back(unitId);
  }
  m_squads.emplace(squadId, std::move(squad));
  return squadId;
}

Protocol::EntityId World::AddSiegeEngine(std::uint8_t _owner, EngineKind _kind, Protocol::VoxelPosition _cellVoxels,
                                         std::uint32_t _quarterTurns)
{
  const Protocol::EntityId id = NewEntityId();
  m_siegeEngines.emplace(id, SiegeEngine{.id = id,
                                         .owner = _owner,
                                         .kind = _kind,
                                         .position = CellFloorMiddle(_cellVoxels),
                                         .cellVoxels = _cellVoxels,
                                         .reloadTicks = 0,
                                         .quarterTurns = _quarterTurns % 4});
  return id;
}

Protocol::EntityId World::AddStructure(std::uint8_t _owner, std::string_view _templateId, Protocol::VoxelPosition _originVoxels,
                                       std::uint32_t _quarterTurns, std::string_view _name)
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
        // Clockwise seen from above, as the voxel pass turns a batch: one turn takes +z to +x, and the
        // footprint's minimum corner stays at the origin.
        std::int32_t turnedX = x;
        std::int32_t turnedZ = z;
        switch (turns)
        {
        case 1:
          turnedX = z;
          turnedZ = width - 1 - x;
          break;
        case 2:
          turnedX = width - 1 - x;
          turnedZ = depth - 1 - z;
          break;
        case 3:
          turnedX = depth - 1 - z;
          turnedZ = x;
          break;
        default:
          break;
        }
        m_voxels.Edit({.xVoxels = _originVoxels.xVoxels + turnedX,
                       .yVoxels = _originVoxels.yVoxels + static_cast<std::int32_t>(y),
                       .zVoxels = _originVoxels.zVoxels + turnedZ},
                      index);
      }
    }
  }
  const Protocol::EntityId id = NewEntityId();
  m_structures.emplace(id, Structure{.id = id,
                                     .owner = _owner,
                                     .templateId = std::string(_templateId),
                                     .originVoxels = _originVoxels,
                                     .quarterTurns = turns,
                                     .name = std::string(_name)});
  return id;
}

const std::map<Protocol::EntityId, Structure>& World::Structures() const noexcept
{
  return m_structures;
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
      RequestSearch(squad->second);
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

void World::AdvanceJobs()
{
  // Step 2: designations and jobs, which M2 has none of.
}

void World::MoveUnits()
{
  // Step 3: the searches, served in the order they were asked for, then squad id, within
  // pathNodesPerTick nodes; one the budget cuts short goes on next tick (M2-F3; ADR-015, decision
  // 5). Then movement along paths (M2-F5).
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
  // Then each squad's figures walk its trail, in squad-id order and each squad's own (ADR-015,
  // decision 6). A squad whose search is not done holds: its figures finish the step they are in and
  // start no other, so the search's start, the cell its lead was stepping into, is still the lead's
  // when the path arrives.
  for (auto& [id, squad] : m_squads)
  {
    if (squad.trail.empty())
    {
      continue;
    }
    const bool holding = std::ranges::any_of(m_searches, [id](const auto& _search) { return _search.first.second == id; });
    for (std::size_t place = 0; place < squad.units.size(); ++place)
    {
      Walk(m_units.at(squad.units[place]), squad, place, holding);
    }
    // The cells behind every figure are walked: the trail keeps only the one a figure is stepping
    // from.
    std::size_t behind = squad.trail.size();
    for (const Protocol::EntityId unitId : squad.units)
    {
      const Unit& unit = m_units.at(unitId);
      behind = std::min<std::size_t>(behind, unit.stepEffort > 0 ? unit.trailStep - 1 : unit.trailStep);
    }
    if (behind > 0 && behind < squad.trail.size())
    {
      squad.trail.erase(squad.trail.begin(), squad.trail.begin() + static_cast<std::ptrdiff_t>(behind));
      for (const Protocol::EntityId unitId : squad.units)
      {
        m_units.at(unitId).trailStep -= static_cast<std::uint32_t>(behind);
      }
    }
  }
}

void World::FormTrail(Squad& _squad)
{
  const std::vector<PathSearch::CellIndex>& path = _squad.path;
  if (path.empty() || _squad.units.empty())
  {
    return;
  }
  const std::uint32_t leadStep = m_units.at(_squad.units.front()).trailStep;
  const bool fileBehindLead =
    std::ranges::all_of(_squad.units, [&](Protocol::EntityId _unit) { return m_units.at(_unit).trailStep <= leadStep; });
  if (leadStep < _squad.trail.size() && _squad.trail[leadStep] == path.front() && fileBehindLead)
  {
    // The path starts where the lead stands or is stepping: the trail behind it stays, and the path
    // takes the place of what lay ahead.
    _squad.trail.resize(std::size_t{leadStep} + 1);
    _squad.trail.insert(_squad.trail.end(), path.begin() + 1, path.end());
    return;
  }
  // The file forms behind the lead: the figures' own cells, the rearmost first, then the path, which
  // starts at the lead's. Each figure stands at its own cell.
  const std::int32_t width = m_navigation.WidthCells();
  std::vector<PathSearch::CellIndex> trail;
  trail.reserve(_squad.units.size() + path.size());
  for (std::size_t place = _squad.units.size() - 1; place > 0; --place)
  {
    const Protocol::VoxelPosition cell = m_units.at(_squad.units[place]).cellVoxels;
    trail.push_back(std::int64_t{cell.zVoxels} * width + cell.xVoxels);
  }
  trail.insert(trail.end(), path.begin(), path.end());
  for (std::size_t place = 0; place < _squad.units.size(); ++place)
  {
    Unit& unit = m_units.at(_squad.units[place]);
    unit.trailStep = static_cast<std::uint32_t>(_squad.units.size() - 1 - place);
    unit.stepEffort = 0;
  }
  _squad.trail = std::move(trail);
}

void World::Walk(Unit& _unit, const Squad& _squad, std::size_t _place, bool _holding)
{
  // Each class's speed from RULE (GameDesign.md, M2's values, item 10).
  const Rules& rules = Data().Tuning();
  std::int64_t speed = rules.footSpeedMetersPerTick.Raw();
  if (_unit.navigationClass == NavigationClass::Cavalry)
  {
    speed = rules.cavalrySpeedMetersPerTick.Raw();
  }
  else if (_unit.navigationClass == NavigationClass::Machines)
  {
    speed = rules.machinesSpeedMetersPerTick.Raw();
  }
  const std::vector<PathSearch::CellIndex>& trail = _squad.trail;
  if (speed <= 0 || _unit.trailStep >= trail.size())
  {
    return;
  }
  // The figure _place behind the lead stops that many cells short of the trail's end.
  const std::size_t end = trail.size() - 1 - std::min(_place, trail.size() - 1);
  std::int64_t budget = speed;
  while (budget > 0)
  {
    if (_unit.stepEffort == 0)
    {
      // It stands at a cell: it steps on only while its squad is not holding, it has not reached its
      // place in the file, and the step is still open to its class.
      if (_holding || _unit.trailStep >= end || !Linked(trail[_unit.trailStep], trail[_unit.trailStep + 1], _unit.navigationClass))
      {
        break;
      }
      ++_unit.trailStep;
    }
    const std::int64_t need = StepEffort(trail[_unit.trailStep - 1], trail[_unit.trailStep], _unit.navigationClass);
    const std::int64_t spent = std::min(budget, need - _unit.stepEffort);
    _unit.stepEffort += spent;
    budget -= spent;
    if (_unit.stepEffort >= need)
    {
      _unit.stepEffort = 0;
    }
  }
  Place(_unit, _squad);
}

void World::Place(Unit& _unit, const Squad& _squad) const
{
  const std::int32_t width = m_navigation.WidthCells();
  const std::vector<PathSearch::CellIndex>& trail = _squad.trail;
  const PathSearch::CellIndex to = trail[_unit.trailStep];
  const PathSearch::CellIndex from = _unit.stepEffort > 0 ? trail[_unit.trailStep - 1] : to;
  const std::int64_t need = _unit.stepEffort > 0 ? StepEffort(from, to, _unit.navigationClass) : 1;
  // Between the two cells' middles in proportion to the effort spent. It stands in the cell it left
  // until it is halfway, and at that cell's height (ADR-015, decision 6).
  const auto along = [&_unit, need](std::int64_t _fromCell, std::int64_t _toCell)
  {
    const std::int64_t half = Core::Fixed::ONE_RAW / 2;
    const std::int64_t start = _fromCell * Core::Fixed::ONE_RAW + half;
    const std::int64_t finish = _toCell * Core::Fixed::ONE_RAW + half;
    return Core::Fixed::FromRaw(static_cast<std::int32_t>(start + Core::FloorDivide((finish - start) * _unit.stepEffort, need)));
  };
  const PathSearch::CellIndex cell = 2 * _unit.stepEffort < need ? from : to;
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
  }
  std::vector<Protocol::EntityId> landed;
  for (auto& [id, shot] : m_shots)
  {
    const ShotStep step = StepShot(shot.arc, shot.ticksFlown, m_voxels, WidthVoxels(), DepthVoxels());
    ++shot.ticksFlown;
    const std::optional<Protocol::VoxelPosition>& hit = step.hit;
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
    else if (step.left)
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
      if (strip.contains({unit.cellVoxels.xVoxels, unit.cellVoxels.zVoxels}))
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
    if (column != filled.end() && column->second >= unit.cellVoxels.yVoxels)
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
  // Step 8: every navigation tile the tick's edits touched, none deferred (ADR-015, decision 4).
  const std::vector<std::pair<std::int32_t, std::int32_t>> tiles = m_navigation.TilesTouchedBy(m_voxels.TickEdits());
  for (const auto& [tileX, tileZ] : tiles)
  {
    m_navigation.RebuildTile(m_voxels.Store(), tileX, tileZ);
  }
  if (tiles.empty())
  {
    return;
  }
  // Then, in squad-id order, the squads to search again in the next tick's step 3 (ADR-015,
  // decision 5): each short of its goal, each whose path crosses a rebuilt tile, and each whose
  // search is not done, since the cells it has closed may have changed under it.
  const std::set<std::pair<std::int32_t, std::int32_t>> rebuilt(tiles.begin(), tiles.end());
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
  const std::int32_t width = m_navigation.WidthCells();
  std::set<Protocol::EntityId> displaced;
  for (auto& [id, unit] : m_units)
  {
    const std::int32_t x = unit.cellVoxels.xVoxels;
    const std::int32_t z = unit.cellVoxels.zVoxels;
    const std::int16_t height = m_navigation.WalkableHeight(x, z);
    if (height == NavigationGrid::NOT_WALKABLE || !m_navigation.Usable(x, z, unit.navigationClass))
    {
      // No longer walkable: to the nearest cell its class can use, ties to the lower index. Its squad
      // forms its file again from where its figures stand.
      const std::int64_t nearest = m_navigation.NearestUsable(x, z, unit.navigationClass);
      if (nearest < 0)
      {
        continue;
      }
      const auto nearestX = static_cast<std::int32_t>(nearest % width);
      const auto nearestZ = static_cast<std::int32_t>(nearest / width);
      unit.cellVoxels = {.xVoxels = nearestX, .yVoxels = m_navigation.WalkableHeight(nearestX, nearestZ), .zVoxels = nearestZ};
      unit.position = CellFloorMiddle(unit.cellVoxels);
      displaced.insert(unit.squad);
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
      m_tickCrushes.push_back({.unit = id, .collapse = Protocol::NO_ENTITY, .dropVoxels = unit.cellVoxels.yVoxels - height});
    }
    unit.cellVoxels.yVoxels = height;
    unit.position.yMeters = Core::Fixed::FromWhole(height);
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
      unit.trailStep = 0;
      unit.stepEffort = 0;
      unit.position = CellFloorMiddle(unit.cellVoxels);
    }
    RequestSearch(squad->second);
  }
}

void World::CheckRules()
{
  // Step 10: the win and loss rules, which M2 has none of.
}

void World::PublishState()
{
  // Step 11: the state update, filtered by what the player sees (ADR-002, decision 4; ADR-020,
  // decision 3). Publish() hands the chunks to whoever asks.
  m_state = StateFor(0);
}

Protocol::StateUpdate World::StateFor([[maybe_unused]] std::uint8_t _player) const
{
  Protocol::StateUpdate state{
    .tick = m_tick, .structures = {}, .engines = {}, .squads = {}, .units = {}, .shots = {}, .collapses = {}, .hits = {}};
  for (const auto& [id, structure] : m_structures)
  {
    state.structures.push_back({.id = id,
                                .owner = structure.owner,
                                .templateId = structure.templateId,
                                .originVoxels = structure.originVoxels,
                                .quarterTurns = structure.quarterTurns,
                                .name = structure.name});
  }
  for (const auto& [id, engine] : m_siegeEngines)
  {
    state.engines.push_back({.id = id,
                             .owner = engine.owner,
                             .kind = engine.kind,
                             .cellVoxels = engine.cellVoxels,
                             .quarterTurns = engine.quarterTurns,
                             .reloadTicks = engine.reloadTicks});
  }
  for (const auto& [id, squad] : m_squads)
  {
    state.squads.push_back({.id = id,
                            .owner = squad.owner,
                            .units = squad.units,
                            .hasGoal = squad.hasGoal,
                            .goalVoxels = squad.goalVoxels,
                            .shortOfGoal = squad.shortOfGoal});
  }
  for (const auto& [id, unit] : m_units)
  {
    state.units.push_back(
      {.id = id, .squad = unit.squad, .owner = unit.owner, .kind = unit.kind, .position = unit.position, .cellVoxels = unit.cellVoxels});
  }
  for (const auto& [id, shot] : m_shots)
  {
    state.shots.push_back({.id = id, .engine = shot.engine, .position = ShotPositionAt(shot.arc, shot.ticksFlown)});
  }
  // The collapses still running and those whose fall ended this tick, in id order.
  for (const auto& [id, collapse] : m_collapses)
  {
    state.collapses.push_back(ViewOf(collapse));
  }
  state.collapses.insert(state.collapses.end(), m_endedCollapses.begin(), m_endedCollapses.end());
  std::ranges::sort(state.collapses, {}, &Protocol::CollapseView::id);
  for (const ShotHit& hit : m_tickHits)
  {
    state.hits.push_back({.shot = hit.shot, .voxel = hit.voxel});
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

void World::AnswerQueries()
{
  // Step 12: the queries, oldest first, on the tick's state, within queryWorkPerTick units of work
  // (ADR-009, decision 5; ADR-020, decision 6). A query that does not fit what is left waits for the
  // next tick, unless nothing has been answered yet this tick, so that one dearer than the whole
  // budget cannot block the queue for good.
  constexpr std::int32_t AIM_CHECK_WORK = 1;
  constexpr std::int32_t TILE_REACH_WORK = 1;
  constexpr std::int32_t SECTION_INTEGRITY_WORK = 8; // the most support searches it runs (M3-B5)
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
    Protocol::Encode(Protocol::Answer{.queryId = request.id, .tick = m_tick, .reading = Answer(asked.player, request.query)}, writer);
    m_answers.push_back(writer.TakeBytes());
    left -= work;
    m_pendingQueries.pop_front();
    if (left <= 0)
    {
      break;
    }
  }
}

Protocol::Reading World::Answer([[maybe_unused]] std::uint8_t _player, const Protocol::Query& _query) const
{
  // Empty readings until M3-B4 to B6 land each kind's own; the budget and the plumbing are this
  // order's. Answers pass the state update's filter, which passes everything in M3.
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
    return Protocol::TileReachReading{.tileX = reach->tileX, .tileZ = reach->tileZ, .usable = {}, .reached = {}};
  }
  return AimReadingFor(_player, std::get<Protocol::AimCheck>(_query));
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
