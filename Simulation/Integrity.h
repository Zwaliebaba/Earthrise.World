#pragma once

#include "EntityId.h"
#include "Query.h"
#include "World.h"

namespace Simulation
{

/// A section's integrity reading (M3-B5; ADR-021, decision 4): the breach-prediction paper test's
/// support readout (Design/Review/PaperTests/BreachPrediction.md, *The readouts*), read from the
/// world and the resolver's own search, never from a copy of its rule (ADR-004, decision 5).
///
/// The section is read in its own frame: columns along its template's width, layers through its
/// depth and rows up its height, turned as the structure is. Every reading is over the whole
/// thickness:
///   - **The widest hole:** of the openings, 4-connected regions of (column, row) cells below the
///     top row where every layer is empty, the widest by its longest run of columns in a row, the
///     lower first on a tie. Its fall width is the width at which the support search finds stone of
///     the section unsupported, with the hole widened one column at a time on alternate sides,
///     the higher column first, on a scratch copy of the chunks; 0 when none falls within the section.
///   - **The lowest opening:** the one with the lowest floor, the wider first on a tie: its floor's
///     row and what lies under it (ground, rubble or stone), how many rows stand clear above its floor
///     to the section's top, and whether foot can stand on its floor now, through every layer.
///   - **The gap under the foundation:** the longest run of columns with nothing under the lowest
///     course through the whole thickness, and the run at which the support search drops stone of
///     the section, widened as the hole is.
///   - **The weak point:** the first reading past its mark (a route, a hole that falls, a gap that
///     falls), else the one nearest it; an empty box, min above max, when none is near.
///
/// Pure: it reads _world and changes nothing, and gives the same reading for the same world. It runs
/// at most eight support searches, which it counts into _supportSearches when that is not null.
/// Throws std::out_of_range for a structure the world does not hold.
[[nodiscard]] Protocol::IntegrityReading ReadIntegrity(const World& _world, Protocol::EntityId _structure,
                                                       std::uint32_t* _supportSearches = nullptr);

} // namespace Simulation
