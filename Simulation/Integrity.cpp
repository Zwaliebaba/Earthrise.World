#include "pch.h"

#include "Integrity.h"

#include "Generator.h"
#include "SupportSearch.h"

namespace Simulation
{

namespace
{

/// A cell of the section's face: its column along the section and its row up from its origin.
using Cell = std::pair<std::int32_t, std::int32_t>;

/// The section's own frame, turned as the structure is (World::AddStructure).
struct Frame
{
  Protocol::VoxelPosition origin;
  std::uint32_t turns;
  std::int32_t length;    // the template's width: columns
  std::int32_t thickness; // its depth: layers
  std::int32_t height;    // rows

  [[nodiscard]] Protocol::VoxelPosition At(std::int32_t _column, std::int32_t _layer, std::int32_t _row) const noexcept
  {
    std::int32_t x = _column;
    std::int32_t z = _layer;
    switch (turns)
    {
    case 1:
      x = _layer;
      z = length - 1 - _column;
      break;
    case 2:
      x = length - 1 - _column;
      z = thickness - 1 - _layer;
      break;
    case 3:
      x = thickness - 1 - _layer;
      z = _column;
      break;
    default:
      break;
    }
    return {.xVoxels = origin.xVoxels + x, .yVoxels = origin.yVoxels + _row, .zVoxels = origin.zVoxels + z};
  }
};

/// An opening: its cells, lowest then leftmost first, and what the readout reads off it.
struct Opening
{
  std::vector<Cell> cells;
  std::int32_t floorRow = 0;
  std::vector<std::int32_t> floorColumns;
  std::int32_t clearRows = 0;
  std::int32_t width = 0;
  std::int32_t heightRows = 0;
  std::int32_t lowColumn = 0;
  std::int32_t highColumn = 0;
};

std::int32_t LongestRun(std::vector<std::int32_t> _columns)
{
  std::ranges::sort(_columns);
  std::int32_t best = 0;
  std::int32_t run = 0;
  std::optional<std::int32_t> last;
  for (const std::int32_t column : _columns)
  {
    run = last.has_value() && column == *last + 1 ? run + 1 : (last.has_value() && column == *last ? run : 1);
    best = std::max(best, run);
    last = column;
  }
  return best;
}

/// 4-connected regions of _cells, each sorted by row then column, ordered by their lowest, then
/// leftmost, cell, as the paper test orders them.
std::vector<std::vector<Cell>> Regions(const std::set<Cell>& _cells)
{
  const auto byRowThenColumn = [](const Cell& _a, const Cell& _b)
  { return std::pair{_a.second, _a.first} < std::pair{_b.second, _b.first}; };
  std::set<Cell> remaining = _cells;
  std::vector<std::vector<Cell>> regions;
  while (!remaining.empty())
  {
    const Cell start = *std::ranges::min_element(remaining, byRowThenColumn);
    std::vector<Cell> region{start};
    std::vector<Cell> frontier{start};
    remaining.erase(start);
    while (!frontier.empty())
    {
      const Cell cell = frontier.back();
      frontier.pop_back();
      for (const Cell next : {Cell{cell.first + 1, cell.second}, Cell{cell.first - 1, cell.second}, Cell{cell.first, cell.second + 1},
                              Cell{cell.first, cell.second - 1}})
      {
        if (remaining.erase(next) > 0)
        {
          region.push_back(next);
          frontier.push_back(next);
        }
      }
    }
    std::ranges::sort(region, byRowThenColumn);
    regions.push_back(std::move(region));
  }
  return regions;
}

class Reader
{
public:
  Reader(const World& _world, const Structure& _structure, const ServerTemplate& _template)
    : m_world(_world),
      m_frame{.origin = _structure.originVoxels,
              .turns = _structure.quarterTurns % 4,
              .length = static_cast<std::int32_t>(_template.widthVoxels),
              .thickness = static_cast<std::int32_t>(_template.depthVoxels),
              .height = static_cast<std::int32_t>(_template.heightVoxels)},
      m_materials(_world.Data().Materials())
  {
  }

  [[nodiscard]] std::uint8_t At(std::int32_t _column, std::int32_t _layer, std::int32_t _row) const
  {
    return m_world.Voxels().At(m_frame.At(_column, _layer, _row));
  }

  /// How many layers of a face cell are filled, by anything: stone, rubble or ground.
  [[nodiscard]] std::int32_t Thickness(std::int32_t _column, std::int32_t _row) const
  {
    std::int32_t filled = 0;
    for (std::int32_t layer = 0; layer < m_frame.thickness; ++layer)
    {
      filled += At(_column, layer, _row) != Protocol::Chunk::EMPTY ? 1 : 0;
    }
    return filled;
  }

