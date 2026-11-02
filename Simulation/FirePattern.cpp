#include "pch.h"

#include "FirePattern.h"

#include "ServerData.h"

namespace Simulation
{

namespace
{

/// The templates whose pattern runs top-down: T1's frame stops in 18 shots fired top-down from outside the castle, and
/// stands at 76 % after 30 fired bottom-up, its walls taking the low shots (measured on the Siege castle, M4-F5).
constexpr std::array<std::string_view, 1> TOP_DOWN_TEMPLATES = {"T1"};

/// A template voxel's place across the ground after _turns clockwise quarter turns seen from above, as World places it.
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

} // namespace

Protocol::VoxelPosition PatternAim(const ServerData& _data, const Protocol::StructureView& _structure, Protocol::VoxelPosition _standVoxels,
                                   std::uint32_t _shot)
{
  const ServerTemplate* found = _data.FindTemplate(_structure.templateId);
  if (found == nullptr)
  {
    return _structure.originVoxels;
  }
  // The part's box, in the map's voxels.
  Protocol::VoxelPosition low{.xVoxels = std::numeric_limits<std::int32_t>::max(),
                              .yVoxels = std::numeric_limits<std::int32_t>::max(),
                              .zVoxels = std::numeric_limits<std::int32_t>::max()};
  Protocol::VoxelPosition high{.xVoxels = std::numeric_limits<std::int32_t>::min(),
                               .yVoxels = std::numeric_limits<std::int32_t>::min(),
                               .zVoxels = std::numeric_limits<std::int32_t>::min()};
  const bool whole = found->criticalPart.empty();
  std::size_t index = 0;
  for (std::uint32_t y = 0; y < found->heightVoxels; ++y)
  {
    for (std::uint32_t z = 0; z < found->depthVoxels; ++z)
    {
      for (std::uint32_t x = 0; x < found->widthVoxels; ++x, ++index)
      {
        if (found->indices.at(index) == Protocol::Chunk::EMPTY || (!whole && found->parts.at(index) != ServerTemplate::Part::Critical))
        {
          continue;
        }
        const auto [turnedX, turnedZ] =
          Turned(static_cast<std::int32_t>(x), static_cast<std::int32_t>(z), static_cast<std::int32_t>(found->widthVoxels),
                 static_cast<std::int32_t>(found->depthVoxels), _structure.quarterTurns);
        const Protocol::VoxelPosition at{.xVoxels = _structure.originVoxels.xVoxels + turnedX,
                                         .yVoxels = _structure.originVoxels.yVoxels + static_cast<std::int32_t>(y),
                                         .zVoxels = _structure.originVoxels.zVoxels + turnedZ};
        low = {.xVoxels = std::min(low.xVoxels, at.xVoxels),
               .yVoxels = std::min(low.yVoxels, at.yVoxels),
               .zVoxels = std::min(low.zVoxels, at.zVoxels)};
        high = {.xVoxels = std::max(high.xVoxels, at.xVoxels),
                .yVoxels = std::max(high.yVoxels, at.yVoxels),
                .zVoxels = std::max(high.zVoxels, at.zVoxels)};
      }
    }
  }
  if (low.xVoxels > high.xVoxels)
  {
    return _structure.originVoxels;
  }
  // The face nearest the stand: across z when the stand lies farther from the box's middle along z than along x, ties to
  // z; the middle in doubled voxels, so that it is whole.
  const std::int64_t middleX = std::int64_t{low.xVoxels} + high.xVoxels;
  const std::int64_t middleZ = std::int64_t{low.zVoxels} + high.zVoxels;
  const std::int64_t offsetX = (2 * std::int64_t{_standVoxels.xVoxels}) - middleX;
  const std::int64_t offsetZ = (2 * std::int64_t{_standVoxels.zVoxels}) - middleZ;
  const bool acrossZ = std::abs(offsetZ) >= std::abs(offsetX);
  const std::int32_t lateralLow = acrossZ ? low.xVoxels : low.zVoxels;
  const std::int32_t width = (acrossZ ? high.xVoxels : high.zVoxels) - lateralLow + 1;
  const std::int32_t height = high.yVoxels - low.yVoxels + 1;
  const std::int32_t depth = (acrossZ ? high.zVoxels - low.zVoxels : high.xVoxels - low.xVoxels) + 1;
  const std::uint32_t perFace = static_cast<std::uint32_t>(width) * static_cast<std::uint32_t>(height);
  const auto layer = static_cast<std::int32_t>((_shot / perFace) % static_cast<std::uint32_t>(depth));
  const auto row = static_cast<std::int32_t>((_shot % perFace) / static_cast<std::uint32_t>(width));
  const auto column = static_cast<std::int32_t>((_shot % perFace) % static_cast<std::uint32_t>(width));
  // From the middle outwards: the middle, one past it, one before it, and so on.
  const std::int32_t lateral = lateralLow + ((width - 1) / 2) + ((column % 2) == 0 ? column / 2 : -((column + 1) / 2));
  const bool topDown = std::ranges::find(TOP_DOWN_TEMPLATES, std::string_view{_structure.templateId}) != TOP_DOWN_TEMPLATES.end();
  const std::int32_t y = topDown ? high.yVoxels - row : low.yVoxels + row;
  if (acrossZ)
  {
    return {.xVoxels = lateral, .yVoxels = y, .zVoxels = offsetZ < 0 ? low.zVoxels + layer : high.zVoxels - layer};
  }
  return {.xVoxels = offsetX < 0 ? low.xVoxels + layer : high.xVoxels - layer, .yVoxels = y, .zVoxels = lateral};
}

} // namespace Simulation
