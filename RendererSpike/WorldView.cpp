#include "pch.h"

#include "WorldView.h"

#include "Catalog.h"
#include "ChunkCoordinate.h"
#include "VoxelGridView.h"

namespace Spike
{

namespace
{

// The palette's faction colour, which a model draws in its side's colour (GameData/Server/Palette.json).
constexpr std::uint8_t FACTION_INDEX = 16;

std::span<const std::byte> BytesOf(std::span<const Engine::VoxelRecord> _records)
{
  return std::as_bytes(_records);
}

} // namespace

WorldView::WorldView(ID3D12Device* _device, float _worldVoxelMeters)
  : m_device(_device),
    m_worldVoxelMeters(_worldVoxelMeters)
{
}

void WorldView::TakeSnapshots(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring,
                              std::span<const Protocol::ChunkSnapshot> _snapshots)
{
  std::vector<std::uint64_t> stale;
  for (const Protocol::ChunkSnapshot& snapshot : _snapshots)
  {
    const std::uint64_t key = Protocol::PackChunkKey(snapshot.coordinate);
    m_snapshots.insert_or_assign(key, snapshot);
    stale.push_back(key);
    const Protocol::ChunkCoordinate c = snapshot.coordinate;
    for (const Protocol::ChunkCoordinate neighbor :
         {Protocol::ChunkCoordinate{c.xChunks - 1, c.yChunks, c.zChunks}, Protocol::ChunkCoordinate{c.xChunks + 1, c.yChunks, c.zChunks},
          Protocol::ChunkCoordinate{c.xChunks, c.yChunks - 1, c.zChunks}, Protocol::ChunkCoordinate{c.xChunks, c.yChunks + 1, c.zChunks},
          Protocol::ChunkCoordinate{c.xChunks, c.yChunks, c.zChunks - 1}, Protocol::ChunkCoordinate{c.xChunks, c.yChunks, c.zChunks + 1}})
    {
      if (Protocol::FitsChunkKey(neighbor))
      {
        stale.push_back(Protocol::PackChunkKey(neighbor));
      }
    }
  }
  std::ranges::sort(stale);
  const auto [first, last] = std::ranges::unique(stale);
  stale.erase(first, last);
  for (const std::uint64_t key : stale)
  {
    if (m_snapshots.contains(key))
    {
      RebuildChunk(_commandList, _ring, key);
    }
  }
}

void WorldView::RebuildChunk(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring, std::uint64_t _key)
{
  const Protocol::ChunkSnapshot& snapshot = m_snapshots.at(_key);
  const Protocol::ChunkCoordinate coordinate = snapshot.coordinate;
  constexpr std::int32_t EDGE = Protocol::CHUNK_EDGE_VOXELS;

  // A cell beyond the chunk is solid when the neighbouring snapshot says so; with no snapshot it is air.
  const auto solidOutside = [this, coordinate](std::int32_t _x, std::int32_t _y, std::int32_t _z)
  {
    const Protocol::VoxelPosition world{
      .xVoxels = coordinate.xChunks * EDGE + _x, .yVoxels = coordinate.yChunks * EDGE + _y, .zVoxels = coordinate.zChunks * EDGE + _z};
    const Protocol::ChunkCoordinate owner = Protocol::ChunkContaining(world);
    const auto found = m_snapshots.find(Protocol::PackChunkKey(owner));
    if (found == m_snapshots.end())
    {
      return false;
    }
    const Protocol::VoxelPosition inside = Protocol::PositionInChunk(world);
    return found->second.voxels->At(inside.xVoxels, inside.yVoxels, inside.zVoxels) != Protocol::Chunk::EMPTY;
  };

  const std::vector<Engine::VoxelRecord> records = Engine::BuildVisibleRecords(
    {.paletteIndices = snapshot.voxels->Voxels(), .xVoxels = EDGE, .yVoxels = EDGE, .zVoxels = EDGE}, solidOutside);
  Batch batch = Upload(_commandList, _ring, records);
  batch.originMeters = {static_cast<float>(coordinate.xChunks * EDGE) * m_worldVoxelMeters,
                        static_cast<float>(coordinate.yChunks * EDGE) * m_worldVoxelMeters,
                        static_cast<float>(coordinate.zChunks * EDGE) * m_worldVoxelMeters};
  batch.voxelEdgeMeters = m_worldVoxelMeters;
  const auto old = m_chunks.find(_key);
  if (old != m_chunks.end() && old->second.records)
  {
    m_replaced.push_back(std::move(old->second.records));
  }
  m_chunks.insert_or_assign(_key, std::move(batch));
}

void WorldView::PlaceModels(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring,
                            const std::vector<ModelPlacement>& _placements)
{
  for (const ModelPlacement& placement : _placements)
  {
    if (m_models.contains(placement.modelId))
    {
      continue;
    }
    const Content::ModelView* model = Content::FindModel(placement.modelId);
    if (model == nullptr)
    {
      throw std::runtime_error("the scene places a model Content does not have");
    }
    const std::vector<Engine::VoxelRecord> records =
      Engine::BuildVisibleRecords({.paletteIndices = model->grid.indices,
                                   .xVoxels = model->grid.widthVoxels,
                                   .yVoxels = model->grid.heightVoxels,
                                   .zVoxels = model->grid.depthVoxels},
                                  [](std::int32_t, std::int32_t, std::int32_t) { return false; });
    Batch batch = Upload(_commandList, _ring, records);
    batch.voxelEdgeMeters = static_cast<float>(model->grid.voxelEdgeMillimeters) / 1000.0F;
    batch.widthCells = model->grid.widthVoxels;
    batch.depthCells = model->grid.depthVoxels;
    m_models.emplace(placement.modelId, std::move(batch));
  }
  m_placements = _placements;
}

WorldView::Batch WorldView::Upload(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring,
                                   std::span<const Engine::VoxelRecord> _records) const
{
  Batch batch{.records = nullptr,
              .count = static_cast<std::uint32_t>(_records.size()),
              .originMeters = {},
              .voxelEdgeMeters = 0.0F,
              .widthCells = 0,
              .depthCells = 0};
  if (!_records.empty())
  {
    batch.records = std::make_unique<Engine::StaticBuffer>(m_device, _records.size_bytes());
    batch.records->Upload(_commandList, _ring, BytesOf(_records));
  }
  return batch;
}

void WorldView::Draw(ID3D12GraphicsCommandList* _commandList, const Engine::VoxelPass& _pass) const
{
  for (const auto& [key, batch] : m_chunks)
  {
    if (batch.records)
    {
      _pass.Draw(_commandList, batch.records->GpuAddress(), batch.count,
                 {.originMeters = batch.originMeters,
                  .voxelEdgeMeters = batch.voxelEdgeMeters,
                  .quarterTurns = 0,
                  .widthCells = 0,
                  .depthCells = 0,
                  .swapFromIndex = 0,
                  .swapToIndex = 0});
    }
  }
  for (const ModelPlacement& placement : m_placements)
  {
    const Batch& batch = m_models.at(placement.modelId);
    if (batch.records)
    {
      _pass.Draw(_commandList, batch.records->GpuAddress(), batch.count,
                 {.originMeters = {placement.xMeters, placement.yMeters, placement.zMeters},
                  .voxelEdgeMeters = batch.voxelEdgeMeters,
                  .quarterTurns = placement.quarterTurns,
                  .widthCells = batch.widthCells,
                  .depthCells = batch.depthCells,
                  // A model's faction colour is drawn as its side's (RendererLook.md).
                  .swapFromIndex = placement.sideColorIndex != 0 ? FACTION_INDEX : std::uint8_t{0},
                  .swapToIndex = placement.sideColorIndex});
    }
  }
}

void WorldView::Retire(std::uint64_t _fenceValue)
{
  for (std::unique_ptr<Engine::StaticBuffer>& buffer : m_replaced)
  {
    m_retired.emplace_back(_fenceValue, std::move(buffer));
  }
  m_replaced.clear();
}

void WorldView::Release(std::uint64_t _completedFenceValue)
{
  std::erase_if(m_retired, [_completedFenceValue](const auto& _retired) { return _retired.first <= _completedFenceValue; });
}

std::uint64_t WorldView::RecordsDrawn() const noexcept
{
  std::uint64_t total = 0;
  for (const auto& [key, batch] : m_chunks)
  {
    total += batch.count;
  }
  for (const ModelPlacement& placement : m_placements)
  {
    const auto found = m_models.find(placement.modelId);
    total += found == m_models.end() ? 0 : found->second.count;
  }
  return total;
}

} // namespace Spike
