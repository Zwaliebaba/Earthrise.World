#include "pch.h"

#include "Minimap.h"

#include "Interpolation.h"
#include "Palette.h"

namespace Client
{

namespace
{

constexpr std::int32_t EDGE = Protocol::CHUNK_EDGE_VOXELS;
// The base's shade by height: the lowest ground at this share of its colour, the highest at all of it.
constexpr float LOWEST_SHADE = 0.7F;

std::size_t ColumnIndex(std::int32_t _x, std::int32_t _z, std::int32_t _width) noexcept
{
  return (static_cast<std::size_t>(_z) * static_cast<std::size_t>(_width)) + static_cast<std::size_t>(_x);
}

} // namespace

void Minimap::Reset(std::int32_t _widthCells, std::int32_t _depthCells, std::int32_t _heightChunks)
{
  m_widthCells = std::max(0, _widthCells);
  m_depthCells = std::max(0, _depthCells);
  m_heightChunks = std::max(0, _heightChunks);
  const std::size_t columns = static_cast<std::size_t>(m_widthCells) * static_cast<std::size_t>(m_depthCells);
  m_topIndex.assign(columns, std::uint8_t{0});
  m_topY.assign(columns, std::int16_t{0});
  m_dirty = true;
}

void Minimap::Take(std::span<const Protocol::ChunkSnapshot> _snapshots, const Session& _session)
{
  std::vector<std::pair<std::int32_t, std::int32_t>> stacks;
  for (const Protocol::ChunkSnapshot& snapshot : _snapshots)
  {
    stacks.emplace_back(snapshot.coordinate.xChunks, snapshot.coordinate.zChunks);
  }
  std::ranges::sort(stacks);
  const auto [first, last] = std::ranges::unique(stacks);
  stacks.erase(first, last);
  for (const auto& [x, z] : stacks)
  {
    TakeColumns(x, z, _session);
  }
}

void Minimap::TakeColumns(std::int32_t _xChunk, std::int32_t _zChunk, const Session& _session)
{
  for (std::int32_t z = 0; z < EDGE; ++z)
  {
    for (std::int32_t x = 0; x < EDGE; ++x)
    {
      const std::int32_t columnX = (_xChunk * EDGE) + x;
      const std::int32_t columnZ = (_zChunk * EDGE) + z;
      if (columnX < 0 || columnZ < 0 || columnX >= m_widthCells || columnZ >= m_depthCells)
      {
        continue;
      }
      // The column's top voxel, from the highest chunk down.
      std::uint8_t index = 0;
      std::int32_t topY = 0;
      for (std::int32_t yChunk = m_heightChunks - 1; yChunk >= 0 && index == 0; --yChunk)
      {
        const Protocol::ChunkSnapshot* chunk = _session.Chunk({.xChunks = _xChunk, .yChunks = yChunk, .zChunks = _zChunk});
        if (chunk == nullptr)
        {
          continue;
        }
        for (std::int32_t y = EDGE - 1; y >= 0; --y)
        {
          const std::uint8_t at = chunk->voxels->At(x, y, z);
          if (at != Protocol::Chunk::EMPTY)
          {
            index = at;
            topY = (yChunk * EDGE) + y;
            break;
          }
        }
      }
      const std::size_t column = ColumnIndex(columnX, columnZ, m_widthCells);
      if (m_topIndex.at(column) != index || m_topY.at(column) != topY)
      {
        m_topIndex.at(column) = index;
        m_topY.at(column) = static_cast<std::int16_t>(topY);
        m_dirty = true;
      }
    }
  }
}

bool Minimap::BaseDirty() const noexcept
{
  return m_dirty;
}

std::pair<std::int32_t, std::int32_t> Minimap::ColumnsOf(std::uint32_t _pixel, std::uint32_t _sidePixels, std::int32_t _cells) noexcept
{
  if (_sidePixels == 0 || _cells <= 0)
  {
    return {0, 0};
  }
  const auto cells = static_cast<std::int64_t>(_cells);
  const auto first = static_cast<std::int32_t>((static_cast<std::int64_t>(_pixel) * cells) / _sidePixels);
  const auto last = static_cast<std::int32_t>(((static_cast<std::int64_t>(_pixel) + 1) * cells) / _sidePixels);
  return {first, std::max(last, first + 1)};
}

std::vector<std::uint32_t> Minimap::BuildBase(std::uint32_t _sidePixels)
{
  m_dirty = false;
  std::vector<std::uint32_t> texels(static_cast<std::size_t>(_sidePixels) * _sidePixels, 0U);
  if (m_widthCells == 0 || m_depthCells == 0)
  {
    return texels;
  }
  const std::int32_t heightVoxels = std::max(1, m_heightChunks * EDGE);
  for (std::uint32_t row = 0; row < _sidePixels; ++row)
  {
    // North up: the first row is the map's far edge, its highest z.
    const auto [zFirst, zLast] = ColumnsOf(_sidePixels - 1 - row, _sidePixels, m_depthCells);
    for (std::uint32_t column = 0; column < _sidePixels; ++column)
    {
      const auto [xFirst, xLast] = ColumnsOf(column, _sidePixels, m_widthCells);
      std::array<float, 3> sum{};
      std::int32_t seen = 0;
      for (std::int32_t z = zFirst; z < std::min(zLast, m_depthCells); ++z)
      {
        for (std::int32_t x = xFirst; x < std::min(xLast, m_widthCells); ++x)
        {
          const std::size_t at = ColumnIndex(x, z, m_widthCells);
          const std::uint8_t index = m_topIndex.at(at);
          if (index == 0)
          {
            continue;
          }
          const std::uint32_t srgb = Content::PaletteColorSrgb(index);
          const float shade = LOWEST_SHADE + ((1.0F - LOWEST_SHADE) *
                                              std::clamp(static_cast<float>(m_topY.at(at)) / static_cast<float>(heightVoxels), 0.0F, 1.0F));
          sum = {sum[0] + (shade * static_cast<float>((srgb >> 16) & 0xFFU)), sum[1] + (shade * static_cast<float>((srgb >> 8) & 0xFFU)),
                 sum[2] + (shade * static_cast<float>(srgb & 0xFFU))};
          ++seen;
        }
      }
      if (seen == 0)
      {
        continue;
      }
      const auto channel = [seen](float _sum) { return static_cast<std::uint32_t>(std::lround(_sum / static_cast<float>(seen))) & 0xFFU; };
      texels.at((static_cast<std::size_t>(row) * _sidePixels) + column) =
        0xFF000000U | (channel(sum[2]) << 16) | (channel(sum[1]) << 8) | channel(sum[0]);
    }
  }
  return texels;
}

DirectX::XMFLOAT2 Minimap::ToMinimap(float _xMeters, float _zMeters, std::int32_t _widthCells, std::int32_t _depthCells,
                                     std::uint32_t _sidePixels) noexcept
{
  const auto side = static_cast<float>(_sidePixels);
  return {(_xMeters / static_cast<float>(std::max(1, _widthCells))) * side,
          side - ((_zMeters / static_cast<float>(std::max(1, _depthCells))) * side)};
}

DirectX::XMFLOAT2 Minimap::ToMap(float _xPixels, float _yPixels, std::int32_t _widthCells, std::int32_t _depthCells,
                                 std::uint32_t _sidePixels) noexcept
{
  const auto side = static_cast<float>(std::max(1U, _sidePixels));
  return {(_xPixels / side) * static_cast<float>(_widthCells), ((side - _yPixels) / side) * static_cast<float>(_depthCells)};
}

std::int32_t Minimap::GroundAt(float _xMeters, float _zMeters) const noexcept
{
  const auto x = static_cast<std::int32_t>(std::floor(_xMeters));
  const auto z = static_cast<std::int32_t>(std::floor(_zMeters));
  if (x < 0 || z < 0 || x >= m_widthCells || z >= m_depthCells)
  {
    return 0;
  }
  const std::size_t at = ColumnIndex(x, z, m_widthCells);
  return m_topIndex[at] == 0 ? 0 : m_topY[at];
}

std::vector<HudDot> Minimap::Dots(const Protocol::StateUpdate& _state, const Selection& _selection, std::uint32_t _sidePixels)
{
  const auto team = [&_state](std::uint8_t _owner)
  {
    const auto player = std::ranges::find(_state.players, _owner, &Protocol::PlayerView::player);
    const bool attacker = player != _state.players.end() ? player->side == Protocol::Side::Attacker : _owner == _state.player;
    return attacker ? std::uint8_t{0} : std::uint8_t{1};
  };
  std::vector<HudDot> dots;
  for (const Protocol::StructureView& structure : _state.structures)
  {
    const float x = static_cast<float>(structure.originVoxels.xVoxels) + (0.5F * static_cast<float>(structure.extentVoxels.xVoxels));
    const float z = static_cast<float>(structure.originVoxels.zVoxels) + (0.5F * static_cast<float>(structure.extentVoxels.zVoxels));
    dots.push_back({.positionPixels = ToMinimap(x, z, _state.widthCells, _state.depthCells, _sidePixels),
                    .team = team(structure.owner),
                    .structure = true,
                    .selected = _selection.structure == structure.id});
  }
  for (const Protocol::SquadView& squad : _state.squads)
  {
    DirectX::XMFLOAT3 sum{0.0F, 0.0F, 0.0F};
    std::size_t figures = 0;
    for (const Protocol::UnitView& unit : _state.units)
    {
      if (unit.squad == squad.id)
      {
        const DirectX::XMFLOAT3 feet = Meters(unit.position);
        sum = {sum.x + feet.x, sum.y + feet.y, sum.z + feet.z};
        ++figures;
      }
    }
    if (figures == 0)
    {
      continue; // only where the update puts it
    }
    const auto count = static_cast<float>(figures);
    dots.push_back({.positionPixels = ToMinimap(sum.x / count, sum.z / count, _state.widthCells, _state.depthCells, _sidePixels),
                    .team = team(squad.owner),
                    .structure = false,
                    .selected = std::ranges::find(_selection.squads, squad.id) != _selection.squads.end()});
  }
  return dots;
}

std::array<DirectX::XMFLOAT2, 4> Minimap::ViewQuad(std::span<const Engine::PickRay, 4> _corners, float _groundMeters,
                                                   std::int32_t _widthCells, std::int32_t _depthCells, std::uint32_t _sidePixels) noexcept
{
  std::array<DirectX::XMFLOAT2, 4> quad{};
  for (std::size_t corner = 0; corner < quad.size(); ++corner)
  {
    const Engine::PickRay& ray = _corners[corner];
    const float along = ray.direction.y == 0.0F ? 0.0F : (_groundMeters - ray.originMeters.y) / ray.direction.y;
    quad.at(corner) = ToMinimap(ray.originMeters.x + (along * ray.direction.x), ray.originMeters.z + (along * ray.direction.z), _widthCells,
                                _depthCells, _sidePixels);
  }
  return quad;
}

} // namespace Client
