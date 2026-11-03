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
inline constexpr std::string_view SIEGE_OBJECTIVE_TITLE = "TAKE THE KEEP"; // the Siege castle's (M4-B4; Hud.md §10)
inline constexpr std::string_view TURN_LEFT = "↺";
inline constexpr std::string_view TURN_RIGHT = "↻";
inline constexpr std::string_view ZOOM_OUT = "-";
inline constexpr std::string_view ZOOM_IN = "+";

// The selection's chip names it (M3-E3; M3-H3), with FIRE's key, or RELOADING in off text (M3-E4).
inline constexpr std::string_view TREBUCHET_NAME = "TREBUCHET";
inline constexpr std::string_view FIRE_READY_DETAIL = "F FIRE";
inline constexpr std::string_view RELOADING_DETAIL = "RELOADING";

// The mode banner and the alerts (M3-E2).
inline constexpr std::string_view CANCEL_ESC = "CANCEL · ESC";
inline constexpr std::string_view JUMP = "JUMP";
inline constexpr std::string_view FIRE_BANNER = "FIRE · RIGHT-CLICK A TARGET";

// The control groups' strip above the deck (Hud.md §3; M4-I1): groups 1 to 5 by their numbers, then SET.
inline constexpr std::string_view SET_GROUP = "SET";

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
// Free fire's banner's hint, until the trebuchet is first selected (M3-H3).
inline constexpr std::string_view FREE_FIRE_HINT = "CLICK THE TREBUCHET · F TO FIRE";
// What runs at ten times (M3-H2): the prior shots, SETTING UP ITEM 3 · 2 SHOTS · 10×, and the asked ones,
// 2 SHOTS AT THE MARK · 10×, the pieces joined by BANNER_SEPARATOR.
inline constexpr std::string_view SETTING_UP_ITEM = "SETTING UP ITEM {}";
inline constexpr std::string_view SETTING_UP_PRACTICE = "SETTING UP THE PRACTICE ITEM";
inline constexpr std::string_view PRIOR_SHOTS = "{} SHOTS";
inline constexpr std::string_view PRIOR_ONE_SHOT = "1 SHOT";
inline constexpr std::string_view ASKED_SHOTS = "{} SHOTS AT THE MARK";
inline constexpr std::string_view ASKED_ONE_SHOT = "1 SHOT AT THE MARK";
inline constexpr std::string_view FAST_FORWARD = "{}×";
inline constexpr std::string_view BANNER_SEPARATOR = " · ";
inline constexpr std::string_view QUESTION_BANNER = "WILL W2 BE OPEN TO FOOT AFTER {} SHOTS AT THE MARK?";
// After the question's buttons, the keys of the views, which with the camera are all a participant may use (M3-H4).
inline constexpr std::string_view VIEWS_HINT = "F1 F2 VIEWS";
// In an item, the top bar's one resource while the asked shots run, and at the result (M3-H4).
inline constexpr std::string_view ASKED_SHOTS_KEY = "SHOTS AT THE MARK";
inline constexpr std::string_view ASKED_SHOTS_VALUE = "{} OF {}";
inline constexpr std::string_view QUESTION_ONE_SHOT_BANNER = "WILL W2 BE OPEN TO FOOT AFTER 1 SHOT AT THE MARK?";
inline constexpr std::string_view OPEN_BUTTON = "OPEN · O";
inline constexpr std::string_view SHUT_BUTTON = "SHUT · H"; // not S, which pans (Hud.md §7)
inline constexpr std::string_view RESULT_OPEN_BANNER = "W2 IS OPEN · SPACE FOR THE NEXT";
inline constexpr std::string_view RESULT_SHUT_BANNER = "W2 IS SHUT · SPACE FOR THE NEXT";
inline constexpr std::string_view FINISHED_BANNER = "THE LAB IS DONE · THANK YOU";
inline constexpr std::string_view PRACTICE_OBJECTIVE = "PRACTICE";
inline constexpr std::string_view ITEM_OBJECTIVE = "ITEM {} OF {}";
inline constexpr std::string_view FINISHED_OBJECTIVE = "DONE";

