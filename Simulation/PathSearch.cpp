#include "pch.h"

#include "PathSearch.h"

namespace Simulation
{

namespace
{

constexpr std::int64_t STRAIGHT_COST = 10;
constexpr std::int64_t DIAGONAL_COST = 14;
constexpr PathSearch::CellIndex NO_CELL = -1;

} // namespace

PathSearch::PathSearch(Protocol::EntityId _squad, NavigationClass _class, CellIndex _start, CellIndex _goal, std::int32_t _widthCells)
  : m_squad(_squad),
    m_class(_class),
    m_start(_start),
    m_goal(_goal),
    m_widthCells(_widthCells),
    m_nearest(_start)
{
  if (m_widthCells <= 0 || m_start < 0)
  {
    throw std::invalid_argument("a path search needs a grid and a start on it");
  }
  m_nodes.emplace(m_start, Node{.cost = 0, .parent = NO_CELL, .closed = false});
  m_open.insert({Heuristic(m_start), Heuristic(m_start), m_start});
}

std::int64_t PathSearch::Octile(CellIndex _from, CellIndex _to, std::int32_t _widthCells) noexcept
{
  const std::int64_t dx = std::abs(_from % _widthCells - _to % _widthCells);
  const std::int64_t dz = std::abs(_from / _widthCells - _to / _widthCells);
  return STRAIGHT_COST * std::max(dx, dz) + (DIAGONAL_COST - STRAIGHT_COST) * std::min(dx, dz);
}

std::int64_t PathSearch::Heuristic(CellIndex _cell) const noexcept
{
  return Octile(_cell, m_goal, m_widthCells);
}

std::uint32_t PathSearch::Advance(const NavigationGrid& _grid, std::uint32_t _budget)
{
  std::uint32_t used = 0;
  while (!m_done && used < _budget)
  {
    if (m_open.empty())
    {
      Finish(m_nearest);
      break;
    }
    const std::int64_t h = std::get<1>(*m_open.begin());
    const CellIndex cell = std::get<2>(*m_open.begin());
    m_open.erase(m_open.begin());
    Node& node = m_nodes.at(cell);
    node.closed = true;
    ++used;
    ++m_expanded;
    if (h < Heuristic(m_nearest) || (h == Heuristic(m_nearest) && cell < m_nearest))
    {
      m_nearest = cell;
    }
    if (cell == m_goal)
    {
      Finish(cell);
      break;
    }
    const auto x = static_cast<std::int32_t>(cell % m_widthCells);
    const auto z = static_cast<std::int32_t>(cell / m_widthCells);
    const std::uint8_t links = _grid.Links(x, z, m_class);
    const NavigationGrid::ClassRule& rule = _grid.Class(m_class);
    for (std::size_t direction = 0; direction < NavigationGrid::NEIGHBORS.size(); ++direction)
    {
      if ((links & (1U << direction)) == 0)
      {
        continue;
      }
      const auto [dx, dz] = NavigationGrid::NEIGHBORS.at(direction);
      const std::int32_t toX = x + dx;
      const std::int32_t toZ = z + dz;
      const CellIndex next = std::int64_t{toZ} * m_widthCells + toX;
      const NavigationGrid::Ground ground = _grid.GroundAt(toX, toZ);
      const std::int64_t factor = ground == NavigationGrid::Ground::Rubble ? rule.rubbleFactor
                                  : ground == NavigationGrid::Ground::Ford ? rule.fordFactor
                                                                           : 1;
      const std::int64_t cost = node.cost + (dx != 0 && dz != 0 ? DIAGONAL_COST : STRAIGHT_COST) * factor;
      const auto found = m_nodes.find(next);
      if (found == m_nodes.end())
      {
        m_nodes.emplace(next, Node{.cost = cost, .parent = cell, .closed = false});
        m_open.insert({cost + Heuristic(next), Heuristic(next), next});
      }
      else if (!found->second.closed && cost < found->second.cost)
      {
        m_open.erase({found->second.cost + Heuristic(next), Heuristic(next), next});
        found->second.cost = cost;
        found->second.parent = cell;
        m_open.insert({cost + Heuristic(next), Heuristic(next), next});
      }
    }
  }
  return used;
}

void PathSearch::Finish(CellIndex _end)
{
  m_done = true;
  m_short = _end != m_goal;
  m_cost = m_nodes.at(_end).cost;
  for (CellIndex cell = _end; cell != NO_CELL; cell = m_nodes.at(cell).parent)
  {
    m_path.push_back(cell);
  }
  std::ranges::reverse(m_path);
  // What the search kept to get here is no longer needed.
  m_nodes.clear();
  m_open.clear();
}

bool PathSearch::Done() const noexcept
{
  return m_done;
}

const std::vector<PathSearch::CellIndex>& PathSearch::Path() const noexcept
{
  return m_path;
}

bool PathSearch::ShortOfGoal() const noexcept
{
  return m_short;
}

std::int64_t PathSearch::Cost() const noexcept
{
  return m_cost;
}

std::uint64_t PathSearch::NodesExpanded() const noexcept
{
  return m_expanded;
}

Protocol::EntityId PathSearch::Squad() const noexcept
{
  return m_squad;
}

NavigationClass PathSearch::Class() const noexcept
{
  return m_class;
}

PathSearch::CellIndex PathSearch::Goal() const noexcept
{
  return m_goal;
}

void PathSearch::Write(Core::ByteWriter& _writer) const
{
  _writer.WriteU64(m_squad);
  _writer.WriteU8(static_cast<std::uint8_t>(m_class));
  _writer.WriteI64(m_start);
  _writer.WriteI64(m_goal);
  _writer.WriteI32(m_widthCells);
  _writer.WriteI64(m_nearest);
  _writer.WriteU64(m_expanded);
  _writer.WriteU8(m_done ? std::uint8_t{1} : std::uint8_t{0});
  _writer.WriteU8(m_short ? std::uint8_t{1} : std::uint8_t{0});
  _writer.WriteI64(m_cost);
  _writer.WriteU32(static_cast<std::uint32_t>(m_path.size()));
  for (const CellIndex cell : m_path)
  {
    _writer.WriteI64(cell);
  }
  // The nodes in cell order; the open set is the nodes not closed, which Read rebuilds.
  _writer.WriteU32(static_cast<std::uint32_t>(m_nodes.size()));
  for (const auto& [cell, node] : m_nodes)
  {
    _writer.WriteI64(cell);
    _writer.WriteI64(node.cost);
    _writer.WriteI64(node.parent);
    _writer.WriteU8(node.closed ? std::uint8_t{1} : std::uint8_t{0});
  }
}

PathSearch PathSearch::Read(Core::ByteReader& _reader)
{
  const Protocol::EntityId squad = _reader.ReadU64();
  const std::uint8_t navigationClass = _reader.ReadU8();
  if (navigationClass >= NAVIGATION_CLASS_COUNT)
  {
    throw std::invalid_argument("a path search of a navigation class this build does not know");
  }
  const CellIndex start = _reader.ReadI64();
  const CellIndex goal = _reader.ReadI64();
  const std::int32_t width = _reader.ReadI32();
  PathSearch search(squad, static_cast<NavigationClass>(navigationClass), start, goal, width);
  search.m_nodes.clear();
  search.m_open.clear();
  search.m_nearest = _reader.ReadI64();
  search.m_expanded = _reader.ReadU64();
  search.m_done = _reader.ReadU8() != 0;
  search.m_short = _reader.ReadU8() != 0;
  search.m_cost = _reader.ReadI64();
  const std::uint32_t path = _reader.ReadU32();
  for (std::uint32_t step = 0; step < path; ++step)
  {
    search.m_path.push_back(_reader.ReadI64());
  }
  const std::uint32_t nodes = _reader.ReadU32();
  for (std::uint32_t number = 0; number < nodes; ++number)
  {
    const CellIndex cell = _reader.ReadI64();
    const std::int64_t cost = _reader.ReadI64();
    const CellIndex parent = _reader.ReadI64();
    const bool closed = _reader.ReadU8() != 0;
    search.m_nodes.emplace(cell, Node{.cost = cost, .parent = parent, .closed = closed});
    if (!closed)
    {
      search.m_open.insert({cost + search.Heuristic(cell), search.Heuristic(cell), cell});
    }
  }
  return search;
}

} // namespace Simulation
