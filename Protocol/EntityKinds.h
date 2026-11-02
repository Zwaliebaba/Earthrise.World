#pragma once

#include <cstddef>
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
  Trebuchet,
  Ram
};

/// Which figure a unit is, and so which model the client draws (MvpAssets.md, U1 to U5, S1 to S3): M3's six, and the
/// engines' bodies, so that one table of statistics serves every unit (ADR-023, decision 1).
enum class UnitKind : std::uint8_t
{
  Laborer,
  Spearman,
  Archer,
  Outrider,
  ManAtArms,
  Sapper,
  Ram,
  Trebuchet
};

inline constexpr std::size_t UNIT_KIND_COUNT = 8;

/// A trebuchet's state (MvpPlan.md, M4, input 17). A ram stays Packed: it moves and never deploys.
enum class EngineState : std::uint8_t
{
  Packed,
  Deploying,
  Deployed,
  Packing
};

/// What armor is counted against (ADR-023, decision 2).
enum class DamageType : std::uint8_t
{
  Melee,
  Pierce,
  Crush
};

enum class Side : std::uint8_t
{
  Attacker,
  Defender
};

/// What a squad was last ordered to do (ADR-023, decision 4; ADR-024).
enum class SquadOrder : std::uint8_t
{
  Idle,
  Move,
  Attack,
  Storm,
  Hold,
  Trench,
  Repair,
  Garrisoned
};

enum class ShotKind : std::uint8_t
{
  Stone,
  Arrow
};

/// Which of the Siege trial's two versions a scenario plays (ADR-028).
enum class TrialVersion : std::uint8_t
{
  Voxel,
  Control
};

enum class TrialOutcome : std::uint8_t
{
  Running,
  Won,
  LostToClock,
  LostArmy
};

/// Why the simulation refused a command, which it tells the player who sent it (ADR-023, decision 10).
enum class Refusal : std::uint8_t
{
  NotOwned,
  NotEnoughStone,
  NotEnoughTimber,
  Reloading,
  NotDeployed,
  NotPacked,
  Full,
  NotWorking,
  Invalid
};

} // namespace Protocol
