#include "pch.h"

#include "Generator.h"
#include "Hash.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The MVP map's generator (ADR-003; GameDesign.md, M2's values, item 3).

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t GRASS = 1;
constexpr std::uint8_t EARTH = 2;
constexpr std::uint8_t COMMON_ROCK = 3;
constexpr std::uint8_t BUILDING_STONE = 4;
constexpr std::uint8_t BEDROCK = 5;
constexpr std::uint8_t TIMBER = 7;
constexpr std::uint8_t WATER = 9;
constexpr std::uint8_t LEAVES_A = 10;
constexpr std::uint8_t LEAVES_B = 11;
constexpr std::int32_t EDGE = Protocol::CHUNK_EDGE_VOXELS;

Simulation::Generator::Desc MapDesc(std::uint64_t _worldSeed, std::uint64_t _resourceSeed)
{
  return {.worldSeed = _worldSeed,
          .resourceSeed = _resourceSeed,
          .widthVoxels = Simulation::Generator::MAP_WIDTH_VOXELS,
          .heightVoxels = Simulation::Generator::MAP_HEIGHT_VOXELS,
          .depthVoxels = Simulation::Generator::MAP_DEPTH_VOXELS};
}

/// The whole map as one array, x fastest, then z, then y, generated chunk by chunk.
struct Map
{
  std::int32_t width;
  std::int32_t height;
  std::int32_t depth;
  std::vector<std::uint8_t> voxels;

  explicit Map(const Simulation::Generator& _generator)
    : width(_generator.Description().widthVoxels),
      height(_generator.Description().heightVoxels),
      depth(_generator.Description().depthVoxels),
      voxels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * static_cast<std::size_t>(depth))
  {
    for (std::int32_t cy = 0; cy < _generator.HeightChunks(); ++cy)
    {
      for (std::int32_t cz = 0; cz < _generator.DepthChunks(); ++cz)
      {
        for (std::int32_t cx = 0; cx < _generator.WidthChunks(); ++cx)
        {
          const auto chunk = std::make_unique<Protocol::Chunk>();
          _generator.GenerateChunk({.xChunks = cx, .yChunks = cy, .zChunks = cz}, *chunk);
          for (std::int32_t y = 0; y < EDGE; ++y)
          {
            for (std::int32_t z = 0; z < EDGE; ++z)
            {
              for (std::int32_t x = 0; x < EDGE; ++x)
              {
                voxels[Index(cx * EDGE + x, cy * EDGE + y, cz * EDGE + z)] = chunk->At(x, y, z);
              }
            }
          }
        }
      }
    }
  }

  [[nodiscard]] std::size_t Index(std::int32_t _x, std::int32_t _y, std::int32_t _z) const noexcept
  {
    return (static_cast<std::size_t>(_y) * static_cast<std::size_t>(depth) + static_cast<std::size_t>(_z)) *
             static_cast<std::size_t>(width) +
           static_cast<std::size_t>(_x);
  }

  [[nodiscard]] std::uint8_t At(std::int32_t _x, std::int32_t _y, std::int32_t _z) const noexcept
  {
    return voxels[Index(_x, _y, _z)];
  }

  /// The height of a column's top voxel, or -1 for none.
  [[nodiscard]] std::int32_t Top(std::int32_t _x, std::int32_t _z) const noexcept
  {
    for (std::int32_t y = height - 1; y >= 0; --y)
    {
      if (At(_x, y, _z) != 0)
      {
        return y;
      }
    }
    return -1;
  }
};

/// Every chunk's voxels hashed, keyed by its packed coordinate, generating the chunks in _order.
std::map<std::uint64_t, std::uint64_t> ChunkHashes(const Simulation::Generator& _generator, bool _reversed)
{
  std::vector<Protocol::ChunkCoordinate> order;
  for (std::int32_t cy = 0; cy < _generator.HeightChunks(); ++cy)
  {
    for (std::int32_t cz = 0; cz < _generator.DepthChunks(); ++cz)
    {
      for (std::int32_t cx = 0; cx < _generator.WidthChunks(); ++cx)
      {
        order.push_back({.xChunks = cx, .yChunks = cy, .zChunks = cz});
      }
    }
  }
  if (_reversed)
  {
    std::ranges::reverse(order);
  }
  std::map<std::uint64_t, std::uint64_t> hashes;
  for (const Protocol::ChunkCoordinate& coordinate : order)
  {
    const auto chunk = std::make_unique<Protocol::Chunk>();
    _generator.GenerateChunk(coordinate, *chunk);
    hashes.emplace(Protocol::PackChunkKey(coordinate), Core::Fnv1a64(std::as_bytes(chunk->Voxels())));
  }
  return hashes;
}

