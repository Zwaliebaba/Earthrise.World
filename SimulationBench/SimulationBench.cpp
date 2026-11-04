#include "pch.h"

#include "BreachLab.h"
#include "BreachRun.h"
#include "ChunkStore.h"
#include "Generator.h"
#include "Hash.h"
#include "Integrity.h"
#include "NavigationGrid.h"
#include "PathSearch.h"
#include "Replay.h"
#include "Save.h"
#include "Scenario.h"
#include "FirePattern.h"
#include "ServerData.h"
#include "SiegeAttacker.h"
#include "SupportSearch.h"
#include "TestWorld.h"

// Headless measurements for the renderer spike's question 6 (GameDesign.md) and ADR-006's table
// (MvpPlan.md, M1 tasks 11 and 12): memory for a world, one support search on the test town after
// a breach, and one navigation tile rebuild, on the MVP map and on the synthetic 2 km world that
// carries a season's edits, at 1 m and at 0.5 m.
//
// The 0.5 m world is the 1 m world with each voxel split into eight (GameDesign.md, M1's open
// values, item 8). Its memory is counted from which 16-voxel octants of each 1 m chunk hold any
// voxel, which is exactly the chunks the split world would hold; its search and its tiles are
// timed on a split copy of the region they read. Wall-clock time is fine here: the bench measures
// the simulation, it is not part of it.
//
// The materials and the templates are the server bake's, read from ServerData\ beside the bench's
// own executable (ADR-016), so the bench reads no client content (ADR-010, amended).

