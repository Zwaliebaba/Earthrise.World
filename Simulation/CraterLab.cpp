#include "pch.h"

#include "CraterLab.h"

#include "BreachLab.h"
#include "ByteWriter.h"
#include "Hash.h"

namespace Simulation
{

namespace
{

/// A stone from the lab's trebuchet, south of W2, goes on northwards, along +z, past the voxel it strikes.
constexpr std::int32_t SHOT_BEHIND_Z = 1;

} // namespace

std::span<const CraterItem> CraterItems()
{
  // Four by the ram and four by the trebuchet, k from 1 to 3 and the marks spread along W2, the ram's k and the
  // trebuchet's alike in sum, so that the count of holes does not tell the engine. C0 is the practice item.
  static const std::array<CraterItem, 9> ITEMS = {{
    {.id = "C0", .engine = CraterEngine::Trebuchet, .offsetVoxels = 3, .hits = 1},
    {.id = "C1", .engine = CraterEngine::Ram, .offsetVoxels = 1, .hits = 2},
    {.id = "C2", .engine = CraterEngine::Trebuchet, .offsetVoxels = 4, .hits = 1},
    {.id = "C3", .engine = CraterEngine::Trebuchet, .offsetVoxels = 2, .hits = 3},
    {.id = "C4", .engine = CraterEngine::Ram, .offsetVoxels = 5, .hits = 1},
    {.id = "C5", .engine = CraterEngine::Ram, .offsetVoxels = 3, .hits = 3},
    {.id = "C6", .engine = CraterEngine::Trebuchet, .offsetVoxels = 1, .hits = 2},
    {.id = "C7", .engine = CraterEngine::Ram, .offsetVoxels = 4, .hits = 2},
    {.id = "C8", .engine = CraterEngine::Trebuchet, .offsetVoxels = 5, .hits = 2},
  }};
  return ITEMS;
}

std::uint64_t CraterItemsHash()
{
  // Each item as its id's length and bytes, its engine, its mark's offset and its hits.
  Core::ByteWriter writer;
  for (const CraterItem& item : CraterItems())
  {
    writer.WriteU32(static_cast<std::uint32_t>(item.id.size()));
    writer.WriteBytes(std::as_bytes(std::span(item.id)));
    writer.WriteU8(static_cast<std::uint8_t>(item.engine));
    writer.WriteI32(item.offsetVoxels);
    writer.WriteU32(item.hits);
  }
  return Core::Fnv1a64(writer.Bytes());
}

CraterSite BuildCraterItem(World& _world, const CraterItem& _item)
{
  return StrikeCraterItem(_world, BuildLabEnclosure(_world), _item);
}

CraterSite StrikeCraterItem(World& _world, Protocol::EntityId _target, const CraterItem& _item)
{
  const Protocol::EntityId target = _target;
  const Structure& wall = _world.Structures().at(target);
  const ServerTemplate* shape = _world.Data().FindTemplate(wall.templateId);
  if (shape == nullptr)
  {
    throw std::invalid_argument("W2's template is not in the server data");
  }
  const bool across = wall.quarterTurns % 2 == 1;
  const auto widthVoxels = static_cast<std::int32_t>(across ? shape->depthVoxels : shape->widthVoxels);
  if (_item.hits == 0 || _item.offsetVoxels < 0 || _item.offsetVoxels + static_cast<std::int32_t>(_item.hits) > widthVoxels)
  {
    throw std::invalid_argument("the crater item " + _item.id + "'s hits leave W2");
  }
  // The outer face is W2's south side, its least z, and its base the bottom course, which the ram's blow reaches.
  const Protocol::VoxelPosition mark{
    .xVoxels = wall.originVoxels.xVoxels + _item.offsetVoxels, .yVoxels = wall.originVoxels.yVoxels, .zVoxels = wall.originVoxels.zVoxels};
  for (std::uint32_t hit = 0; hit < _item.hits; ++hit)
  {
    const Protocol::VoxelPosition voxel{
      .xVoxels = mark.xVoxels + static_cast<std::int32_t>(hit), .yVoxels = mark.yVoxels, .zVoxels = mark.zVoxels};
    if (_item.engine == CraterEngine::Ram)
    {
      _world.StrikeAsRam(target, voxel);
    }
    else
    {
      _world.StrikeAsStone({.shot = Protocol::NO_ENTITY, .voxel = voxel, .behindX = 0, .behindZ = SHOT_BEHIND_Z});
    }
  }
  return {.target = target, .markVoxels = mark};
}

} // namespace Simulation
