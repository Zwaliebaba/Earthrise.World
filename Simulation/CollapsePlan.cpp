#include "pch.h"

#include "CollapsePlan.h"

#include "IntegerMath.h"

namespace Simulation
{

namespace
{

using ColumnKey = std::pair<std::int32_t, std::int32_t>;
using PositionKey = std::tuple<std::int32_t, std::int32_t, std::int32_t>;

PositionKey KeyOf(Protocol::VoxelPosition _voxel) noexcept
{
  return {_voxel.xVoxels, _voxel.yVoxels, _voxel.zVoxels};
}

/// The piece's box.
struct Box
{
  std::int32_t minX;
  std::int32_t maxX;
  std::int32_t minY;
  std::int32_t maxY;
  std::int32_t minZ;
  std::int32_t maxZ;
};

Box BoxOf(const Piece& _piece)
{
  Box box{.minX = INT32_MAX, .maxX = INT32_MIN, .minY = INT32_MAX, .maxY = INT32_MIN, .minZ = INT32_MAX, .maxZ = INT32_MIN};
  for (const Protocol::VoxelPosition& voxel : _piece)
  {
    box.minX = std::min(box.minX, voxel.xVoxels);
    box.maxX = std::max(box.maxX, voxel.xVoxels);
    box.minY = std::min(box.minY, voxel.yVoxels);
    box.maxY = std::max(box.maxY, voxel.yVoxels);
    box.minZ = std::min(box.minZ, voxel.zVoxels);
    box.maxZ = std::max(box.maxZ, voxel.zVoxels);
  }
  return box;
}

FallDirection DirectionOf(const Box& _box, std::span<const Protocol::VoxelPosition> _cause, std::int32_t _reach)
{
  // Twice each coordinate, so the middle of a voxel and of the box are whole: a voxel's middle is
  // 2x + 1, and the box's is minX + maxX + 1.
  std::int64_t sumX = 0;
  std::int64_t sumZ = 0;
  std::int64_t count = 0;
  for (const Protocol::VoxelPosition& voxel : _cause)
  {
    if (voxel.yVoxels > _box.maxY || voxel.xVoxels < _box.minX - _reach || voxel.xVoxels > _box.maxX + _reach ||
        voxel.zVoxels < _box.minZ - _reach || voxel.zVoxels > _box.maxZ + _reach)
    {
      continue;
    }
    sumX += 2 * std::int64_t{voxel.xVoxels} + 1 - (std::int64_t{_box.minX} + _box.maxX + 1);
    sumZ += 2 * std::int64_t{voxel.zVoxels} + 1 - (std::int64_t{_box.minZ} + _box.maxZ + 1);
    ++count;
  }
  if (count == 0)
  {
    return FallDirection::Down;
  }
  const bool alongX = std::abs(sumX) >= std::abs(sumZ);
  const std::int64_t offset = alongX ? sumX : sumZ;
  const std::int64_t width = alongX ? _box.maxX - _box.minX + 1 : _box.maxZ - _box.minZ + 1;
  // The mean offset is offset / (2 count); it is under a quarter of the width when 2|offset| < width count.
  if (2 * std::abs(offset) < width * count)
  {
    return FallDirection::Down;
  }
  if (alongX)
  {
    return offset > 0 ? FallDirection::East : FallDirection::West;
  }
  return offset > 0 ? FallDirection::North : FallDirection::South;
}

} // namespace

std::uint32_t WarningTicks(std::int32_t _heightVoxels, const Rules& _rules)
{
  const std::int64_t speed = _rules.footSpeedMetersPerTick.Raw();
  if (speed <= 0 || _heightVoxels <= 0)
  {
    throw std::invalid_argument("a collapse's warning needs a walking speed and a piece with height");
  }
  // The walk up the piece's height, in ticks, rounded up.
  const std::int64_t walk = (std::int64_t{_heightVoxels} * Core::Fixed::ONE_RAW + speed - 1) / speed;
  return static_cast<std::uint32_t>(_rules.warningBaseTicks + walk);
}

std::uint32_t FallTicks(std::int32_t _heightVoxels, const Rules& _rules)
{
  const std::int64_t gravity = _rules.gravityMetersPerTickSquared.Raw();
  if (gravity <= 0 || _heightVoxels <= 0)
  {
    throw std::invalid_argument("a collapse's fall needs gravity and a piece with height");
  }
  // The fewest ticks n in which g n^2 / 2 covers the height.
  const std::int64_t needed = 2 * std::int64_t{_heightVoxels} * Core::Fixed::ONE_RAW;
  std::int64_t ticks = Core::IntegerSquareRoot(static_cast<std::uint64_t>(needed / gravity));
  while (ticks * ticks * gravity < needed)
  {
    ++ticks;
  }
  while (ticks > 1 && (ticks - 1) * (ticks - 1) * gravity >= needed)
  {
    --ticks;
  }
  return static_cast<std::uint32_t>(std::max<std::int64_t>(ticks, 1));
}

CollapsePlan PlanCollapse(const Piece& _piece, std::span<const Protocol::VoxelPosition> _cause, const VoxelWorld& _world,
                          const ServerData& _data)
{
  if (_piece.empty())
  {
    throw std::invalid_argument("a collapse plan needs a piece");
  }
  const MaterialRules& materials = _data.Materials();
  const Rules& rules = _data.Tuning();
  const Box box = BoxOf(_piece);
  const std::int32_t height = box.maxY - box.minY + 1;
  CollapsePlan plan{.direction = DirectionOf(box, _cause, static_cast<std::int32_t>(LongestSpanVoxels(materials))),
                    .warningTicks = WarningTicks(height, rules),
                    .fallTicks = FallTicks(height, rules),
                    .landing = {},
                    .strip = {},
                    .fallenVoxels = 0};

  std::set<PositionKey> falling;
  for (const Protocol::VoxelPosition& voxel : _piece)
  {
    falling.insert(KeyOf(voxel));
  }
  // What the world holds where the plan has not yet written, the falling piece read as gone.
  std::map<PositionKey, std::uint8_t> written;
  const auto at = [&](Protocol::VoxelPosition _voxel) -> std::uint8_t
  {
    const auto found = written.find(KeyOf(_voxel));
    if (found != written.end())
    {
      return found->second;
    }
    return falling.contains(KeyOf(_voxel)) ? Protocol::Chunk::EMPTY : _world.At(_voxel);
  };
  // A column's ground: the highest terrain or rubble at or below the piece's top.
  const auto groundOf = [&](std::int32_t _x, std::int32_t _z)
  {
    for (std::int32_t y = box.maxY; y >= 0; --y)
    {
      const MaterialRule& rule = materials.at(at({.xVoxels = _x, .yVoxels = y, .zVoxels = _z}));
      if (rule.walkableGround || rule.support == Support::Anchors)
      {
        return y;
      }
    }
    return -1;
  };
  const auto isStructure = [&](std::uint8_t _index)
  {
    const MaterialRule& rule = materials.at(_index);
    return _index != Protocol::Chunk::EMPTY && !rule.walkableGround && !rule.liquid && rule.support != Support::Anchors;
  };

  // Where each voxel that leaves rubble lands, and in what order: nearest the foot first.
  struct Landing
  {
    std::int32_t distance; // from the foot, along the fall
    std::int32_t x;
    std::int32_t z;
    Protocol::VoxelPosition from;
    std::uint8_t rubbleIndex;
  };
  std::vector<Landing> landings;
  for (const Protocol::VoxelPosition& voxel : _piece)
  {
    const std::uint8_t rubble = materials.at(_world.At(voxel)).rubbleIndex;
    if (rubble == 0)
    {
      continue; // leaves leave nothing
    }
    const std::int32_t raised = voxel.yVoxels - box.minY;
    Landing landing{.distance = raised, .x = voxel.xVoxels, .z = voxel.zVoxels, .from = voxel, .rubbleIndex = rubble};
    switch (plan.direction)
    {
    case FallDirection::East:
      landing.x = box.maxX + 1 + raised;
      break;
    case FallDirection::West:
      landing.x = box.minX - 1 - raised;
      break;
    case FallDirection::North:
      landing.z = box.maxZ + 1 + raised;
      break;
    case FallDirection::South:
      landing.z = box.minZ - 1 - raised;
      break;
    case FallDirection::Down:
      break;
    }
    landings.push_back(landing);
  }
  std::ranges::sort(landings,
                    [](const Landing& _a, const Landing& _b)
                    {
                      return std::tuple{_a.distance, _a.x, _a.z, _a.from.yVoxels, _a.from.zVoxels, _a.from.xVoxels} <
                             std::tuple{_b.distance, _b.x, _b.z, _b.from.yVoxels, _b.from.zVoxels, _b.from.xVoxels};
                    });

  // The strip, at each column's ground before the fall.
  std::map<ColumnKey, std::int32_t> tops;
  const auto addStrip = [&](std::int32_t _x, std::int32_t _z)
  {
    if (!tops.contains({_x, _z}))
    {
      const std::int32_t ground = groundOf(_x, _z);
      tops.emplace(ColumnKey{_x, _z}, ground);
      plan.strip.push_back({.xVoxels = _x, .yVoxels = ground + 1, .zVoxels = _z});
    }
  };
  if (plan.direction == FallDirection::Down)
  {
    for (const Protocol::VoxelPosition& voxel : _piece)
    {
      addStrip(voxel.xVoxels, voxel.zVoxels);
    }
  }
  else
  {
    for (std::int32_t distance = 1; distance <= height; ++distance)
    {
      const bool alongX = plan.direction == FallDirection::East || plan.direction == FallDirection::West;
      const std::int32_t from = alongX ? box.minZ : box.minX;
      const std::int32_t to = alongX ? box.maxZ : box.maxX;
      for (std::int32_t across = from; across <= to; ++across)
      {
        switch (plan.direction)
        {
        case FallDirection::East:
          addStrip(box.maxX + distance, across);
          break;
        case FallDirection::West:
          addStrip(box.minX - distance, across);
          break;
        case FallDirection::North:
          addStrip(across, box.maxZ + distance);
          break;
        case FallDirection::South:
          addStrip(across, box.minZ - distance);
          break;
        case FallDirection::Down:
          break;
        }
      }
    }
  }

  // Stack each landing voxel on its column, crushing the structure in its way.
  for (const Landing& landing : landings)
  {
    if (!tops.contains({landing.x, landing.z}))
    {
      tops.emplace(ColumnKey{landing.x, landing.z}, groundOf(landing.x, landing.z));
    }
    std::int32_t& top = tops.at({landing.x, landing.z});
    while (true)
    {
      const Protocol::VoxelPosition next{.xVoxels = landing.x, .yVoxels = top + 1, .zVoxels = landing.z};
      const std::uint8_t there = at(next);
      if (!isStructure(there))
      {
        plan.landing.push_back({.position = next, .paletteIndex = landing.rubbleIndex});
        written[KeyOf(next)] = landing.rubbleIndex;
        ++plan.fallenVoxels;
        top = next.yVoxels;
        break;
      }
      // Structure below the rubble's top becomes rubble of its own material, or nothing.
      const std::uint8_t crushed = materials.at(there).rubbleIndex;
      plan.landing.push_back({.position = next, .paletteIndex = crushed});
      written[KeyOf(next)] = crushed;
      if (crushed != Protocol::Chunk::EMPTY)
      {
        top = next.yVoxels;
      }
    }
  }
  return plan;
}

} // namespace Simulation
