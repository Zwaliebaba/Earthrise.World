#pragma once

#include <cstdint>

namespace Protocol
{

// The kinds a state update's views carry (ADR-020, decision 3). The simulation decides by them and
// the client draws by them, so they live here, and Simulation names them through using-declarations.

/// How a unit moves: by its climb, headroom, width and fords (ADR-015, decision 2).
enum class NavigationClass : std::uint8_t
{
  Foot,
  Cavalry,
  Machines
};

enum class EngineKind : std::uint8_t
{
  Trebuchet
};

/// Which figure a unit is, and so which model the client draws (MvpAssets.md, U1 to U5 and S2).
enum class UnitKind : std::uint8_t
{
  Laborer,
  Spearman,
  Archer,
  Outrider,
  ManAtArms,
  Sapper
};

} // namespace Protocol
