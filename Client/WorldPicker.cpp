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
/// How much further along the ray than the voxel it meets a footprint may be and still be picked: the ground under an
/// engine's footprint is the same plane, met at the same distance give or take float's rounding.
constexpr float FOOTPRINT_TOLERANCE_METERS = 0.01F;

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

WorldPick WorldPicker::Pick(const Engine::PickRay& _ray, std::span<const EngineFootprint> _engines) const
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

  // The footprints, each where the ray comes down to the ground it stands on.
  if (_ray.direction.y < 0.0F)
  {
    float nearestMeters =
      pick.voxel.has_value() ? pick.voxel->distanceMeters + FOOTPRINT_TOLERANCE_METERS : std::numeric_limits<float>::infinity();
    for (const EngineFootprint& footprint : _engines)
    {
      const float along = (footprint.groundMeters - _ray.originMeters.y) / _ray.direction.y;
      const float x = _ray.originMeters.x + (along * _ray.direction.x);
      const float z = _ray.originMeters.z + (along * _ray.direction.z);
      const bool inside = x >= footprint.xMeters && x < footprint.xMeters + footprint.widthMeters && z >= footprint.zMeters &&
                          z < footprint.zMeters + footprint.depthMeters;
      if (along >= 0.0F && inside && along <= nearestMeters)
      {
        pick.engine = footprint.engine;
        nearestMeters = along;
      }
    }
  }
  return pick;
}

} // namespace Client
