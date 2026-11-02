#include "pch.h"

#include "SiegeAttacker.h"

#include "FirePattern.h"
#include "ServerData.h"

namespace Simulation
{

namespace
{

// The plan's places, measured on the Siege castle (M4-F5), from the keep's flank face and its far end, the end away from
// the gatehouse:
// - the waypoint, 75 m to the flank of the flank face and 52 m short of the south towers' outer faces, 50 m or more from
//   every garrison, so that the engines go round the flank's south tower outside its reach;
// - the first stand, 68 m to the flank and 20 m beyond the far end, 42 m or more from every garrison, with the north flank
//   tower's platform within 43 m;
// - the second, 43 m to the flank and 9 m beyond the far end, which only the north flank tower's garrison reaches, 42.8 m
//   from every other, with every aim of the keep's pattern within 46 m.
// A second engine stands 5 cells beyond the first, away from the gatehouse.
constexpr std::int32_t WAYPOINT_ACROSS_VOXELS = 75;
constexpr std::int32_t WAYPOINT_SHORT_VOXELS = 52;
constexpr std::int32_t FIRST_STAND_ACROSS_VOXELS = 68;
constexpr std::int32_t FIRST_STAND_BEYOND_VOXELS = 20;
constexpr std::int32_t SECOND_STAND_ACROSS_VOXELS = 43;
constexpr std::int32_t SECOND_STAND_BEYOND_VOXELS = 9;
constexpr std::int32_t ENGINE_SPACING_CELLS = 5;
// An engine counts as at its stand within 2 cells of it, as far as the machines' width puts it, or within 6 once it can get
// no nearer.
constexpr std::int32_t AT_STAND_CELLS = 2;
constexpr std::int32_t NEAR_STAND_CELLS = 6;
// The keep's foot it holds: 2 cells out from the keep's far face, from 1 short of its flank end, six places 2 cells apart
// along it and then a second row a cell nearer, where only the keep's and the two north towers' garrisons reach. The
// squads walk there without stopping to fight, since a storm that engages turns aside into the towers' reach; there a
// squad falls on an enemy figure within 6 cells of its lead.
constexpr std::int32_t HOLD_OUT_CELLS = 2;
constexpr std::int32_t HOLD_SPACING_CELLS = 2;
constexpr std::int32_t HOLD_PLACES = 6;
constexpr std::int64_t HUNT_REACH_CELLS = 6;
// The storm gathers first, 14 cells out from the section's outer face, beyond the ditch, and goes in together once a figure
// of every storming squad stands within 6 cells of that point, or 30 s after it began to gather.
constexpr std::int32_t GATHER_OUT_CELLS = 14;
constexpr std::int64_t GATHERED_CELLS = 6;
constexpr std::uint32_t GATHER_TICKS = 600;
// While the storming clock is contested, a defender stands somewhere in the keep's ring: the first squad of the storm that
// stands within 6 cells of the keep sweeps it, corner to corner 2 cells out from the keep, falling on an enemy figure within
// 8 cells; a corner counts as reached within 3.
constexpr std::int32_t SWEEPERS = 1;
constexpr std::int64_t SWEEP_REACH_CELLS = 8;
constexpr std::int64_t SWEEP_ARRIVED_CELLS = 3;
constexpr std::int32_t AT_KEEP_CELLS = 6;
// The squads not storming stand in rows of four behind the first trebuchet, from 4 cells, 3 a row, two columns either side
// of a lane the engines come up by, around the trebuchet where it stands while the engines move, so that a sally finds
// them beside it; a squad is sent on again once its place has moved 4 cells. They fall on an enemy figure within 35 m of
// an engine.
constexpr std::int32_t ESCORT_BEHIND_CELLS = 4;
constexpr std::int32_t ESCORT_ROW_CELLS = 3;
constexpr std::array<std::int32_t, 4> ESCORT_ACROSS_CELLS = {-5, 9, -9, 13};
constexpr std::int64_t ESCORT_REACH_CELLS = 35;
constexpr std::int64_t ESCORT_FOLLOW_CELLS = 4;
// Once a structure it fires at has stopped, it fires 6 more shots at it, about 14 voxels: stopped at its threshold, a tower
// works again once repair puts one voxel back, a voxel each 12.5 s it is not locked (measured, M4-F5).
constexpr std::uint32_t OVERSHOOT_SHOTS = 6;

/// Twice a structure's middle across the ground, so that it is whole.
std::pair<std::int64_t, std::int64_t> MiddleTwice(const Protocol::StructureView& _structure)
{
  return {(2 * std::int64_t{_structure.originVoxels.xVoxels}) + _structure.extentVoxels.xVoxels - 1,
          (2 * std::int64_t{_structure.originVoxels.zVoxels}) + _structure.extentVoxels.zVoxels - 1};
}

std::int64_t DistanceSquared(Protocol::VoxelPosition _from, Protocol::VoxelPosition _to)
{
  const std::int64_t dx = std::int64_t{_to.xVoxels} - _from.xVoxels;
  const std::int64_t dz = std::int64_t{_to.zVoxels} - _from.zVoxels;
  return (dx * dx) + (dz * dz);
}

} // namespace

/// Its keep, the towers it stops, the section it opens, and which way the flank and the gatehouse lie.
struct SiegeAttacker::Castle
{
  const Protocol::StructureView* keep = nullptr;
  const Protocol::StructureView* middle = nullptr;     // ST, which names the flank
  const Protocol::StructureView* northFlank = nullptr; // the tower on the flank beyond the keep's far end
  const Protocol::StructureView* section = nullptr;    // the flank curtain's section nearest the keep's foot it holds
  std::int32_t flank = 1;                              // +1 east of the gatehouse, -1 west
  std::int32_t south = -1;                             // the gatehouse's side of the keep along z: -1 for lower z
  std::int32_t flankFaceX = 0;                         // the keep's flank face
  std::int32_t farEndZ = 0;                            // the keep's end away from the gatehouse
  std::int32_t southFaceZ = 0;                         // ST's outer face
  std::int32_t groundY = 0;
};

std::optional<SiegeAttacker::Castle> SiegeAttacker::ReadCastle(const CommanderView& _view)
{
  const Protocol::StateUpdate& state = _view.State();
  Castle castle;
  const Protocol::StructureView* gate = nullptr;
  for (const Protocol::StructureView& structure : state.structures)
  {
    castle.keep = structure.id == state.trial.keep ? &structure : castle.keep;
    castle.middle = structure.name == "ST" ? &structure : castle.middle;
    gate = structure.name == "GH" ? &structure : gate;
  }
  if (castle.keep == nullptr || castle.middle == nullptr || gate == nullptr)
  {
    return std::nullopt;
  }
  const auto [keepX, keepZ] = MiddleTwice(*castle.keep);
  const auto [gateX, gateZ] = MiddleTwice(*gate);
  castle.flank = MiddleTwice(*castle.middle).first >= gateX ? 1 : -1;
  castle.south = gateZ < keepZ ? -1 : 1;
  castle.flankFaceX =
    castle.flank > 0 ? castle.keep->originVoxels.xVoxels + castle.keep->extentVoxels.xVoxels - 1 : castle.keep->originVoxels.xVoxels;
  castle.farEndZ =
    castle.south < 0 ? castle.keep->originVoxels.zVoxels + castle.keep->extentVoxels.zVoxels - 1 : castle.keep->originVoxels.zVoxels;
  castle.southFaceZ =
    castle.south < 0 ? castle.middle->originVoxels.zVoxels : castle.middle->originVoxels.zVoxels + castle.middle->extentVoxels.zVoxels - 1;
  castle.groundY = castle.keep->originVoxels.yVoxels;
  // The flank curtain's far corner, where the north flank tower stands: far along z from the gatehouse, out along x.
  const Protocol::VoxelPosition corner{
    .xVoxels = castle.flankFaceX + (castle.flank * 1000), .yVoxels = castle.groundY, .zVoxels = castle.farEndZ - (castle.south * 1000)};
  std::int64_t nearestSection = std::numeric_limits<std::int64_t>::max();
  for (const Protocol::StructureView& structure : state.structures)
  {
    const auto [x, z] = MiddleTwice(structure);
    const ServerTemplate* found = _view.Data().FindTemplate(structure.templateId);
    if (found == nullptr || structure.owner == _view.Player() || (x - keepX) * castle.flank <= 0)
    {
      continue;
    }
    // The north tower on the flank: a tower, as ST is, beyond the keep's far end.
    if (structure.templateId == castle.middle->templateId && (z - keepZ) * castle.south < 0)
    {
      castle.northFlank = &structure;
    }
    // The section: a structure whose template names no critical part, beyond the keep's flank face and running along z as
    // the flank curtain does, nearest the north flank tower's corner, whose inner mouth only the keep's and that tower's
    // garrisons reach.
    const Protocol::VoxelPosition middle{
      .xVoxels = static_cast<std::int32_t>(x / 2), .yVoxels = castle.groundY, .zVoxels = static_cast<std::int32_t>(z / 2)};
    const std::int64_t distance = DistanceSquared(corner, middle);
    if (found->criticalPart.empty() && structure.extentVoxels.zVoxels > structure.extentVoxels.xVoxels &&
        (x - (2 * std::int64_t{castle.flankFaceX})) * castle.flank > 0 && distance < nearestSection)
    {
      castle.section = &structure;
      nearestSection = distance;
    }
  }
  if (castle.northFlank == nullptr || castle.section == nullptr)
  {
    return std::nullopt;
  }
  return castle;
}

SiegeAttacker::SiegeAttacker(Core::ByteReader* _saved)
{
  if (_saved == nullptr)
  {
    return;
  }
  if (_saved->ReadU8() != RECORD_VERSION)
  {
    throw std::invalid_argument("the siege attacker's record is of a version this build does not read");
  }
  m_phase = static_cast<Phase>(std::min<std::uint8_t>(_saved->ReadU8(), static_cast<std::uint8_t>(Phase::Storm)));
  m_target = _saved->ReadU64();
  m_overshoot = _saved->ReadU32();
  m_sweep = _saved->ReadU32();
  m_stormIn = _saved->ReadU8() != 0;
  m_gatherStartTick = _saved->ReadU32();
  const std::uint32_t targets = _saved->ReadU32();
  for (std::uint32_t target = 0; target < targets; ++target)
  {
    const Protocol::EntityId structure = _saved->ReadU64();
    m_shots.insert_or_assign(structure, _saved->ReadU32());
  }
  const std::uint32_t pending = _saved->ReadU32();
  for (std::uint32_t engine = 0; engine < pending; ++engine)
  {
    const Protocol::EntityId id = _saved->ReadU64();
    m_pending.insert_or_assign(id, _saved->ReadU64());
  }
}

Protocol::VoxelPosition SiegeAttacker::Aim(const CommanderView& _view, const Castle& /*_castle*/, const Protocol::StructureView& _target,
                                           Protocol::VoxelPosition _stand, std::uint32_t _shot)
{
  // Every target by its template's pattern, the section's from its middle, whose slot brings down what stood over it.
  return PatternAim(_view.Data(), _target, _stand, _shot);
}

const Protocol::StructureView* SiegeAttacker::Target(const CommanderView& _view, const Castle& _castle)
{
  // At the first stand the north flank tower; at the second that again whenever it works, then the keep, then the section.
  // Done with a section once it is open, until the storm, which keeps firing at it whenever nothing else needs stopping
  // again, so that its lock holds off the laborer who would close the breach behind the storm; with a structure with a
  // critical part once it has stopped and, while it is the one fired at, once OVERSHOOT_SHOTS more have flown at it since.
  const auto done = [&_view, this](const Protocol::StructureView& _structure)
  {
    const ServerTemplate* found = _view.Data().FindTemplate(_structure.templateId);
    if (found == nullptr || found->criticalPart.empty())
    {
      return _structure.opened && m_phase != Phase::Storm;
    }
    return !_structure.working && (_structure.id != m_target || m_overshoot >= OVERSHOOT_SHOTS);
  };
  const std::array<const Protocol::StructureView*, 3> plan = {_castle.northFlank, _castle.keep, _castle.section};
  const std::size_t targets = m_phase == Phase::FirstStand ? 1 : plan.size();
  // The first in the plan's order that is not done, so that a tower working again comes before the keep; the one it fires
  // at keeps it until its margin even once it has stopped, over those after it.
  const Protocol::StructureView* target = nullptr;
  for (std::size_t at = 0; at < targets && target == nullptr; ++at)
  {
    // The section waits for the keep: open, it would let the defender's reserve out and the storm in too early.
    if (plan.at(at) == _castle.section && !done(*_castle.keep))
    {
      continue;
    }
    target = done(*plan.at(at)) ? nullptr : plan.at(at);
  }
  const Protocol::EntityId chosen = target == nullptr ? Protocol::NO_ENTITY : target->id;
  m_overshoot = chosen == m_target ? m_overshoot : 0;
  m_target = chosen;
  return target;
}

void SiegeAttacker::Decide(CommanderView& _view, std::vector<Protocol::Command>& _commands)
{
  const Protocol::StateUpdate& state = _view.State();
  const std::optional<Castle> read = ReadCastle(_view);
  if (!read.has_value())
  {
    return;
  }
  const Castle& castle = *read;
  const std::int32_t beyond = -castle.south;
  Protocol::VoxelPosition stand{.xVoxels = castle.flankFaceX + (castle.flank * SECOND_STAND_ACROSS_VOXELS),
                                .yVoxels = castle.groundY,
                                .zVoxels = castle.farEndZ + (beyond * SECOND_STAND_BEYOND_VOXELS)};
  if (m_phase == Phase::ToWaypoint)
  {
    stand = {.xVoxels = castle.flankFaceX + (castle.flank * WAYPOINT_ACROSS_VOXELS),
             .yVoxels = castle.groundY,
             .zVoxels = castle.southFaceZ + (castle.south * WAYPOINT_SHORT_VOXELS)};
  }
  else if (m_phase == Phase::ToFirstStand || m_phase == Phase::FirstStand)
  {
    stand = {.xVoxels = castle.flankFaceX + (castle.flank * FIRST_STAND_ACROSS_VOXELS),
             .yVoxels = castle.groundY,
             .zVoxels = castle.farEndZ + (beyond * FIRST_STAND_BEYOND_VOXELS)};
  }
  const auto standOf = [&stand, beyond](std::int32_t _engine)
  {
    Protocol::VoxelPosition at = stand;
    at.zVoxels += beyond * _engine * ENGINE_SPACING_CELLS;
    return at;
  };

  // The trebuchets: each to its stand, deployed there, and firing at the plan's target.
  std::vector<const Protocol::EngineView*> engines;
  for (const Protocol::EngineView& engine : state.engines)
  {
    if (engine.owner == _view.Player() && engine.kind == Protocol::EngineKind::Trebuchet)
    {
      engines.push_back(&engine);
    }
  }
  bool allDeployed = !engines.empty();
  std::vector<std::pair<const Protocol::EngineView*, Protocol::VoxelPosition>> ready;
  for (std::size_t index = 0; index < engines.size(); ++index)
  {
    const Protocol::EngineView& engine = *engines.at(index);
    const Protocol::VoxelPosition at = standOf(static_cast<std::int32_t>(index));
    const auto body = std::ranges::find(state.squads, engine.squad, &Protocol::SquadView::id);
    const bool nearStand = std::abs(engine.cellVoxels.xVoxels - at.xVoxels) <= NEAR_STAND_CELLS &&
                           std::abs(engine.cellVoxels.zVoxels - at.zVoxels) <= NEAR_STAND_CELLS;
    const bool arrived =
      (std::abs(engine.cellVoxels.xVoxels - at.xVoxels) <= AT_STAND_CELLS &&
       std::abs(engine.cellVoxels.zVoxels - at.zVoxels) <= AT_STAND_CELLS) ||
      (nearStand && body != state.squads.end() && (body->order != Protocol::SquadOrder::Move || !body->hasGoal || body->shortOfGoal));
    if (m_phase == Phase::ToWaypoint)
    {
      // Passing through: packed, on to the first stand once every engine is near the waypoint.
      allDeployed = allDeployed && nearStand;
      if (engine.state == Protocol::EngineState::Packed && body != state.squads.end() && !nearStand &&
          (body->order != Protocol::SquadOrder::Move || body->goalVoxels != at))
      {
        _commands.emplace_back(Protocol::MoveSquad{.squad = body->id, .goalVoxels = at});
      }
      continue;
    }
    if (engine.state == Protocol::EngineState::Deployed && !arrived)
    {
      _commands.emplace_back(Protocol::PackEngine{.engine = engine.id});
    }
    else if (engine.state == Protocol::EngineState::Packed && !arrived && body != state.squads.end() &&
             (body->order != Protocol::SquadOrder::Move || body->goalVoxels != at))
    {
      _commands.emplace_back(Protocol::MoveSquad{.squad = body->id, .goalVoxels = at});
    }
    else if (engine.state == Protocol::EngineState::Packed && arrived)
    {
      _commands.emplace_back(Protocol::DeployEngine{.engine = engine.id, .quarterTurns = 0});
    }
    const bool deployedHere = engine.state == Protocol::EngineState::Deployed && arrived;
    allDeployed = allDeployed && deployedHere;
    if (deployedHere)
    {
      ready.emplace_back(&engine, at);
    }
  }
  if (allDeployed)
  {
    m_phase = m_phase == Phase::ToWaypoint      ? Phase::ToFirstStand
              : m_phase == Phase::ToFirstStand  ? Phase::FirstStand
              : m_phase == Phase::ToSecondStand ? Phase::SecondStand
                                                : m_phase;
  }
  if (!ready.empty() && m_phase != Phase::ToWaypoint && m_phase != Phase::ToFirstStand && m_phase != Phase::ToSecondStand)
  {
    const Protocol::StructureView* target = Target(_view, castle);
    if (target == nullptr)
    {
      m_phase = m_phase == Phase::FirstStand ? Phase::ToSecondStand : Phase::Storm;
    }
    for (const auto& [engine, at] : ready)
    {
      // A pattern's shot counts once its stone flew: a refused one, out of range, is aimed again.
      const bool inFlight = std::ranges::any_of(state.shots, [engine](const Protocol::ShotView& _shot)
                                                { return _shot.engine == engine->id && _shot.kind == Protocol::ShotKind::Stone; });
      const auto pending = m_pending.find(engine->id);
      if (pending != m_pending.end() && (engine->reloadTicks > 0 || inFlight))
      {
        ++m_shots[pending->second];
        // A shot at the target it keeps on after it stopped counts towards the overshoot.
        const auto struck = std::ranges::find(state.structures, pending->second, &Protocol::StructureView::id);
        m_overshoot += pending->second == m_target && struck != state.structures.end() && !struck->working ? 1 : 0;
      }
      m_pending.erase(engine->id);
      if (target != nullptr && engine->reloadTicks == 0 && !inFlight)
      {
        _commands.emplace_back(
          Protocol::FireEngine{.engine = engine->id, .targetVoxels = Aim(_view, castle, *target, at, m_shots[target->id])});
        m_pending.insert_or_assign(engine->id, target->id);
      }
    }
  }

  // The squads: the storm's to the keep's flank foot once the plan storms, holding there; the rest behind the trebuchets,
  // falling on what comes near them.
  // A squad's figure nearest _at: a squad strung out on the march has stragglers, and its first figure may be one.
  const auto figureNearest = [&state](const Protocol::SquadView& _squad, Protocol::VoxelPosition _at) -> const Protocol::UnitView*
  {
    const Protocol::UnitView* nearest = nullptr;
    for (const Protocol::EntityId id : _squad.units)
    {
      const auto unit = std::ranges::find(state.units, id, &Protocol::UnitView::id);
      if (unit != state.units.end() &&
          (nearest == nullptr || DistanceSquared(unit->cellVoxels, _at) < DistanceSquared(nearest->cellVoxels, _at)))
      {
        nearest = &*unit;
      }
    }
    return nearest;
  };
  const auto [keepMiddleX, keepMiddleZ] = MiddleTwice(*castle.keep);
  const Protocol::VoxelPosition keepMiddle{.xVoxels = static_cast<std::int32_t>(keepMiddleX / 2),
                                           .yVoxels = castle.groundY,
                                           .zVoxels = static_cast<std::int32_t>(keepMiddleZ / 2)};
  const auto nearestEnemy = [&state, &_view](Protocol::VoxelPosition _at, std::int64_t _reach) -> const Protocol::UnitView*
  {
    const Protocol::UnitView* nearest = nullptr;
    for (const Protocol::UnitView& unit : state.units)
    {
      const std::int64_t distance = DistanceSquared(_at, unit.cellVoxels);
      if (unit.owner != _view.Player() && distance <= _reach * _reach &&
          (nearest == nullptr || distance < DistanceSquared(_at, nearest->cellVoxels)))
      {
        nearest = &unit;
      }
    }
    return nearest;
  };
  const Protocol::UnitView* threat = nullptr;
  for (const Protocol::EngineView* engine : engines)
  {
    const Protocol::UnitView* enemy = nearestEnemy(engine->cellVoxels, ESCORT_REACH_CELLS);
    threat = threat == nullptr ? enemy : threat;
  }
  const bool moving = m_phase == Phase::ToWaypoint || m_phase == Phase::ToFirstStand || m_phase == Phase::ToSecondStand;
  const Protocol::VoxelPosition guarded = moving && !engines.empty() ? engines.front()->cellVoxels : stand;
  const auto [keepX, keepZ] = MiddleTwice(*castle.keep);
  const std::int64_t awayX = (2 * std::int64_t{guarded.xVoxels}) - keepX;
  const std::int64_t awayZ = (2 * std::int64_t{guarded.zVoxels}) - keepZ;
  const bool behindAlongX = std::abs(awayX) > std::abs(awayZ);
  const std::int32_t behindSign = (behindAlongX ? awayX : awayZ) >= 0 ? 1 : -1;
  std::int32_t escorts = 0;
  std::int32_t storming = 0;
  std::int32_t sweeping = 0;
  const auto [sectionX, sectionZ] = MiddleTwice(*castle.section);
  const std::int32_t sectionOuterX = castle.flank > 0 ? castle.section->originVoxels.xVoxels + castle.section->extentVoxels.xVoxels - 1
                                                      : castle.section->originVoxels.xVoxels;
  const Protocol::VoxelPosition gather{.xVoxels = sectionOuterX + (castle.flank * GATHER_OUT_CELLS),
                                       .yVoxels = castle.groundY,
                                       .zVoxels = static_cast<std::int32_t>(sectionZ / 2)};
  if (m_phase == Phase::Storm && !m_stormIn)
  {
    m_gatherStartTick = m_gatherStartTick == NOT_STARTED ? _view.Tick() : m_gatherStartTick;
    bool gathered = true;
    for (const Protocol::SquadView& squad : state.squads)
    {
      const bool stormer = squad.owner == _view.Player() && squad.garrisonedIn == Protocol::NO_ENTITY && !squad.units.empty() &&
                           squad.kind == Protocol::UnitKind::ManAtArms;
      const Protocol::UnitView* lead = stormer ? figureNearest(squad, gather) : nullptr;
      gathered = gathered && (lead == nullptr || DistanceSquared(lead->cellVoxels, gather) <= GATHERED_CELLS * GATHERED_CELLS);
    }
    m_stormIn = gathered || _view.Tick() - m_gatherStartTick >= GATHER_TICKS;
  }
  static_cast<void>(sectionX);
  for (const Protocol::SquadView& squad : state.squads)
  {
    const bool combat = squad.kind == Protocol::UnitKind::ManAtArms || squad.kind == Protocol::UnitKind::Spearman ||
                        squad.kind == Protocol::UnitKind::Archer || squad.kind == Protocol::UnitKind::Outrider;
    const bool stormingNow = squad.kind == Protocol::UnitKind::ManAtArms && m_phase == Phase::Storm;
    const Protocol::UnitView* lead =
      squad.units.empty() ? nullptr : figureNearest(squad, stormingNow ? (m_stormIn ? keepMiddle : gather) : guarded);
    if (squad.owner != _view.Player() || !combat || lead == nullptr || squad.garrisonedIn != Protocol::NO_ENTITY)
    {
      continue;
    }
    // The men-at-arms storm: their armour takes an arrow for 1, where a spearman takes 5.
    const bool foot = squad.kind == Protocol::UnitKind::ManAtArms;
    if (foot && m_phase == Phase::Storm)
    {
      const std::int32_t index = storming++;
      if (!m_stormIn)
      {
        const Protocol::VoxelPosition slot{.xVoxels = gather.xVoxels + (castle.flank * (index / 4) * ESCORT_ROW_CELLS),
                                           .yVoxels = castle.groundY,
                                           .zVoxels = gather.zVoxels + (((index % 4) * 3) - 4)};
        if ((squad.order != Protocol::SquadOrder::Move || squad.goalVoxels != slot) && DistanceSquared(lead->cellVoxels, slot) > 4)
        {
          _commands.emplace_back(Protocol::MoveSquad{.squad = squad.id, .goalVoxels = slot});
        }
        continue;
      }
      const auto outside = [](std::int32_t _at, std::int32_t _low, std::int32_t _extent)
      { return std::max({_low - _at, 0, _at - (_low + _extent - 1)}); };
      const bool atKeep =
        std::max(outside(lead->cellVoxels.xVoxels, castle.keep->originVoxels.xVoxels, castle.keep->extentVoxels.xVoxels),
                 outside(lead->cellVoxels.zVoxels, castle.keep->originVoxels.zVoxels, castle.keep->extentVoxels.zVoxels)) <= AT_KEEP_CELLS;
      if (state.trial.holdContested && atKeep && sweeping < SWEEPERS)
      {
        const bool leadSweeper = sweeping == 0;
        ++sweeping;
        // Round the keep's ring: the far corner away from the flank, the near one, the near one on the flank, the far one.
        const std::int32_t otherFaceX =
          castle.flank > 0 ? castle.keep->originVoxels.xVoxels : castle.keep->originVoxels.xVoxels + castle.keep->extentVoxels.xVoxels - 1;
        const std::int32_t southEndZ = castle.farEndZ + (castle.south * (castle.keep->extentVoxels.zVoxels - 1));
        const std::array<Protocol::VoxelPosition, 4> corners = {
          Protocol::VoxelPosition{.xVoxels = otherFaceX - (castle.flank * HOLD_OUT_CELLS),
                                  .yVoxels = castle.groundY,
                                  .zVoxels = castle.farEndZ + (beyond * HOLD_OUT_CELLS)},
          Protocol::VoxelPosition{.xVoxels = otherFaceX - (castle.flank * HOLD_OUT_CELLS),
                                  .yVoxels = castle.groundY,
                                  .zVoxels = southEndZ + (castle.south * HOLD_OUT_CELLS)},
          Protocol::VoxelPosition{.xVoxels = castle.flankFaceX + (castle.flank * HOLD_OUT_CELLS),
                                  .yVoxels = castle.groundY,
                                  .zVoxels = southEndZ + (castle.south * HOLD_OUT_CELLS)},
          Protocol::VoxelPosition{.xVoxels = castle.flankFaceX + (castle.flank * HOLD_OUT_CELLS),
                                  .yVoxels = castle.groundY,
                                  .zVoxels = castle.farEndZ + (beyond * HOLD_OUT_CELLS)}};
        const Protocol::VoxelPosition corner = corners.at(m_sweep % corners.size());
        const Protocol::UnitView* enemy = nearestEnemy(lead->cellVoxels, SWEEP_REACH_CELLS);
        if (enemy != nullptr)
        {
          if (squad.order != Protocol::SquadOrder::Attack || squad.target != enemy->squad)
          {
            _commands.emplace_back(Protocol::AttackTarget{.squad = squad.id, .target = enemy->squad});
          }
        }
        else if (DistanceSquared(lead->cellVoxels, corner) <= SWEEP_ARRIVED_CELLS * SWEEP_ARRIVED_CELLS)
        {
          m_sweep = leadSweeper ? m_sweep + 1 : m_sweep;
        }
        else if (squad.order != Protocol::SquadOrder::Move || squad.goalVoxels != corner)
        {
          _commands.emplace_back(Protocol::MoveSquad{.squad = squad.id, .goalVoxels = corner});
        }
        continue;
      }
      const Protocol::VoxelPosition goal{.xVoxels = castle.flankFaceX - (castle.flank * (1 + ((index % HOLD_PLACES) * HOLD_SPACING_CELLS))),
                                         .yVoxels = castle.groundY,
                                         .zVoxels = castle.farEndZ + (beyond * (HOLD_OUT_CELLS - (index / HOLD_PLACES)))};
      const bool there = std::abs(lead->cellVoxels.xVoxels - goal.xVoxels) <= HOLD_SPACING_CELLS &&
                         std::abs(lead->cellVoxels.zVoxels - goal.zVoxels) <= HOLD_SPACING_CELLS;
      const Protocol::UnitView* enemy = there ? nearestEnemy(lead->cellVoxels, HUNT_REACH_CELLS) : nullptr;
      if (enemy != nullptr)
      {
        if (squad.order != Protocol::SquadOrder::Attack || squad.target != enemy->squad)
        {
          _commands.emplace_back(Protocol::AttackTarget{.squad = squad.id, .target = enemy->squad});
        }
      }
      else if (there && squad.order != Protocol::SquadOrder::Hold)
      {
        _commands.emplace_back(Protocol::HoldSquad{.squad = squad.id});
      }
      else if (!there && (squad.order != Protocol::SquadOrder::Move || squad.goalVoxels != goal))
      {
        _commands.emplace_back(Protocol::MoveSquad{.squad = squad.id, .goalVoxels = goal});
      }
      continue;
    }
    if (threat != nullptr)
    {
      if (squad.order != Protocol::SquadOrder::Attack || squad.target != threat->squad)
      {
        _commands.emplace_back(Protocol::AttackTarget{.squad = squad.id, .target = threat->squad});
      }
      continue;
    }
    const auto columns = static_cast<std::int32_t>(ESCORT_ACROSS_CELLS.size());
    const std::int32_t row = escorts / columns;
    const std::int32_t across = ESCORT_ACROSS_CELLS.at(static_cast<std::size_t>(escorts % columns));
    ++escorts;
    const std::int32_t behind = behindSign * (ESCORT_BEHIND_CELLS + (row * ESCORT_ROW_CELLS));
    const Protocol::VoxelPosition slot{.xVoxels = guarded.xVoxels + (behindAlongX ? behind : across),
                                       .yVoxels = castle.groundY,
                                       .zVoxels = guarded.zVoxels + (behindAlongX ? across : behind)};
    const bool sent =
      squad.order == Protocol::SquadOrder::Move && DistanceSquared(squad.goalVoxels, slot) <= ESCORT_FOLLOW_CELLS * ESCORT_FOLLOW_CELLS;
    if (!sent && DistanceSquared(lead->cellVoxels, slot) > ESCORT_FOLLOW_CELLS * ESCORT_FOLLOW_CELLS)
    {
      _commands.emplace_back(Protocol::MoveSquad{.squad = squad.id, .goalVoxels = slot});
    }
  }
}

void SiegeAttacker::Write(Core::ByteWriter& _writer) const
{
  // Its phase, the structure it fires at and its overshoot, the shots flown at each structure in id order, and each engine's shot not yet
  // seen to fly, in engine order.
  _writer.WriteU8(RECORD_VERSION);
  _writer.WriteU8(static_cast<std::uint8_t>(m_phase));
  _writer.WriteU64(m_target);
  _writer.WriteU32(m_overshoot);
  _writer.WriteU32(m_sweep);
  _writer.WriteU8(m_stormIn ? 1 : 0);
  _writer.WriteU32(m_gatherStartTick);
  _writer.WriteU32(static_cast<std::uint32_t>(m_shots.size()));
  for (const auto& [structure, shots] : m_shots)
  {
    _writer.WriteU64(structure);
    _writer.WriteU32(shots);
  }
  _writer.WriteU32(static_cast<std::uint32_t>(m_pending.size()));
  for (const auto& [engine, target] : m_pending)
  {
    _writer.WriteU64(engine);
    _writer.WriteU64(target);
  }
}

std::string_view SiegeAttacker::Kind() const noexcept
{
  return KIND;
}

std::unique_ptr<Commander> SiegeAttacker::Clone() const
{
  return std::make_unique<SiegeAttacker>(*this);
}

} // namespace Simulation