namespace
{

constexpr std::uint64_t SEED = 0x5eed2026ULL;
constexpr std::int32_t MVP_MAP_VOXELS = 512;       // GameDesign.md: one map, 512 m x 512 m
constexpr std::int32_t SEASON_WORLD_VOXELS = 2048; // about 2 km x 2 km
constexpr std::int32_t SEASON_HOLDINGS = 32;       // the most seats a world has
constexpr std::int32_t ANCHOR_DEPTH_METERS = 8;    // GameDesign.md, M1's open values, item 2
constexpr std::int32_t CLIMB_METERS = 1;           // foot soldiers, 1 m in each 1 m step
constexpr int REPEATS = 20;
constexpr double BYTES_PER_MEGABYTE = 1024.0 * 1024.0;
constexpr std::int32_t HALF_METER_CHUNK_SPAN = Protocol::CHUNK_EDGE_VOXELS / 2; // 1 m voxels a 0.5 m chunk covers

using Clock = std::chrono::steady_clock;

/// The build id (ADR-014, decision 6): Core::Fnv1a64 over the executable's own file, which saves and
/// replays record and refuse to run against any other build.
std::uint64_t BuildIdOf(const std::filesystem::path& _executable)
{
  std::ifstream stream(_executable, std::ios::binary);
  if (!stream)
  {
    throw std::runtime_error("the bench could not read its own executable to compute its build id");
  }
  const std::vector<char> raw{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
  return Core::Fnv1a64(std::as_bytes(std::span(raw)));
}

double MillisecondsSince(Clock::time_point _start)
{
  return std::chrono::duration<double, std::milli>(Clock::now() - _start).count();
}

/// The median and the slowest of _repeats runs of _run, in milliseconds.
struct Timing
{
  double medianMilliseconds;
  double slowestMilliseconds;
};

Timing Time(int _repeats, const std::function<void()>& _run)
{
  std::vector<double> times;
  for (int repeat = 0; repeat < _repeats; ++repeat)
  {
    const Clock::time_point start = Clock::now();
    _run();
    times.push_back(MillisecondsSince(start));
  }
  std::ranges::sort(times);
  return {.medianMilliseconds = times[times.size() / 2], .slowestMilliseconds = times.back()};
}

/// The chunks a store holds, and the chunks the same world would hold at 0.5 m: each 1 m chunk's
/// octants of 16 voxels that hold any voxel.
struct ChunkCounts
{
  std::size_t oneMeter;
  std::size_t halfMeter;
};

ChunkCounts CountChunks(const Simulation::ChunkStore& _store)
{
  ChunkCounts counts{.oneMeter = 0, .halfMeter = 0};
  for (const Protocol::ChunkSnapshot& snapshot : _store.Snapshots())
  {
    ++counts.oneMeter;
    std::array<bool, 8> occupied{};
    const Protocol::Chunk& chunk = *snapshot.voxels;
    for (std::int32_t y = 0; y < Protocol::CHUNK_EDGE_VOXELS; ++y)
    {
      for (std::int32_t z = 0; z < Protocol::CHUNK_EDGE_VOXELS; ++z)
      {
        for (std::int32_t x = 0; x < Protocol::CHUNK_EDGE_VOXELS; ++x)
        {
          if (chunk.At(x, y, z) != Protocol::Chunk::EMPTY)
          {
            const auto octant = static_cast<std::size_t>((x / HALF_METER_CHUNK_SPAN) | ((y / HALF_METER_CHUNK_SPAN) << 1) |
                                                         ((z / HALF_METER_CHUNK_SPAN) << 2));
            occupied.at(octant) = true;
          }
        }
      }
    }
    counts.halfMeter += static_cast<std::size_t>(std::ranges::count(occupied, true));
  }
  return counts;
}

double Megabytes(std::size_t _chunks)
{
  return static_cast<double>(_chunks) * static_cast<double>(sizeof(Protocol::Chunk)) / BYTES_PER_MEGABYTE;
}

/// One world's measurements, at 1 m and at 0.5 m.
void Measure(std::string_view _name, Simulation::ChunkStore& _store, const Simulation::TestWorld& _world, double _buildMilliseconds,
             const Simulation::ServerData& _data)
{
  const Clock::time_point publishStart = Clock::now();
  static_cast<void>(_store.Publish());
  const double publishMilliseconds = MillisecondsSince(publishStart);
  const ChunkCounts chunks = CountChunks(_store);
  std::println("{}: {} m x {} m, built in {:.0f} ms and published in {:.0f} ms, {} voxels changed from the generated ground", _name,
               _world.edgeVoxels, _world.edgeVoxels, _buildMilliseconds, publishMilliseconds, _world.editedVoxels);
  std::println("  memory: {:.1f} MB in {} chunks at 1 m; {:.1f} MB in {} chunks at 0.5 m", Megabytes(chunks.oneMeter), chunks.oneMeter,
               Megabytes(chunks.halfMeter), chunks.halfMeter);

  // The breach: the curtain section just north of the test town's breach collapses into rubble.
  const Simulation::MaterialRules& oneMeterRules = _data.Materials();
  Simulation::TestWorldEdits edits(_store, _world, SEED);
  const std::vector<Protocol::VoxelPosition> collapse = edits.Collapse(_store);
  const Simulation::SupportRules support{.materials = &oneMeterRules,
                                         .anchorDepthVoxels = ANCHOR_DEPTH_METERS,
                                         .surfaceVoxels = [&_world](std::int32_t _x, std::int32_t _z) { return _world.SurfaceAt(_x, _z); },
                                         .heightVoxels = Simulation::TestWorld::HEIGHT_VOXELS};
  Simulation::SupportSearchResult found;
  const Timing search = Time(REPEATS, [&] { found = Simulation::FindUnsupported(_store, support, collapse); });
  std::println("  support search after the breach, 1 m: {:.3f} ms median, {:.3f} ms slowest; {} voxels changed, {} read, {} pieces fell",
               search.medianMilliseconds, search.slowestMilliseconds, collapse.size(), found.voxelsSearched, found.pieces.size());

  Simulation::NavigationGrid grid({.widthCells = _world.edgeVoxels,
                                   .depthCells = _world.edgeVoxels,
                                   .heightVoxels = Simulation::TestWorld::HEIGHT_VOXELS,
                                   .climbVoxels = CLIMB_METERS},
                                  oneMeterRules);
  const Clock::time_point allStart = Clock::now();
  grid.RebuildAll(_store);
  const double allMilliseconds = MillisecondsSince(allStart);
  const std::vector<std::pair<std::int32_t, std::int32_t>> tiles = grid.TilesTouchedBy(collapse);
  const auto [tileX, tileZ] = tiles.front();
  const Timing tile = Time(REPEATS, [&] { grid.RebuildTile(_store, tileX, tileZ); });
  std::println(
    "  navigation, 1 m: every tile in {:.0f} ms; one 32 m tile in {:.3f} ms median, {:.3f} ms slowest; the breach touches {} tiles",
    allMilliseconds, tile.medianMilliseconds, tile.slowestMilliseconds, tiles.size());

  // At 0.5 m, on a split copy of the region the search reads and of the touched tile.
  const auto [minX, maxX] = std::ranges::minmax(collapse | std::views::transform(&Protocol::VoxelPosition::xVoxels));
  const auto [minZ, maxZ] = std::ranges::minmax(collapse | std::views::transform(&Protocol::VoxelPosition::zVoxels));
  const Simulation::MaterialRules halfMeterRules = Simulation::ScaleSpans(oneMeterRules, 2);
  const auto reach = static_cast<std::int32_t>(Simulation::LongestSpanVoxels(oneMeterRules)) * 2 + 1;
  const std::int32_t regionMinX = std::min(minX - reach, tileX * Simulation::NavigationGrid::TILE_EDGE_CELLS);
  const std::int32_t regionMinZ = std::min(minZ - reach, tileZ * Simulation::NavigationGrid::TILE_EDGE_CELLS);
  const std::int32_t regionMaxX = std::max(maxX + reach, (tileX + 1) * Simulation::NavigationGrid::TILE_EDGE_CELLS - 1);
  const std::int32_t regionMaxZ = std::max(maxZ + reach, (tileZ + 1) * Simulation::NavigationGrid::TILE_EDGE_CELLS - 1);
  Simulation::ChunkStore half;
  Simulation::SplitToHalfMeters(_store, regionMinX, regionMinZ, regionMaxX, regionMaxZ, half);
  std::vector<Protocol::VoxelPosition> halfCollapse;
  for (const Protocol::VoxelPosition& voxel : collapse)
  {
    std::ranges::copy(Simulation::HalfMeterVoxels(voxel), std::back_inserter(halfCollapse));
  }
  const Simulation::SupportRules halfSupport{.materials = &halfMeterRules,
                                             .anchorDepthVoxels = 2 * ANCHOR_DEPTH_METERS,
                                             .surfaceVoxels = [&_world](std::int32_t _x, std::int32_t _z)
                                             { return 2 * _world.SurfaceAt(_x / 2, _z / 2); },
                                             .heightVoxels = 2 * Simulation::TestWorld::HEIGHT_VOXELS};
  Simulation::SupportSearchResult halfFound;
  const Timing halfSearch = Time(REPEATS, [&] { halfFound = Simulation::FindUnsupported(half, halfSupport, halfCollapse); });
  std::println("  support search after the breach, 0.5 m: {:.3f} ms median, {:.3f} ms slowest; {} voxels changed, {} read, {} pieces fell",
               halfSearch.medianMilliseconds, halfSearch.slowestMilliseconds, halfCollapse.size(), halfFound.voxelsSearched,
               halfFound.pieces.size());

  // The same 32 m of ground in 0.5 m cells is four tiles of 32 cells.
  const std::int32_t halfEdgeCells = 2 * _world.edgeVoxels;
  Simulation::NavigationGrid halfGrid({.widthCells = halfEdgeCells,
                                       .depthCells = halfEdgeCells,
                                       .heightVoxels = 2 * Simulation::TestWorld::HEIGHT_VOXELS,
                                       .climbVoxels = 2 * CLIMB_METERS},
                                      halfMeterRules);
  const Timing halfTile = Time(REPEATS,
                               [&]
                               {
                                 for (std::int32_t dz = 0; dz < 2; ++dz)
                                 {
                                   for (std::int32_t dx = 0; dx < 2; ++dx)
                                   {
                                     halfGrid.RebuildTile(half, 2 * tileX + dx, 2 * tileZ + dz);
                                   }
                                 }
                               });
  std::println("  navigation, 0.5 m: the same 32 m of ground in {:.3f} ms median, {:.3f} ms slowest", halfTile.medianMilliseconds,
               halfTile.slowestMilliseconds);
}

/// The generated MVP map, every chunk of it, at the generator's fixed seeds.
void GenerateMvpMap(Simulation::ChunkStore& _store)
{
  const Simulation::Generator generator({.worldSeed = SEED,
                                         .resourceSeed = SEED + 1,
                                         .widthVoxels = Simulation::Generator::MAP_WIDTH_VOXELS,
                                         .heightVoxels = Simulation::Generator::MAP_HEIGHT_VOXELS,
                                         .depthVoxels = Simulation::Generator::MAP_DEPTH_VOXELS});
  for (std::int32_t y = 0; y < generator.HeightChunks(); ++y)
  {
    for (std::int32_t z = 0; z < generator.DepthChunks(); ++z)
    {
      for (std::int32_t x = 0; x < generator.WidthChunks(); ++x)
      {
        const auto chunk = std::make_unique<Protocol::Chunk>();
        generator.GenerateChunk({.xChunks = x, .yChunks = y, .zChunks = z}, *chunk);
        _store.SetChunk({.xChunks = x, .yChunks = y, .zChunks = z}, *chunk);
      }
    }
  }
}

constexpr std::array<std::pair<Simulation::NavigationClass, const char*>, Simulation::NAVIGATION_CLASS_COUNT> CLASS_NAMES = {
  {{Simulation::NavigationClass::Foot, "foot"},
   {Simulation::NavigationClass::Cavalry, "cavalry"},
   {Simulation::NavigationClass::Machines, "machines"}}};

/// Cross-map path searches on the generated MVP map, for each class (M2-F3; ADR-015, decision 5):
/// the nodes each expands, its time and its path, corner to corner both ways.
void MeasureSearches(const Simulation::NavigationGrid& _grid)
{
  const std::int32_t width = _grid.WidthCells();
  constexpr std::int32_t CORNER = 8;
  const std::array<std::pair<std::int64_t, std::int64_t>, 2> trips = {
    {{std::int64_t{CORNER} * width + CORNER, std::int64_t{width - CORNER} * width + (width - CORNER)},
     {std::int64_t{width - CORNER} * width + CORNER, std::int64_t{CORNER} * width + (width - CORNER)}}};
  for (const auto& [navigationClass, name] : CLASS_NAMES)
  {
    for (const auto& [from, to] : trips)
    {
      Simulation::PathSearch search(1, navigationClass, from, to, to, width);
      const Clock::time_point start = Clock::now();
      static_cast<void>(search.Advance(_grid, UINT32_MAX));
      const double milliseconds = MillisecondsSince(start);
      std::println("  {} from ({}, {}) to ({}, {}): {} nodes in {:.1f} ms, {} cells{}", name, from % width, from / width, to % width,
                   to / width, search.NodesExpanded(), milliseconds, search.Path().size(),
                   search.ShortOfGoal() ? ", short of the goal" : "");
    }
  }
}

/// Region labels over the whole map, the alternative M2-F4 measured against per-tile components: each
/// class's cells flooded along its links, labelled from 1 in cell order. Returns the regions found.
std::size_t LabelRegions(const Simulation::NavigationGrid& _grid, std::vector<std::uint32_t>& _regions)
{
  const std::int32_t width = _grid.WidthCells();
  const std::int32_t depth = _grid.DepthCells();
  std::size_t found = 0;
  std::vector<std::pair<std::int32_t, std::int32_t>> open;
  for (const auto& [navigationClass, name] : CLASS_NAMES)
  {
    _regions.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(depth), 0);
    std::uint32_t next = 1;
    const auto at = [width](std::int32_t _x, std::int32_t _z)
    { return static_cast<std::size_t>(_z) * static_cast<std::size_t>(width) + static_cast<std::size_t>(_x); };
    for (std::int32_t z = 0; z < depth; ++z)
    {
      for (std::int32_t x = 0; x < width; ++x)
      {
        if (_regions[at(x, z)] != 0 || !_grid.Usable(x, z, navigationClass))
        {
          continue;
        }
        _regions[at(x, z)] = next;
        open.assign(1, {x, z});
        while (!open.empty())
        {
          const auto [fromX, fromZ] = open.back();
          open.pop_back();
          const std::uint8_t links = _grid.Links(fromX, fromZ, navigationClass);
          for (std::size_t direction = 0; direction < Simulation::NavigationGrid::NEIGHBORS.size(); ++direction)
          {
            const std::int32_t toX = fromX + Simulation::NavigationGrid::NEIGHBORS.at(direction)[0];
            const std::int32_t toZ = fromZ + Simulation::NavigationGrid::NEIGHBORS.at(direction)[1];
            if ((links & (1U << direction)) != 0 && _regions[at(toX, toZ)] == 0)
            {
              _regions[at(toX, toZ)] = next;
              open.emplace_back(toX, toZ);
            }
          }
        }
        ++next;
      }
    }
    found += next - 1;
  }
  return found;
}

/// How reach is known without searching the map (M2-F4): region labels over the whole map, which an
/// edit anywhere makes the next search label again, against per-tile components, which an edit
/// labels again only in the tiles whose links changed, joined at each query by the links between
/// tiles. Both on the generated MVP map, for every class.
void MeasureReach(Simulation::NavigationGrid& _grid, Simulation::ChunkStore& _store)
{
  std::println("Reach on the generated MVP map, every class:");
  std::vector<std::uint32_t> regions;
  std::size_t regionCount = 0;
  const Timing labelling = Time(REPEATS, [&] { regionCount = LabelRegions(_grid, regions); });
  std::println("  region labels, the whole map: {} regions, {:.2f} ms median, {:.2f} ms slowest", regionCount, labelling.medianMilliseconds,
               labelling.slowestMilliseconds);
  Clock::time_point start = Clock::now();
  _grid.UpdateReach();
  const double everyTile = MillisecondsSince(start);
  std::size_t components = 0;
  for (const auto& [navigationClass, name] : CLASS_NAMES)
  {
    components += _grid.Components(navigationClass);
  }
  std::println("  per-tile components, every tile: {} components in {:.2f} ms", components, everyTile);
  // An edit's tiles labelled again: a masonry wall 4 m high across the middle tile, from its south
  // edge to its north, raised and then taken down, each time rebuilding the tiles it touches and
  // timing only UpdateReach after it. Region labels would label the whole map again instead.
  constexpr std::uint8_t MASONRY = 6;
  constexpr std::int32_t WALL_HEIGHT_VOXELS = 4;
  const std::int32_t wallX =
    (_grid.TilesAcross() / 2) * Simulation::NavigationGrid::TILE_EDGE_CELLS + Simulation::NavigationGrid::TILE_EDGE_CELLS / 2;
  const std::int32_t wallZ = (_grid.TilesDeep() / 2) * Simulation::NavigationGrid::TILE_EDGE_CELLS;
  std::vector<Protocol::VoxelPosition> wall;
  for (std::int32_t z = wallZ; z < wallZ + Simulation::NavigationGrid::TILE_EDGE_CELLS; ++z)
  {
    const std::int16_t surface = _grid.WalkableHeight(wallX, z);
    for (std::int32_t y = surface; surface != Simulation::NavigationGrid::NOT_WALKABLE && y < surface + WALL_HEIGHT_VOXELS; ++y)
    {
      if (_store.At({.xVoxels = wallX, .yVoxels = y, .zVoxels = z}) == 0)
      {
        wall.push_back({.xVoxels = wallX, .yVoxels = y, .zVoxels = z});
      }
    }
  }
  const std::vector<std::pair<std::int32_t, std::int32_t>> tiles = _grid.TilesTouchedBy(wall);
  std::vector<double> times;
  for (int repeat = 0; repeat < REPEATS; ++repeat)
  {
    for (const std::uint8_t index : {MASONRY, std::uint8_t{0}})
    {
      for (const Protocol::VoxelPosition& voxel : wall)
      {
        _store.Set(voxel, index);
      }
      for (const auto& [tileX, tileZ] : tiles)
      {
        _grid.RebuildTile(_store, tileX, tileZ);
      }
      start = Clock::now();
      _grid.UpdateReach();
      times.push_back(MillisecondsSince(start));
    }
  }
  std::ranges::sort(times);
  std::println("  per-tile components, a wall across one tile raised or taken down ({} tiles rebuilt): {:.3f} ms median, {:.3f} ms "
               "slowest",
               tiles.size(), times[times.size() / 2], times.back());
  const std::int32_t width = _grid.WidthCells();
  constexpr std::int32_t CORNER = 8;
  const std::int64_t from = std::int64_t{CORNER} * width + CORNER;
  const std::int64_t to = std::int64_t{width - CORNER} * width + (width - CORNER);
  for (const auto& [navigationClass, name] : CLASS_NAMES)
  {
    std::int64_t nearest = 0;
    const Timing query = Time(REPEATS, [&] { nearest = _grid.NearestReachable(from, to, navigationClass); });
    std::println("  {} from ({}, {}) toward ({}, {}): reaches ({}, {}), {:.3f} ms median", name, from % width, from / width, to % width,
                 to / width, nearest % width, nearest / width, query.medianMilliseconds);
  }
}

/// An environment variable's value, or "unknown". _dupenv_s, not std::getenv, which MSVC deprecates
/// (C4996), and /WX would make that an error; both are the CRT's, so no Windows header is needed.
std::string Environment(const char* _name)
{
  char* value = nullptr;
  std::size_t length = 0;
  if (_dupenv_s(&value, &length, _name) != 0 || value == nullptr)
  {
    return "unknown";
  }
  const std::unique_ptr<char, decltype(&std::free)> owned(value, &std::free);
  return {owned.get()};
}

/// The breach run's every tick, timed (M2-G2): the per-tick times go to _csv with the build id and
/// the CPU. The run is judged by its slowest tick of play against MvpPlan.md's input 2, 25 ms, half
/// a tick: every tick but tick 0, which takes in the run's own levelling and enclosure as edits and
/// is reported on its own (the owner, after M2's report, in place of the 95th percentile over every
/// tick, which the idle ticks made meaningless). The 95th percentile is still reported. Building the
/// world and the run is not a tick, and is not timed.
int RunBreach(const std::filesystem::path& _executable, const std::filesystem::path& _csv)
{
  constexpr double TARGET_MILLISECONDS = 25.0;
  constexpr double PERCENTILE = 0.95;
  const std::uint64_t buildId = BuildIdOf(_executable);
  const Simulation::ServerData data = Simulation::ServerData::Load(_executable.parent_path() / "ServerData" / "ServerData.bin");
  Simulation::World world(Simulation::BreachRunDesc(data, buildId));
  const Simulation::BreachRun run = Simulation::BuildBreachRun(world);
  std::vector<double> times;
  times.reserve(run.ticks);
  for (std::uint32_t tick = 0; tick < run.ticks; ++tick)
  {
    const Clock::time_point start = Clock::now();
    Simulation::RunScript(world, run.script, 1);
    times.push_back(MillisecondsSince(start));
  }
  if (times.size() < 2)
  {
    throw std::runtime_error("the breach run has no tick of play to time");
  }
  // The slowest tick of play: every tick after tick 0.
  const auto slowestPlay = std::ranges::max_element(times.begin() + 1, times.end());
  const auto slowestPlayTick = static_cast<std::size_t>(slowestPlay - times.begin());
  const bool within = *slowestPlay <= TARGET_MILLISECONDS;
  std::vector<double> sorted = times;
  std::ranges::sort(sorted);
  // The nearest-rank percentile: the smallest time at least 95% of the ticks do not exceed.
  const auto rank = static_cast<std::size_t>(std::ceil(PERCENTILE * static_cast<double>(sorted.size())));
  const double percentile = sorted[std::max<std::size_t>(rank, 1) - 1];
  const std::string cpu = Environment("PROCESSOR_IDENTIFIER");
  const std::string processors = Environment("NUMBER_OF_PROCESSORS");
  const Simulation::Rules& rules = data.Tuning();

  std::ofstream csv(_csv);
  if (!csv)
  {
    throw std::runtime_error("cannot write " + _csv.string());
  }
  csv << std::format("build id,{:016x}\ncpu,\"{}\"\nprocessors,{}\n", buildId, cpu, processors);
  csv << std::format("target milliseconds,{:.1f}\nslowest tick of play milliseconds,{:.3f}\nslowest tick of play,{}\n", TARGET_MILLISECONDS,
                     *slowestPlay, slowestPlayTick);
  csv << std::format("tick 0 milliseconds,{:.3f}\n95th percentile milliseconds,{:.3f}\n", times.front(), percentile);
  csv << std::format("slumpVoxelsPerTick,{}\npathNodesPerTick,{}\ntick,milliseconds\n", rules.slumpVoxelsPerTick, rules.pathNodesPerTick);
  for (std::size_t tick = 0; tick < times.size(); ++tick)
  {
    csv << std::format("{},{:.4f}\n", tick, times[tick]);
  }
  std::println("The M2 breach run: {} ticks; the slowest tick of play, {}, took {:.3f} ms against {:.0f} ms, {}", times.size(),
               slowestPlayTick, *slowestPlay, TARGET_MILLISECONDS, within ? "within it" : "OVER IT");
  std::println("tick 0, the run's set-up, took {:.3f} ms; the 95th percentile of every tick is {:.3f} ms", times.front(), percentile);
  std::println("build id {:016x}; CPU {}, {} logical processors; written to {}", buildId, cpu, processors, _csv.string());
  return within ? 0 : 2;
}

/// The integrity reading's cost (M3-B5): on the breach run, W2's reading before the first shot and
/// after each that strikes, the median and the slowest of REPEATS readings, with the support searches
/// each runs, against the query budget's charge of 8 for it (ADR-020, decision 6).
int RunIntegrity(const std::filesystem::path& _executable)
{
  const std::uint64_t buildId = BuildIdOf(_executable);
  const Simulation::ServerData data = Simulation::ServerData::Load(_executable.parent_path() / "ServerData" / "ServerData.bin");
  Simulation::World world(Simulation::BreachRunDesc(data, buildId));
  const Simulation::BreachRun run = Simulation::BuildBreachRun(world);
  Protocol::EntityId section = Protocol::NO_ENTITY;
  for (const auto& [id, structure] : world.Structures())
  {
    section = structure.name == "W2" ? id : section;
  }
  if (section == Protocol::NO_ENTITY)
  {
    throw std::runtime_error("the breach run has no W2");
  }
  const auto measure = [&](std::uint32_t _tick)
  {
    std::uint32_t searches = 0;
    Protocol::IntegrityReading reading{};
    const Timing timing = Time(REPEATS, [&] { reading = Simulation::ReadIntegrity(world, section, &searches); });
    std::println("tick {}: W2's hole {} wide, falls at {}; gap {}; {} support searches; {:.3f} ms median, {:.3f} ms slowest", _tick,
                 reading.holeWidthVoxels, reading.holeFallWidthVoxels, reading.gapRunVoxels, searches, timing.medianMilliseconds,
                 timing.slowestMilliseconds);
  };
  std::println("The integrity reading on the M2 breach run's W2, build id {:016x}", buildId);
  Simulation::RunScript(world, run.script, 1);
  measure(0);
  for (std::uint32_t tick = 1; tick < run.ticks; ++tick)
  {
    Simulation::RunScript(world, run.script, 1);
    if (!world.LastTickHits().empty())
    {
      measure(tick);
    }
  }
  return 0;
}

/// Every lab item headless (M3-B7): each on a fresh copy of the lab's site, its prior shots, then its
/// asked shots, each tick timed. Prints the item's id, its key, the world's ticks at the end, its state
/// hash and its slowest tick of play, every tick but tick 0, which takes in the site's own edits.
int RunLabItems(const std::filesystem::path& _executable)
{
  const std::uint64_t buildId = BuildIdOf(_executable);
  const Simulation::ServerData data = Simulation::ServerData::Load(_executable.parent_path() / "ServerData" / "ServerData.bin");
  const Simulation::World::Desc desc = Simulation::LabDesc(data, buildId);
  Simulation::World site(desc);
  const Simulation::LabSetup setup = Simulation::BuildLabSite(site);
  std::println("The breach lab's items, build id {:016x}, item set {:016x}", buildId, Simulation::LabItemsHash());
  bool timedOut = false;
  for (const Simulation::LabItem& item : Simulation::LabItems())
  {
    Simulation::World world = site;
    double slowest = 0.0;
    std::uint32_t slowestTick = 0;
    const auto play = [&](std::span<const Protocol::VoxelPosition> _aims)
    {
      Simulation::LabShots shots(setup, _aims);
      for (bool done = false; !done;)
      {
        const std::uint32_t tick = world.TickCount();
        const Clock::time_point start = Clock::now();
        done = shots.Advance(world);
        const double milliseconds = MillisecondsSince(start);
        if (tick > 0 && milliseconds > slowest)
        {
          slowest = milliseconds;
          slowestTick = tick;
        }
      }
      return shots.TimedOut();
    };
    const std::vector<Protocol::VoxelPosition> asked(item.shotsAsked, item.markVoxels);
    const bool priorTimedOut = play(item.priorAimVoxels);
    const bool askedTimedOut = play(asked);
    const bool itemTimedOut = priorTimedOut || askedTimedOut;
    timedOut = timedOut || itemTimedOut;
    std::println("{}: {}{}, {} ticks, state hash {:016x}, slowest tick {} at {:.3f} ms", item.id,
                 Simulation::CourtyardOpen(world, setup) ? "OPEN" : "SHUT", itemTimedOut ? " (TIMED OUT)" : "", world.TickCount(),
                 Simulation::StateHash(world), slowestTick, slowest);
  }
  return timedOut ? 2 : 0;
}

/// The nearest-rank percentile of _times: the smallest time at least _share of them do not exceed.
double Percentile(std::vector<double> _times, double _share)
{
  std::ranges::sort(_times);
  const auto rank = static_cast<std::size_t>(std::ceil(_share * static_cast<double>(_times.size())));
  return _times.at(std::max<std::size_t>(rank, 1) - 1);
}

/// The trial of a world its scenario gave one, as the Siege's always does.
const Simulation::TrialRules& TrialOf(const Simulation::World& _world)
{
  const std::optional<Simulation::TrialRules>& trial = _world.Trial();
  if (!trial.has_value())
  {
    throw std::runtime_error("the world has no trial");
  }
  return *trial;
}

/// The Siege trial played to its end by the scripted attacker against the defender (M4-F5; ADR-027), in the voxel version
/// or with --control the control's (ADR-028), every tick timed as the breach run's are: the slowest tick of play, tick 0
/// and the 95th percentile, and the slowest from tick 10, past the first ticks' work after the load, and how many ticks
/// of play took longer than M2's 25 ms; fog's visibility alone after 100 ticks, as M4-E1 measured it; the outcome and its tick, the
/// stones thrown and the stone each side spent; when each of the attacker's targets first stopped or opened; and the
/// attacker's figures through each opened section a minute, from the trial's events.
int RunSiegeTrial(const std::filesystem::path& _executable, bool _control)
{
  constexpr double PERCENTILE = 0.95;
  constexpr std::uint32_t VISIBILITY_AT_TICK = 100;
  constexpr std::uint32_t TICKS_PER_MINUTE = 1200;
  constexpr std::size_t SETTLED_TICK = 10;
  constexpr double TICK_BUDGET_MILLISECONDS = 25.0;
  const std::uint64_t buildId = BuildIdOf(_executable);
  const Simulation::ServerData data = Simulation::ServerData::Load(_executable.parent_path() / "ServerData" / "ServerData.bin");
  Simulation::World world(Simulation::ScenarioDesc(data, "Siege", buildId));
  Simulation::LoadScenario(world, "Siege",
                           {.version = _control ? Protocol::TrialVersion::Control : Protocol::TrialVersion::Voxel, .armies = true});
  const std::uint8_t attacker = TrialOf(world).attacker;
  const std::uint8_t defender = TrialOf(world).defender;
  world.SetCommander(attacker, std::make_unique<Simulation::SiegeAttacker>(nullptr));
  const std::int32_t attackerStone = world.Players().at(attacker).stoneHundredthsCubicMeters;
  const std::int32_t defenderStone = world.Players().at(defender).stoneHundredthsCubicMeters;
  std::vector<double> times;
  times.reserve(TrialOf(world).timeLimitTicks);
  Timing visibility{};
  std::map<std::string, std::uint32_t> first;
  std::map<Protocol::EntityId, std::map<std::uint32_t, std::int32_t>> passedByMinute;
  std::uint32_t shots = 0;
  while (world.State(attacker).trial.outcome == Protocol::TrialOutcome::Running)
  {
    const Clock::time_point start = Clock::now();
    world.Tick();
    times.push_back(MillisecondsSince(start));
    if (world.TickCount() == VISIBILITY_AT_TICK)
    {
      visibility = Time(REPEATS, [&world] { world.RecomputeVisibility(); });
    }
    for (const Simulation::TrialEvent& event : world.LastTickEvents())
    {
      // A shot is the stone its engine spends: ShotFired comes with the trial's log (M4-J2).
      shots +=
        event.kind == Simulation::TrialEventKind::StoneSpent && event.player == attacker && world.SiegeEngines().contains(event.subject)
          ? 1
          : 0;
      if (event.kind == Simulation::TrialEventKind::FigurePassed && event.player == attacker)
      {
        passedByMinute[event.other][event.tick / TICKS_PER_MINUTE] += event.amount;
      }
    }
    for (const auto& [id, structure] : world.Structures())
    {
      const Protocol::StructureView& view = *std::ranges::find(world.State(defender).structures, id, &Protocol::StructureView::id);
      if ((!view.working || view.opened) && !first.contains(structure.name))
      {
        first.emplace(structure.name, world.TickCount());
      }
    }
  }
  const Protocol::TrialView& trial = world.State(attacker).trial;
  const auto slowestPlay = std::ranges::max_element(times.begin() + 1, times.end());
  const auto slowestSettled = std::ranges::max_element(times.begin() + SETTLED_TICK, times.end());
  std::println("The Siege trial, {}, build id {:016x}: the scripted attacker against the defender",
               _control ? "the control" : "the voxel version", buildId);
  std::println("outcome {} at tick {} ({} s), {} stones thrown; stone spent: attacker {} m^3, defender {} m^3",
               static_cast<int>(trial.outcome), world.TickCount(), world.TickCount() / 20, shots,
               static_cast<double>(attackerStone - world.Players().at(attacker).stoneHundredthsCubicMeters) / 100.0,
               static_cast<double>(defenderStone - world.Players().at(defender).stoneHundredthsCubicMeters) / 100.0);
  std::println("the slowest tick of play, {}, took {:.3f} ms; tick 0 {:.3f} ms; the 95th percentile {:.3f} ms", slowestPlay - times.begin(),
               *slowestPlay, times.front(), Percentile(times, PERCENTILE));
  std::println("the slowest from tick {}, {}, took {:.3f} ms; {} ticks of play took longer than {} ms", SETTLED_TICK,
               slowestSettled - times.begin(), *slowestSettled,
               std::ranges::count_if(times.begin() + 1, times.end(), [](double _ms) { return _ms > TICK_BUDGET_MILLISECONDS; }),
               TICK_BUDGET_MILLISECONDS);
  std::println("visibility, every player, at tick {}: {:.3f} ms median, {:.3f} ms slowest", VISIBILITY_AT_TICK,
               visibility.medianMilliseconds, visibility.slowestMilliseconds);
  for (const auto& [name, tick] : first)
  {
    std::println("  {} first stopped or opened at tick {} ({} s)", name, tick, tick / 20);
  }
  for (const auto& [section, minutes] : passedByMinute)
  {
    for (const auto& [minute, figures] : minutes)
    {
      std::println("  section {}: {} figures through, net inwards, in minute {}", world.Structures().at(section).name, figures, minute);
    }
  }
  std::println("CPU {}, {} logical processors", Environment("PROCESSOR_IDENTIFIER"), Environment("NUMBER_OF_PROCESSORS"));
  return 0;
}

/// A template voxel's place across the ground after _turns clockwise quarter turns seen from above, as the world places it.
std::pair<std::int32_t, std::int32_t> Turned(std::int32_t _x, std::int32_t _z, std::int32_t _width, std::int32_t _depth,
                                             std::uint32_t _turns)
{
  switch (_turns % 4)
  {
  case 1:
    return {_z, _width - 1 - _x};
  case 2:
    return {_width - 1 - _x, _depth - 1 - _z};
  case 3:
    return {_depth - 1 - _z, _x};
  default:
    return {_x, _z};
  }
}

/// How many of _structure's critical part's voxels stand.
std::size_t CriticalStanding(const Simulation::World& _world, const Simulation::Structure& _structure)
{
  const Simulation::ServerTemplate& found = *_world.Data().FindTemplate(_structure.templateId);
  std::size_t standing = 0;
  std::size_t index = 0;
  for (std::uint32_t y = 0; y < found.heightVoxels; ++y)
  {
    for (std::uint32_t z = 0; z < found.depthVoxels; ++z)
    {
      for (std::uint32_t x = 0; x < found.widthVoxels; ++x, ++index)
      {
        if (found.parts.at(index) != Simulation::ServerTemplate::Part::Critical || found.indices.at(index) == Protocol::Chunk::EMPTY)
        {
          continue;
        }
        const auto [turnedX, turnedZ] =
          Turned(static_cast<std::int32_t>(x), static_cast<std::int32_t>(z), static_cast<std::int32_t>(found.widthVoxels),
                 static_cast<std::int32_t>(found.depthVoxels), _structure.quarterTurns);
        const Protocol::VoxelPosition at{.xVoxels = _structure.originVoxels.xVoxels + turnedX,
                                         .yVoxels = _structure.originVoxels.yVoxels + static_cast<std::int32_t>(y),
                                         .zVoxels = _structure.originVoxels.zVoxels + turnedZ};
        standing += _world.Voxels().At(at) != Protocol::Chunk::EMPTY ? 1 : 0;
      }
    }
  }
  return standing;
}

/// The control's calibration (M4-F5; ADR-028, decision 5, as the owner amended it on 2026-11-02): on the Siege castle alone,
/// unopposed and unrepaired, on the voxel version, one structure of each template in it is fired at from a stand by its
/// template's fixed pattern until it stops, or, for a section, opens; T5 instead takes the ram's blows until its gate breaks,
/// since stones reach its gate only from inside the bailey. Each template's hit points are its count times the control's
/// damage for a shot or a blow, written into _structures, GameData/Server/Structures.json, with the measurement in its note.
int CalibrateControl(const std::filesystem::path& _executable, const std::filesystem::path& _structures)
{
  struct Calibration
  {
    std::string_view templateId;
    std::string_view structure;
    Protocol::VoxelPosition standVoxels; // the trebuchet's, or the ram's start
    bool ram;
  };
  // The stands, measured on the Siege castle: each within the trebuchet's 12 to 50 m of its target's pattern, the keep and
  // E3 from off the flank, ST from south of the gate, the hall from outside the west curtain; the ram on the causeway.
  constexpr std::array<Calibration, 5> CALIBRATIONS = {{
    {.templateId = "T1", .structure = "HALL", .standVoxels = {.xVoxels = 300, .yVoxels = 9, .zVoxels = 270}, .ram = false},
    {.templateId = "T2", .structure = "KEEP", .standVoxels = {.xVoxels = 389, .yVoxels = 9, .zVoxels = 316}, .ram = false},
    {.templateId = "T3", .structure = "E3", .standVoxels = {.xVoxels = 389, .yVoxels = 9, .zVoxels = 316}, .ram = false},
    {.templateId = "T4", .structure = "ST", .standVoxels = {.xVoxels = 359, .yVoxels = 9, .zVoxels = 256}, .ram = false},
    {.templateId = "T5", .structure = "GH", .standVoxels = {.xVoxels = 335, .yVoxels = 9, .zVoxels = 290}, .ram = true},
  }};
  constexpr std::uint32_t TICK_LIMIT = 40000;
  const std::uint64_t buildId = BuildIdOf(_executable);
  const Simulation::ServerData data = Simulation::ServerData::Load(_executable.parent_path() / "ServerData" / "ServerData.bin");
  const Simulation::Rules& rules = data.Tuning();
  std::println("The control's calibration on the Siege castle alone, unopposed and unrepaired, voxel version, build id {:016x}", buildId);
  std::vector<std::pair<std::string_view, std::int32_t>> hitPoints;
  std::string measured;
  for (const Calibration& calibration : CALIBRATIONS)
  {
    Simulation::World world(Simulation::ScenarioDesc(data, "Siege", buildId));
    Simulation::LoadScenario(world, "Siege", {.version = Protocol::TrialVersion::Voxel, .armies = false});
    const std::uint8_t attacker = TrialOf(world).attacker;
    world.Tick();
    const auto target =
      std::ranges::find_if(world.Structures(), [&calibration](const auto& _entry) { return _entry.second.name == calibration.structure; });
    const Protocol::EntityId structure = target->first;
    const bool section = world.Data().FindTemplate(target->second.templateId)->criticalPart.empty();
    const auto done = [&world, structure, section]()
    {
      const Protocol::StructureView& view = *std::ranges::find(world.State(2).structures, structure, &Protocol::StructureView::id);
      return section ? view.opened : !view.working;
    };
    const auto order = [&world, attacker](const Protocol::Command& _command)
    {
      Core::ByteWriter writer;
      Protocol::Encode(_command, writer);
      world.Submit(attacker, writer.Bytes());
    };
    std::uint32_t count = 0;
    if (calibration.ram)
    {
      // Each blow on the timber gate takes one of its voxels (ramTimberCraterVoxels), so the blows are the gate's voxels gone.
      const Protocol::EntityId ram = world.AddSiegeEngine(attacker, Simulation::EngineKind::Ram, calibration.standVoxels);
      world.Tick();
      order(Protocol::AttackTarget{.squad = world.SiegeEngines().at(ram).squad, .target = structure});
      const std::size_t whole = CriticalStanding(world, world.Structures().at(structure));
      for (std::uint32_t tick = 0; tick < TICK_LIMIT && !done(); ++tick)
      {
        world.Tick();
      }
      count = static_cast<std::uint32_t>(whole - CriticalStanding(world, world.Structures().at(structure)));
    }
    else
    {
      const Protocol::EntityId engine = world.AddSiegeEngine(attacker, Simulation::EngineKind::Trebuchet, calibration.standVoxels);
      bool pending = false;
      for (std::uint32_t tick = 0; tick < TICK_LIMIT && !done(); ++tick)
      {
        const Simulation::SiegeEngine& trebuchet = world.SiegeEngines().at(engine);
        // A shot counts once its stone flew; one refused is aimed again.
        if (pending && (trebuchet.reloadTicks > 0 || !world.Shots().empty()))
        {
          ++count;
        }
        pending = false;
        if (trebuchet.reloadTicks == 0 && world.Shots().empty())
        {
          const Protocol::StructureView& view = *std::ranges::find(world.State(2).structures, structure, &Protocol::StructureView::id);
          order(Protocol::FireEngine{.engine = engine,
                                     .targetVoxels = Simulation::PatternAim(world.Data(), view, calibration.standVoxels, count)});
          pending = true;
        }
        world.Tick();
      }
    }
    if (!done())
    {
      throw std::runtime_error(std::format("{} did not {} within {} ticks", calibration.structure, section ? "open" : "stop", TICK_LIMIT));
    }
    const std::int32_t damage = calibration.ram ? rules.controlRamDamage : rules.controlShotDamage;
    const std::int32_t points = static_cast<std::int32_t>(count) * damage;
    hitPoints.emplace_back(calibration.templateId, points);
    measured += std::format("{}{} {} {} {} by tick {}, {} hit points", measured.empty() ? "" : "; ", calibration.templateId, count,
                            calibration.ram ? "blows" : "shots", section ? "opened" : "stopped", world.TickCount(), points);
    std::println("  {} ({}): {} after {} {} at tick {}: {} hit points", calibration.templateId, calibration.structure,
                 section ? "opened" : "stopped", count, calibration.ram ? "ram blows" : "shots", world.TickCount(), points);
  }
  // Each template's controlHitPoints, and the note's sentence on them, rewritten in place.
  std::ifstream input(_structures, std::ios::binary);
  if (!input)
  {
    throw std::runtime_error("cannot read " + _structures.string());
  }
  std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  input.close();
  for (const auto& [templateId, points] : hitPoints)
  {
    const std::size_t entry = text.find(std::format("\"template\": \"{}\"", templateId));
    const std::string key = "\"controlHitPoints\": \"";
    const std::size_t value = entry == std::string::npos ? std::string::npos : text.find(key, entry);
    if (value == std::string::npos)
    {
      throw std::runtime_error(std::format("{} has no controlHitPoints for {}", _structures.string(), templateId));
    }
    const std::size_t end = text.find('"', value + key.size());
    text.replace(value + key.size(), end - (value + key.size()), std::to_string(points));
  }
  const std::string from = "The hit points are the control's (ADR-028): ";
  const std::size_t sentence = text.find(from);
  const std::size_t sentenceEnd = sentence == std::string::npos ? std::string::npos : text.find(" A cubic metre of repair", sentence);
  if (sentenceEnd == std::string::npos)
  {
    throw std::runtime_error(_structures.string() + "'s note has no sentence on the hit points");
  }
  text.replace(sentence, sentenceEnd - sentence,
               from +
                 std::format("calibrated by SimulationBench --calibrate-control (M4-F5; ADR-028, decision 5) on the Siege castle alone, "
                             "unopposed and unrepaired, on the voxel version, each template's shots by its fixed pattern to stop or "
                             "open it times 100, and T5's ram blows to break its gate times 100, as the owner decided on 2026-11-02: {}. "
                             "T6 and T7 stand in no castle and keep input 19's first 900.",
                             measured));
  std::ofstream output(_structures, std::ios::binary);
  output << text;
  std::println("written to {}", _structures.string());
  return 0;
}

int Run(const std::filesystem::path& _executable)
{
  std::println("SimulationBench: the renderer spike's question 6, at 1 m and at 0.5 m (ADR-006). Times are this machine's.");
  std::println("build id {:016x}", BuildIdOf(_executable));
  // The server's data sits beside the executable, in the build's output folder (ADR-010, decision 3).
  const Simulation::ServerData data = Simulation::ServerData::Load(_executable.parent_path() / "ServerData" / "ServerData.bin");
  {
    Simulation::ChunkStore store;
    const Clock::time_point start = Clock::now();
    const Simulation::TestWorld world = Simulation::BuildTestWorld(store, MVP_MAP_VOXELS, SEED, data.Templates());
    Measure("The MVP map (the test world at 512 m)", store, world, MillisecondsSince(start), data);
  }
  {
    Simulation::ChunkStore store;
    const Clock::time_point start = Clock::now();
    const Simulation::TestWorld world = Simulation::BuildSeasonWorld(store, SEASON_WORLD_VOXELS, SEASON_HOLDINGS, SEED, data.Templates());
    Measure("The season world (2 km, 32 holdings)", store, world, MillisecondsSince(start), data);
  }
  {
    Simulation::ChunkStore store;
    GenerateMvpMap(store);
    Simulation::NavigationGrid grid({.widthCells = Simulation::Generator::MAP_WIDTH_VOXELS,
                                     .depthCells = Simulation::Generator::MAP_DEPTH_VOXELS,
                                     .heightVoxels = Simulation::Generator::MAP_HEIGHT_VOXELS,
                                     .climbVoxels = 1,
                                     .rules = &data.Tuning()},
                                    data.Materials());
    const Clock::time_point rebuildStart = Clock::now();
    grid.RebuildAll(store);
    std::println("The generated MVP map: every navigation tile, with its classes and clearance, in {:.0f} ms",
                 MillisecondsSince(rebuildStart));
    MeasureSearches(grid);
    MeasureReach(grid, store);
  }
  return 0;
}

/// A log's comment lines, `# key: value`, as a map: what a replay beside it was written from (M4-J2's head).
std::map<std::string, std::string> LogHead(const std::filesystem::path& _log)
{
  std::map<std::string, std::string> head;
  std::ifstream input(_log, std::ios::binary);
  std::string line;
  while (std::getline(input, line) && line.starts_with('#'))
  {
    if (!line.empty() && line.back() == '\r')
    {
      line.pop_back();
    }
    const std::size_t colon = line.find(':');
    if (colon != std::string::npos && colon >= 2)
    {
      head.emplace(line.substr(2, colon - 2), colon + 2 <= line.size() ? line.substr(colon + 2) : std::string());
    }
  }
  return head;
}

std::vector<std::byte> ReadBytes(const std::filesystem::path& _file)
{
  std::ifstream input(_file, std::ios::binary);
  const std::vector<char> raw{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
  const std::span<const std::byte> bytes = std::as_bytes(std::span(raw));
  return {bytes.begin(), bytes.end()};
}

std::string_view OutcomeName(Protocol::TrialOutcome _outcome) noexcept
{
  switch (_outcome)
  {
  case Protocol::TrialOutcome::Won:
    return "won";
  case Protocol::TrialOutcome::LostToClock:
    return "lost to the clock";
  case Protocol::TrialOutcome::LostArmy:
    return "lost by the army";
  case Protocol::TrialOutcome::Running:
    break;
  }
  return "no outcome";
}

/// SimulationBench --check-replays <folder> (M5-B3; MvpPlan-M5.md): every replay in _folder played headless and every
/// recorded hash compared, each from the scenario, the build id and the server-data hash the log beside it names. The bench
/// is another executable, so its own build id never matches Earthrise's: it takes the log's, which is sound only when the
/// two were built from one commit, the playtest's frozen one (GameDesign.md, M5's values, item 15). Exits 1 when a replay
/// does not match, cannot be loaded, or was written with other server data, or when the folder holds none.
int CheckReplays(const std::filesystem::path& _executable, const std::filesystem::path& _folder)
{
  const Simulation::ServerData data = Simulation::ServerData::Load(_executable.parent_path() / "ServerData" / "ServerData.bin");
  const std::string dataHash = std::format("{:016x}", data.Hash());
  std::vector<std::filesystem::path> replays;
  for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(_folder))
  {
    if (entry.path().extension() == ".errp")
    {
      replays.push_back(entry.path());
    }
  }
  std::ranges::sort(replays);
  if (replays.empty())
  {
    std::println(stderr, "SimulationBench: no replay in {}", _folder.string());
    return 1;
  }
  std::size_t matched = 0;
  for (const std::filesystem::path& replay : replays)
  {
    std::filesystem::path log = replay;
    log.replace_extension(".csv");
    const std::map<std::string, std::string> head = LogHead(log);
    const auto scenario = head.find("scenario");
    const auto build = head.find("build id");
    const auto hash = head.find("server-data hash");
    if (scenario == head.end() || build == head.end() || hash == head.end())
    {
      std::println("{}: its log names no scenario, build id or server-data hash", replay.filename().string());
      continue;
    }
    if (hash->second != dataHash)
    {
      std::println("{}: written with server data {}, not this bench's {}", replay.filename().string(), hash->second, dataHash);
      continue;
    }
    try
    {
      const Simulation::ReplayCheck check =
        Simulation::CheckScenarioReplay(ReadBytes(replay), data, scenario->second, std::stoull(build->second, nullptr, 16));
      if (check.result.matched)
      {
        ++matched;
        std::println("{}: matched, {} ticks, {}", replay.filename().string(), check.result.ticksReplayed, OutcomeName(check.outcome));
      }
      else
      {
        std::println("{}: differs at tick {}", replay.filename().string(), check.result.firstDifferingTick.value_or(0));
      }
    }
    catch (const std::exception& error) // a start LoadSave refuses, or a log's build id that is not a number
    {
      std::println("{}: not replayed, {}", replay.filename().string(), error.what());
    }
  }
  std::println("{} of {} replays matched", matched, replays.size());
  return matched == replays.size() ? 0 : 1;
}

} // namespace

