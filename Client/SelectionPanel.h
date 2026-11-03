#pragma once

#include "HudModel.h"
#include "Query.h"
#include "Selection.h"
#include "StateUpdate.h"

namespace Client
{

/// The selection panel's model (Hud.md §5; M4-I2), from the selection and the update alone: nothing; one squad,
/// laborer, engine or structure with its portrait, name, owner, figures, hit points and up to three stat rows; or,
/// for several squads, the title and one tile per type in Hud.md's order. An enemy's garrison reads ? since only its
/// owner's update counts it. A wall section's second row is input 22's: OPEN yes once a route runs through it, else its
/// weak reading against its fall from _reading, the section's integrity as last read, such as WEAK HOLE 3/5 M, else
/// OPEN no (M4-I6).
[[nodiscard]] HudPanel BuildPanel(const Selection& _selection, const Protocol::StateUpdate& _state,
                                  const Protocol::IntegrityReading* _reading = nullptr);

} // namespace Client
