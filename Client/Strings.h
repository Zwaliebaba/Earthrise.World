#pragma once

#include <array>
#include <string_view>

namespace Client
{

// Every string the client can draw (ADR-019, decision 4), in one table, so that a test can check that Earthrise Mason
// has a glyph for every code point of every one. A string with figures in it is a std::format string: its {} take the
// figures, which are digits, and the test checks the text around them.

/// The frame's figures, drawn in the top-left corner while the lab has no HUD of its own (M3-E1).
inline constexpr std::string_view FIGURES = "VOXEL PASS {:.2f} MS · TICK {} · {}X";

/// Every string above, for the glyph coverage test.
inline constexpr std::array<std::string_view, 1> ALL_STRINGS = {FIGURES};

} // namespace Client
