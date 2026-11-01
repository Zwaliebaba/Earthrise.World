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

/// What one palette index does for support and for walking.
struct MaterialRule
{
  Support support;
  std::uint8_t spanVoxels; // for Spans: the most horizontal steps it may be from an anchor
  bool walkableGround;     // a unit may stand on it: terrain and rubble, never a structure (GameDesign.md)
  bool liquid;             // water: a unit may stand in one voxel of it, a ford, but not in two
};

using MaterialRules = std::array<MaterialRule, 256>;

/// The M1 stand-in for the palette's behaviours (GameData/Server/Palette.json), until the server
/// half of the bake writes them (M2). Spans are the owner's (GameDesign.md): stone 2 m, timber 5 m.
/// Leaves span 3 m, a canopy's 2.2 m radius rounded up, and common rock spans as stone does. A
/// decoration, iron or an opening, takes timber's span, the longer of its hosts', so that a fitting
/// never falls before the timber or stone it is painted on. _voxelsPerMeter is 1 for the 1 m world
/// and 2 for the 0.5 m world of ADR-006's comparison, and multiplies every span.
[[nodiscard]] MaterialRules StandInMaterialRules(std::uint32_t _voxelsPerMeter);

/// The longest span of any material in _rules, which bounds the region an edit can affect.
[[nodiscard]] std::uint32_t LongestSpanVoxels(const MaterialRules& _rules) noexcept;

} // namespace Simulation
