#pragma once

#include "framework.h"

#include <unknwn.h>

#include <DirectXMath.h>
#include <d3d12.h>
#include <winrt/base.h>

#include "ChunkSnapshot.h"
#include "SceneScript.h"
#include "StaticBuffer.h"
#include "UploadRing.h"
#include "VoxelGridView.h"
#include "VoxelPass.h"
#include "VoxelRecord.h"

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace Spike
{

/// The palette entry fords are drawn in: water one world voxel deep over a solid floor draws
/// lighter, so fords show (RendererLook.md §3, index 9). Its colour is water's with its HSL
/// lightness raised by FORD_LIGHTNESS_SCALE, which RendererLook.md proposes as 15% and leaves to the
/// spike to tune.
inline constexpr std::uint8_t FORD_COLOR_INDEX = 242;
inline constexpr float FORD_LIGHTNESS_SCALE = 1.15F;

/// What the spike draws, and only from what it was told (ADR-002): one batch of voxel records per
/// world chunk, built from the chunk snapshots the simulation publishes, and one per model, drawn at
/// each placement. Both go through the one voxel pass (MvpPlan.md, M1: two voxel sources).
class WorldView
{
public:
  /// Draws world chunks with voxels _worldVoxelMeters on an edge: 1, or 0.5 for ADR-006's comparison.
  WorldView(ID3D12Device* _device, float _worldVoxelMeters);

  /// Takes newly published snapshots and rebuilds their chunks' batches, and those of the
  /// neighbours whose exposed faces or corner shades a changed voxel can reach: any of the 26 a
  /// voxel on the chunk's boundary touches. Records the uploads on _commandList.
  void TakeSnapshots(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring,
                     std::span<const Protocol::ChunkSnapshot> _snapshots);

  /// Draws these placements from now on, building a batch for each model they use that has none
  /// yet. Called each frame, as units move. Throws std::runtime_error for an unknown model.
  void PlaceModels(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring, const std::vector<ModelPlacement>& _placements);

  /// Draws every chunk batch and every model placement, through whichever of the pass's pipelines
  /// was last begun: the scene's, or a shadow map's.
  void Draw(ID3D12GraphicsCommandList* _commandList, const Engine::VoxelPass& _pass) const;
  /// The world's chunks alone, for the static shadow map.
  void DrawChunks(ID3D12GraphicsCommandList* _commandList, const Engine::VoxelPass& _pass) const;
  /// The model placements alone, for the dynamic shadow map.
  void DrawModels(ID3D12GraphicsCommandList* _commandList, const Engine::VoxelPass& _pass) const;

  /// Records across every chunk batch and model placement: what one frame draws.
  [[nodiscard]] std::uint64_t RecordsDrawn() const noexcept;

  /// Stamps the buffers replaced since the last call with the fence of the frame that last read
  /// them, and frees those whose fence the GPU has passed. A rebuilt chunk's old buffer may still be
  /// read by a frame in flight, so it is never freed at once.
  void Retire(std::uint64_t _fenceValue);
  void Release(std::uint64_t _completedFenceValue);

private:
  struct Batch
  {
    std::unique_ptr<Engine::StaticBuffer> records;
    std::uint32_t count;
    DirectX::XMFLOAT3 originMeters;
    float voxelEdgeMeters;
    std::uint32_t widthCells; // a model's extent along x and z, which turning it needs
    std::uint32_t depthCells;
  };

  void RebuildChunk(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring, std::uint64_t _key);
  /// The palette index at a world voxel, from the snapshots taken so far; 0 where none holds it.
  [[nodiscard]] std::uint8_t IndexAt(Protocol::VoxelPosition _position) const;
  [[nodiscard]] Batch Upload(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring,
                             std::span<const Engine::VoxelRecord> _records) const;

  ID3D12Device* m_device;
  float m_worldVoxelMeters;
  std::uint32_t m_jitterCellShift; // world voxels to a 1 m jitter cell, as a power of two
  Engine::PaletteShading m_shading;
  std::map<std::uint64_t, Protocol::ChunkSnapshot> m_snapshots;
  std::map<std::uint64_t, Batch> m_chunks;
  std::map<std::string_view, Batch> m_models;
  std::vector<ModelPlacement> m_placements;
  std::vector<std::unique_ptr<Engine::StaticBuffer>> m_replaced;                          // not yet stamped
  std::vector<std::pair<std::uint64_t, std::unique_ptr<Engine::StaticBuffer>>> m_retired; // fence, buffer
};

} // namespace Spike