int wmain(int _argumentCount, wchar_t** _arguments)
{
  // std::println throws std::format_error on a bad format string, and ServerData refuses a file it
  // cannot read. A bench that fails says why and exits non-zero, rather than letting the exception
  // escape wmain.
  try
  {
    if (_argumentCount < 1 || _arguments == nullptr || _arguments[0] == nullptr)
    {
      throw std::runtime_error("the bench was started without its own path, so it cannot find its ServerData folder");
    }
    const std::filesystem::path executable = std::filesystem::absolute(_arguments[0]);
    // SimulationBench --breach-run [file.csv] times the breach run (M2-G2), --integrity the integrity
    // reading on it (M3-B5), --lab-items the breach lab's items (M3-B7), --siege [--control] plays the Siege trial out with
    // the scripted attacker and times it (M4-F5), and --calibrate-control <Structures.json> calibrates the control's hit
    // points into that file (M4-F5), and --check-replays <folder> replays every trial run in a folder headless (M5-B3); without
    // any, the M1 measurements and the navigation's run.
    const std::span<wchar_t*> arguments(_arguments, static_cast<std::size_t>(_argumentCount));
    if (arguments.size() >= 2 && std::wstring_view(arguments[1]) == L"--breach-run")
    {
      return RunBreach(executable, arguments.size() >= 3 ? std::filesystem::path(arguments[2]) : std::filesystem::path("BreachRun.csv"));
    }
    if (arguments.size() >= 2 && std::wstring_view(arguments[1]) == L"--integrity")
    {
      return RunIntegrity(executable);
    }
    if (arguments.size() >= 2 && std::wstring_view(arguments[1]) == L"--lab-items")
    {
      return RunLabItems(executable);
    }
    if (arguments.size() >= 2 && std::wstring_view(arguments[1]) == L"--siege")
    {
      return RunSiegeTrial(executable, arguments.size() >= 3 && std::wstring_view(arguments[2]) == L"--control");
    }
    if (arguments.size() >= 3 && std::wstring_view(arguments[1]) == L"--check-replays")
    {
      return CheckReplays(executable, std::filesystem::path(arguments[2]));
    }
    if (arguments.size() >= 3 && std::wstring_view(arguments[1]) == L"--calibrate-control")
    {
      return CalibrateControl(executable, std::filesystem::path(arguments[2]));
    }
    return Run(executable);
  }
  catch (const std::exception& error)
  {
    std::fputs(error.what(), stderr);
    return 1;
  }
}
