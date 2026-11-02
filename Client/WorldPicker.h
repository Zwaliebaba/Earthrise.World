#pragma once

#include <DirectXMath.h>

#include "ChunkSnapshot.h"
#include "EntityId.h"
#include "VoxelPick.h"

#include <cstdint>
#include <map>
#include <optional>
#include <span>

namespace Client
{

/// An engine's footprint on the ground, where a click selects it: a click is tested against the footprint, not the
/// model's voxels (Hud.md §5).
struct EngineFootprint
{
  Protocol::EntityId engine;
  float xMeters; // its minimum corner
  float zMeters;
  float widthMeters; // along x and z, as it stands turned
  float depthMeters;
  float groundMeters; // the height it stands at
};

/// What the ray through a pixel meets.
struct WorldPick
{
  Protocol::EntityId engine;            // the engine whose footprint it reaches first, or NO_ENTITY
  std::optional<Engine::PickHit> voxel; // the first world voxel it meets, in world voxels
};

/// Picks in the world as the client was told it (ADR-002): over the session's chunk snapshots on the CPU (M3-E3), so
/// what a click finds is what was drawn, give or take a box's edge (ADR-001, *Consequences*).
class WorldPicker
{
public:
  /// Takes newly published snapshots, keeping the newest of each chunk.
  void TakeSnapshots(std::span<const Protocol::ChunkSnapshot> _snapshots);

  /// Whether the world voxel at _position holds anything, as the snapshots taken so far have it.
  [[nodiscard]] bool SolidAt(const Protocol::VoxelPosition& _position) const;

  /// The ray through the centre of the pixel at (_xPixels, _yPixels) of a target _widthPixels by _heightPixels, from the
  /// view-projection it was drawn with: from the near plane, along the view.
  [[nodiscard]] static Engine::PickRay RayThrough(const DirectX::XMFLOAT4X4& _viewProjection, std::int32_t _xPixels, std::int32_t _yPixels,
                                                  std::uint32_t _widthPixels, std::uint32_t _heightPixels) noexcept;

  /// The first world voxel _ray meets, and the engine of _engines whose footprint it reaches no further along.
  [[nodiscard]] WorldPick Pick(const Engine::PickRay& _ray, std::span<const EngineFootprint> _engines) const;

private:
  std::map<std::uint64_t, Protocol::ChunkSnapshot> m_snapshots;
  float m_topMeters = 0.0F; // the top of the highest chunk taken
};

} // namespace Client
