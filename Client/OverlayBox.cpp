#include "pch.h"

#include "OverlayBox.h"

#include "VoxelRecord.h"

namespace Client
{

namespace
{

// A cell as one key that sorts by x, then z, then y: 21 bits an axis, each about an offset, so that a neighbor's key is
// the cell's plus a fixed step.
constexpr std::uint32_t AXIS_BITS = 21;
constexpr std::int64_t AXIS_OFFSET = std::int64_t{1} << 20;
constexpr std::int64_t Y_STEP = 1;
constexpr std::int64_t Z_STEP = std::int64_t{1} << AXIS_BITS;
constexpr std::int64_t X_STEP = std::int64_t{1} << (2 * AXIS_BITS);

std::uint64_t KeyOf(const Protocol::VoxelPosition& _cell) noexcept
{
  return (static_cast<std::uint64_t>(_cell.xVoxels + AXIS_OFFSET) << (2 * AXIS_BITS)) |
         (static_cast<std::uint64_t>(_cell.zVoxels + AXIS_OFFSET) << AXIS_BITS) | static_cast<std::uint64_t>(_cell.yVoxels + AXIS_OFFSET);
}

/// A neighbor's step in keys, and the joined bit of the face that touches it.
struct Neighbor
{
  std::int64_t step;
  std::uint8_t bit;
};

constexpr std::array<Neighbor, 6> NEIGHBORS = {{{.step = -X_STEP, .bit = Engine::JoinedBit(Engine::VoxelFace::NegativeX)},
                                                {.step = X_STEP, .bit = Engine::JoinedBit(Engine::VoxelFace::PositiveX)},
                                                {.step = Y_STEP, .bit = Engine::JoinedBit(Engine::VoxelFace::PositiveY)},
                                                {.step = -Z_STEP, .bit = Engine::JoinedBit(Engine::VoxelFace::NegativeZ)},
                                                {.step = Z_STEP, .bit = Engine::JoinedBit(Engine::VoxelFace::PositiveZ)},
                                                {.step = -Y_STEP, .bit = Engine::JOINED_BOTTOM}}};

} // namespace

std::vector<std::uint8_t> JoinedFacesOf(std::span<const OverlayBox> _boxes)
{
  // The boxes in key order. Each neighbor's key is a box's plus the same step, so the keys wanted rise with the boxes'
  // and each of the six is found in one pass over them, where a search a box would cost milliseconds a frame for the
  // passability view's tens of thousands.
  std::vector<std::pair<std::uint64_t, std::size_t>> keyed;
  keyed.reserve(_boxes.size());
  std::size_t index = 0;
  for (const OverlayBox& box : _boxes)
  {
    keyed.emplace_back(KeyOf(box.cellVoxels), index);
    ++index;
  }
  std::ranges::sort(keyed);
  std::vector<std::uint8_t> joined(_boxes.size(), 0);
  for (const Neighbor& neighbor : NEIGHBORS)
  {
    auto ahead = keyed.begin();
    for (const auto& [key, at] : keyed)
    {
      // Unsigned, so a step down wraps to the key below, which the offsets keep above 0.
      const std::uint64_t wanted = key + static_cast<std::uint64_t>(neighbor.step);
      ahead =
        std::find_if(ahead, keyed.end(), [wanted](const std::pair<std::uint64_t, std::size_t>& _entry) { return _entry.first >= wanted; });
      if (ahead != keyed.end() && ahead->first == wanted)
      {
        joined.at(at) = static_cast<std::uint8_t>(joined.at(at) | neighbor.bit);
      }
    }
  }
  return joined;
}

} // namespace Client
