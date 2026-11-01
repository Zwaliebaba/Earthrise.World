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

// The collapse alerts (Hud.md §10; M3-F1), named for the collapse's structure, or A WALL for one with none.
inline constexpr std::string_view WILL_FALL_ALERT = "{} WILL FALL IN {} S";
inline constexpr std::string_view HAS_FALLEN_ALERT = "{} HAS FALLEN";
inline constexpr std::string_view UNNAMED_WALL = "A WALL";

// The integrity view's rows (ADR-021, decision 4; M3-F2), in the paper test's words
// (Design/Review/PaperTests/BreachPrediction.md, *The readouts*), each a KEY and its value.
inline constexpr std::string_view HOLE_NONE = "HOLE none";
inline constexpr std::string_view HOLE_FALLS_AT = "HOLE {} m · falls at {} m";
inline constexpr std::string_view HOLE_HOLDS = "HOLE {} m · holds";
inline constexpr std::string_view OPENING_NONE = "OPENING none";
inline constexpr std::string_view OPENING = "OPENING {} {} m · {}"; // the floor, the clear height and the verdict
inline constexpr std::string_view FLOOR_GROUND = "ground";
inline constexpr std::string_view FLOOR_RUBBLE = "rubble";
inline constexpr std::string_view FLOOR_STONE = "stone";
inline constexpr std::string_view FLOOR_UP = "{} {} m up";
inline constexpr std::string_view ROUTE_NOW = "a route now";
inline constexpr std::string_view NO_FOOTING = "no footing";
inline constexpr std::string_view TOO_LOW = "too low";
inline constexpr std::string_view NO_ROUTE = "no route";
inline constexpr std::string_view GAP_NONE = "GAP none";
inline constexpr std::string_view GAP_FALLS_AT = "GAP {} m · falls at {} m";
inline constexpr std::string_view GAP_HOLDS = "GAP {} m · holds";
inline constexpr std::string_view GROUND_UNDERMINED = "GROUND {} · can be undermined";
inline constexpr std::string_view GROUND_SAFE = "GROUND {} · cannot be undermined";
inline constexpr std::string_view GROUND_EARTH = "earth";
inline constexpr std::string_view GROUND_ROCK = "rock";
inline constexpr std::string_view GROUND_BEDROCK = "bedrock";
inline constexpr std::string_view FALLS = "FALLS";

/// The passability view's alert (Hud.md §10; M3-F3): the lab's target is W2.
inline constexpr std::string_view BREACH_OPEN_ALERT = "BREACH OPEN AT W2";

// The breach lab (M3-G1; GameDesign.md, M3's values, item 2): its banners, its answers and its objective.
inline constexpr std::string_view FREE_FIRE_BANNER = "FREE FIRE · {}:{:02} LEFT";
inline constexpr std::string_view WATCH_BANNER = "WATCH";
inline constexpr std::string_view QUESTION_BANNER = "WILL W2 BE OPEN TO FOOT AFTER {} SHOTS AT THE MARK?";
inline constexpr std::string_view QUESTION_ONE_SHOT_BANNER = "WILL W2 BE OPEN TO FOOT AFTER 1 SHOT AT THE MARK?";
inline constexpr std::string_view OPEN_BUTTON = "OPEN · O";
inline constexpr std::string_view SHUT_BUTTON = "SHUT · H"; // not S, which pans (Hud.md §7)
inline constexpr std::string_view RESULT_OPEN_BANNER = "W2 IS OPEN · SPACE FOR THE NEXT";
inline constexpr std::string_view RESULT_SHUT_BANNER = "W2 IS SHUT · SPACE FOR THE NEXT";
inline constexpr std::string_view FINISHED_BANNER = "THE LAB IS DONE · THANK YOU";
inline constexpr std::string_view PRACTICE_OBJECTIVE = "PRACTICE";
inline constexpr std::string_view ITEM_OBJECTIVE = "ITEM {} OF {}";
inline constexpr std::string_view FINISHED_OBJECTIVE = "DONE";

/// Every string above, for the glyph coverage test.
inline constexpr auto ALL_STRINGS = std::to_array<std::string_view>({FIGURES,
                                                                     TIME_KEY,
                                                                     TIME_VALUE,
                                                                     SHOTS_KEY,
                                                                     SHOTS_VALUE,
                                                                     BREACH_OBJECTIVE_TITLE,
                                                                     BREACH_OBJECTIVE_DETAIL,
                                                                     TURN_LEFT,
                                                                     TURN_RIGHT,
                                                                     ZOOM_OUT,
                                                                     ZOOM_IN,
                                                                     TREBUCHET_NAME,
                                                                     FIRE_READY_DETAIL,
                                                                     RELOADING_DETAIL,
                                                                     CANCEL_ESC,
                                                                     JUMP,
                                                                     FIRE_BANNER,
                                                                     AIM_IN_RANGE,
                                                                     AIM_BLOCKED,
                                                                     AIM_TOO_CLOSE,
                                                                     AIM_OUT_OF_RANGE,
                                                                     AIM_WAITING,
                                                                     WILL_FALL_ALERT,
                                                                     HAS_FALLEN_ALERT,
                                                                     UNNAMED_WALL,
                                                                     HOLE_NONE,
                                                                     HOLE_FALLS_AT,
                                                                     HOLE_HOLDS,
                                                                     OPENING_NONE,
                                                                     OPENING,
                                                                     FLOOR_GROUND,
                                                                     FLOOR_RUBBLE,
                                                                     FLOOR_STONE,
                                                                     FLOOR_UP,
                                                                     ROUTE_NOW,
                                                                     NO_FOOTING,
                                                                     TOO_LOW,
                                                                     NO_ROUTE,
                                                                     GAP_NONE,
                                                                     GAP_FALLS_AT,
                                                                     GAP_HOLDS,
                                                                     GROUND_UNDERMINED,
                                                                     GROUND_SAFE,
                                                                     GROUND_EARTH,
                                                                     GROUND_ROCK,
                                                                     GROUND_BEDROCK,
                                                                     FALLS,
                                                                     BREACH_OPEN_ALERT,
                                                                     FREE_FIRE_BANNER,
                                                                     WATCH_BANNER,
                                                                     QUESTION_BANNER,
                                                                     QUESTION_ONE_SHOT_BANNER,
                                                                     OPEN_BUTTON,
                                                                     SHUT_BUTTON,
                                                                     RESULT_OPEN_BANNER,
                                                                     RESULT_SHUT_BANNER,
                                                                     FINISHED_BANNER,
                                                                     PRACTICE_OBJECTIVE,
                                                                     ITEM_OBJECTIVE,
                                                                     FINISHED_OBJECTIVE});

} // namespace Client