  [[nodiscard]] std::vector<Opening> Openings() const
  {
    std::set<Cell> cells;
    for (std::int32_t column = 0; column < m_frame.length; ++column)
    {
      for (std::int32_t row = 0; row < m_frame.height - 1; ++row)
      {
        if (Thickness(column, row) == 0)
        {
          cells.insert({column, row});
        }
      }
    }
    std::vector<Opening> openings;
    for (std::vector<Cell>& region : Regions(cells))
    {
      Opening opening;
      opening.floorRow = region.front().second;
      std::int32_t topRow = opening.floorRow;
      std::map<std::int32_t, std::vector<std::int32_t>> rows;
      opening.lowColumn = region.front().first;
      opening.highColumn = region.front().first;
      for (const auto& [column, row] : region)
      {
        rows[row].push_back(column);
        topRow = std::max(topRow, row);
        opening.lowColumn = std::min(opening.lowColumn, column);
        opening.highColumn = std::max(opening.highColumn, column);
        if (row == opening.floorRow)
        {
          opening.floorColumns.push_back(column);
        }
      }
      for (const std::int32_t column : opening.floorColumns)
      {
        std::int32_t clear = 0;
        while (opening.floorRow + clear < m_frame.height && Thickness(column, opening.floorRow + clear) == 0)
        {
          ++clear;
        }
        opening.clearRows = std::max(opening.clearRows, clear);
      }
      for (const auto& [row, columns] : rows)
      {
        opening.width = std::max(opening.width, LongestRun(columns));
      }
      opening.heightRows = topRow - opening.floorRow + 1;
      opening.cells = std::move(region);
      openings.push_back(std::move(opening));
    }
    return openings;
  }

  /// What lies under an opening's floor: stone of a structure, rubble, or ground.
  [[nodiscard]] Protocol::OpeningFloor FloorOf(const Opening& _opening) const
  {
    bool rubble = false;
    for (const std::int32_t column : _opening.floorColumns)
    {
      for (std::int32_t layer = 0; layer < m_frame.thickness; ++layer)
      {
        const std::uint8_t index = At(column, layer, _opening.floorRow - 1);
        const MaterialRule& rule = m_materials.at(index);
        if (index != Protocol::Chunk::EMPTY && !rule.walkableGround && !rule.liquid)
        {
          return Protocol::OpeningFloor::Stone;
        }
        rubble = rubble || rule.behavior == Behavior::StoneRubble || rule.behavior == Behavior::WasteRubble;
      }
    }
    return rubble ? Protocol::OpeningFloor::Rubble : Protocol::OpeningFloor::Ground;
  }

  /// Whether foot can stand on an opening's floor now, through every layer of one of its columns.
  [[nodiscard]] bool RouteNow(const Opening& _opening) const
  {
    const NavigationGrid& navigation = m_world.Navigation();
    for (const std::int32_t column : _opening.floorColumns)
    {
      bool through = true;
      for (std::int32_t layer = 0; layer < m_frame.thickness && through; ++layer)
      {
        const Protocol::VoxelPosition cell = m_frame.At(column, layer, _opening.floorRow);
        through = cell.xVoxels >= 0 && cell.zVoxels >= 0 && cell.xVoxels < navigation.WidthCells() &&
                  cell.zVoxels < navigation.DepthCells() && navigation.WalkableHeight(cell.xVoxels, cell.zVoxels) == cell.yVoxels &&
                  navigation.Usable(cell.xVoxels, cell.zVoxels, NavigationClass::Foot);
      }
      if (through)
      {
        return true;
      }
    }
    return false;
  }

  /// The columns with nothing under the lowest course through the whole thickness.
  [[nodiscard]] std::vector<std::int32_t> GapColumns() const
  {
    std::vector<std::int32_t> columns;
    for (std::int32_t column = 0; column < m_frame.length; ++column)
    {
      bool gone = true;
      for (std::int32_t layer = 0; layer < m_frame.thickness; ++layer)
      {
        gone = gone && At(column, layer, -1) == Protocol::Chunk::EMPTY;
      }
      if (gone)
      {
        columns.push_back(column);
      }
    }
    return columns;
  }

