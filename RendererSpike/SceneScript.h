#pragma once

#include "ChunkStore.h"
#include "TestWorld.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace RendererSpike
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

/// The key scene, "The Breach at the West Wall" (MvpPlan.md, M1 task 9): Simulation's test world,
/// the v5 scene's layout in Design/Reference/SiegePrototypeV2/ on a seeded integer height field,
/// with GameData's templates and models where they exist, and the units and models only the
/// client draws. It is matched by eye, not voxel for voxel.
struct TestScene
{
  Simulation::TestWorld world;
  std::vector<ModelPlacement> fixedModels; // the trebuchets, the mantlets and the banner
  std::vector<Walker> walkers;             // about 95 soldiers and 21 labourers
  float centerXMeters;                     // the castle's middle, where the camera looks
  float centerYMeters;
  float centerZMeters;
};

/// Writes the scene's world into _store from _templates, the server bake's (ServerData::Templates),
/// publishes nothing, and returns it with its models. Throws std::runtime_error when a template it
/// needs is not in _templates or a model is not in Content.
[[nodiscard]] TestScene BuildTestScene(Simulation::ChunkStore& _store, std::uint64_t _seed, const Simulation::TemplateLookup& _templates);

/// Where each walker is at _frame, at 60 frames a second, standing on the ground there and facing
/// the way it walks, in whole quarter turns. Appends to _placements.
void PlaceWalkers(const Simulation::ChunkStore& _store, const TestScene& _scene, std::uint32_t _frame,
                  std::vector<ModelPlacement>& _placements);

} // namespace RendererSpike
