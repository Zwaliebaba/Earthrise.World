#pragma once

#include "ChunkCoordinate.h"
#include "HudModel.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Client
{

/// What the lab shows of an item: its id, N, and where the mark is. The executable fills it from Simulation's items,
/// which the client never includes (ADR-020, decision 1).
struct LabItemSpec
{
  std::string id; // P0 the practice item, then L01 to L16
  std::uint32_t shotsAsked;
  Protocol::VoxelPosition markVoxels;
};

enum class LabAnswer : std::uint8_t
{
  Open,
  Shut
};

enum class LabPhase : std::uint8_t
{
  FreeFire, // two minutes on the intact site, FIRE free
  Watching, // an item's prior shots, at ten times
  Question, // the clock paused, the question up
  Shooting, // the asked shots, at ten times
  Result,   // the clock paused, the verdict up
  Finished
};

/// An answered item, as the log's item row has it (MvpPlan-M3.md, *The lab's log*).
struct LabItemRow
{
  std::string id;
  std::uint32_t shotsAsked;
  bool keyOpen; // the headless run's verdict: the simulation's key
  LabAnswer answer;
  double secondsToAnswer;
  bool integrityOn; // the views on at the answer
  bool passabilityOn;
  std::int32_t cameraTurns; // quarter turns, and zoom notches either way, while the question was up
  double zoomNotches;
  std::uint64_t liveHash;
  std::uint64_t headlessHash;

  [[nodiscard]] bool Correct() const noexcept;
  [[nodiscard]] bool Matched() const noexcept;
};

/// What the lab's log opens with (MvpPlan-M3.md, *The lab's log*).
struct LabLogInfo
{
  std::string participant;
  std::uint64_t buildId;
  std::uint64_t serverDataHash;
  std::uint64_t itemsHash;
  std::uint32_t widthPixels;
  std::uint32_t heightPixels;
  float uiScale;
  std::string startTime;
};

/// The breach lab's flow (M3-G1; GameDesign.md, M3's values, items 2 to 4), as the executable's loop drives it: free
/// fire first, then the practice item and the items in the participant's order, each watched, asked, shot and shown.
/// The executable builds the worlds and runs the shots, and tells the model what happened; the model decides what
/// comes next, what the HUD shows and what the log says.
class LabModel
{
public:
  /// Two minutes of game time.
  static constexpr std::uint32_t FREE_FIRE_TICKS = 2400;
  /// An item's automatic parts run at ten times (item 4).
  static constexpr double FAST_FORWARD_SPEED = 10.0;
  /// The KeyedDraw salt that names the item order: "LABORDER".
  static constexpr std::uint64_t ORDER_SALT = 0x4c41424f52444552ULL;

  /// _items is P0 first, then L01 to L16, as Simulation::LabItems has them.
  LabModel(std::vector<LabItemSpec> _items, std::uint64_t _seed);

  /// The order of _count items for _seed: the first, the practice item, first, then the rest shuffled by Fisher and
  /// Yates with KeyedDraw.
  [[nodiscard]] static std::vector<std::size_t> Order(std::size_t _count, std::uint64_t _seed);

  [[nodiscard]] LabPhase Phase() const noexcept;
  /// The item being run, or nullptr in free fire and once finished.
  [[nodiscard]] const LabItemSpec* Current() const noexcept;
  /// How many items have started: each a fresh world.
  [[nodiscard]] std::size_t Started() const noexcept;

  /// The free fire's world reached _tick: at FREE_FIRE_TICKS the practice item starts.
  void FreeFireTicked(std::uint32_t _tick);
  /// The prior shots are done: the question goes up at _nowSeconds.
  void PriorShotsDone(double _nowSeconds);
  /// The answer, by key or by the banner's buttons, with the views on at _nowSeconds. Ignored unless the question is
  /// up; returns whether it was taken.
  bool Answer(LabAnswer _answer, double _nowSeconds, bool _integrityOn, bool _passabilityOn);
  /// The camera moved: counted while the question is up.
  void NoteCamera(std::int32_t _quarterTurns, double _zoomNotches);
  /// The asked shots are done, and the courtyard reads _open: the result goes up.
  void AskedShotsDone(bool _open);
  /// Space at the result: closes the item with its live and headless hashes and the headless key, starts the next
  /// item or finishes, and returns the item's row. Nothing outside the result.
  std::optional<LabItemRow> Next(std::uint64_t _liveHash, std::uint64_t _headlessHash, bool _keyOpen);

  /// The banner: FREE FIRE · m:ss LEFT at the free fire's _tick, WATCH, the question with OPEN · O and SHUT · S, the
  /// result with SPACE FOR THE NEXT, or the end.
  [[nodiscard]] HudMode Mode(std::uint32_t _tick) const;
  /// PRACTICE, ITEM k OF 16 or DONE, or nothing in free fire.
  [[nodiscard]] std::optional<HudObjective> Objective() const;
  [[nodiscard]] bool FireAllowed() const noexcept;
  /// The passability view is forced on from the answer to the next item.
  [[nodiscard]] bool PassabilityForced() const noexcept;
  /// The clock pauses while the question and the result are up, and once finished.
  [[nodiscard]] bool Paused() const noexcept;
  [[nodiscard]] double Speed() const noexcept;
  /// The mark shows from the question until the next item.
  [[nodiscard]] bool MarkShown() const noexcept;

  /// The log's comment lines and its header row.
  [[nodiscard]] static std::string LogHeader(const LabLogInfo& _info);
  [[nodiscard]] static std::string ItemRowText(const LabItemRow& _row);
  /// A command the player issued in free fire, with its source, key or click (Hud.md §11).
  [[nodiscard]] static std::string CommandRowText(std::uint32_t _tick, double _seconds, std::string_view _command,
                                                  std::string_view _source);

private:
  std::vector<LabItemSpec> m_items;
  std::vector<std::size_t> m_order;
  LabPhase m_phase = LabPhase::FreeFire;
  std::size_t m_started = 0; // items started; the current is m_order[m_started - 1]
  double m_askedSeconds = 0.0;
  std::optional<LabItemRow> m_row; // the current item's, from the answer
  bool m_resultOpen = false;
};

} // namespace Client
