#pragma once

#include "ChunkCoordinate.h"
#include "StateUpdate.h"

#include <cstdint>

namespace Simulation
{

class ServerData;

/// The _shot-th aim, from 0, of a template's fixed pattern on _structure from a trebuchet at _standVoxels (M4-F5;
/// ADR-028, decision 5), which the scripted attacker fires and the control's calibration counts. The pattern fires at the
/// template's critical part, or at every voxel of a template with none: rows across the part's face nearest the stand,
/// each from its middle outwards, a face at a time and then the next layer in. The rows run bottom-up, which undercuts a
/// part so that the support search brings down what stood on what the shots took, except on T1, whose frame its own walls
/// hide from low shots and which they fire top-down.
[[nodiscard]] Protocol::VoxelPosition PatternAim(const ServerData& _data, const Protocol::StructureView& _structure,
                                                 Protocol::VoxelPosition _standVoxels, std::uint32_t _shot);

} // namespace Simulation