  /// What the section stands on: earth if any column of its lowest course stands on earth or loose
  /// material, else rock, else bedrock, from the first solid voxel under each.
  [[nodiscard]] Protocol::GroundKind Ground() const
  {
    bool earth = false;
    bool rock = false;
    for (std::int32_t column = 0; column < m_frame.length; ++column)
    {
      for (std::int32_t layer = 0; layer < m_frame.thickness; ++layer)
      {
        for (std::int32_t row = -1; row >= -ANCHOR_DEPTH_VOXELS && m_frame.At(column, layer, row).yVoxels >= 0; --row)
        {
          const std::uint8_t index = At(column, layer, row);
          if (index == Protocol::Chunk::EMPTY)
          {
            continue;
          }
          const Behavior behavior = m_materials.at(index).behavior;
          earth = earth || behavior == Behavior::Earth || behavior == Behavior::StoneRubble || behavior == Behavior::WasteRubble;
          rock = rock || behavior == Behavior::CommonRock || behavior == Behavior::BuildingStone;
          break;
        }
      }
    }
    return earth ? Protocol::GroundKind::Earth : (rock ? Protocol::GroundKind::Rock : Protocol::GroundKind::Bedrock);
  }

  /// The first width, from _base columns, at which removing the columns of _order in turn, at rows
  /// _lowRow to _highRow through the whole thickness, leaves stone of the section unsupported: _base
  /// itself when it already does, and 0 when none within the section does. A binary search over a
  /// scratch copy of the chunks, since a wider removal never supports more (ADR-004, decision 2).
  [[nodiscard]] std::int32_t FallWidth(const std::vector<Cell>& _cells, std::int32_t _base, const std::vector<std::int32_t>& _order,
                                       std::int32_t _lowRow, std::int32_t _highRow) const
  {
    if (_base == 0)
    {
      return 0;
    }
    ChunkStore scratch = m_world.Voxels().Store();
    std::vector<Protocol::VoxelPosition> around;
    for (const auto& [column, row] : _cells)
    {
      for (std::int32_t layer = 0; layer < m_frame.thickness; ++layer)
      {
        around.push_back(m_frame.At(column, layer, row));
      }
    }
    const auto falls = [&](std::size_t _extra)
    {
      std::vector<std::pair<Protocol::VoxelPosition, std::uint8_t>> removed;
      std::vector<Protocol::VoxelPosition> edits = around;
      for (std::size_t index = 0; index < _extra; ++index)
      {
        for (std::int32_t row = _lowRow; row <= _highRow; ++row)
        {
          for (std::int32_t layer = 0; layer < m_frame.thickness; ++layer)
          {
            const Protocol::VoxelPosition voxel = m_frame.At(_order[index], layer, row);
            removed.emplace_back(voxel, scratch.At(voxel));
            scratch.Set(voxel, Protocol::Chunk::EMPTY);
            edits.push_back(voxel);
          }
        }
      }
      const bool found = Unsupported(scratch, edits);
      for (auto entry = removed.rbegin(); entry != removed.rend(); ++entry)
      {
        scratch.Set(entry->first, entry->second);
      }
      return found;
    };
    if (falls(0))
    {
      return _base;
    }
    std::size_t low = 1;
    std::size_t high = _order.size();
    if (high == 0 || !falls(high))
    {
      return 0;
    }
    while (low < high)
    {
      const std::size_t middle = low + (high - low) / 2;
      if (falls(middle))
      {
        high = middle;
      }
      else
      {
        low = middle + 1;
      }
    }
    return _base + static_cast<std::int32_t>(low);
  }

  /// The columns beyond _low to _high, alternately on each side, the higher first, within the section.
  [[nodiscard]] std::vector<std::int32_t> WideningOrder(std::int32_t _low, std::int32_t _high) const
  {
    std::vector<std::int32_t> order;
    for (std::int32_t step = 1; _high + step < m_frame.length || _low - step >= 0; ++step)
    {
      if (_high + step < m_frame.length)
      {
        order.push_back(_high + step);
      }
      if (_low - step >= 0)
      {
        order.push_back(_low - step);
      }
    }
    return order;
  }

