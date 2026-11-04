#include "pch.h"

#include "ModelTurn.h"

#include <array>
#include <cmath>

namespace Client
{

namespace
{

constexpr std::uint32_t QUARTER_TURNS = 4;
// A record's cell: x, y and z, 6 bits each from bit 0 of its first word (Engine/VoxelRecord.h).
constexpr std::uint32_t CELL_BITS = 6;
constexpr std::uint32_t CELL_MASK = (1U << CELL_BITS) - 1;

} // namespace

float NormalizedYaw(float _yawRadians) noexcept
{
  const float yaw = std::fmod(_yawRadians, FULL_TURN_RADIANS);
  const float normalized = yaw < 0.0F ? yaw + FULL_TURN_RADIANS : yaw;
  return normalized >= FULL_TURN_RADIANS ? 0.0F : normalized;
}

std::optional<std::uint32_t> WholeQuarterTurns(float _yawRadians) noexcept
{
  const float yaw = NormalizedYaw(_yawRadians);
  const float quarters = std::round(yaw / QUARTER_TURN_RADIANS);
  if (std::abs(yaw - (quarters * QUARTER_TURN_RADIANS)) > WHOLE_TURN_TOLERANCE_RADIANS)
  {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(quarters) % QUARTER_TURNS;
}

float ShorterTurn(float _fromRadians, float _toRadians) noexcept
{
  const float turn = NormalizedYaw(_toRadians - _fromRadians);
  return turn > std::numbers::pi_v<float> ? turn - FULL_TURN_RADIANS : turn;
}

std::pair<float, float> YawCosSin(float _yawRadians) noexcept
{
  if (const std::optional<std::uint32_t> quarters = WholeQuarterTurns(_yawRadians); quarters.has_value())
  {
    constexpr std::array<std::pair<float, float>, QUARTER_TURNS> EXACT = {{{1.0F, 0.0F}, {0.0F, 1.0F}, {-1.0F, 0.0F}, {0.0F, -1.0F}}};
    return EXACT.at(*quarters);
  }
  return {std::cos(_yawRadians), std::sin(_yawRadians)};
}

std::pair<float, float> TurnedOffset(float _xMeters, float _zMeters, float _yawRadians) noexcept
{
  const auto [cosine, sine] = YawCosSin(_yawRadians);
  return {(_xMeters * cosine) + (_zMeters * sine), (_zMeters * cosine) - (_xMeters * sine)};
}

std::pair<float, float> TurnedExtent(float _widthMeters, float _depthMeters, float _yawRadians) noexcept
{
  const auto [cosine, sine] = YawCosSin(_yawRadians);
  return {(_widthMeters * std::abs(cosine)) + (_depthMeters * std::abs(sine)),
          (_widthMeters * std::abs(sine)) + (_depthMeters * std::abs(cosine))};
}

DirectX::XMFLOAT4 YawRotation(float _yawRadians) noexcept
{
  const float half = 0.5F * _yawRadians;
  return {0.0F, std::sin(half), 0.0F, std::cos(half)};
}

DirectX::XMFLOAT3 TurnedCellCenter(const ModelTurn& _turn, std::int32_t _x, std::int32_t _y, std::int32_t _z) noexcept
{
  const float edge = _turn.voxelEdgeMeters;
  // The cell's centre from the footprint's middle, in the model's own frame, then turned.
  const float ownX = ((static_cast<float>(_x) + 0.5F) * edge) - (0.5F * edge * static_cast<float>(_turn.widthCells));
  const float ownZ = ((static_cast<float>(_z) + 0.5F) * edge) - (0.5F * edge * static_cast<float>(_turn.depthCells));
  const auto [x, z] = TurnedOffset(ownX, ownZ, _turn.yawRadians);
  return {_turn.middleMeters.x + x, _turn.middleMeters.y + ((static_cast<float>(_y) + 0.5F) * edge), _turn.middleMeters.z + z};
}

void TurnedVoxels(std::span<const Engine::VoxelRecord> _records, const ModelTurn& _turn, std::uint8_t _swapFrom, std::uint8_t _swapTo,
                  std::vector<Engine::OrientedVoxel>& _voxels)
{
  const DirectX::XMFLOAT4 rotation = YawRotation(_turn.yawRadians);
  _voxels.reserve(_voxels.size() + _records.size());
  for (const Engine::VoxelRecord& record : _records)
  {
    const std::uint32_t cell = record.cellAndPalette;
    const DirectX::XMFLOAT3 center =
      TurnedCellCenter(_turn, static_cast<std::int32_t>(cell & CELL_MASK), static_cast<std::int32_t>((cell >> CELL_BITS) & CELL_MASK),
                       static_cast<std::int32_t>((cell >> (2 * CELL_BITS)) & CELL_MASK));
    Engine::OrientedVoxel voxel = Engine::OrientedVoxelOf(record, center, 0.5F * _turn.voxelEdgeMeters, rotation);
    if (_swapFrom != 0 && voxel.paletteIndex == _swapFrom)
    {
      voxel.paletteIndex = _swapTo;
    }
    _voxels.push_back(voxel);
  }
}

void TurnedSilhouette(std::span<const Engine::VoxelRecord> _records, const ModelTurn& _turn, std::uint8_t _paletteIndex,
                      std::vector<Engine::OrientedVoxel>& _voxels)
{
  const DirectX::XMFLOAT4 rotation = YawRotation(_turn.yawRadians);
  _voxels.reserve(_voxels.size() + _records.size());
  for (const Engine::VoxelRecord& record : _records)
  {
    const std::uint32_t cell = record.cellAndPalette;
    _voxels.push_back({.centerMeters = TurnedCellCenter(_turn, static_cast<std::int32_t>(cell & CELL_MASK),
                                                        static_cast<std::int32_t>((cell >> CELL_BITS) & CELL_MASK),
                                                        static_cast<std::int32_t>((cell >> (2 * CELL_BITS)) & CELL_MASK)),
                       .halfEdgeMeters = 0.5F * _turn.voxelEdgeMeters,
                       .rotation = rotation,
                       .paletteIndex = _paletteIndex,
                       .lightnessOffset = 0.0F,
                       .cornerLevels = 0,
                       .lastFaceAndShade = 0});
  }
}

} // namespace Client
