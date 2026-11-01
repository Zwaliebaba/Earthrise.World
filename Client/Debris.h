#pragma once

#include <DirectXMath.h>

#include "OrientedVoxel.h"
#include "PlacedVoxel.h"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace Client
{

/// A collapse's debris (RendererLook.md §1; GameDesign.md, M3's values, item 16): the v3 package's pieces, D1 to D6,
/// chosen by the rubble they lie on (Content::FindDebrisChoice) and scattered as MvpAssets.md, *Poses, projectiles,
/// gates and debris*, says. Each falls with the collapse for FALL_SECONDS, turning, as its model's voxels, and then
/// rests on the rubble at a quarter turn of yaw for good. Every piece comes from the collapse's id by keyed draws
/// (Core::KeyedDraw), so every client shows the same, and none is in the world: debris blocks no path and no shot.
class Debris
{
public:
  static constexpr float FALL_SECONDS = 1.5F;
  /// One piece for about this many square meters of a rubble's top.
  static constexpr std::uint32_t COLUMNS_PER_PIECE = 4;

  /// A piece, where it falls from and where it comes to rest.
  struct Piece
  {
    std::string_view modelId;
    std::int32_t cellXVoxels; // the landing column it rests on
    std::int32_t cellZVoxels;
    float restingBottomMeters; // the top face of that column's top voxel
    std::uint32_t quarterTurns;
    std::uint8_t tintIndex; // every voxel drawn in this palette index, or 0 for the model's own
    DirectX::XMFLOAT3 fromMeters;
    DirectX::XMFLOAT3 restingCenterMeters;
    DirectX::XMFLOAT4 fromRotation;
    DirectX::XMFLOAT4 restingRotation;
    float delaySeconds; // after the collapse starts to fall, so the pieces do not all leave at once
  };

  /// Scatters a collapse's pieces at _startSeconds, from the voxels that fell, over the top voxel of each column of
  /// its landing. A column whose top is a rubble with no debris choice gets none. Replaces any debris scattered
  /// before.
  void Scatter(std::uint64_t _collapseId, std::span<const Protocol::PlacedVoxel> _fallen, std::span<const Protocol::PlacedVoxel> _landing,
               float _startSeconds);

  /// Every voxel of every piece as it is at _seconds, appended to _voxels: falling, turning, or at rest.
  void Pose(float _seconds, std::vector<Engine::OrientedVoxel>& _voxels) const;

  [[nodiscard]] std::span<const Piece> Pieces() const noexcept;

private:
  /// A model voxel of a piece: where it sits from the model's center, before the piece's turn.
  struct PieceVoxel
  {
    DirectX::XMFLOAT3 offsetMeters;
    std::uint8_t paletteIndex;
    float lightnessOffset;
  };

  std::vector<Piece> m_pieces;
  std::vector<std::vector<PieceVoxel>> m_voxels; // by piece
  float m_startSeconds = 0.0F;
};

} // namespace Client