  /// The world box of _cells through the whole thickness.
  [[nodiscard]] std::pair<Protocol::VoxelPosition, Protocol::VoxelPosition> Box(const std::vector<Cell>& _cells) const
  {
    Protocol::VoxelPosition low{.xVoxels = INT32_MAX, .yVoxels = INT32_MAX, .zVoxels = INT32_MAX};
    Protocol::VoxelPosition high{.xVoxels = INT32_MIN, .yVoxels = INT32_MIN, .zVoxels = INT32_MIN};
    for (const auto& [column, row] : _cells)
    {
      for (std::int32_t layer = 0; layer < m_frame.thickness; ++layer)
      {
        const Protocol::VoxelPosition voxel = m_frame.At(column, layer, row);
        low = {std::min(low.xVoxels, voxel.xVoxels), std::min(low.yVoxels, voxel.yVoxels), std::min(low.zVoxels, voxel.zVoxels)};
        high = {std::max(high.xVoxels, voxel.xVoxels), std::max(high.yVoxels, voxel.yVoxels), std::max(high.zVoxels, voxel.zVoxels)};
      }
    }
    return {low, high};
  }

  [[nodiscard]] std::uint32_t Searches() const noexcept
  {
    return m_searches;
  }

private:
  /// Whether the support search, around _edits, finds stone of the section unsupported in _store.
  [[nodiscard]] bool Unsupported(const ChunkStore& _store, const std::vector<Protocol::VoxelPosition>& _edits) const
  {
    ++m_searches;
    const World::Desc& desc = m_world.Description();
    const SupportRules rules{.materials = &m_materials,
                             .anchorDepthVoxels = ANCHOR_DEPTH_VOXELS,
                             .surfaceVoxels = desc.surfaceVoxels,
                             .heightVoxels = desc.heightChunks * Protocol::CHUNK_EDGE_VOXELS};
    const auto [low, high] = Box(SectionCells());
    for (const Piece& piece : FindUnsupported(_store, rules, _edits).pieces)
    {
      for (const Protocol::VoxelPosition& voxel : piece)
      {
        if (voxel.xVoxels >= low.xVoxels && voxel.xVoxels <= high.xVoxels && voxel.zVoxels >= low.zVoxels &&
            voxel.zVoxels <= high.zVoxels && voxel.yVoxels >= low.yVoxels && voxel.yVoxels <= high.yVoxels)
        {
          return true;
        }
      }
    }
    return false;
  }

  [[nodiscard]] std::vector<Cell> SectionCells() const
  {
    return {{0, 0}, {m_frame.length - 1, m_frame.height - 1}};
  }

