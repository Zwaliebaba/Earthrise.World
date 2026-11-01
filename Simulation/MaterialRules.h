#pragma once

#include <array>
#include <cstdint>

namespace Simulation
{

/// How a material takes part in support (ADR-004, decision 2).
enum class Support : std::uint8_t
{
  None,    // empty, water and what only models hold: carries nothing
  Carries, // earth and rubble: carries support and is never judged by span; slump rules it
  Spans,   // judged: it stands while its support distance is within its span
  Anchors  // bedrock: an anchor wherever it lies
};

/// A palette index's behaviour (GameData/Server/Palette.json), numbered in this order in ServerData's
/// MATL section (ADR-016). None is an index that never reaches the world: one that does not exist,
/// one only models use, and a decoration, which the bake writes as its host (M2's values, item 7).
enum class Behavior : std::uint8_t
{
  None,
  Earth,
  CommonRock,
  BuildingStone,
  Bedrock,
  Timber,
  Leaves,
  Water,
  StoneRubble,
  WasteRubble
};

/// What one palette index does for support, for walking and when it falls: ServerData's MATL record.
struct MaterialRule
{
  Behavior behavior;
  Support support;
  std::uint8_t spanVoxels;  // for Spans: the most horizontal steps it may be from an anchor
  bool walkableGround;      // a unit may stand on it: terrain and rubble, never a structure (GameDesign.md)
  bool liquid;              // water: a unit may stand in one voxel of it, a ford, but not in two
  std::uint8_t rubbleIndex; // what a voxel of it becomes when it falls or is crushed; 0 when it leaves no rubble
};

using MaterialRules = std::array<MaterialRule, 256>;

/// _rules with every span multiplied by _voxelsPerMeter: 2 for the 0.5 m world of ADR-006's
/// comparison, whose spans count its own voxels. Throws std::invalid_argument for 0 or more than 4
/// voxels a meter, or a span that no longer fits.
[[nodiscard]] MaterialRules ScaleSpans(const MaterialRules& _rules, std::uint32_t _voxelsPerMeter);

/// The longest span of any material in _rules, which bounds the region an edit can affect.
[[nodiscard]] std::uint32_t LongestSpanVoxels(const MaterialRules& _rules) noexcept;

} // namespace Simulation