// The selection panel (Hud.md §5; M4-I2): nothing, one thing, or several by type.
inline constexpr std::string_view NOTHING_SELECTED = "NOTHING SELECTED";
inline constexpr std::string_view HINT_CLICK = "CLICK A SQUAD";
inline constexpr std::string_view HINT_DRAG = "DRAG TO BOX-SELECT";
inline constexpr std::string_view HINT_GROUPS = "1–9 GROUPS";
inline constexpr std::string_view YOURS = "YOURS";
inline constexpr std::string_view ENEMY = "ENEMY";
inline constexpr std::string_view OUT_OF = "{} / {}";  // figures, hit points and a garrison
inline constexpr std::string_view UNKNOWN_COUNT = "?"; // an enemy's garrison, which only its owner's update counts
inline constexpr std::string_view SEVERAL_SQUADS = "{} SQUADS · TAB CYCLES TYPE";
inline constexpr std::string_view SPEARMEN_NAME = "SPEARMEN";
inline constexpr std::string_view ARCHERS_NAME = "ARCHERS";
inline constexpr std::string_view OUTRIDERS_NAME = "OUTRIDERS";
inline constexpr std::string_view MEN_AT_ARMS_NAME = "MEN-AT-ARMS";
inline constexpr std::string_view SAPPERS_NAME = "SAPPERS";
inline constexpr std::string_view LABORER_NAME = "LABORER";
inline constexpr std::string_view RAM_NAME = "RAM";
inline constexpr std::string_view TREB_LABEL = "TREB";
inline constexpr std::string_view RAM_LABEL = "RAM";
inline constexpr std::string_view ARMS_LABEL = "ARMS";
inline constexpr std::string_view SPEAR_LABEL = "SPEAR";
inline constexpr std::string_view ARCHER_LABEL = "ARCHER";
inline constexpr std::string_view RIDER_LABEL = "RIDER";
inline constexpr std::string_view SAPPER_LABEL = "SAPPER";
inline constexpr std::string_view LABOR_LABEL = "LABOR";
// Its stat rows, KEY and value (Hud.md §5's table; GameDesign.md, M4's values, items 5, 6 and 10).
inline constexpr std::string_view ROLE_KEY = "ROLE";
inline constexpr std::string_view ARMOR_KEY = "ARMOR";
inline constexpr std::string_view STRONG_KEY = "STRONG";
inline constexpr std::string_view WEAK_KEY = "WEAK";
inline constexpr std::string_view HEIGHT_KEY = "HEIGHT";
inline constexpr std::string_view DIGS_KEY = "DIGS";
inline constexpr std::string_view TARGET_KEY = "TARGET";
inline constexpr std::string_view STATE_KEY = "STATE";
inline constexpr std::string_view RANGE_KEY = "RANGE";
inline constexpr std::string_view SHOT_KEY = "SHOT";
inline constexpr std::string_view GARRISON_KEY = "GARRISON";
inline constexpr std::string_view PLATFORM_KEY = "PLATFORM";
inline constexpr std::string_view GATE_KEY = "GATE";
inline constexpr std::string_view TOWER_KEY = "TOWER";
inline constexpr std::string_view REPAIR_KEY = "REPAIR";
inline constexpr std::string_view TO_WIN_KEY = "TO WIN";
inline constexpr std::string_view NAME_KEY = "NAME";
inline constexpr std::string_view OPEN_KEY = "OPEN";
inline constexpr std::string_view ORDER_KEY = "ORDER";
inline constexpr std::string_view LABORER_ROLE = "Build, dig";
inline constexpr std::string_view NO_ARMOR = "None";
inline constexpr std::string_view SPEARMEN_STRONG = "Horse";
inline constexpr std::string_view SPEARMEN_WEAK = "Archers, arms";
inline constexpr std::string_view ARCHERS_HEIGHT = "+1 band, +15%";
inline constexpr std::string_view ARCHERS_WEAK = "Horse, arms";
inline constexpr std::string_view OUTRIDERS_STRONG = "Bows, engines";
inline constexpr std::string_view OUTRIDERS_WEAK = "Spear, arms";
inline constexpr std::string_view MEN_AT_ARMS_STRONG = "Spear, bows";
inline constexpr std::string_view MEN_AT_ARMS_ROLE = "Storm breach";
inline constexpr std::string_view SAPPERS_DIGS = "2× trench";
inline constexpr std::string_view SAPPERS_ARMOR = "Pierce";
inline constexpr std::string_view RAM_TARGET = "Gates, timber";
inline constexpr std::string_view RAM_ARMOR = "Arrows";
inline constexpr std::string_view STATE_DEPLOYED = "Deployed";
inline constexpr std::string_view STATE_PACKING = "Packing…";
inline constexpr std::string_view STATE_PACKED = "Packed";
inline constexpr std::string_view STATE_DEPLOYING = "Deploying…";
inline constexpr std::string_view TREBUCHET_RANGE = "12–50 m"; // GameDesign.md, M2's values, item 12
inline constexpr std::string_view TREBUCHET_SHOT = "1.5 m³";   // GameDesign.md, M4's values, item 5
inline constexpr std::string_view PERCENT_VALUE = "{}%";
inline constexpr std::string_view REPAIR_READY = "Ready";
inline constexpr std::string_view REPAIR_LOCKED = "Locked · {} s";
inline constexpr std::string_view TO_WIN_VALUE = "Hold foot {}:{:02}";
inline constexpr std::string_view OPEN_YES = "yes";
inline constexpr std::string_view OPEN_NO = "no";
inline constexpr std::string_view ORDER_MOVING = "Moving";

