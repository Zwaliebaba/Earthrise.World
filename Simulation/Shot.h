#pragma once

#include "ChunkCoordinate.h"
#include "Fixed.h"
#include "FixedPosition.h"
#include "MaterialRules.h"
#include "Rules.h"
#include "VoxelWorld.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace Simulation
{

using Protocol::FixedPosition;

/// A trebuchet shot's arc (GameDesign.md, M2's values, item 12): it leaves shotLaunchHeightVoxels
/// above the middle of the engine's cell and comes down at exactly 45 degrees onto the middle of the
/// target voxel's top face, under gravity. With the launch h above the aim point and the aim d away
/// across the ground, that fixes the flight's T ticks by T^2 = 2(d - h) / g, its speed across the
/// ground at d / T, and its first rise at gT - d / T a tick. Every value is Core::Fixed, so the
/// arc is the same on every machine (ADR-013).
struct ShotArc
{
  FixedPosition start;
  Core::Fixed acrossXMeters; // from the start to the aim point, across the ground
  Core::Fixed acrossZMeters;
  Core::Fixed flightTicks;       // T: when it reaches the aim point
  Core::Fixed riseMetersPerTick; // its vertical speed at launch, up positive
  Core::Fixed gravityMetersPerTickSquared;
};

/// The arc from an engine standing in _engineCellVoxels to _targetVoxels, or nothing when the shot
/// cannot be fired: the aim point lies outside shotMinRangeVoxels to shotMaxRangeVoxels across the
/// ground, or too far below the launch to be reached at 45 degrees.
[[nodiscard]] std::optional<ShotArc> AimShot(Protocol::VoxelPosition _engineCellVoxels, Protocol::VoxelPosition _targetVoxels,
                                             const Rules& _rules);

/// Where the shot is _ticks after launch: the arc in closed form, so nothing accumulates.
[[nodiscard]] FixedPosition ShotPositionAt(const ShotArc& _arc, std::uint32_t _ticks);

/// Visits, in order, every voxel the segment from _from to _to passes through, by the integer 3D
/// traversal of Amanatides and Woo (1987), until _visit returns true. Returns the voxel it stopped
/// at, or nothing when it reached _to. Where the segment crosses an edge or a corner, it steps x,
/// then y, then z, so every voxel it touches is visited.
std::optional<Protocol::VoxelPosition> TraverseVoxels(const FixedPosition& _from, const FixedPosition& _to,
                                                      const std::function<bool(Protocol::VoxelPosition)>& _visit);

/// The voxels a shot's crater takes when it strikes _hit, travelling along _behindX or _behindZ, its
/// larger horizontal axis (M2-E2; GameDesign.md, M2's values, item 12). The struck voxel's behaviour
/// sets the crater's size in RULE, on the ladder of 1, 2, 4 and 7 voxels:
///   1, the voxel; 2, it and the one behind it; 4, it, the one behind it, and the one below each;
///   7, it and its six face neighbours of its own behaviour only.
/// Stone and common rock take 2, earth and every rubble 4, timber and leaves 7, and bedrock and water
/// none. No crater takes bedrock or water. _at reads a voxel's palette index. In a fixed order: the
/// struck voxel first.
[[nodiscard]] std::vector<Protocol::VoxelPosition> CraterVoxels(Protocol::VoxelPosition _hit, std::int32_t _behindX, std::int32_t _behindZ,
                                                                const std::function<std::uint8_t(Protocol::VoxelPosition)>& _at,
                                                                const MaterialRules& _materials, const Rules& _rules);

/// The crater a behaviour takes, in voxels, from RULE.
[[nodiscard]] std::int32_t CraterSizeVoxels(Behavior _behavior, const Rules& _rules) noexcept;

/// One tick of a shot's flight: the first solid voxel the step from _ticksFlown to the next tick
/// crosses, if any, and, missing, whether the shot has left a world _widthVoxels by _depthVoxels,
/// below the ground or past an edge (M2-E1).
struct ShotStep
{
  std::optional<Protocol::VoxelPosition> hit;
  bool left;
};
[[nodiscard]] ShotStep StepShot(const ShotArc& _arc, std::uint32_t _ticksFlown, const VoxelWorld& _world, std::int32_t _widthVoxels,
                                std::int32_t _depthVoxels);

/// The first solid voxel a shot on _arc meets, stepping as the shot flies, or nothing when it leaves
/// the world (M3-B4). Step 4 and the aim check both step through StepShot, so they cannot disagree.
[[nodiscard]] std::optional<Protocol::VoxelPosition> TraceShot(const ShotArc& _arc, const VoxelWorld& _world, std::int32_t _widthVoxels,
                                                               std::int32_t _depthVoxels);

/// The voxel holding a point: each coordinate floored.
[[nodiscard]] Protocol::VoxelPosition VoxelHolding(const FixedPosition& _point) noexcept;

} // namespace Simulation
