#pragma once

#include "ChunkStore.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace Spike
{

/// The palette entries the two sides' faction colour is drawn in, beyond the materials'
/// (RendererLook.md: red #b23a2e, blue #2f5d93).
inline constexpr std::uint8_t ATTACKER_COLOR_INDEX = 240;
inline constexpr std::uint8_t DEFENDER_COLOR_INDEX = 241;
inline constexpr std::uint32_t ATTACKER_COLOR_SRGB = 0xb23a2e;
inline constexpr std::uint32_t DEFENDER_COLOR_SRGB = 0x2f5d93;

/// A model drawn at one place: the minimum corner of its footprint after the turn, its quarter
/// turns clockwise seen from above (one turns its front, +z, towards +x), and the palette entry its
/// faction colour is drawn in, or 0 for a model that has none.
struct ModelPlacement
{
  std::string_view modelId;
  float xMeters;
  float yMeters;
  float zMeters;
  std::uint32_t quarterTurns;
  std::uint8_t sideColorIndex;
};

/// A unit that walks a circle about where the v5 scene stands it (the owner's choice of 2026-11-01).
struct Walker
{
  std::string_view modelId;
  float centerXMeters;
  float centerZMeters;
  float phaseRadians;
  std::int32_t homeGroundVoxels; // the ground's height where it stands, which bounds how far it climbs
  std::uint8_t sideColorIndex;
};

/// The key scene, "The Breach at the West Wall" (MvpPlan.md, M1 task 9): the v5 scene's layout in
/// Design/Reference/SiegePrototypeV2/, on a seeded integer height field, with GameData's templates
/// and models where they exist. It is matched by eye, not voxel for voxel.
struct TestScene
{
  std::vector<ModelPlacement> fixedModels; // the trebuchets, the mantlets and the banner
  std::vector<Walker> walkers;             // about 95 soldiers and 21 labourers
  float centerXMeters;                     // the castle's middle, where the camera looks
  float centerYMeters;
  float centerZMeters;
};

/// Writes the scene's voxels into _store, publishes nothing, and returns its models. Throws
/// std::runtime_error when a template or model it needs is not in Content.
[[nodiscard]] TestScene BuildTestScene(Simulation::ChunkStore& _store, std::uint64_t _seed);

/// Where each walker is at _frame, at 60 frames a second, standing on the ground there and facing
/// the way it walks, in whole quarter turns. Appends to _placements.
void PlaceWalkers(const Simulation::ChunkStore& _store, const TestScene& _scene, std::uint32_t _frame,
                  std::vector<ModelPlacement>& _placements);

/// The scene's scripted edits, stepped by frame at 60 a second for the measured path's 60 s, then
/// stopped (the owner's choices of 2026-11-01): quarrying at the four-faction peak the Skirmish
/// tempo implies, and one section of the west curtain collapsing at 10 s.
class SceneEdits
{
public:
  static constexpr std::uint32_t FRAMES_PER_SECOND = 60;
  /// 94,000 voxel edits a second at 10 cm (the design review, F-51) are 94 at 1 m.
  static constexpr std::uint32_t QUARRY_VOXELS_PER_SECOND = 94;
  static constexpr std::uint32_t COLLAPSE_FRAME = 10 * FRAMES_PER_SECOND;
  static constexpr std::uint32_t LAST_FRAME = 60 * FRAMES_PER_SECOND;

  /// Reads the quarry's surface from _store, which BuildTestScene has filled.
  SceneEdits(const Simulation::ChunkStore& _store, std::uint64_t _seed);

  /// Makes frame _frame's edits in _store and returns how many voxels changed.
  std::uint32_t Apply(Simulation::ChunkStore& _store, std::uint32_t _frame);

private:
  struct Column
  {
    std::int32_t xVoxels;
    std::int32_t zVoxels;
    std::int32_t topVoxels; // the height of its highest solid voxel
  };

  std::uint32_t Quarry(Simulation::ChunkStore& _store, std::uint32_t _voxels);
  std::uint32_t Collapse(Simulation::ChunkStore& _store) const;

  std::vector<Column> m_quarry; // in the seeded order that breaks ties between equal heights
  std::uint64_t m_seed;
};

} // namespace Spike
