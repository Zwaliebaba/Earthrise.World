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
// The materials RendererLook.md §3's colour rules pick for (GameData/Server/Palette.json).
constexpr std::uint8_t MASONRY = 6;
constexpr std::uint8_t RUBBLE_OF_STONE = 8;
constexpr std::uint8_t WATER = 9;
// Rubble of masonry shows about 12% of its voxels in the masonry colour (RendererLook.md §3). Which
// ones is picked by the jitter's hash under a material ID no palette entry has, so the pick does
// not follow the jitter.
constexpr float RUBBLE_SHOWING_MASONRY = 0.12F;
constexpr std::uint8_t RUBBLE_PICK_SALT = 255;

std::span<const std::byte> BytesOf(std::span<const Engine::VoxelRecord> _records)
{
  return std::as_bytes(_records);
}

/// Each palette index's jitter and corner-shade table, from Content, with the entries the spike adds
/// beyond the materials': the two sides' colours jitter as faction does, and fords as water.
Engine::PaletteShading MakeShading()
{
  Engine::PaletteShading shading{};
  for (std::size_t index = 0; index < shading.jitter.size(); ++index)
  {
    const auto paletteIndex = static_cast<std::uint8_t>(index);
    shading.jitter[index] = Content::PaletteJitter(paletteIndex);
    shading.shadeTable[index] = static_cast<std::uint8_t>(Content::PaletteCornerShadeSet(paletteIndex));
  }
  for (const auto& [added, like] :
       {std::pair{ATTACKER_COLOR_INDEX, FACTION_INDEX}, std::pair{DEFENDER_COLOR_INDEX, FACTION_INDEX}, std::pair{FORD_COLOR_INDEX, WATER}})
  {
    shading.jitter[added] = shading.jitter[like];
    shading.shadeTable[added] = shading.shadeTable[like];
  }
  return shading;
}

constexpr std::int32_t EDGE = Protocol::CHUNK_EDGE_VOXELS;

/// A cell's offset in a chunk's voxels: x fastest, then z, then y.
std::size_t CellIndex(std::int32_t _x, std::int32_t _y, std::int32_t _z) noexcept
{
  constexpr auto EDGE_CELLS = static_cast<std::size_t>(EDGE);
  return (static_cast<std::size_t>(_y) * EDGE_CELLS + static_cast<std::size_t>(_z)) * EDGE_CELLS + static_cast<std::size_t>(_x);
}

/// The neighbours of a chunk that a changed voxel at _cell in it can reach: those across each face,
/// edge and corner of the chunk within one voxel of it horizontally and _yBand vertically. A voxel's
/// exposure reads its six face neighbours, its corner shades the cells about its faces' corners, one
/// step in each axis, and the ford rule a world meter of water above and below it.
void AddReachedNeighbors(Protocol::ChunkCoordinate _chunk, Protocol::VoxelPosition _cell, std::int32_t _yBand,
                         std::vector<std::uint64_t>& _stale)
{
  const auto steps = [](std::int32_t _value, std::int32_t _band)
  {
    const std::int32_t step = _value < _band ? -1 : (_value >= EDGE - _band ? 1 : 0);
    return std::array<std::int32_t, 2>{0, step};
  };
  const std::array<std::int32_t, 2> xSteps = steps(_cell.xVoxels, 1);
  const std::array<std::int32_t, 2> ySteps = steps(_cell.yVoxels, _yBand);
  const std::array<std::int32_t, 2> zSteps = steps(_cell.zVoxels, 1);
  for (std::size_t xi = 0; xi < (xSteps[1] != 0 ? 2U : 1U); ++xi)
  {
    for (std::size_t yi = 0; yi < (ySteps[1] != 0 ? 2U : 1U); ++yi)
    {
      for (std::size_t zi = 0; zi < (zSteps[1] != 0 ? 2U : 1U); ++zi)
      {
        const Protocol::ChunkCoordinate neighbor{
          .xChunks = _chunk.xChunks + xSteps[xi], .yChunks = _chunk.yChunks + ySteps[yi], .zChunks = _chunk.zChunks + zSteps[zi]};
        if ((xi | yi | zi) != 0 && Protocol::FitsChunkKey(neighbor))
        {
          _stale.push_back(Protocol::PackChunkKey(neighbor));
        }
      }
    }
  }
}