/// Every string above, for the glyph coverage test.
inline constexpr auto ALL_STRINGS = std::to_array<std::string_view>({FIGURES,
                                                                     TIME_KEY,
                                                                     TIME_VALUE,
                                                                     SHOTS_KEY,
                                                                     SHOTS_VALUE,
                                                                     BREACH_OBJECTIVE_TITLE,
                                                                     BREACH_OBJECTIVE_DETAIL,
                                                                     SIEGE_OBJECTIVE_TITLE,
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
                                                                     SET_GROUP,
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
                                                                     FREE_FIRE_HINT,
                                                                     SETTING_UP_ITEM,
                                                                     SETTING_UP_PRACTICE,
                                                                     PRIOR_SHOTS,
                                                                     PRIOR_ONE_SHOT,
                                                                     ASKED_SHOTS,
                                                                     ASKED_ONE_SHOT,
                                                                     FAST_FORWARD,
                                                                     BANNER_SEPARATOR,
                                                                     QUESTION_BANNER,
                                                                     VIEWS_HINT,
                                                                     ASKED_SHOTS_KEY,
                                                                     ASKED_SHOTS_VALUE,
                                                                     QUESTION_ONE_SHOT_BANNER,
                                                                     OPEN_BUTTON,
                                                                     SHUT_BUTTON,
                                                                     RESULT_OPEN_BANNER,
                                                                     RESULT_SHUT_BANNER,
                                                                     FINISHED_BANNER,
                                                                     PRACTICE_OBJECTIVE,
                                                                     ITEM_OBJECTIVE,
                                                                     FINISHED_OBJECTIVE,
                                                                     NOTHING_SELECTED,
                                                                     HINT_CLICK,
                                                                     HINT_DRAG,
                                                                     HINT_GROUPS,
                                                                     YOURS,
                                                                     ENEMY,
                                                                     OUT_OF,
                                                                     UNKNOWN_COUNT,
                                                                     SEVERAL_SQUADS,
                                                                     SPEARMEN_NAME,
                                                                     ARCHERS_NAME,
                                                                     OUTRIDERS_NAME,
                                                                     MEN_AT_ARMS_NAME,
                                                                     SAPPERS_NAME,
                                                                     LABORER_NAME,
                                                                     RAM_NAME,
                                                                     TREB_LABEL,
                                                                     RAM_LABEL,
                                                                     ARMS_LABEL,
                                                                     SPEAR_LABEL,
                                                                     ARCHER_LABEL,
                                                                     RIDER_LABEL,
                                                                     SAPPER_LABEL,
                                                                     LABOR_LABEL,
                                                                     ROLE_KEY,
                                                                     ARMOR_KEY,
                                                                     STRONG_KEY,
                                                                     WEAK_KEY,
                                                                     HEIGHT_KEY,
                                                                     DIGS_KEY,
                                                                     TARGET_KEY,
                                                                     STATE_KEY,
                                                                     RANGE_KEY,
                                                                     SHOT_KEY,
                                                                     GARRISON_KEY,
                                                                     PLATFORM_KEY,
                                                                     GATE_KEY,
                                                                     TOWER_KEY,
                                                                     REPAIR_KEY,
                                                                     TO_WIN_KEY,
                                                                     NAME_KEY,
                                                                     OPEN_KEY,
                                                                     ORDER_KEY,
                                                                     LABORER_ROLE,
                                                                     NO_ARMOR,
                                                                     SPEARMEN_STRONG,
                                                                     SPEARMEN_WEAK,
                                                                     ARCHERS_HEIGHT,
                                                                     ARCHERS_WEAK,
                                                                     OUTRIDERS_STRONG,
                                                                     OUTRIDERS_WEAK,
                                                                     MEN_AT_ARMS_STRONG,
                                                                     MEN_AT_ARMS_ROLE,
                                                                     SAPPERS_DIGS,
                                                                     SAPPERS_ARMOR,
                                                                     RAM_TARGET,
                                                                     RAM_ARMOR,
                                                                     STATE_DEPLOYED,
                                                                     STATE_PACKING,
                                                                     STATE_PACKED,
                                                                     STATE_DEPLOYING,
                                                                     TREBUCHET_RANGE,
                                                                     TREBUCHET_SHOT,
                                                                     PERCENT_VALUE,
                                                                     REPAIR_READY,
                                                                     REPAIR_LOCKED,
                                                                     TO_WIN_VALUE,
                                                                     OPEN_YES,
                                                                     OPEN_NO,
                                                                     ORDER_MOVING});

} // namespace Client
