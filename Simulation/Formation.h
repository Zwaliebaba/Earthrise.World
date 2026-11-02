#pragma once

#include "ChunkCoordinate.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Simulation
{

/// The eight facings a squad keeps, 0 north (+z) and then clockwise in eighths, as one step across the ground in x
/// and z: the direction of its lead's last step (ADR-022, decision 4).
inline constexpr std::array<std::array<std::int32_t, 2>, 8> FACINGS = {
  {{0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}}};

/// The facing of a step of _dx and _dz, each -1, 0 or 1, not both 0; 0 for no step.
[[nodiscard]] std::uint8_t FacingOf(std::int32_t _dx, std::int32_t _dz) noexcept;

/// ADR-022, decision 4: the slots of a squad of _figures in _ranks ranks behind _leadVoxels, facing _direction (0 north,
/// then clockwise in eighths), 1 m apart: each rank holds the figures a full squad divides into _ranks, ceil(_fullFigures /
/// _ranks), its middle first, then left and right of it, alternately outwards; the front rank first, each rank a step
/// behind the one before. Slot 0 is the lead, at _leadVoxels. Every slot keeps the lead's height.
[[nodiscard]] std::vector<Protocol::VoxelPosition> FormationSlots(Protocol::VoxelPosition _leadVoxels, std::uint8_t _direction,
                                                                  std::uint8_t _figures, std::uint8_t _ranks, std::uint8_t _fullFigures);

/// The same, for a full squad: _figures is the full size.
[[nodiscard]] std::vector<Protocol::VoxelPosition> FormationSlots(Protocol::VoxelPosition _leadVoxels, std::uint8_t _direction,
                                                                  std::uint8_t _figures, std::uint8_t _ranks);

} // namespace Simulation
