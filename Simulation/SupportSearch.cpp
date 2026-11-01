#include "pch.h"

#include "SupportSearch.h"

namespace Simulation
{

namespace
{

constexpr std::uint8_t UNREACHED = 255;

/// The box the search reads: the edits' columns widened by twice the longest span, from the lowest
/// anchor it needs to the top of the world.
struct Box
{
  std::int32_t minX;
  std::int32_t minY;
  std::int32_t minZ;
  std::int32_t width;
  std::int32_t height;
  std::int32_t depth;

  [[nodiscard]] std::size_t Index(std::int32_t _x, std::int32_t _y, std::int32_t _z) const noexcept
  {
    return (static_cast<std::size_t>(_y - minY) * static_cast<std::size_t>(depth) + static_cast<std::size_t>(_z - minZ)) *
             static_cast<std::size_t>(width) +
           static_cast<std::size_t>(_x - minX);
  }

  [[nodiscard]] bool Contains(std::int32_t _x, std::int32_t _y, std::int32_t _z) const noexcept
  {
    return _x >= minX && _x < minX + width && _y >= minY && _y < minY + height && _z >= minZ && _z < minZ + depth;
  }

  [[nodiscard]] std::size_t Volume() const noexcept
  {
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * static_cast<std::size_t>(depth);
  }
};

} // namespace

SupportSearchResult FindUnsupported(const ChunkStore& _store, const SupportRules& _rules, std::span<const Protocol::VoxelPosition> _edits)
{
  if (_rules.materials == nullptr || !_rules.surfaceVoxels || _rules.heightVoxels <= 0)
  {
    throw std::invalid_argument("the support search needs materials, a surface and a world height");
  }
  SupportSearchResult result{.pieces = {}, .voxelsSearched = 0};
  if (_edits.empty())
  {
    return result;
  }
  const MaterialRules& materials = *_rules.materials;
  const auto span = static_cast<std::int32_t>(LongestSpanVoxels(materials));
  if (span >= UNREACHED)
  {
    throw std::invalid_argument("a span must be shorter than 255 voxels");
  }

  // The box: twice the longest span about the edits, so that every path a judged voxel could be
  // supported by, at most one span sideways, lies inside it.
  std::int32_t minX = _edits[0].xVoxels;
  std::int32_t maxX = minX;
  std::int32_t minZ = _edits[0].zVoxels;
  std::int32_t maxZ = minZ;
  std::int32_t lowestEdit = _edits[0].yVoxels;
  for (const Protocol::VoxelPosition& edit : _edits)
  {
    minX = std::min(minX, edit.xVoxels);
    maxX = std::max(maxX, edit.xVoxels);
    minZ = std::min(minZ, edit.zVoxels);
    maxZ = std::max(maxZ, edit.zVoxels);
    lowestEdit = std::min(lowestEdit, edit.yVoxels);
  }
  minX -= 2 * span;
  maxX += 2 * span;
  minZ -= 2 * span;
  maxZ += 2 * span;
  // A path goes down until it meets an anchor, so the box reaches down to the deepest voxel that is
  // an anchor by depth, or to the world's floor.
  // Below this height in each column, every voxel is an anchor by depth.
  std::vector<std::int32_t> anchorTops(static_cast<std::size_t>(maxX - minX + 1) * static_cast<std::size_t>(maxZ - minZ + 1));
  std::int32_t minY = std::max(0, lowestEdit);
  for (std::int32_t z = minZ; z <= maxZ; ++z)
  {
    for (std::int32_t x = minX; x <= maxX; ++x)
    {
      const std::int32_t anchorTop = _rules.surfaceVoxels(x, z) - _rules.anchorDepthVoxels;
      anchorTops[static_cast<std::size_t>(z - minZ) * static_cast<std::size_t>(maxX - minX + 1) + static_cast<std::size_t>(x - minX)] =
        anchorTop;
      minY = std::min(minY, std::max(0, anchorTop - 1));
    }
  }
  const Box box{
    .minX = minX, .minY = minY, .minZ = minZ, .width = maxX - minX + 1, .height = _rules.heightVoxels - minY, .depth = maxZ - minZ + 1};
  if (box.height <= 0)
  {
    return result;
  }
  result.voxelsSearched = box.Volume();

  std::vector<std::uint8_t> indices(box.Volume());
  std::vector<std::uint8_t> distance(box.Volume(), UNREACHED);
  std::deque<Protocol::VoxelPosition> frontier;
  for (std::int32_t y = box.minY; y < box.minY + box.height; ++y)
  {
    for (std::int32_t z = box.minZ; z < box.minZ + box.depth; ++z)
    {
      for (std::int32_t x = box.minX; x < box.minX + box.width; ++x)
      {
        const std::uint8_t index = _store.At({.xVoxels = x, .yVoxels = y, .zVoxels = z});
        const std::size_t at = box.Index(x, y, z);
        indices[at] = index;
        const Support support = materials[index].support;
        if (support == Support::None)
        {
          continue;
        }
        const std::int32_t anchorTop =
          anchorTops[static_cast<std::size_t>(z - box.minZ) * static_cast<std::size_t>(box.width) + static_cast<std::size_t>(x - box.minX)];
        if (support == Support::Anchors || y < anchorTop)
        {
          distance[at] = 0;
          frontier.push_back({.xVoxels = x, .yVoxels = y, .zVoxels = z});
        }
      }
    }
  }

  // Outwards from the anchors: a step up costs nothing, a step sideways one, which is a path from
  // the voxel reached down or sideways to an anchor, read backwards. Distances beyond the longest
  // span are all alike, unsupported, so they are not followed.
  constexpr std::array<std::array<std::int32_t, 2>, 4> SIDEWAYS = {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
  while (!frontier.empty())
  {
    const Protocol::VoxelPosition from = frontier.front();
    frontier.pop_front();
    const std::uint8_t reached = distance[box.Index(from.xVoxels, from.yVoxels, from.zVoxels)];
    const auto visit = [&](std::int32_t _x, std::int32_t _y, std::int32_t _z, std::uint8_t _distance, bool _free)
    {
      if (!box.Contains(_x, _y, _z) || _distance > span)
      {
        return;
      }
      const std::size_t at = box.Index(_x, _y, _z);
      if (materials[indices[at]].support == Support::None || distance[at] <= _distance)
      {
        return;
      }
      distance[at] = _distance;
      if (_free)
      {
        frontier.push_front({.xVoxels = _x, .yVoxels = _y, .zVoxels = _z});
      }
      else
      {
        frontier.push_back({.xVoxels = _x, .yVoxels = _y, .zVoxels = _z});
      }
    };
    visit(from.xVoxels, from.yVoxels + 1, from.zVoxels, reached, true);
    for (const auto& [dx, dz] : SIDEWAYS)
    {
      visit(from.xVoxels + dx, from.yVoxels, from.zVoxels + dz, static_cast<std::uint8_t>(reached + 1), false);
    }
  }

  // Judge the voxels an edit can affect: at or above an edit, and within the longest span of it.
  const auto judged = [&](std::int32_t _x, std::int32_t _y, std::int32_t _z)
  {
    return std::ranges::any_of(_edits, [&](const Protocol::VoxelPosition& _edit)
                               { return _y >= _edit.yVoxels && std::abs(_x - _edit.xVoxels) + std::abs(_z - _edit.zVoxels) <= span; });
  };
  std::vector<bool> unsupported(box.Volume(), false);
  for (std::int32_t y = box.minY; y < box.minY + box.height; ++y)
  {
    for (std::int32_t z = box.minZ; z < box.minZ + box.depth; ++z)
    {
      for (std::int32_t x = box.minX; x < box.minX + box.width; ++x)
      {
        const std::size_t at = box.Index(x, y, z);
        const MaterialRule& rule = materials[indices[at]];
        if (rule.support == Support::Spans && distance[at] > rule.spanVoxels && judged(x, y, z))
        {
          unsupported[at] = true;
        }
      }
    }
  }

  // Join the unsupported voxels into pieces by their faces, in a fixed order.
  constexpr std::array<std::array<std::int32_t, 3>, 6> FACES = {{{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};
  for (std::int32_t y = box.minY; y < box.minY + box.height; ++y)
  {
    for (std::int32_t z = box.minZ; z < box.minZ + box.depth; ++z)
    {
      for (std::int32_t x = box.minX; x < box.minX + box.width; ++x)
      {
        if (!unsupported[box.Index(x, y, z)])
        {
          continue;
        }
        Piece piece;
        std::vector<Protocol::VoxelPosition> open = {{.xVoxels = x, .yVoxels = y, .zVoxels = z}};
        unsupported[box.Index(x, y, z)] = false;
        while (!open.empty())
        {
          const Protocol::VoxelPosition voxel = open.back();
          open.pop_back();
          piece.push_back(voxel);
          for (const auto& [dx, dy, dz] : FACES)
          {
            const std::int32_t nx = voxel.xVoxels + dx;
            const std::int32_t ny = voxel.yVoxels + dy;
            const std::int32_t nz = voxel.zVoxels + dz;
            if (box.Contains(nx, ny, nz) && unsupported[box.Index(nx, ny, nz)])
            {
              unsupported[box.Index(nx, ny, nz)] = false;
              open.push_back({.xVoxels = nx, .yVoxels = ny, .zVoxels = nz});
            }
          }
        }
        std::ranges::sort(piece, {},
                          [](const Protocol::VoxelPosition& _voxel) { return std::tuple{_voxel.yVoxels, _voxel.zVoxels, _voxel.xVoxels}; });
        result.pieces.push_back(std::move(piece));
      }
    }
  }
  return result;
}

} // namespace Simulation
