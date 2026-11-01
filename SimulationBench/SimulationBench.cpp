#include "pch.h"

#include "Catalog.h"
#include "ChunkStore.h"
#include "NavigationGrid.h"
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

Simulation::TemplateLookup ContentTemplates()
{
  // Content's templates until the server bake (M2): the owner's choice of 2026-11-01, ADR-010 amended.
  return [](std::string_view _id) -> std::optional<Simulation::TemplateShape>
  {
    const Content::TemplateView* found = Content::FindTemplate(_id);
    if (found == nullptr)
    {
      return std::nullopt;
    }
    return Simulation::TemplateShape{.widthVoxels = found->grid.widthVoxels,
                                     .heightVoxels = found->grid.heightVoxels,
                                     .depthVoxels = found->grid.depthVoxels,
                                     .indices = found->grid.indices};
  };
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
void Measure(std::string_view _name, Simulation::ChunkStore& _store, const Simulation::TestWorld& _world, double _buildMilliseconds)
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
  const Simulation::MaterialRules oneMeterRules = Simulation::StandInMaterialRules(1);
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
  const Simulation::MaterialRules halfMeterRules = Simulation::StandInMaterialRules(2);
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

int Run()
{
  std::println("SimulationBench: the renderer spike's question 6, at 1 m and at 0.5 m (ADR-006). Times are this machine's.");
  {
    Simulation::ChunkStore store;
    const Clock::time_point start = Clock::now();
    const Simulation::TestWorld world = Simulation::BuildTestWorld(store, MVP_MAP_VOXELS, SEED, ContentTemplates());
    Measure("The MVP map (the test world at 512 m)", store, world, MillisecondsSince(start));
  }
  {
    Simulation::ChunkStore store;
    const Clock::time_point start = Clock::now();
    const Simulation::TestWorld world = Simulation::BuildSeasonWorld(store, SEASON_WORLD_VOXELS, SEASON_HOLDINGS, SEED, ContentTemplates());
    Measure("The season world (2 km, 32 holdings)", store, world, MillisecondsSince(start));
  }
  return 0;
}

} // namespace

int main()
{
  // std::println throws std::format_error on a bad format string. A bench that fails says why and
  // exits non-zero, rather than letting the exception escape main.
  try
  {
    return Run();
  }
  catch (const std::exception& error)
  {
    std::fputs(error.what(), stderr);
    return 1;
  }
}
