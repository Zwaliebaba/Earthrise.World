#pragma once

#include "ByteReader.h"
#include "ByteWriter.h"
#include "EntityId.h"
#include "NavigationGrid.h"

#include <cstdint>
#include <map>
#include <set>
#include <tuple>
#include <vector>

namespace Simulation
{

/// A squad's search for a path to its goal (M2-F3; ADR-015, decision 5): A* over the navigation
/// grid's cells for the squad's class. A step costs 10 straight and 14 diagonally, times its ground's
/// factor for the class where it lands; the heuristic is the octile distance in the same integers,
/// which never overestimates, since no factor is below 1. Ties go by f, then h, then cell index, so
/// equal paths always come out the same.
///
/// It is budgeted in nodes expanded (ADR-013, decision 6): Advance expands at most the nodes it is
/// given and leaves the rest for the next tick, and a save holds it part way through. It searches
/// for its target, which the world sets to the goal, or, for a goal the class cannot reach, to the
/// reachable cell nearest it, which NavigationGrid::NearestReachable finds without searching the map
/// (M2-F4). A path that ends anywhere but the goal falls short of it. Should the target prove out of
/// reach after all, the path ends at the closed cell nearest it by octile distance, ties to the lower
/// index.
class PathSearch
{
public:
  /// A cell's index: z times the grid's width, plus x.
  using CellIndex = std::int64_t;

  /// Searches from _start for _target, on behalf of a squad whose goal is _goal.
  PathSearch(Protocol::EntityId _squad, NavigationClass _class, CellIndex _start, CellIndex _goal, CellIndex _target,
             std::int32_t _widthCells);
  ~PathSearch() = default;
  PathSearch(const PathSearch&) = default;
  PathSearch& operator=(const PathSearch&) = default;
  // A move may allocate under the debug library's checked containers, whose std::map allocates in
  // its move constructor, so it is not noexcept (bugprone-exception-escape).
  PathSearch(PathSearch&&) noexcept(false) = default;
  PathSearch& operator=(PathSearch&&) noexcept(false) = default;

  /// Expands up to _budget nodes on _grid. Returns how many it expanded.
  std::uint32_t Advance(const NavigationGrid& _grid, std::uint32_t _budget);

  [[nodiscard]] bool Done() const noexcept;
  /// The cells from the start to the target, or to the closed cell nearest it, both included.
  [[nodiscard]] const std::vector<CellIndex>& Path() const noexcept;
  [[nodiscard]] bool ShortOfGoal() const noexcept;
  /// Its cost: the sum of its steps' costs.
  [[nodiscard]] std::int64_t Cost() const noexcept;
  /// How many nodes it has expanded, over every Advance.
  [[nodiscard]] std::uint64_t NodesExpanded() const noexcept;

  [[nodiscard]] Protocol::EntityId Squad() const noexcept;
  [[nodiscard]] NavigationClass Class() const noexcept;
  [[nodiscard]] CellIndex Goal() const noexcept;
  [[nodiscard]] CellIndex Target() const noexcept;

  /// The octile distance between two cells, in the search's integers.
  [[nodiscard]] static std::int64_t Octile(CellIndex _from, CellIndex _to, std::int32_t _widthCells) noexcept;

  /// Writes everything the search needs to go on; Read gives it back.
  void Write(Core::ByteWriter& _writer) const;
  [[nodiscard]] static PathSearch Read(Core::ByteReader& _reader);

private:
  struct Node
  {
    std::int64_t cost; // from the start
    CellIndex parent;  // -1 for the start
    bool closed;
  };

  void Finish(CellIndex _end);
  [[nodiscard]] std::int64_t Heuristic(CellIndex _cell) const noexcept;

  Protocol::EntityId m_squad;
  NavigationClass m_class;
  CellIndex m_start;
  CellIndex m_goal;
  CellIndex m_target;
  std::int32_t m_widthCells;
  std::map<CellIndex, Node> m_nodes;
  std::set<std::tuple<std::int64_t, std::int64_t, CellIndex>> m_open; // f, h, cell
  CellIndex m_nearest;                                                // the closed cell nearest the target
  std::uint64_t m_expanded = 0;
  bool m_done = false;
  bool m_short = false;
  std::vector<CellIndex> m_path;
  std::int64_t m_cost = 0;
};

} // namespace Simulation
