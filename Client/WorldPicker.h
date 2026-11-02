#pragma once

#include <DirectXMath.h>

#include "ChunkSnapshot.h"
#include "EntityId.h"
#include "OverlayBox.h"
#include "VoxelPick.h"

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace Client
{

/// The box of an engine's model as placed and turned, its full height, where a click selects it (Hud.md §5, amended
/// for engines by M3-H3): most of a trebuchet on screen is frame and arm, over the ground behind its footprint. Squads
/// keep the footprint, since a figure is a few pixels wide.
struct EngineBox
{
  Protocol::EntityId engine;
  float xMeters; // its minimum corner
  float zMeters;
  float widthMeters; // along x and z, as it stands turned
  float depthMeters;
  float groundMeters; // the height it stands at, its foot
  float heightMeters; // the model's, from its foot
};

/// What the ray through a pixel meets.
struct WorldPick
{
  Protocol::EntityId engine;            // the engine whose box it enters first, or NO_ENTITY
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

  /// The first world voxel _ray meets, and the engine of _engines whose box it enters first, unless that voxel is nearer.
  [[nodiscard]] WorldPick Pick(const Engine::PickRay& _ray, std::span<const EngineBox> _engines) const;

  /// The ground cells around _box's footprint, one cell out from it, where the hover and the selection outline it in
  /// _paletteIndex (M3-H3): the model stands on the cells inside.
  [[nodiscard]] static std::vector<OverlayBox> Outline(const EngineBox& _box, std::uint8_t _paletteIndex);

private:
  std::map<std::uint64_t, Protocol::ChunkSnapshot> m_snapshots;
  float m_topMeters = 0.0F; // the top of the highest chunk taken
};

} // namespace Client
