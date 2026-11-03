#pragma once

#include "StateUpdate.h"

#include <cstdint>
#include <vector>

namespace Client
{

/// A texel of the fog's mask over a cell the update's player sees now; one they do not see is 0 (ADR-025, decision 8).
inline constexpr std::uint8_t SEEN_TEXEL = 255;

/// The fog's column mask from _state's visibility (ADR-025, decisions 2 and 8): one R8 texel a 1 m cell of the map,
/// _state.widthCells to a row and _state.depthCells rows, x along each row and z down them, as
/// Engine::VoxelPass::SetColumnShade reads it; SEEN_TEXEL where the cell's bit, z * widthCells + x, is set, and 0
/// where not. Empty when the update carries no visibility, as in a world without players, which has no fog.
[[nodiscard]] std::vector<std::uint8_t> ColumnMask(const Protocol::StateUpdate& _state);

} // namespace Client
