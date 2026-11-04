#pragma once

#include <DirectXMath.h>

#include "OrientedVoxel.h"
#include "VoxelRecord.h"

#include <cstdint>
#include <numbers>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace Client
{

/// A model's turn about the vertical, as ModelPlacement::yawRadians holds it (ADR-030): clockwise seen from above, so
/// that a quarter turn takes its front, +z, towards +x, as a batch's quarter turn does (Engine::VoxelPass::Batch).
inline constexpr float QUARTER_TURN_RADIANS = std::numbers::pi_v<float> / 2.0F;
inline constexpr float FULL_TURN_RADIANS = 2.0F * std::numbers::pi_v<float>;
/// Within this of a whole quarter turn, a model is drawn at that quarter turn, through the axis-aligned path: far less
/// than any turn shows at a model's size, 0.006° (ADR-030, decision 2).
inline constexpr float WHOLE_TURN_TOLERANCE_RADIANS = 1.0e-4F;

/// _yawRadians brought into [0, 2π).
[[nodiscard]] float NormalizedYaw(float _yawRadians) noexcept;

/// The whole quarter turns _yawRadians is, 0 to 3, when it is within WHOLE_TURN_TOLERANCE_RADIANS of one.
[[nodiscard]] std::optional<std::uint32_t> WholeQuarterTurns(float _yawRadians) noexcept;

/// The turn from _fromRadians to _toRadians the shorter way, in (-π, π].
[[nodiscard]] float ShorterTurn(float _fromRadians, float _toRadians) noexcept;

/// The cosine and the sine of _yawRadians, exact at whole quarter turns.
[[nodiscard]] std::pair<float, float> YawCosSin(float _yawRadians) noexcept;

/// An offset along x and z in a model's own frame turned by _yawRadians into the world's: x' = x cos + z sin and
/// z' = -x sin + z cos.
[[nodiscard]] std::pair<float, float> TurnedOffset(float _xMeters, float _zMeters, float _yawRadians) noexcept;

/// The width along x and the depth along z of the box that bounds a footprint _widthMeters by _depthMeters turned by
/// _yawRadians about its middle: the footprint itself at whole quarter turns, its sides swapped at odd ones.
[[nodiscard]] std::pair<float, float> TurnedExtent(float _widthMeters, float _depthMeters, float _yawRadians) noexcept;

/// The unit quaternion, x, y, z and w, that turns a voxel's own axes by _yawRadians about +y, as Engine::OrientedVoxel
/// holds it.
[[nodiscard]] DirectX::XMFLOAT4 YawRotation(float _yawRadians) noexcept;

/// A model grid as drawn turned: the middle of its footprint at its feet, where it turns about, its yaw, its voxels'
/// edge and its extent in cells along its own x and z.
struct ModelTurn
{
  DirectX::XMFLOAT3 middleMeters;
  float yawRadians;
  float voxelEdgeMeters;
  std::uint32_t widthCells;
  std::uint32_t depthCells;
};

/// Where the centre of the model's cell (_x, _y, _z) lies in the world, turned with it. A cell beyond the grid, as a
/// corner shade reads, is turned the same.
[[nodiscard]] DirectX::XMFLOAT3 TurnedCellCenter(const ModelTurn& _turn, std::int32_t _x, std::int32_t _y, std::int32_t _z) noexcept;

/// _records, a model's built in its own frame, as oriented voxels turned with _turn (ADR-030, decisions 2 and 3): each at
/// its cell's turned centre, with its record's levels, table, lightness and palette entry, _swapFrom drawn as _swapTo
/// when _swapFrom is not 0, as a batch's swap draws it. Appended to _voxels.
void TurnedVoxels(std::span<const Engine::VoxelRecord> _records, const ModelTurn& _turn, std::uint8_t _swapFrom, std::uint8_t _swapTo,
                  std::vector<Engine::OrientedVoxel>& _voxels);

/// The same voxels flat in _paletteIndex, with no levels and no lightness, for the oriented silhouette pipeline
/// (ADR-030, decision 4). Appended to _voxels.
void TurnedSilhouette(std::span<const Engine::VoxelRecord> _records, const ModelTurn& _turn, std::uint8_t _paletteIndex,
                      std::vector<Engine::OrientedVoxel>& _voxels);

} // namespace Client
