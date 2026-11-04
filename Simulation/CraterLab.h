#pragma once

#include "ServerData.h"
#include "World.h"

#include <cstdint>
#include <span>
#include <string>

namespace Simulation
{

// The crater items (M5-D1; MvpPlan-M5.md, D1; GameDesign.md, M5's values, item 12): can a player tell which engine made
// the damage at a mark? Each item is a fresh world at the breach lab's site, its enclosure alone, with W2 struck in its
// bottom metre by k blows of the ram or k stones of the trebuchet. The hits are the engines' own, applied by the world's
// structure rule as step 4 applies them, with no engine and no squad left on the site, so that only the damage can tell.
// The key is the engine, by construction.

enum class CraterEngine : std::uint8_t
{
  Ram,
  Trebuchet
};

struct CraterItem
{
  std::string id;            // C0 the practice item, then C1 to C8
  CraterEngine engine;       // the key
  std::int32_t offsetVoxels; // the mark: this many voxels east of W2's west end, on its outer face at its base
  std::uint32_t hits;        // k, 1 to 3: the mark and the voxels east of it, one a hit
};

/// Where an item's damage lies, for the camera and the mark.
struct CraterSite
{
  Protocol::EntityId target;          // W2
  Protocol::VoxelPosition markVoxels; // the first hit's voxel
};

/// C0 first, then C1 to C8: four by the ram and four by the trebuchet.
[[nodiscard]] std::span<const CraterItem> CraterItems();

/// Core::Fnv1a64 over every item's fields, in order: names the item set in the logs.
[[nodiscard]] std::uint64_t CraterItemsHash();

/// _item on a world from LabDesc, not yet ticked: the lab's enclosure alone, and the item's hits on W2, between ticks.
/// A stone strikes the outer face and goes on along a shot from the south, as the lab's trebuchet throws; a blow strikes
/// the outer face. Throws std::invalid_argument for a world that has ticked, or an item whose hits leave W2.
[[nodiscard]] CraterSite BuildCraterItem(World& _world, const CraterItem& _item);

} // namespace Simulation