/// Every neighbour of a chunk, across its faces, edges and corners.
void AddAllNeighbors(Protocol::ChunkCoordinate _chunk, std::vector<std::uint64_t>& _stale)
{
  for (std::int32_t dy = -1; dy <= 1; ++dy)
  {
    for (std::int32_t dz = -1; dz <= 1; ++dz)
    {
      for (std::int32_t dx = -1; dx <= 1; ++dx)
      {
        const Protocol::ChunkCoordinate neighbor{
          .xChunks = _chunk.xChunks + dx, .yChunks = _chunk.yChunks + dy, .zChunks = _chunk.zChunks + dz};
        if ((dx != 0 || dy != 0 || dz != 0) && Protocol::FitsChunkKey(neighbor))
        {
          _stale.push_back(Protocol::PackChunkKey(neighbor));
        }
      }
    }
  }
}

} // namespace

WorldView::WorldView(ID3D12Device* _device, float _worldVoxelMeters)
  : m_device(_device),
    m_worldVoxelMeters(_worldVoxelMeters),
    m_jitterCellShift(static_cast<std::uint32_t>(std::lround(std::log2(1.0F / _worldVoxelMeters)))),
    m_shading(MakeShading())
{
}

void WorldView::TakeSnapshots(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring,
                              std::span<const Protocol::ChunkSnapshot> _snapshots)
{
  const std::int32_t yBand = std::int32_t{1} << m_jitterCellShift; // a world meter, for the ford rule
  std::vector<std::uint64_t> stale;
  for (const Protocol::ChunkSnapshot& snapshot : _snapshots)
  {
    const std::uint64_t key = Protocol::PackChunkKey(snapshot.coordinate);
    const auto previous = m_snapshots.find(key);
    if (previous == m_snapshots.end())
    {
      AddAllNeighbors(snapshot.coordinate, stale); // a chunk not seen before can reach any of them
    }
    else
    {
      // Only its changed voxels near its boundary reach a neighbour.
      const std::span<const std::uint8_t> before = previous->second.voxels->Voxels();
      const std::span<const std::uint8_t> after = snapshot.voxels->Voxels();
      for (std::int32_t y = 0; y < EDGE; ++y)
      {
        for (std::int32_t z = 0; z < EDGE; ++z)
        {
          for (std::int32_t x = 0; x < EDGE; ++x)
          {
            const bool nearBoundary = x == 0 || z == 0 || x == EDGE - 1 || z == EDGE - 1 || y < yBand || y >= EDGE - yBand;
            const std::size_t index = CellIndex(x, y, z);
            if (nearBoundary && before[index] != after[index])
            {
              AddReachedNeighbors(snapshot.coordinate, {.xVoxels = x, .yVoxels = y, .zVoxels = z}, yBand, stale);
            }
          }
        }
      }
    }
    m_snapshots.insert_or_assign(key, snapshot);
    stale.push_back(key);
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

std::uint8_t WorldView::IndexAt(Protocol::VoxelPosition _position) const
{
  const auto found = m_snapshots.find(Protocol::PackChunkKey(Protocol::ChunkContaining(_position)));
  if (found == m_snapshots.end())
  {
    return Protocol::Chunk::EMPTY;
  }
  const Protocol::VoxelPosition inside = Protocol::PositionInChunk(_position);
  return found->second.voxels->At(inside.xVoxels, inside.yVoxels, inside.zVoxels);
}

void WorldView::RebuildChunk(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring, std::uint64_t _key)
{
  const Protocol::ChunkSnapshot& snapshot = m_snapshots.at(_key);
  const Protocol::ChunkCoordinate coordinate = snapshot.coordinate;
  const std::array<std::int32_t, 3> origin = {coordinate.xChunks * EDGE, coordinate.yChunks * EDGE, coordinate.zChunks * EDGE};
  const auto world = [&origin](std::int32_t _x, std::int32_t _y, std::int32_t _z)
  { return Protocol::VoxelPosition{.xVoxels = origin[0] + _x, .yVoxels = origin[1] + _y, .zVoxels = origin[2] + _z}; };

  // A cell beyond the chunk is solid when the neighbouring snapshot says so, and below the world's
  // floor it is always solid (RendererLook.md §5); elsewhere with no snapshot it is air.
  const auto solidOutside = [this, &world](std::int32_t _x, std::int32_t _y, std::int32_t _z)
  {
    const Protocol::VoxelPosition position = world(_x, _y, _z);
    return position.yVoxels < 0 || IndexAt(position) != Protocol::Chunk::EMPTY;
  };

  // What the chunk is drawn in: its voxels, with RendererLook.md §3's two colour rules applied. They
  // pick colours only; the simulation's voxels are untouched.
  const std::span<const std::uint8_t> voxels = snapshot.voxels->Voxels();
  std::vector<std::uint8_t> shown(voxels.begin(), voxels.end());
  const std::int32_t fordVoxels = std::int32_t{1} << m_jitterCellShift; // one world meter of water
  const auto indexNear = [&](std::int32_t _x, std::int32_t _y, std::int32_t _z)
  {
    return _x >= 0 && _y >= 0 && _z >= 0 && _x < EDGE && _y < EDGE && _z < EDGE ? voxels[CellIndex(_x, _y, _z)]
                                                                                : IndexAt(world(_x, _y, _z));
  };
  std::size_t index = 0;
  for (std::int32_t y = 0; y < EDGE; ++y)
  {
    for (std::int32_t z = 0; z < EDGE; ++z)
    {
      for (std::int32_t x = 0; x < EDGE; ++x, ++index)
      {
        if (voxels[index] == RUBBLE_OF_STONE)
        {
          const Protocol::VoxelPosition cell = world(x, y, z);
          const float pick = Engine::JitterHash(cell.xVoxels >> m_jitterCellShift, cell.yVoxels >> m_jitterCellShift,
                                                cell.zVoxels >> m_jitterCellShift, RUBBLE_PICK_SALT);
          shown[index] = pick < RUBBLE_SHOWING_MASONRY ? MASONRY : RUBBLE_OF_STONE;
        }
        else if (voxels[index] == WATER)
        {
          // A ford: no more than a world meter of water, over a floor that is not water.
          std::int32_t bottom = y;
          while (y - bottom < fordVoxels && indexNear(x, bottom - 1, z) == WATER)
          {
            --bottom;
          }
          std::int32_t top = y;
          while (top - bottom < fordVoxels && indexNear(x, top + 1, z) == WATER)
          {
            ++top;
          }
          const std::uint8_t bed = indexNear(x, bottom - 1, z);
          const bool shallow = top - bottom < fordVoxels && indexNear(x, top + 1, z) != WATER;
          if (shallow && bed != Protocol::Chunk::EMPTY && bed != WATER)
          {
            shown[index] = FORD_COLOR_INDEX;
          }
        }
      }
    }
  }

  const std::vector<Engine::VoxelRecord> records =
    Engine::BuildVisibleRecords({.paletteIndices = shown, .xVoxels = EDGE, .yVoxels = EDGE, .zVoxels = EDGE}, solidOutside,
                                {.palette = &m_shading, .originCells = origin, .jitterCellShift = m_jitterCellShift});
  Batch batch = Upload(_commandList, _ring, records);
  batch.originMeters = {static_cast<float>(origin[0]) * m_worldVoxelMeters, static_cast<float>(origin[1]) * m_worldVoxelMeters,
                        static_cast<float>(origin[2]) * m_worldVoxelMeters};
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
                                  [](std::int32_t, std::int32_t, std::int32_t) { return false; },
                                  {.palette = &m_shading, .originCells = {0, 0, 0}, .jitterCellShift = 0});
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
