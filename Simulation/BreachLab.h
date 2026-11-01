#pragma once

#include "ServerData.h"
#include "World.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Simulation
{

// The breach lab's items (M3-B7; GameDesign.md, M3's values, item 2). Built in C++ as the breach run
// is, so the tests, the bench and Earthrise run the same thing. Each item is a fresh world at the M2
// breach run's site: prior damage from scripted shots, then the question, WILL W2 BE OPEN TO FOOT
// AFTER N SHOTS AT THE MARK?, then the N shots, all at the mark. The key is the simulation's own:
// whether foot can reach the courtyard once the world is quiet again.

struct LabItem
{
  std::string id;                                      // P0 the practice item, then L01 to L16
  std::vector<Protocol::VoxelPosition> priorAimVoxels; // shots before the question, each fired on the first tick the trebuchet is loaded
  Protocol::VoxelPosition markVoxels;                  // where the asked shots are aimed
  std::uint32_t shotsAsked;                            // N, 1 to 5
};

/// The site's entities, and where the squad starts and is sent.
struct LabSetup
{
  Protocol::EntityId engine;
  Protocol::EntityId squad;
  Protocol::EntityId target; // W2
  Protocol::VoxelPosition squadStartVoxels;
  Protocol::VoxelPosition goalVoxels;
};

struct LabOutcome
{
  bool open;               // foot can reach the courtyard: the key
  std::uint32_t ticks;     // the world's, at the end
  std::uint64_t stateHash; // ADR-014, decision 5, at the end
  bool timedOut;           // a phase reached LAB_MAX_TICKS_PER_PHASE before it was quiet
};

/// How long a phase may run before it is given up: 200 s of game time.
inline constexpr std::uint32_t LAB_MAX_TICKS_PER_PHASE = 4000;

/// The tick the site's squad is ordered into the courtyard on, once tick 0's rebuild has put the
/// enclosure into navigation.
inline constexpr std::uint32_t LAB_ORDER_TICK = 1;

/// P0 first, then L01 to L16.
[[nodiscard]] std::span<const LabItem> LabItems();

/// Core::Fnv1a64 over every item's fields, in order: names the item set in the logs.
[[nodiscard]] std::uint64_t LabItemsHash();

/// The breach run's map and seeds.
[[nodiscard]] World::Desc LabDesc(const ServerData& _data, std::uint64_t _buildId);

/// The breach run's site on a world not yet ticked: the footprint levelled; the enclosure as
/// structures W1 to W10 anticlockwise seen from above from the south-west corner, south side first:
/// W1 to W3 the south side west to east, W4 and W5 the east side south to north, W6 to W8 the north
/// side east to west, and W9 and W10 the west side north to south, with W2, the middle south
/// section, the target; the trebuchet 40 m south of W2 facing it; and six men-at-arms 10 m south of
/// the wall, which LabShots orders into the courtyard on LAB_ORDER_TICK. All by edits, between ticks.
[[nodiscard]] LabSetup BuildLabSite(World& _world);

/// Fires shots from the site's trebuchet, one on the first tick it is loaded, then waits for the
/// world to go quiet with the trebuchet loaded. Driven a tick at a time, so that Earthrise draws
/// between ticks and the headless run reaches the same ticks. Whichever LabShots ticks the world
/// through LAB_ORDER_TICK also gives the site's squad its order, as the breach run's script does.
class LabShots
{
public:
  LabShots(const LabSetup& _setup, std::span<const Protocol::VoxelPosition> _aimVoxels);

  /// Submits what this tick needs and ticks _world once. Returns Done(), and does nothing once done.
  bool Advance(World& _world);

  /// Every shot fired, and the world quiet with the trebuchet loaded, or LAB_MAX_TICKS_PER_PHASE
  /// ticks passed.
  [[nodiscard]] bool Done() const noexcept;
  [[nodiscard]] bool TimedOut() const noexcept;

private:
  LabSetup m_setup;
  std::vector<Protocol::VoxelPosition> m_aimVoxels;
  std::size_t m_fired = 0;
  std::uint32_t m_ticks = 0;
  bool m_done = false;
  bool m_timedOut = false;
};

/// Navigation's answer: foot can reach the goal from the squad's start (ADR-015, *Consequences*).
[[nodiscard]] bool CourtyardOpen(const World& _world, const LabSetup& _setup);

/// The headless run of an item on a built site: the prior shots, then the asked shots, then the
/// verdict and the state hash.
[[nodiscard]] LabOutcome ResolveLabItem(World& _world, const LabSetup& _setup, const LabItem& _item);

} // namespace Simulation