/// The seed pairs the golden hashes and the slope invariant are taken on, each with its map's hash:
/// Core::Fnv1a64 over every chunk's voxels, in packed-key order, continued from chunk to chunk.
struct Golden
{
  std::uint64_t worldSeed;
  std::uint64_t resourceSeed;
  std::uint64_t mapHash;
};

// Computed on 2026-11-01 by clang 18 and GCC 13 on Linux, which agreed; CI's MSVC build checks them
// on Windows. A change to the generator's output fails here: an intended one comes with a new
// GENERATOR_VERSION and new hashes (ADR-003, *Consequences*). Release must give the same.
constexpr std::array<Golden, 3> GOLDEN = {{
  {.worldSeed = 1, .resourceSeed = 2, .mapHash = 0xad636116dc91b363ULL},
  {.worldSeed = 0x5eed2026ULL, .resourceSeed = 0x0e5ULL, .mapHash = 0x1178e40f31025b76ULL},
  {.worldSeed = 0xdeadbeefULL, .resourceSeed = 7, .mapHash = 0x56212d74ab5a9df6ULL},
}};

} // namespace

TEST_CLASS(GeneratorTests)
{
public:
  TEST_METHOD(GivesTheSameChunkTwice)
  {
    const Simulation::Generator generator(MapDesc(11, 12));
    const Protocol::ChunkCoordinate coordinate{.xChunks = 6, .yChunks = 0, .zChunks = 9};
    const auto first = std::make_unique<Protocol::Chunk>();
    const auto second = std::make_unique<Protocol::Chunk>();
    generator.GenerateChunk(coordinate, *first);
    generator.GenerateChunk(coordinate, *second);
    Assert::IsTrue(std::ranges::equal(first->Voxels(), second->Voxels()));
    Assert::IsTrue(first->SolidCount() > 0);
    // A second generator from the same seeds agrees too: the layer depends on the seeds alone.
    const Simulation::Generator again(MapDesc(11, 12));
    const auto third = std::make_unique<Protocol::Chunk>();
    again.GenerateChunk(coordinate, *third);
    Assert::IsTrue(std::ranges::equal(first->Voxels(), third->Voxels()));
  }

  TEST_METHOD(GivesTheSameWorldInAnyOrder)
  {
    const Simulation::Generator generator(MapDesc(21, 22));
    Assert::IsTrue(ChunkHashes(generator, false) == ChunkHashes(generator, true));
  }

  TEST_METHOD(MakesEverythingInputThreeAsksFor)
  {
    const Simulation::Generator generator(MapDesc(31, 32));
    const Map map(generator);
    std::array<std::size_t, 256> counts{};
    for (const std::uint8_t index : map.voxels)
    {
      ++counts.at(index);
    }
    for (const std::uint8_t index : {GRASS, EARTH, COMMON_ROCK, BUILDING_STONE, BEDROCK, TIMBER, WATER})
    {
      Assert::IsTrue(counts.at(index) > 0);
    }
    Assert::IsTrue(counts.at(LEAVES_A) + counts.at(LEAVES_B) > 0);
    // Building stone is sparse: under a fortieth of the ground's top.
    Assert::IsTrue(counts.at(BUILDING_STONE) < counts.at(GRASS) / 10);

    // Thin soil over rock, deep earth on bedrock, a ford of one voxel of water and the river's two.
    bool thinSoil = false;
    bool deepEarth = false;
    bool ford = false;
    bool deepWater = false;
    for (std::int32_t z = 0; z < map.depth; ++z)
    {
      for (std::int32_t x = 0; x < map.width; ++x)
      {
        const std::int32_t surface = generator.SurfaceAt(x, z);
        const std::uint8_t top = map.At(x, surface - 1, z);
        const std::uint8_t below = map.At(x, surface - 2, z);
        thinSoil = thinSoil || ((top == GRASS || top == EARTH) && below == COMMON_ROCK);
        deepEarth = deepEarth ||
                    (below == EARTH && surface >= 6 &&
                     std::ranges::none_of(std::views::iota(0, surface), [&](std::int32_t _y) { return map.At(x, _y, z) == COMMON_ROCK; }));
        const bool one = map.At(x, surface, z) == WATER && map.At(x, surface + 1, z) != WATER;
        const bool two = map.At(x, surface, z) == WATER && map.At(x, surface + 1, z) == WATER;
        ford = ford || one;
        deepWater = deepWater || two;
      }
    }
    Assert::IsTrue(thinSoil);
    Assert::IsTrue(deepEarth);
    Assert::IsTrue(ford);
    Assert::IsTrue(deepWater);
  }

  TEST_METHOD(GoldenHashes)
  {
    for (const Golden& golden : GOLDEN)
    {
      const Simulation::Generator generator(MapDesc(golden.worldSeed, golden.resourceSeed));
      std::set<std::uint64_t> keys;
      for (std::int32_t cy = 0; cy < generator.HeightChunks(); ++cy)
      {
        for (std::int32_t cz = 0; cz < generator.DepthChunks(); ++cz)
        {
          for (std::int32_t cx = 0; cx < generator.WidthChunks(); ++cx)
          {
            keys.insert(Protocol::PackChunkKey({.xChunks = cx, .yChunks = cy, .zChunks = cz}));
          }
        }
      }
      std::uint64_t hash = Core::FNV_OFFSET_BASIS;
      for (const std::uint64_t key : keys)
      {
        const auto chunk = std::make_unique<Protocol::Chunk>();
        generator.GenerateChunk(Protocol::UnpackChunkKey(key), *chunk);
        hash = Core::Fnv1a64(std::as_bytes(chunk->Voxels()), hash);
      }
      Assert::AreEqual(golden.mapHash, hash);
    }
  }

  TEST_METHOD(SlopeInvariant)
  {
    // No column whose top voxel is earth or grass stands more than a voxel above a four-neighbour's
    // top, which is when slump (M2-E6) would move it (ADR-003, *Consequences*).
    for (const Golden& golden : GOLDEN)
    {
      const Map map(Simulation::Generator(MapDesc(golden.worldSeed, golden.resourceSeed)));
      std::vector<std::int32_t> tops(static_cast<std::size_t>(map.width) * static_cast<std::size_t>(map.depth));
      for (std::int32_t z = 0; z < map.depth; ++z)
      {
        for (std::int32_t x = 0; x < map.width; ++x)
        {
          tops[static_cast<std::size_t>(z) * static_cast<std::size_t>(map.width) + static_cast<std::size_t>(x)] = map.Top(x, z);
        }
      }
      const auto top = [&](std::int32_t _x, std::int32_t _z)
      { return tops[static_cast<std::size_t>(_z) * static_cast<std::size_t>(map.width) + static_cast<std::size_t>(_x)]; };
      constexpr std::array<std::array<std::int32_t, 2>, 4> SIDES = {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
      std::size_t earthColumns = 0;
      for (std::int32_t z = 0; z < map.depth; ++z)
      {
        for (std::int32_t x = 0; x < map.width; ++x)
        {
          const std::uint8_t material = map.At(x, top(x, z), z);
          if (material != GRASS && material != EARTH)
          {
            continue;
          }
          ++earthColumns;
          for (const auto& [dx, dz] : SIDES)
          {
            const std::int32_t nx = x + dx;
            const std::int32_t nz = z + dz;
            if (nx >= 0 && nx < map.width && nz >= 0 && nz < map.depth)
            {
              Assert::IsTrue(top(x, z) <= top(nx, nz) + 1);
            }
          }
        }
      }
      Assert::IsTrue(earthColumns > 0);
    }
  }

  TEST_METHOD(KeepsTheSurfaceWithinTheRelief)
  {
    const Simulation::Generator generator(MapDesc(41, 42));
    for (std::int32_t z = 0; z < Simulation::Generator::MAP_DEPTH_VOXELS; ++z)
    {
      for (std::int32_t x = 0; x < Simulation::Generator::MAP_WIDTH_VOXELS; ++x)
      {
        const std::int32_t surface = generator.SurfaceAt(x, z);
        Assert::IsTrue(surface >= Simulation::Generator::LOWEST_SURFACE_VOXELS && surface <= Simulation::Generator::HIGHEST_SURFACE_VOXELS);
      }
    }
    // Nothing is generated in the upper chunk of the 64 m map's two.
    const auto sky = std::make_unique<Protocol::Chunk>();
    generator.GenerateChunk({.xChunks = 3, .yChunks = 1, .zChunks = 3}, *sky);
    Assert::AreEqual(std::size_t{0}, sky->SolidCount());
  }
};

} // namespace SimulationTests
