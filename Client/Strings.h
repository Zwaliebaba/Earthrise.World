#pragma once

#include <array>
#include <string_view>

namespace Client
{

// Every string the client can draw (ADR-019, decision 4), in one table, so that a test can check that Earthrise Mason
// has a glyph for every code point of every one. A string with figures in it is a std::format string: its {} take the
// figures, which are digits, and the test checks the text around them.

/// The frame's figures, drawn in the bottom-left corner (M3-E1).
inline constexpr std::string_view FIGURES = "VOXEL PASS {:.2f} MS · TICK {} · {}X";

// The top bar (M3-E2): the resources, the objective and the camera buttons. Hud.md's zoom-out button is a minus sign,
// which the font lacks, so it is drawn as the hyphen, the key that zooms out.
inline constexpr std::string_view TIME_KEY = "TIME";
inline constexpr std::string_view TIME_VALUE = "{}:{:02}";
inline constexpr std::string_view SHOTS_KEY = "SHOTS";
inline constexpr std::string_view SHOTS_VALUE = "{}";
inline constexpr std::string_view BREACH_OBJECTIVE_TITLE = "BREACH W2";
inline constexpr std::string_view BREACH_OBJECTIVE_DETAIL = "OPEN IT TO FOOT";
inline constexpr std::string_view TURN_LEFT = "↺";
inline constexpr std::string_view TURN_RIGHT = "↻";
inline constexpr std::string_view ZOOM_OUT = "-";
inline constexpr std::string_view ZOOM_IN = "+";

// The objective line names the selection (M3-E3), with FIRE's key, or RELOADING in off text (M3-E4).
inline constexpr std::string_view TREBUCHET_NAME = "TREBUCHET";
inline constexpr std::string_view FIRE_READY_DETAIL = "F FIRE";
inline constexpr std::string_view RELOADING_DETAIL = "RELOADING";

// The mode banner and the alerts (M3-E2).
inline constexpr std::string_view CANCEL_ESC = "CANCEL · ESC";
inline constexpr std::string_view JUMP = "JUMP";
inline constexpr std::string_view FIRE_BANNER = "FIRE · RIGHT-CLICK A TARGET";

// The attack cursor's four readings (Hud.md §8; M3-E4), and what it shows while the first is on its way.
inline constexpr std::string_view AIM_IN_RANGE = "IN RANGE · {} m";
inline constexpr std::string_view AIM_BLOCKED = "BLOCKED · {} m";
inline constexpr std::string_view AIM_TOO_CLOSE = "TOO CLOSE · {} m";
inline constexpr std::string_view AIM_OUT_OF_RANGE = "OUT OF RANGE · {} m";
inline constexpr std::string_view AIM_WAITING = "…";

/// Every string above, for the glyph coverage test.
inline constexpr auto ALL_STRINGS = std::to_array<std::string_view>(
  {FIGURES,    TIME_KEY,   TIME_VALUE,  SHOTS_KEY,    SHOTS_VALUE,    BREACH_OBJECTIVE_TITLE, BREACH_OBJECTIVE_DETAIL,
   TURN_LEFT,  TURN_RIGHT, ZOOM_OUT,    ZOOM_IN,      TREBUCHET_NAME, FIRE_READY_DETAIL,      RELOADING_DETAIL,
   CANCEL_ESC, JUMP,       FIRE_BANNER, AIM_IN_RANGE, AIM_BLOCKED,    AIM_TOO_CLOSE,          AIM_OUT_OF_RANGE,
   AIM_WAITING});

} // namespace Client
