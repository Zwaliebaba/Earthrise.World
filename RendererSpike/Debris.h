#pragma once

#include <DirectXMath.h>

#include "OrientedVoxel.h"
#include "TestWorld.h"

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace RendererSpike
{

/// A collapse's debris (RendererLook.md §1; the owner's choice of 2026-11-01, M1's open values,
/// item 7): loose blocks of 0.5 to 1 m and 0.5 m pebbles that fall with the collapse for about
/// 1.5 s, turning, as oriented voxels, and then lie on its rubble at their final angles for good, so
/// that a breach keeps its ragged look. Every piece comes from the collapse's id by a pinned
/// integer hash, so every client shows the same, and none is in the world: debris blocks no path
/// and no shot.
class Debris
{
public:
  static constexpr float FALL_SECONDS = 1.5F;

  /// The height of the world's top at a column, in meters: where a piece comes to rest.
  using RestingTop = std::function<float(float, float)>;

  /// Scatters a collapse's pieces at _startSeconds, from the voxels that fell onto the rubble they
  /// became: each lands on one of the rubble's columns, at the world's top there, so the debris lies
  /// over the rubble (RendererLook.md §1). Replaces any debris scattered before.
  void Scatter(std::uint64_t _collapseId, std::span<const Simulation::FallenVoxel> _fallen,
               std::span<const Protocol::VoxelPosition> _rubble, const RestingTop& _restingTop, float _startSeconds);

  /// Every piece as it is at _seconds, appended to _voxels: falling, turning, or at rest.
  void Pose(float _seconds, std::vector<Engine::OrientedVoxel>& _voxels) const;

  [[nodiscard]] std::size_t Pieces() const noexcept;

private:
  struct Piece
  {
    DirectX::XMFLOAT3 fromMeters;
    DirectX::XMFLOAT3 toMeters;
    DirectX::XMFLOAT4 fromRotation;
    DirectX::XMFLOAT4 toRotation;
    float halfEdgeMeters;
    float delaySeconds; // after the collapse, so the pieces do not all leave at once
    std::uint8_t paletteIndex;
    float lightnessOffset;
  };

  std::vector<Piece> m_pieces;
  float m_startSeconds = 0.0F;
};

} // namespace RendererSpike
