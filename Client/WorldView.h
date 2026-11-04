#pragma once

#include "framework.h"

#include <unknwn.h>

#include <DirectXMath.h>
#include <d3d12.h>
#include <winrt/base.h>

#include "ChunkSnapshot.h"
#include "ModelPlacement.h"
#include "OrientedVoxel.h"
#include "StaticBuffer.h"
#include "UploadRing.h"
#include "VoxelGridView.h"
#include "VoxelPass.h"
#include "VoxelRecord.h"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
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

  /// Leaves _hidden's world voxels out of the world's batches from now on, drawn as air and read as air by their
  /// neighbours' faces and shades and by the models' corner shades: an open gate's, which its open model is drawn over
  /// (M4-H1; MvpAssets.md, *the open gates*). Rebuilds the chunks a voxel hidden or shown again reaches, as
  /// TakeSnapshots does for a changed one, and returns whether there were any. Called each frame, before TakeSnapshots.
  bool Hide(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring, std::span<const Protocol::VoxelPosition> _hidden);

  /// Draws these placements this frame, each with records of its own: its model's voxels, their
  /// corner shades counting the world where it stands as solid as well as the model, so that it is
  /// shaded where it touches the ground (RendererLook.md §5; the owner's choice of 2026-11-01, baked
  /// per instance on the CPU). A placement keeps its records while its model, place and turn are as
  /// they were and no chunk under it was rebuilt; the rest are worked out again. All of them are
  /// copied into _ring, which this frame's draws read. Called each frame, after TakeSnapshots.
  ///
  /// Units are drawn as impostors once a model voxel is smaller than about 3 px on screen, which
  /// _modelVoxelPixels says, and engines stay at full detail (RendererLook.md §7). A placement at a
  /// whole quarter turn, and an impostor, is drawn as records through a turned batch; any other as
  /// oriented voxels built from the same records, each turned about its footprint's middle, its
  /// corner shades read against the world at its turned cells, which DrawTurned draws (ADR-030,
  /// decisions 2 and 3). Throws std::runtime_error for an unknown model, or when the ring has no room.
  void PlaceModels(Engine::UploadRing& _ring, const std::vector<ModelPlacement>& _placements, float _modelVoxelPixels);

  /// Draws every chunk batch and every model placement, through whichever of the pass's pipelines
  /// was last begun: the scene's, or a shadow map's.
  void Draw(ID3D12GraphicsCommandList* _commandList, const Engine::VoxelPass& _pass) const;
  /// The world's chunks alone, for the static shadow map.
  void DrawChunks(ID3D12GraphicsCommandList* _commandList, const Engine::VoxelPass& _pass) const;
  /// The model placements drawn as records alone, for the dynamic shadow map too.
  void DrawModels(ID3D12GraphicsCommandList* _commandList, const Engine::VoxelPass& _pass) const;
  /// The turned placements' oriented voxels, in one draw, through whichever of the pass's oriented pipelines was last
  /// begun: the scene's, or the dynamic shadow map's (ADR-030, decision 2).
  void DrawTurned(ID3D12GraphicsCommandList* _commandList, const Engine::VoxelPass& _pass) const;
  /// The turned placements' silhouettes, in one draw, through the pass's oriented silhouette pipeline, with
  /// DrawSilhouettes (ADR-030, decision 4).
  void DrawTurnedSilhouettes(ID3D12GraphicsCommandList* _commandList, const Engine::VoxelPass& _pass) const;
  /// Whether this frame's placements hold a turned model, and a turned silhouette.
  [[nodiscard]] bool HasTurned() const noexcept;
  [[nodiscard]] bool HasTurnedSilhouettes() const noexcept;
  /// The silhouettes of the placements drawn as one, each its model's records in its faction entry, through the pass's
  /// silhouette pipeline after the chunks and before the models (M4-H3; ADR-021, decision 6, amended).
  void DrawSilhouettes(ID3D12GraphicsCommandList* _commandList, const Engine::VoxelPass& _pass) const;
  /// Whether this frame's placements drawn as records hold a silhouette.
  [[nodiscard]] bool HasSilhouettes() const noexcept;

  /// Forgets the world: every chunk, snapshot and placement, for a fresh world such as the lab's next item. The caller
  /// first makes sure the GPU no longer reads their buffers (a flush).
  void Forget();

  /// Each palette index's jitter and corner-shade table, as the world's and the models' records are built with: the
  /// portraits' too (M4-I2).
  [[nodiscard]] const Engine::PaletteShading& Shading() const noexcept;

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
    std::vector<Engine::VoxelRecord> silhouetteRecords; // its records flat in its faction entry, or none
    D3D12_GPU_VIRTUAL_ADDRESS silhouetteAddress;
    // Turned to an angle other than a quarter turn: its records as oriented voxels instead, and its silhouette's, which
    // DrawTurned and DrawTurnedSilhouettes draw with every other turned placement's.
    std::vector<Engine::OrientedVoxel> turnedVoxels;
    std::vector<Engine::OrientedVoxel> turnedSilhouette;
    // What is drawn: the model as placed, or its impostor, which stands where the model's footprint
    // is centred and is never turned.
    DirectX::XMFLOAT3 drawnOriginMeters; // the minimum corner of the box that bounds it
    std::uint32_t drawnQuarterTurns;
    float voxelEdgeMeters;
    std::uint32_t widthCells; // the drawn grid's extent along x and z, which turning it needs
    std::uint32_t depthCells;
    float drawnWidthMeters; // the box that bounds it, along x and z
    float drawnDepthMeters;
  };

  void RebuildChunk(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring, std::uint64_t _key);
  /// Works out a placement's records against the world it stands in: its model's, or its
  /// impostor's when _asImpostor.
  void BuildPlaced(Placed& _placed, bool _asImpostor) const;
  /// Whether a chunk rebuilt since the last PlaceModels lies under a placement, or beside it.
  [[nodiscard]] bool WorldChangedUnder(const Placed& _placed) const;
  /// The palette index at a world voxel, from the snapshots taken so far; 0 where none holds it.
  [[nodiscard]] std::uint8_t IndexAt(Protocol::VoxelPosition _position) const;
  /// Whether a world voxel is one Hide leaves out.
  [[nodiscard]] bool Hidden(Protocol::VoxelPosition _position) const;
  [[nodiscard]] Batch Upload(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring,
                             std::span<const Engine::VoxelRecord> _records) const;

  ID3D12Device* m_device;
  float m_worldVoxelMeters;
  std::uint32_t m_jitterCellShift; // world voxels to a 1 m jitter cell, as a power of two
  Engine::PaletteShading m_shading;
  std::map<std::uint64_t, Protocol::ChunkSnapshot> m_snapshots;
  std::map<std::uint64_t, Batch> m_chunks;
  std::vector<std::uint64_t> m_rebuiltChunks;     // since the last PlaceModels
  std::set<std::array<std::int32_t, 3>> m_hidden; // x, y and z of each voxel Hide leaves out
  std::vector<Placed> m_placed;
  // This frame's turned voxels and silhouettes, every turned placement's in one run each, and where their copies are.
  std::vector<Engine::OrientedVoxel> m_turnedVoxels;
  std::vector<Engine::OrientedVoxel> m_turnedSilhouettes;
  D3D12_GPU_VIRTUAL_ADDRESS m_turnedAddress = 0;
  D3D12_GPU_VIRTUAL_ADDRESS m_turnedSilhouettesAddress = 0;
  bool m_impostors = false;                                                               // units are drawn as impostors
  std::vector<std::unique_ptr<Engine::StaticBuffer>> m_replaced;                          // not yet stamped
  std::vector<std::pair<std::uint64_t, std::unique_ptr<Engine::StaticBuffer>>> m_retired; // fence, buffer
};

} // namespace Client
