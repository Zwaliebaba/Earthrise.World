#include "pch.h"

#include "SiegeDefender.h"

#include "Arrow.h"
#include "ServerData.h"

namespace Simulation
{

namespace
{

/// The squared distance across the ground from _cell to the nearest cell of a structure's footprint.
std::int64_t DistanceSquared(const Protocol::StructureView& _structure, Protocol::VoxelPosition _cell)
{
  const auto outside = [](std::int32_t _at, std::int32_t _low, std::int32_t _extent)
  { return std::int64_t{std::max({_low - _at, 0, _at - (_low + _extent - 1)})}; };
  const std::int64_t dx = outside(_cell.xVoxels, _structure.originVoxels.xVoxels, _structure.extentVoxels.xVoxels);
  const std::int64_t dz = outside(_cell.zVoxels, _structure.originVoxels.zVoxels, _structure.extentVoxels.zVoxels);
  return (dx * dx) + (dz * dz);
}

/// How much a garrison wants a target of _kind: sappers first, then rams, then the trebuchet, then anything else.
int Priority(Protocol::UnitKind _kind) noexcept
{
  switch (_kind)
  {
  case Protocol::UnitKind::Sapper:
    return 0;
  case Protocol::UnitKind::Ram:
    return 1;
  case Protocol::UnitKind::Trebuchet:
    return 2;
  default:
    return 3;
  }
}

constexpr int ANYTHING = 3;

/// The structure's template's critical part's name, or empty.
std::string_view CriticalPart(const CommanderView& _view, const Protocol::StructureView& _structure)
{
  const ServerTemplate* found = _view.Data().FindTemplate(_structure.templateId);
  return found == nullptr ? std::string_view{"?"} : std::string_view{found->criticalPart};
}

/// The keep's view: the trial's keep, or else the player's first T2; null without one.
const Protocol::StructureView* KeepOf(const CommanderView& _view)
{
  const Protocol::StateUpdate& state = _view.State();
  for (const Protocol::StructureView& structure : state.structures)
  {
    if (structure.owner == _view.Player() &&
        (structure.id == state.trial.keep || (state.trial.keep == Protocol::NO_ENTITY && structure.templateId == "T2")))
    {
      return &structure;
    }
  }
  return nullptr;
}

/// Whether _cell lies on the side of _structure's centre line, along its longer side, that holds _keep, as the
/// simulation reads its owner's side (ADR-024, decisions 3 and 5); any side without a keep.
bool InnerSide(const Protocol::StructureView& _structure, const Protocol::StructureView* _keep, Protocol::VoxelPosition _cell)
{
  if (_keep == nullptr)
  {
    return true;
  }
  const auto centerTwice = [](const Protocol::StructureView& _of)
  {
    return std::pair{(2 * std::int64_t{_of.originVoxels.xVoxels}) + _of.extentVoxels.xVoxels - 1,
                     (2 * std::int64_t{_of.originVoxels.zVoxels}) + _of.extentVoxels.zVoxels - 1};
  };
  const auto [centerX, centerZ] = centerTwice(_structure);
  const auto [keepX, keepZ] = centerTwice(*_keep);
  const std::int64_t towardsX = keepX - centerX;
  const std::int64_t towardsZ = keepZ - centerZ;
  const bool alongX = _structure.extentVoxels.xVoxels != _structure.extentVoxels.zVoxels
                        ? _structure.extentVoxels.xVoxels > _structure.extentVoxels.zVoxels
                        : std::abs(towardsZ) >= std::abs(towardsX);
  return alongX ? (((2 * std::int64_t{_cell.zVoxels}) - centerZ) * towardsZ) > 0
                : (((2 * std::int64_t{_cell.xVoxels}) - centerX) * towardsX) > 0;
}

} // namespace

SiegeDefender::SiegeDefender(Core::ByteReader* _saved)
{
  if (_saved == nullptr)
  {
    return;
  }
  if (_saved->ReadU8() != RECORD_VERSION)
  {
    throw std::invalid_argument("the siege defender's record is of a version this build does not read");
  }
  m_sally = static_cast<Sally>(std::min<std::uint8_t>(_saved->ReadU8(), static_cast<std::uint8_t>(Sally::Breach)));
  m_sallyTarget = _saved->ReadU64();
  const std::uint32_t homes = _saved->ReadU32();
  for (std::uint32_t home = 0; home < homes; ++home)
  {
    const Protocol::EntityId squad = _saved->ReadU64();
    const std::int32_t x = _saved->ReadI32();
    const std::int32_t y = _saved->ReadI32();
    m_homes.insert_or_assign(squad, Protocol::VoxelPosition{.xVoxels = x, .yVoxels = y, .zVoxels = _saved->ReadI32()});
  }
  const std::uint32_t answered = _saved->ReadU32();
  for (std::uint32_t section = 0; section < answered; ++section)
  {
    m_answered.insert(_saved->ReadU64());
  }
}

void SiegeDefender::Decide(CommanderView& _view, std::vector<Protocol::Command>& _commands)
{
  AimGarrisons(_view, _commands);
  SendLaborers(_view, _commands);
  RegarrisonArchers(_view, _commands);
  AnswerBreaches(_view, _commands);
  SallyAndGate(_view, _commands);
}

void SiegeDefender::Write(Core::ByteWriter& _writer) const
{
  // Its sally's state, its force's homes in squad order and the sections it answered in id order.
  _writer.WriteU8(RECORD_VERSION);
  _writer.WriteU8(static_cast<std::uint8_t>(m_sally));
  _writer.WriteU64(m_sallyTarget);
  _writer.WriteU32(static_cast<std::uint32_t>(m_homes.size()));
  for (const auto& [squad, home] : m_homes)
  {
    _writer.WriteU64(squad);
    _writer.WriteI32(home.xVoxels);
    _writer.WriteI32(home.yVoxels);
    _writer.WriteI32(home.zVoxels);
  }
  _writer.WriteU32(static_cast<std::uint32_t>(m_answered.size()));
  for (const Protocol::EntityId section : m_answered)
  {
    _writer.WriteU64(section);
  }
}

void SiegeDefender::AnswerBreaches(const CommanderView& _view, std::vector<Protocol::Command>& _commands)
{
  // A section of its own that has opened, the lowest id first, draws the force to its inner mouth, 2 m inside its inner
  // face at its middle, storming, once; the force then holds it (input 18). A sally out or coming back finishes first.
  if (m_sally == Sally::Out || m_sally == Sally::Returning)
  {
    return;
  }
  const Protocol::StateUpdate& state = _view.State();
  const Protocol::StructureView* keep = KeepOf(_view);
  for (const Protocol::StructureView& section : state.structures)
  {
    if (section.owner != _view.Player() || !section.opened || m_answered.contains(section.id) || !CriticalPart(_view, section).empty())
    {
      continue;
    }
    m_answered.insert(section.id);
    m_sally = Sally::Breach;
    const Protocol::VoxelPosition& origin = section.originVoxels;
    const Protocol::VoxelPosition& extent = section.extentVoxels;
    Protocol::VoxelPosition mouth{
      .xVoxels = origin.xVoxels + (extent.xVoxels / 2), .yVoxels = origin.yVoxels, .zVoxels = origin.zVoxels + (extent.zVoxels / 2)};
    const bool alongX = extent.xVoxels >= extent.zVoxels;
    for (const std::int32_t inward : {extent.zVoxels + 1, -2})
    {
      Protocol::VoxelPosition candidate = mouth;
      (alongX ? candidate.zVoxels : candidate.xVoxels) =
        (alongX ? origin.zVoxels : origin.xVoxels) + (alongX ? inward : (inward == -2 ? -2 : extent.xVoxels + 1));
      if (InnerSide(section, keep, candidate))
      {
        mouth = candidate;
        break;
      }
    }
    for (const Protocol::SquadView& squad : state.squads)
    {
      if (squad.owner == _view.Player() && squad.garrisonedIn == Protocol::NO_ENTITY && !squad.units.empty() &&
          (squad.kind == Protocol::UnitKind::ManAtArms || squad.kind == Protocol::UnitKind::Outrider))
      {
        _commands.emplace_back(Protocol::StormTo{.squad = squad.id, .goalVoxels = mouth});
      }
    }
    return;
  }
}

void SiegeDefender::SallyAndGate(const CommanderView& _view, std::vector<Protocol::Command>& _commands)
{
  // The sally force is its men-at-arms and outriders (the route table's sally; input 5). It goes after the nearest enemy
  // engine it sees within sallyLeashVoxels of a section of its curtain, ties to the lower id, while the force has half
  // its figures or more, and comes home when that engine is gone from its sight, the force falls below half, or a figure
  // of it stands past the leash. The gate opens while the force must pass it, and shuts behind it, or at once when an
  // enemy soldier stands within gateShutVoxels of the gatehouse and no figure of the force as near (input 13).
  const Protocol::StateUpdate& state = _view.State();
  const Rules& rules = _view.Data().Tuning();
  const std::int64_t leash = std::int64_t{rules.sallyLeashVoxels} * rules.sallyLeashVoxels;
  const std::int64_t shut = std::int64_t{rules.gateShutVoxels} * rules.gateShutVoxels;
  const Protocol::StructureView* keep = KeepOf(_view);
  std::vector<const Protocol::SquadView*> force;
  std::size_t figures = 0;
  std::size_t full = 0;
  for (const Protocol::SquadView& squad : state.squads)
  {
    if (squad.owner == _view.Player() && squad.garrisonedIn == Protocol::NO_ENTITY &&
        (squad.kind == Protocol::UnitKind::ManAtArms || squad.kind == Protocol::UnitKind::Outrider))
    {
      force.push_back(&squad);
      figures += squad.units.size();
      full += squad.fullFigures;
    }
  }
  const auto cellOf = [&state](Protocol::EntityId _unit) -> std::optional<Protocol::VoxelPosition>
  {
    const auto unit = std::ranges::find(state.units, _unit, &Protocol::UnitView::id);
    return unit == state.units.end() ? std::nullopt : std::optional{unit->cellVoxels};
  };
  for (const Protocol::SquadView* squad : force)
  {
    if (!m_homes.contains(squad->id) && !squad->units.empty())
    {
      if (const std::optional<Protocol::VoxelPosition> lead = cellOf(squad->units.front()); lead.has_value())
      {
        m_homes.emplace(squad->id, *lead);
      }
    }
  }
  const auto fromCurtain = [&](Protocol::VoxelPosition _cell)
  {
    std::int64_t nearest = std::numeric_limits<std::int64_t>::max();
    for (const Protocol::StructureView& section : state.structures)
    {
      if (section.owner == _view.Player() && CriticalPart(_view, section).empty())
      {
        nearest = std::min(nearest, DistanceSquared(section, _cell));
      }
    }
    return nearest;
  };
  const Protocol::StructureView* gate = nullptr;
  for (const Protocol::StructureView& structure : state.structures)
  {
    if (gate == nullptr && structure.owner == _view.Player() && CriticalPart(_view, structure) == "Gate")
    {
      gate = &structure;
    }
  }
  // Where each figure of the force stands: inside the castle, in the gate, or outside, and past the leash.
  bool anyInside = false;
  bool anyOutside = false;
  bool pastLeash = false;
  std::int64_t forceNearGate = std::numeric_limits<std::int64_t>::max();
  for (const Protocol::SquadView* squad : force)
  {
    for (const Protocol::EntityId unit : squad->units)
    {
      const std::optional<Protocol::VoxelPosition> cell = cellOf(unit);
      if (!cell.has_value())
      {
        continue;
      }
      const bool inGate = gate != nullptr && DistanceSquared(*gate, *cell) == 0;
      const bool inside = gate == nullptr || (!inGate && InnerSide(*gate, keep, *cell));
      anyInside = anyInside || inside || inGate;
      anyOutside = anyOutside || !inside || inGate;
      pastLeash = pastLeash || (!inside && fromCurtain(*cell) > leash);
      forceNearGate = gate == nullptr ? forceNearGate : std::min(forceNearGate, DistanceSquared(*gate, *cell));
    }
  }
  const bool strong = full > 0 && 2 * figures >= full;
  const auto engine = std::ranges::find(state.engines, m_sallyTarget, &Protocol::EngineView::id);
  switch (m_sally)
  {
  case Sally::Home:
  {
    const Protocol::EngineView* nearest = nullptr;
    std::int64_t best = 0;
    for (const Protocol::EngineView& candidate : state.engines)
    {
      const std::int64_t distance = fromCurtain(candidate.cellVoxels);
      if (candidate.owner != _view.Player() && distance <= leash && (nearest == nullptr || distance < best))
      {
        nearest = &candidate;
        best = distance;
      }
    }
    if (nearest != nullptr && strong && !force.empty())
    {
      m_sally = Sally::Out;
      m_sallyTarget = nearest->id;
    }
    break;
  }
  case Sally::Out:
    if (engine == state.engines.end() || !strong || pastLeash)
    {
      m_sally = Sally::Returning;
    }
    break;
  case Sally::Returning:
    if (!anyOutside)
    {
      m_sally = Sally::Home;
      m_sallyTarget = Protocol::NO_ENTITY;
    }
    break;
  case Sally::Breach:
    break;
  }
  for (const Protocol::SquadView* squad : force)
  {
    const auto home = m_homes.find(squad->id);
    if (m_sally == Sally::Out && (squad->order != Protocol::SquadOrder::Attack || squad->target != m_sallyTarget))
    {
      _commands.emplace_back(Protocol::AttackTarget{.squad = squad->id, .target = m_sallyTarget});
    }
    else if (m_sally == Sally::Returning && home != m_homes.end() &&
             (squad->order != Protocol::SquadOrder::Move || !squad->hasGoal || squad->goalVoxels != home->second))
    {
      _commands.emplace_back(Protocol::MoveSquad{.squad = squad->id, .goalVoxels = home->second});
    }
  }
  if (gate == nullptr)
  {
    return;
  }
  std::int64_t enemyNearGate = std::numeric_limits<std::int64_t>::max();
  for (const Protocol::UnitView& unit : state.units)
  {
    if (unit.owner != _view.Player() && unit.kind != Protocol::UnitKind::Ram && unit.kind != Protocol::UnitKind::Trebuchet)
    {
      enemyNearGate = std::min(enemyNearGate, DistanceSquared(*gate, unit.cellVoxels));
    }
  }
  const bool passing = (m_sally == Sally::Out && anyInside) || (m_sally == Sally::Returning && anyOutside);
  const bool danger = enemyNearGate <= shut && forceNearGate > enemyNearGate;
  const bool open = passing && !danger && gate->working;
  if (open != gate->gateOpen)
  {
    _commands.emplace_back(Protocol::SetGate{.structure = gate->id, .open = open});
  }
}

std::string_view SiegeDefender::Kind() const noexcept
{
  return KIND;
}

std::unique_ptr<Commander> SiegeDefender::Clone() const
{
  return std::make_unique<SiegeDefender>(*this);
}

void SiegeDefender::AimGarrisons(const CommanderView& _view, std::vector<Protocol::Command>& _commands)
{
  // Each ranged garrison takes the enemy figure it wants most of those its reach from its building covers, from the cell
  // outside its face nearest each and at its stand height, as the simulation measures it (ADR-024, decision 5), the
  // nearest of those, ties to the lower id. It is aimed at that figure's squad only for a sapper, a ram or the trebuchet;
  // otherwise the simulation's nearest stands.
  const Protocol::StateUpdate& state = _view.State();
  const ServerData& data = _view.Data();
  const Rules& rules = data.Tuning();
  for (const Protocol::SquadView& squad : state.squads)
  {
    const UnitStats& stats = data.Stats(squad.kind);
    const auto building = std::ranges::find(state.structures, squad.garrisonedIn, &Protocol::StructureView::id);
    if (squad.owner != _view.Player() || squad.garrisonedIn == Protocol::NO_ENTITY || stats.attack <= 0 || stats.rangeVoxels <= 0 ||
        building == state.structures.end())
    {
      continue;
    }
    const TemplateRules* templateRules = data.FindTemplateRules(building->templateId);
    const std::int32_t standY = building->originVoxels.yVoxels + (templateRules == nullptr ? 0 : templateRules->standHeightVoxels);
    std::optional<std::tuple<int, std::int64_t, Protocol::EntityId>> best;
    Protocol::EntityId wanted = Protocol::NO_ENTITY;
    for (const Protocol::UnitView& enemy : state.units)
    {
      if (enemy.owner == _view.Player())
      {
        continue;
      }
      const std::int64_t distance = DistanceSquared(*building, enemy.cellVoxels);
      const std::int64_t reach =
        ArrowRangeVoxels(stats.rangeVoxels, ElevationSteps(standY, enemy.cellVoxels.yVoxels, rules), rules) + 1; // from the cell outside
      const std::tuple<int, std::int64_t, Protocol::EntityId> key{Priority(enemy.kind), distance, enemy.id};
      if (distance <= reach * reach && (!best.has_value() || key < *best))
      {
        best = key;
        wanted = enemy.squad;
      }
    }
    if (best.has_value() && std::get<0>(*best) < ANYTHING && squad.target != wanted)
    {
      _commands.emplace_back(Protocol::AttackTarget{.squad = squad.id, .target = wanted});
    }
  }
}

void SiegeDefender::SendLaborers(const CommanderView& _view, std::vector<Protocol::Command>& _commands)
{
  // The structures of its own that stand least whole and are not locked, ties to the lower id, each given the nearest
  // laborer not yet given one, ties to the lower id, until the laborers run out (MvpPlan.md, M4, input 18).
  const Protocol::StateUpdate& state = _view.State();
  std::vector<const Protocol::StructureView*> damaged;
  for (const Protocol::StructureView& structure : state.structures)
  {
    if (structure.owner == _view.Player() && structure.standingPercent < 100 && structure.repairLockTicks == 0)
    {
      damaged.push_back(&structure);
    }
  }
  std::ranges::stable_sort(damaged, {}, [](const Protocol::StructureView* _structure) { return _structure->standingPercent; });
  std::vector<const Protocol::SquadView*> laborers;
  for (const Protocol::SquadView& squad : state.squads)
  {
    if (squad.owner == _view.Player() && squad.kind == Protocol::UnitKind::Laborer && squad.garrisonedIn == Protocol::NO_ENTITY &&
        !squad.units.empty())
    {
      laborers.push_back(&squad);
    }
  }
  const auto leadOf = [&state](const Protocol::SquadView& _squad) -> const Protocol::UnitView*
  {
    const auto unit = std::ranges::find(state.units, _squad.units.front(), &Protocol::UnitView::id);
    return unit == state.units.end() ? nullptr : &*unit;
  };
  for (const Protocol::StructureView* structure : damaged)
  {
    const Protocol::SquadView* nearest = nullptr;
    std::int64_t best = 0;
    for (const Protocol::SquadView* laborer : laborers)
    {
      const Protocol::UnitView* lead = leadOf(*laborer);
      const std::int64_t distance =
        lead == nullptr ? std::numeric_limits<std::int64_t>::max() : DistanceSquared(*structure, lead->cellVoxels);
      if (nearest == nullptr || distance < best)
      {
        nearest = laborer;
        best = distance;
      }
    }
    if (nearest == nullptr)
    {
      return;
    }
    std::erase(laborers, nearest);
    if (nearest->order != Protocol::SquadOrder::Repair || nearest->target != structure->id)
    {
      _commands.emplace_back(Protocol::RepairStructure{.laborer = nearest->id, .structure = structure->id});
    }
  }
}

void SiegeDefender::RegarrisonArchers(const CommanderView& _view, std::vector<Protocol::Command>& _commands)
{
  // Archers of its own standing out, and not already on their way into a building, go into the nearest working building
  // of its own with room for them, counting those already on their way there, ties to the lower id.
  const Protocol::StateUpdate& state = _view.State();
  std::map<Protocol::EntityId, std::size_t> coming;
  for (const Protocol::SquadView& squad : state.squads)
  {
    if (squad.owner == _view.Player() && squad.order == Protocol::SquadOrder::Move && squad.garrisonedIn == Protocol::NO_ENTITY)
    {
      coming[squad.target] += squad.units.size();
    }
  }
  for (const Protocol::SquadView& squad : state.squads)
  {
    const bool goingIn =
      squad.order == Protocol::SquadOrder::Move &&
      std::ranges::any_of(state.structures, [&squad](const Protocol::StructureView& _structure) { return _structure.id == squad.target; });
    if (squad.owner != _view.Player() || squad.kind != Protocol::UnitKind::Archer || squad.garrisonedIn != Protocol::NO_ENTITY ||
        squad.units.empty() || goingIn)
    {
      continue;
    }
    const auto lead = std::ranges::find(state.units, squad.units.front(), &Protocol::UnitView::id);
    if (lead == state.units.end())
    {
      continue;
    }
    const Protocol::StructureView* nearest = nullptr;
    std::int64_t best = 0;
    for (const Protocol::StructureView& building : state.structures)
    {
      const std::size_t room = building.garrisonCapacity > building.garrisonCount + coming[building.id]
                                 ? building.garrisonCapacity - building.garrisonCount - coming[building.id]
                                 : 0;
      const std::int64_t distance = DistanceSquared(building, lead->cellVoxels);
      if (building.owner == _view.Player() && building.working && room >= squad.units.size() && (nearest == nullptr || distance < best))
      {
        nearest = &building;
        best = distance;
      }
    }
    if (nearest != nullptr)
    {
      coming[nearest->id] += squad.units.size();
      _commands.emplace_back(Protocol::GarrisonSquad{.squad = squad.id, .structure = nearest->id});
    }
  }
}

} // namespace Simulation
