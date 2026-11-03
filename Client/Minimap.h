#pragma once

#include <DirectXMath.h>

#include "ChunkSnapshot.h"
#include "HudModel.h"
#include "Selection.h"
#include "Session.h"
#include "StateUpdate.h"
#include "VoxelPick.h"

#include <array>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace Client
{

/// The minimap (Hud.md §6; M4-I5): north up, from what the player knows. Its base is the top voxel of each 1 m column
/// of the session's last-seen chunks, its palette colour shaded by height, reduced to the drawn size, never a 1 m texture
/// scaled; it keeps each column's top and works out again only the columns of the chunks that changed. Squads,
/// structures, the camera's view quad and the alerts' pings are drawn over it from the update alone.
class Minimap
{
public:
  /// Remembers the map's size in cells and how many chunks tall the world is, and forgets every column.
  void Reset(std::int32_t _widthCells, std::int32_t _depthCells, std::int32_t _heightChunks);

  /// Works out again the columns of _snapshots' chunks from the session's newest chunks (ADR-025, decision 4), and marks
  /// the base dirty when any changed.
  void Take(std::span<const Protocol::ChunkSnapshot> _snapshots, const Session& _session);

  /// Whether the base changed since the last BuildBase.
  [[nodiscard]] bool BaseDirty() const noexcept;

  /// The base, _sidePixels square, row by row from the north-west corner, each texel 0xAABBGGRR in sRGB: the columns it
  /// covers, their top voxels' colours averaged and shaded by height; transparent where none has been seen.
  [[nodiscard]] std::vector<std::uint32_t> BuildBase(std::uint32_t _sidePixels);

  /// The first and one past the last of the map's _cells that minimap pixel _pixel of _sidePixels covers: the pixels
  /// share the columns out with none left out and none twice.
  [[nodiscard]] static std::pair<std::int32_t, std::int32_t> ColumnsOf(std::uint32_t _pixel, std::uint32_t _sidePixels,
                                                                       std::int32_t _cells) noexcept;

  /// Where a point of the map, in meters, is on a minimap _sidePixels square, from its top-left corner: x to the right,
  /// north up.
  [[nodiscard]] static DirectX::XMFLOAT2 ToMinimap(float _xMeters, float _zMeters, std::int32_t _widthCells, std::int32_t _depthCells,
                                                   std::uint32_t _sidePixels) noexcept;
  /// And back: the map's point, in meters, under minimap pixel (_x, _y).
  [[nodiscard]] static DirectX::XMFLOAT2 ToMap(float _xPixels, float _yPixels, std::int32_t _widthCells, std::int32_t _depthCells,
                                               std::uint32_t _sidePixels) noexcept;

  /// The height of the top voxel of the column under map point (_xMeters, _zMeters), the one a pick on the ground there
  /// would hit, or 0 unseen.
  [[nodiscard]] std::int32_t GroundAt(float _xMeters, float _zMeters) const noexcept;

  /// The squads as 3 px dots and the structures as 7 px dots, each where the update puts it, in its team's colour or
  /// #ffe2a8 when selected (Hud.md §6).
  [[nodiscard]] static std::vector<HudDot> Dots(const Protocol::StateUpdate& _state, const Selection& _selection,
                                                std::uint32_t _sidePixels);

  /// The camera's view quad on the minimap: where the rays through the screen's four corners, top-left, top-right,
  /// bottom-right and bottom-left, meet the ground's plane at _groundMeters.
  [[nodiscard]] static std::array<DirectX::XMFLOAT2, 4> ViewQuad(std::span<const Engine::PickRay, 4> _corners, float _groundMeters,
                                                                 std::int32_t _widthCells, std::int32_t _depthCells,
                                                                 std::uint32_t _sidePixels) noexcept;

private:
  void TakeColumns(std::int32_t _xChunk, std::int32_t _zChunk, const Session& _session);

  std::int32_t m_widthCells = 0;
  std::int32_t m_depthCells = 0;
  std::int32_t m_heightChunks = 0;
  std::vector<std::uint8_t> m_topIndex; // each column's top voxel's palette index, 0 unseen
  std::vector<std::int16_t> m_topY;     // and its height in voxels
  bool m_dirty = true;
};

} // namespace Client