  const World& m_world;
  Frame m_frame;
  const MaterialRules& m_materials;
  mutable std::uint32_t m_searches = 0; // support searches run, for the bench (M3-B5)
};

} // namespace

Protocol::IntegrityReading ReadIntegrity(const World& _world, Protocol::EntityId _structure, std::uint32_t* _supportSearches)
{
  const Structure& structure = _world.Structures().at(_structure);
  const ServerTemplate* found = _world.Data().FindTemplate(structure.templateId);
  if (found == nullptr)
  {
    throw std::out_of_range("the structure's template is not in the server data");
  }
  const Reader reader(_world, structure, *found);
  constexpr std::int32_t FOOT_HEADROOM_ROWS = 2; // what foot soldiers need (GameDesign.md, M2's values, item 10)

  Protocol::IntegrityReading reading{};
  reading.structure = _structure;
  reading.ground = reader.Ground();
  reading.canBeUndermined = reading.ground == Protocol::GroundKind::Earth;
  reading.falling =
    std::ranges::any_of(_world.Collapses(), [_structure](const auto& _entry) { return _entry.second.structure == _structure; });

  const std::vector<Opening> openings = reader.Openings();
  const Opening* hole = nullptr;
  const Opening* lowest = nullptr;
  for (const Opening& opening : openings)
  {
    if (hole == nullptr || opening.width > hole->width || (opening.width == hole->width && opening.floorRow < hole->floorRow))
    {
      hole = &opening;
    }
    if (lowest == nullptr || opening.floorRow < lowest->floorRow || (opening.floorRow == lowest->floorRow && opening.width > lowest->width))
    {
      lowest = &opening;
    }
  }
  bool holeFalls = false;
  if (hole != nullptr)
  {
    reading.holeWidthVoxels = static_cast<std::uint8_t>(hole->width);
    reading.holeHeightVoxels = static_cast<std::uint8_t>(hole->heightRows);
    reading.holeFallWidthVoxels =
      static_cast<std::uint8_t>(reader.FallWidth(hole->cells, hole->width, reader.WideningOrder(hole->lowColumn, hole->highColumn),
                                                 hole->floorRow, hole->floorRow + hole->heightRows - 1));
    holeFalls = reading.holeFallWidthVoxels != 0 && reading.holeWidthVoxels >= reading.holeFallWidthVoxels;
  }
  bool onGround = false;
  if (lowest != nullptr)
  {
    reading.hasOpening = true;
    reading.openingFloorVoxels = static_cast<std::uint8_t>(lowest->floorRow);
    reading.openingFloor = reader.FloorOf(*lowest);
    reading.openingClearVoxels = static_cast<std::uint8_t>(lowest->clearRows);
    onGround = reading.openingFloor != Protocol::OpeningFloor::Stone;
    reading.routeNow = onGround && reader.RouteNow(*lowest);
  }
  else
  {
    reading.openingFloor = Protocol::OpeningFloor::Stone;
  }

  const std::vector<std::int32_t> gapColumns = reader.GapColumns();
  reading.gapRunVoxels = static_cast<std::uint8_t>(LongestRun(gapColumns));
  std::vector<Cell> gapCells;
  bool gapFalls = false;
  if (reading.gapRunVoxels > 0)
  {
    // The longest run, its first if several are as long.
    std::int32_t runStart = gapColumns.front();
    std::int32_t bestStart = runStart;
    std::int32_t bestLength = 0;
    for (std::size_t index = 0; index < gapColumns.size(); ++index)
    {
      if (index > 0 && gapColumns[index] != gapColumns[index - 1] + 1)
      {
        runStart = gapColumns[index];
      }
      if (gapColumns[index] - runStart + 1 > bestLength)
      {
        bestLength = gapColumns[index] - runStart + 1;
        bestStart = runStart;
      }
    }
    for (std::int32_t column = bestStart; column < bestStart + bestLength; ++column)
    {
      gapCells.emplace_back(column, -1);
    }
    reading.gapFallRunVoxels = static_cast<std::uint8_t>(
      reader.FallWidth(gapCells, bestLength, reader.WideningOrder(bestStart, bestStart + bestLength - 1), -1, -1));
    gapFalls = reading.gapFallRunVoxels != 0 && reading.gapRunVoxels >= reading.gapFallRunVoxels;
  }

  // The weak point: the first reading past its mark, in this order, else the one nearest it.
  // Nearness is a fraction, a reading over its mark, held as two integers: the simulation holds no
  // floating point (ADR-013), and fractions compare by cross-multiplying.
  struct Nearness
  {
    std::int64_t over;
    std::int64_t under; // positive
  };
  const auto nearer = [](const Nearness& _a, const Nearness& _b) { return _a.over * _b.under > _b.over * _a.under; };
  struct Candidate
  {
    bool past;
    Nearness nearness;
    std::vector<Cell> cells;
  };
  const auto ratio = [](std::int32_t _width, std::int32_t _mark) { return _mark > 0 ? Nearness{_width, _mark} : Nearness{0, 1}; };
  std::array<Candidate, 3> candidates = {{
    {.past = reading.routeNow,
     .nearness = reading.routeNow ? Nearness{1, 1} : Nearness{0, 1},
     .cells = lowest != nullptr ? lowest->cells : std::vector<Cell>{}},
    {.past = holeFalls,
     .nearness = hole != nullptr ? ratio(hole->width, reading.holeFallWidthVoxels) : Nearness{0, 1},
     .cells = hole != nullptr ? hole->cells : std::vector<Cell>{}},
    {.past = gapFalls, .nearness = ratio(reading.gapRunVoxels, reading.gapFallRunVoxels), .cells = gapCells},
  }};
  if (lowest != nullptr && onGround)
  {
    const Nearness clear{lowest->clearRows, FOOT_HEADROOM_ROWS};
    if (nearer(clear, candidates[0].nearness))
    {
      candidates[0].nearness = clear;
    }
  }
  const Candidate* weak = nullptr;
  for (const Candidate& candidate : candidates)
  {
    if (candidate.past && weak == nullptr)
    {
      weak = &candidate;
    }
  }
  if (weak == nullptr)
  {
    for (const Candidate& candidate : candidates)
    {
      if (candidate.nearness.over > 0 && (weak == nullptr || nearer(candidate.nearness, weak->nearness)))
      {
        weak = &candidate;
      }
    }
  }
  if (weak != nullptr && !weak->cells.empty())
  {
    std::tie(reading.weakMinVoxels, reading.weakMaxVoxels) = reader.Box(weak->cells);
  }
  else
  {
    reading.weakMinVoxels = {.xVoxels = 1, .yVoxels = 1, .zVoxels = 1};
    reading.weakMaxVoxels = {.xVoxels = 0, .yVoxels = 0, .zVoxels = 0};
  }
  if (_supportSearches != nullptr)
  {
    *_supportSearches = reader.Searches();
  }
  return reading;
}

} // namespace Simulation
