#include "pch.h"

#include "AlertWatch.h"

#include "Overlays.h"
#include "Strings.h"
#include "TickClock.h"

#include <limits>

namespace Client
{

namespace
{

// The keys the alerts are raised under, apart from the collapses', whose ids are below 2^32.
constexpr std::uint64_t HIT_KEY = std::uint64_t{1} << 32U;
constexpr std::uint64_t REFUSAL_KEY = std::uint64_t{2} << 32U;

/// The middle of a structure's footprint, at its ground.
Protocol::VoxelPosition FootOf(const Protocol::StructureView& _structure) noexcept
{
  return {.xVoxels = _structure.originVoxels.xVoxels + (_structure.extentVoxels.xVoxels / 2),
          .yVoxels = _structure.originVoxels.yVoxels,
          .zVoxels = _structure.originVoxels.zVoxels + (_structure.extentVoxels.zVoxels / 2)};
}

/// The squared distance from _voxels to _structure's box, 0 inside it.
std::int64_t DistanceSquared(const Protocol::StructureView& _structure, const Protocol::VoxelPosition& _voxels) noexcept
{
  const auto outside = [](std::int32_t _at, std::int32_t _origin, std::int32_t _extent) -> std::int64_t
  {
    if (_at < _origin)
    {
      return _origin - _at;
    }
    return _at >= _origin + _extent ? _at - (_origin + _extent - 1) : 0;
  };
  const std::int64_t x = outside(_voxels.xVoxels, _structure.originVoxels.xVoxels, _structure.extentVoxels.xVoxels);
  const std::int64_t y = outside(_voxels.yVoxels, _structure.originVoxels.yVoxels, _structure.extentVoxels.yVoxels);
  const std::int64_t z = outside(_voxels.zVoxels, _structure.originVoxels.zVoxels, _structure.extentVoxels.zVoxels);
  return (x * x) + (y * y) + (z * z);
}

/// The owner of shot _shot, from the update it hit in or the one before, where a shot that hit in its tick last stood.
std::optional<std::uint8_t> ShotOwner(const Session& _session, Protocol::EntityId _shot)
{
  for (const Protocol::StateUpdate* state : {&_session.State(), &_session.PreviousState()})
  {
    const auto shot = std::ranges::find(state->shots, _shot, &Protocol::ShotView::id);
    if (shot != state->shots.end())
    {
      return shot->owner;
    }
  }
  return std::nullopt;
}

} // namespace

void AlertWatch::Observe(const Session& _session, double _nowSeconds, HudAlerts& _alerts)
{
  const Protocol::StateUpdate& state = _session.State();
  if (m_tick == state.tick)
  {
    return;
  }
  m_tick = state.tick;
  const double expires = _nowSeconds + HudAlerts::LIFETIME_SECONDS;

  // Enemy sappers, each squad the first time it is seen.
  for (const Protocol::SquadView& squad : state.squads)
  {
    if (squad.owner == state.player || squad.kind != Protocol::UnitKind::Sapper || m_sappersSeen.contains(squad.id))
    {
      continue;
    }
    const auto unit = std::ranges::find(state.units, squad.id, &Protocol::UnitView::squad);
    if (unit == state.units.end())
    {
      continue; // its squad is known, but no figure of it is in sight
    }
    m_sappersSeen.insert(squad.id);
    const std::optional<std::string> nearestName = NearestName(state, unit->cellVoxels);
    _alerts.Push({.text = nearestName.has_value() ? std::format(SAPPERS_AT_ALERT, *nearestName) : std::string(SAPPERS_SEEN_ALERT),
                  .sub = "",
                  .location = unit->cellVoxels,
                  .urgent = false,
                  .expiresSeconds = expires});
  }

  // A route through a structure, the first time it reads opened.
  for (const Protocol::StructureView& structure : state.structures)
  {
    if (!structure.opened || m_opened.contains(structure.id))
    {
      continue;
    }
    m_opened.insert(structure.id);
    _alerts.Push({.text = std::format(BREACH_OPEN_AT_ALERT, structure.name.empty() ? structure.templateId : structure.name),
                  .sub = "",
                  .location = FootOf(structure),
                  .urgent = false,
                  .expiresSeconds = expires});
  }

  // The player's shots that landed on an enemy's structure and locked its repair.
  for (const Protocol::ShotHitView& hit : state.hits)
  {
    if (ShotOwner(_session, hit.shot) != state.player)
    {
      continue;
    }
    const Protocol::EntityId id = Overlays::SectionAt(state, hit.voxel);
    const auto structure = std::ranges::find(state.structures, id, &Protocol::StructureView::id);
    if (structure == state.structures.end() || structure->owner == state.player || structure->repairLockTicks == 0)
    {
      continue;
    }
    const std::uint32_t seconds = (structure->repairLockTicks + TickClock::TICKS_PER_SECOND - 1) / TickClock::TICKS_PER_SECOND;
    _alerts.Raise(HIT_KEY | structure->id,
                  {.text = std::format(HIT_LOCKED_ALERT, structure->name.empty() ? structure->templateId : structure->name, seconds),
                   .sub = "",
                   .location = hit.voxel,
                   .urgent = false,
                   .expiresSeconds = expires});
  }

  // The refusals for want of stone or timber.
  for (const Protocol::RefusalView& refusal : state.refusals)
  {
    if (refusal.reason != Protocol::Refusal::NotEnoughStone && refusal.reason != Protocol::Refusal::NotEnoughTimber)
    {
      continue;
    }
    _alerts.Raise(
      REFUSAL_KEY | static_cast<std::uint64_t>(refusal.reason),
      {.text = std::string(refusal.reason == Protocol::Refusal::NotEnoughStone ? NOT_ENOUGH_STONE_ALERT : NOT_ENOUGH_TIMBER_ALERT),
       .sub = "",
       .location = std::nullopt,
       .urgent = false,
       .expiresSeconds = expires});
  }
}

std::optional<Protocol::VoxelPosition> AlertWatch::NewestPlace(std::span<const HudAlert> _alerts)
{
  const auto newest = std::ranges::find_if(_alerts, [](const HudAlert& _alert) { return _alert.location.has_value(); });
  return newest != _alerts.end() ? newest->location : std::nullopt;
}

std::optional<std::string> AlertWatch::NearestName(const Protocol::StateUpdate& _state, const Protocol::VoxelPosition& _voxels)
{
  std::optional<std::string> name;
  std::int64_t nearest = std::numeric_limits<std::int64_t>::max();
  for (const Protocol::StructureView& structure : _state.structures)
  {
    const std::int64_t distance = DistanceSquared(structure, _voxels);
    if (!structure.name.empty() && distance < nearest)
    {
      nearest = distance;
      name = structure.name;
    }
  }
  return name;
}

} // namespace Client
