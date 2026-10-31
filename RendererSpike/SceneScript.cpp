#include "pch.h"

#include "SceneScript.h"

#include "Catalog.h"

namespace Spike
{

namespace
{

constexpr std::int32_t GROUND_EDGE_VOXELS = 96; // three chunks a side
constexpr std::int32_t BASE_HEIGHT_VOXELS = 4;
constexpr std::uint8_t GRASS = 1;
constexpr std::uint8_t EARTH = 2;
constexpr std::uint8_t BEDROCK = 5;

// A seeded integer hash (SplitMix64's finaliser), so the ground is the same on every machine (R16).
constexpr std::uint64_t Mix(std::uint64_t _value) noexcept
{
  _value += 0x9e3779b97f4a7c15ULL;
  _value = (_value ^ (_value >> 30)) * 0xbf58476d1ce4e5b9ULL;
  _value = (_value ^ (_value >> 27)) * 0x94d049bb133111ebULL;
  return _value ^ (_value >> 31);
}

// Gentle ground: 0 to 2 m above the base, smoothed over 8 m cells, in whole voxels.
std::int32_t GroundHeight(std::int32_t _xVoxels, std::int32_t _zVoxels, std::uint64_t _seed) noexcept
{
  const std::int32_t cellX = _xVoxels / 8;
  const std::int32_t cellZ = _zVoxels / 8;
  const auto bump = [_seed](std::int32_t _x, std::int32_t _z)
  { return static_cast<std::int32_t>(Mix(_seed ^ (static_cast<std::uint64_t>(_x) << 32) ^ static_cast<std::uint64_t>(_z)) % 3); };
  return BASE_HEIGHT_VOXELS + (bump(cellX, cellZ) + bump(cellX + 1, cellZ) + bump(cellX, cellZ + 1)) / 3;
}

const Content::TemplateView& Template(std::string_view _id)
{
  const Content::TemplateView* found = Content::FindTemplate(_id);
  if (found == nullptr)
  {
    throw std::runtime_error("the test scene needs a template Content does not have");
  }
  return *found;
}

// Places a template with its minimum corner at the given voxel, facing +z as it is authored.
void Place(Simulation::ChunkStore& _store, const Content::TemplateView& _template, std::int32_t _xVoxels, std::int32_t _yVoxels,
           std::int32_t _zVoxels)
{
  const Content::VoxelGrid& grid = _template.grid;
  for (std::uint32_t y = 0; y < grid.heightVoxels; ++y)
  {
    for (std::uint32_t z = 0; z < grid.depthVoxels; ++z)
    {
      for (std::uint32_t x = 0; x < grid.widthVoxels; ++x)
      {
        const std::uint8_t index = grid.At(x, y, z);
        if (index != Content::VoxelGrid::EMPTY)
        {
          _store.Set({.xVoxels = _xVoxels + static_cast<std::int32_t>(x),
                      .yVoxels = _yVoxels + static_cast<std::int32_t>(y),
                      .zVoxels = _zVoxels + static_cast<std::int32_t>(z)},
                     index);
        }
      }
    }
  }
}

} // namespace

TestScene BuildTestScene(Simulation::ChunkStore& _store, std::uint64_t _seed)
{
  for (std::int32_t z = 0; z < GROUND_EDGE_VOXELS; ++z)
  {
    for (std::int32_t x = 0; x < GROUND_EDGE_VOXELS; ++x)
    {
      const std::int32_t height = GroundHeight(x, z, _seed);
      _store.Set({.xVoxels = x, .yVoxels = 0, .zVoxels = z}, BEDROCK);
      for (std::int32_t y = 1; y < height; ++y)
      {
        _store.Set({.xVoxels = x, .yVoxels = y, .zVoxels = z}, EARTH);
      }
      _store.Set({.xVoxels = x, .yVoxels = height, .zVoxels = z}, GRASS);
    }
  }

  // A level platform for the castle, so templates sit on the ground rather than in it.
  constexpr std::int32_t FLOOR_VOXELS = BASE_HEIGHT_VOXELS + 1;
  for (std::int32_t z = 24; z < 90; ++z)
  {
    for (std::int32_t x = 16; x < 80; ++x)
    {
      for (std::int32_t y = 1; y < FLOOR_VOXELS; ++y)
      {
        _store.Set({.xVoxels = x, .yVoxels = y, .zVoxels = z}, EARTH);
      }
      _store.Set({.xVoxels = x, .yVoxels = FLOOR_VOXELS, .zVoxels = z}, GRASS);
      for (std::int32_t y = FLOOR_VOXELS + 1; y < FLOOR_VOXELS + 4; ++y)
      {
        _store.Set({.xVoxels = x, .yVoxels = y, .zVoxels = z}, 0);
      }
    }
  }

  const Content::TemplateView& wall = Template("T3");
  const Content::TemplateView& tower = Template("T4");
  const Content::TemplateView& gatehouse = Template("T5");
  const Content::TemplateView& keep = Template("T2");
  const Content::TemplateView& hall = Template("T1");
  constexpr std::int32_t BASE = FLOOR_VOXELS + 1;
  constexpr std::int32_t CURTAIN_Z = 58; // the outside is +z

  // Tower, two sections, the gatehouse, two sections, tower: the curtain faces +z, towards the units.
  std::int32_t x = 18;
  Place(_store, tower, x, BASE, CURTAIN_Z - 2);
  x += static_cast<std::int32_t>(tower.grid.widthVoxels);
  for (int section = 0; section < 2; ++section, x += static_cast<std::int32_t>(wall.grid.widthVoxels))
  {
    Place(_store, wall, x, BASE, CURTAIN_Z);
  }
  Place(_store, gatehouse, x, BASE, CURTAIN_Z - 2);
  x += static_cast<std::int32_t>(gatehouse.grid.widthVoxels);
  for (int section = 0; section < 2; ++section, x += static_cast<std::int32_t>(wall.grid.widthVoxels))
  {
    Place(_store, wall, x, BASE, CURTAIN_Z);
  }
  Place(_store, tower, x, BASE, CURTAIN_Z - 2);

  Place(_store, keep, 40, BASE, 36);
  Place(_store, hall, 22, BASE, 40);

  TestScene scene{.models = {}, .centerXMeters = 48.0F, .centerZMeters = 58.0F};
  constexpr std::array<std::string_view, 4> UNITS = {"U2", "U3", "U4", "U5"};
  for (int row = 0; row < 4; ++row)
  {
    for (int column = 0; column < 6; ++column)
    {
      scene.models.push_back({.modelId = UNITS[static_cast<std::size_t>(row)],
                              .xMeters = 30.0F + 2.0F * static_cast<float>(column),
                              .yMeters = static_cast<float>(BASE),
                              .zMeters = 66.0F + 2.0F * static_cast<float>(row)});
    }
  }
  scene.models.push_back({.modelId = "S3", .xMeters = 62.0F, .yMeters = static_cast<float>(BASE), .zMeters = 82.0F});
  return scene;
}

} // namespace Spike
