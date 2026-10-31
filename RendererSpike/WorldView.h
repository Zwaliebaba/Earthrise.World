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

/// What the spike draws, and only from what it was told (ADR-002): one batch of voxel records per
/// world chunk, built from the chunk snapshots the simulation publishes, and one per model, drawn at
/// each placement. Both go through the one voxel pass (MvpPlan.md, M1: two voxel sources).
class WorldView
{
public:
  explicit WorldView(ID3D12Device* _device);

  /// Takes newly published snapshots and rebuilds their chunks' batches, and their neighbours',
  /// whose exposed faces can change with them. Records the uploads on _commandList.
  void TakeSnapshots(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring,
                     std::span<const Protocol::ChunkSnapshot> _snapshots);

  /// Builds a batch for each model the placements use. Throws std::runtime_error for an unknown model.
  void PlaceModels(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring, const std::vector<ModelPlacement>& _placements);

  void Draw(ID3D12GraphicsCommandList* _commandList, const Engine::VoxelPass& _pass) const;

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
  };

  void RebuildChunk(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring, std::uint64_t _key);
  [[nodiscard]] Batch Upload(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring,
                             std::span<const Engine::VoxelRecord> _records) const;

  ID3D12Device* m_device;
  std::map<std::uint64_t, Protocol::ChunkSnapshot> m_snapshots;
  std::map<std::uint64_t, Batch> m_chunks;
  std::map<std::string_view, Batch> m_models;
  std::vector<ModelPlacement> m_placements;
  std::vector<std::unique_ptr<Engine::StaticBuffer>> m_replaced;                          // not yet stamped
  std::vector<std::pair<std::uint64_t, std::unique_ptr<Engine::StaticBuffer>>> m_retired; // fence, buffer
};

} // namespace Spike
