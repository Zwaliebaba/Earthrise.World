#include "pch.h"

#include "WorldPicker.h"

#include <limits>

namespace Client
{

namespace
{

constexpr float WORLD_VOXEL_METERS = 1.0F;
/// The camera's far plane: nothing is picked further along a ray than the view draws.
constexpr float MAX_PICK_METERS = 2000.0F;
/// How much further along the ray than the voxel it meets an engine's box may be and still be picked: a box's foot
/// stands on the ground, which a ray along its side meets at the same distance give or take float's rounding.
constexpr float BOX_TOLERANCE_METERS = 0.01F;

/// How far along _ray it enters _box, or nothing when it misses: the slabs' test, axis by axis.
std::optional<float> Enters(const Engine::PickRay& _ray, const EngineBox& _box) noexcept
{
  const std::array<float, 3> origin = {_ray.originMeters.x, _ray.originMeters.y, _ray.originMeters.z};
  const std::array<float, 3> direction = {_ray.direction.x, _ray.direction.y, _ray.direction.z};
  const std::array<float, 3> low = {_box.xMeters, _box.groundMeters, _box.zMeters};
  const std::array<float, 3> high = {_box.xMeters + _box.widthMeters, _box.groundMeters + _box.heightMeters,
                                     _box.zMeters + _box.depthMeters};
  float enter = 0.0F;
  float leave = std::numeric_limits<float>::infinity();
  for (std::size_t axis = 0; axis < origin.size(); ++axis)
  {
    if (direction.at(axis) == 0.0F)
    {
      if (origin.at(axis) < low.at(axis) || origin.at(axis) >= high.at(axis))
      {
        return std::nullopt;
      }
      continue;
    }
    const float first = (low.at(axis) - origin.at(axis)) / direction.at(axis);
    const float second = (high.at(axis) - origin.at(axis)) / direction.at(axis);
    enter = std::max(enter, std::min(first, second));
    leave = std::min(leave, std::max(first, second));
    if (enter > leave)
    {
      return std::nullopt;
    }
  }
  return enter;
}

} // namespace

void WorldPicker::TakeSnapshots(std::span<const Protocol::ChunkSnapshot> _snapshots)
{
  for (const Protocol::ChunkSnapshot& snapshot : _snapshots)
  {
    const std::uint64_t key = Protocol::PackChunkKey(snapshot.coordinate);
    const auto found = m_snapshots.find(key);
    if (found == m_snapshots.end() || found->second.version <= snapshot.version)
    {
      m_snapshots.insert_or_assign(key, snapshot);
    }
    m_topMeters = std::max(m_topMeters, static_cast<float>((snapshot.coordinate.yChunks + 1) * Protocol::CHUNK_EDGE_VOXELS));
  }
}

bool WorldPicker::SolidAt(const Protocol::VoxelPosition& _position) const
{
  const auto found = m_snapshots.find(Protocol::PackChunkKey(Protocol::ChunkContaining(_position)));
  if (found == m_snapshots.end())
  {
    return false;
  }
  const Protocol::VoxelPosition inside = Protocol::PositionInChunk(_position);
  return found->second.voxels->At(inside.xVoxels, inside.yVoxels, inside.zVoxels) != Protocol::Chunk::EMPTY;
}

Engine::PickRay WorldPicker::RayThrough(const DirectX::XMFLOAT4X4& _viewProjection, std::int32_t _xPixels, std::int32_t _yPixels,
                                        std::uint32_t _widthPixels, std::uint32_t _heightPixels) noexcept
{
  // The pixel's centre in clip space, y up, at the near plane (depth 1 under reversed Z) and the far one (0).
  const float x = (2.0F * (static_cast<float>(_xPixels) + 0.5F) / static_cast<float>(_widthPixels)) - 1.0F;
  const float y = 1.0F - (2.0F * (static_cast<float>(_yPixels) + 0.5F) / static_cast<float>(_heightPixels));
  const DirectX::XMMATRIX inverse = DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&_viewProjection));
  const DirectX::XMVECTOR nearPoint = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(x, y, 1.0F, 1.0F), inverse);
  const DirectX::XMVECTOR farPoint = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(x, y, 0.0F, 1.0F), inverse);
  Engine::PickRay ray{};
  DirectX::XMStoreFloat3(&ray.originMeters, nearPoint);
  DirectX::XMStoreFloat3(&ray.direction, DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(farPoint, nearPoint)));
  return ray;
}

WorldPick WorldPicker::Pick(const Engine::PickRay& _ray, std::span<const EngineBox> _engines) const
{
  // The ray starts at the near plane, far above the world: it is walked from where it comes down to the top of the
  // highest chunk, which keeps the float steps few, and stops once it is below the world's floor.
  Engine::PickRay walked = _ray;
  float skippedMeters = 0.0F;
  float reachMeters = MAX_PICK_METERS;
  if (_ray.direction.y < 0.0F)
  {
    if (_ray.originMeters.y > m_topMeters)
    {
      skippedMeters = (m_topMeters - _ray.originMeters.y) / _ray.direction.y;
      walked.originMeters = {_ray.originMeters.x + (skippedMeters * _ray.direction.x), m_topMeters,
                             _ray.originMeters.z + (skippedMeters * _ray.direction.z)};
    }
    reachMeters = std::min(reachMeters, ((-WORLD_VOXEL_METERS - walked.originMeters.y) / _ray.direction.y));
  }
  const auto solid = [this](std::int32_t _x, std::int32_t _y, std::int32_t _z)
  { return SolidAt({.xVoxels = _x, .yVoxels = _y, .zVoxels = _z}); };
  WorldPick pick{.engine = Protocol::NO_ENTITY, .voxel = Engine::PickVoxel(walked, WORLD_VOXEL_METERS, reachMeters, solid)};
  if (pick.voxel.has_value())
  {
    pick.voxel->distanceMeters += skippedMeters;
  }

  // The engines' boxes, each where the ray enters it, the nearest first; a world voxel nearer than that hides it.
  float nearestMeters = pick.voxel.has_value() ? pick.voxel->distanceMeters + BOX_TOLERANCE_METERS : std::numeric_limits<float>::infinity();
  for (const EngineBox& box : _engines)
  {
    const std::optional<float> enters = Enters(_ray, box);
    if (enters.has_value() && *enters <= nearestMeters)
    {
      pick.engine = box.engine;
      nearestMeters = *enters;
    }
  }
  return pick;
}

std::vector<OverlayBox> WorldPicker::Outline(const EngineBox& _box, std::uint8_t _paletteIndex)
{
  // The ring of cells one out from those the footprint covers, on the voxel the engine stands on.
  const auto low = [](float _meters) { return static_cast<std::int32_t>(std::floor(_meters)) - 1; };
  const auto high = [](float _meters) { return static_cast<std::int32_t>(std::ceil(_meters)); };
  const std::int32_t west = low(_box.xMeters);
  const std::int32_t east = high(_box.xMeters + _box.widthMeters);
  const std::int32_t south = low(_box.zMeters);
  const std::int32_t north = high(_box.zMeters + _box.depthMeters);
  const auto ground = static_cast<std::int32_t>(std::floor(_box.groundMeters)) - 1;
  std::vector<OverlayBox> ring;
  for (std::int32_t z = south; z <= north; ++z)
  {
    for (std::int32_t x = west; x <= east; ++x)
    {
      if (x == west || x == east || z == south || z == north)
      {
        ring.push_back({.cellVoxels = {.xVoxels = x, .yVoxels = ground, .zVoxels = z}, .paletteIndex = _paletteIndex});
      }
    }
  }
  return ring;
}

} // namespace Client
