#pragma once

#include "HudModel.h"
#include "Selection.h"
#include "StateUpdate.h"

namespace Client
{

/// The selection panel's model (Hud.md §5; M4-I2), from the selection and the update alone: nothing; one squad,
/// laborer, engine or structure with its portrait, name, owner, figures, hit points and up to three stat rows; or,
/// for several squads, the title and one tile per type in Hud.md's order. An enemy's garrison reads ? since only its
/// owner's update counts it.
[[nodiscard]] HudPanel BuildPanel(const Selection& _selection, const Protocol::StateUpdate& _state);

} // namespace Client
