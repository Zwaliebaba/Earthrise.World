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

} // namespace

SiegeDefender::SiegeDefender(Core::ByteReader* _saved)
{
  if (_saved != nullptr && _saved->ReadU8() != RECORD_VERSION)
  {
    throw std::invalid_argument("the siege defender's record is of a version this build does not read");
  }
}

void SiegeDefender::Decide(CommanderView& _view, std::vector<Protocol::Command>& _commands)
{
  AimGarrisons(_view, _commands);
  SendLaborers(_view, _commands);
  RegarrisonArchers(_view, _commands);
}

void SiegeDefender::Write(Core::ByteWriter& _writer) const
{
  _writer.WriteU8(RECORD_VERSION);
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
