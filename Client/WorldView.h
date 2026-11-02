#pragma once

#include "framework.h"

#include <unknwn.h>

#include <DirectXMath.h>
#include <d3d12.h>
#include <winrt/base.h>

#include "ChunkSnapshot.h"
#include "ModelPlacement.h"
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

namespace Client
{

/// What the client draws, and only from what it was told (ADR-002): one batch of voxel records per
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

  /// Draws these placements this frame, each with records of its own: its model's voxels, their
  /// corner shades counting the world where it stands as solid as well as the model, so that it is
  /// shaded where it touches the ground (RendererLook.md §5; the owner's choice of 2026-11-01, baked
  /// per instance on the CPU). A placement keeps its records while its model, place and turn are as
  /// they were and no chunk under it was rebuilt; the rest are worked out again. All of them are
  /// copied into _ring, which this frame's draws read. Called each frame, after TakeSnapshots.
  ///
  /// Units are drawn as impostors once a model voxel is smaller than about 3 px on screen, which
  /// _modelVoxelPixels says, and engines stay at full detail (RendererLook.md §7). Throws
  /// std::runtime_error for an unknown model, or when the ring has no room.
  void PlaceModels(Engine::UploadRing& _ring, const std::vector<ModelPlacement>& _placements, float _modelVoxelPixels);

  /// Draws every chunk batch and every model placement, through whichever of the pass's pipelines
  /// was last begun: the scene's, or a shadow map's.
  void Draw(ID3D12GraphicsCommandList* _commandList, const Engine::VoxelPass& _pass) const;
  /// The world's chunks alone, for the static shadow map.
  void DrawChunks(ID3D12GraphicsCommandList* _commandList, const Engine::VoxelPass& _pass) const;
  /// The model placements alone, for the dynamic shadow map.
  void DrawModels(ID3D12GraphicsCommandList* _commandList, const Engine::VoxelPass& _pass) const;

  /// Forgets the world: every chunk, snapshot and placement, for a fresh world such as the lab's next item. The caller
  /// first makes sure the GPU no longer reads their buffers (a flush).
  void Forget();

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

  /// A model placed this frame: its records, and where this frame's copy of them is in the ring.
  struct Placed
  {
    ModelPlacement placement;
    bool impostor; // drawn as its unit's impostor
    std::vector<Engine::VoxelRecord> records;
    D3D12_GPU_VIRTUAL_ADDRESS recordsAddress;
    // What is drawn: the model as placed, or its impostor, which stands where the model's footprint
    // is centred and is never turned.
    DirectX::XMFLOAT3 drawnOriginMeters;
    std::uint32_t drawnQuarterTurns;
    float voxelEdgeMeters;
    std::uint32_t widthCells; // the drawn grid's extent along x and z, which turning it needs
    std::uint32_t depthCells;
  };

  void RebuildChunk(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring, std::uint64_t _key);
  /// Works out a placement's records against the world it stands in: its model's, or its
  /// impostor's when _asImpostor.
  void BuildPlaced(Placed& _placed, bool _asImpostor) const;
  /// Whether a chunk rebuilt since the last PlaceModels lies under a placement, or beside it.
  [[nodiscard]] bool WorldChangedUnder(const Placed& _placed) const;
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
  std::vector<std::uint64_t> m_rebuiltChunks; // since the last PlaceModels
  std::vector<Placed> m_placed;
  bool m_impostors = false;                                                               // units are drawn as impostors
  std::vector<std::unique_ptr<Engine::StaticBuffer>> m_replaced;                          // not yet stamped
  std::vector<std::pair<std::uint64_t, std::unique_ptr<Engine::StaticBuffer>>> m_retired; // fence, buffer
};

} // namespace Client
