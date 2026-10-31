#pragma once

#include "ChunkStore.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace Spike
{

/// A 0.5 m model placed in the scene: the client draws it, the simulation never sees it (ADR-006).
struct ModelPlacement
{
  std::string_view modelId;
  float xMeters;
  float yMeters;
  float zMeters;
};

/// A first scene for the voxel pass (MvpPlan.md, M1 task 5): ground from a seeded integer height
/// field, a short curtain of T3 sections between two T4 towers, a T5 gatehouse in it, the T2 keep
/// behind and the T1 hall beside it, and a few units in front. The v5 layout of task 9 replaces it.
struct TestScene
{
  std::vector<ModelPlacement> models;
  float centerXMeters;
  float centerZMeters;
};

/// Writes the scene's voxels into _store, publishes nothing, and returns where its models stand.
/// Throws std::runtime_error when a template or model it needs is not in Content.
[[nodiscard]] TestScene BuildTestScene(Simulation::ChunkStore& _store, std::uint64_t _seed);

} // namespace Spike
