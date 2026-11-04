#pragma once

#include "CommandLog.h"
#include "DeckCoverage.h"
#include "EntityKinds.h"
#include "PlaytestGame.h"
#include "ChunkCoordinate.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace Client
{

/// The siege work a hit row records (M5-B1; MvpPlan-M5.md, *The trial's log*): a stone that struck a structure, a ram's
/// blow on one, or a cubic metre a sapper dug beside a section.
enum class SiegeWork : std::uint8_t
{
  Stone,
  Blow,
  Trench
};

/// The trial's log's comment lines (MvpPlan-M4.md, *The trial's log*), each `# key: value`.
struct SiegeLogHeader
{
  std::string participant;
  bool control;
  std::uint64_t buildId;
  std::uint64_t serverDataHash;
  std::string scenario;
  std::string nearest; // the section nearest the camp
  std::string weak;    // and the weak one
  std::string gate;    // and the gatehouse, the third role the reader sorts breaches by
  std::uint32_t widthPixels;
  std::uint32_t heightPixels;
  float uiScale;
  std::string startTime; // yyyy-mm-dd hh:mm:ss, local
  /// Under --game, the playtest's game, written as the game, cell and group lines (M5-C1; MvpPlan-M5.md, *The trial's log*).
  std::optional<PlaytestGame> playtest = std::nullopt;
};

/// The Siege trial's log (MvpPlan-M4.md, *The trial's log*; ADR-026, decisions 5 and 6; M4-J2), as rows: Earthrise hands
/// it the simulation's events, the player's commands and where each breach and warning was on screen, and writes the
/// rows it gives. Every row has COLUMNS fields, led by its kind and its tick, the rest empty where a kind has fewer; a
/// voxel is one field, x y z. It counts the figures through each section a game minute and writes them when the minute
/// ends, and after each breach or warning waits for the first frame that frames it.
class SiegeLog
{
public:
  static constexpr std::size_t COLUMNS = 7;
  static constexpr std::uint32_t TICKS_PER_MINUTE = 1200;

  /// Siege-<participant>-<voxel|control>-<Siege|SiegeMirror>-<yyyymmdd-hhmmss>, with no extension.
  [[nodiscard]] static std::string FileStem(std::string_view _participant, bool _control, std::string_view _scenario,
                                            std::string_view _stamp);
  /// The comment lines, then the header row.
  [[nodiscard]] static std::vector<std::string> Head(const SiegeLogHeader& _header);
  /// _fields as a row of COLUMNS, LF not included.
  [[nodiscard]] static std::string Row(std::initializer_list<std::string_view> _fields);

  /// Writes the passed rows of every minute before _tick's.
  void Advance(std::uint32_t _tick);

  void Shot(std::uint32_t _tick, std::uint8_t _player, Protocol::EntityId _engine, const Protocol::VoxelPosition& _target,
            const std::optional<Protocol::VoxelPosition>& _strike, std::string_view _structure);
  void Opened(std::uint32_t _tick, std::string_view _structure);
  /// _player's siege work on _structure, at the control's price in hit points in both versions (M5-B1).
  void Hit(std::uint32_t _tick, std::uint8_t _player, SiegeWork _work, std::string_view _structure, std::int32_t _hitPoints);
  /// A figure through _structure, of the attacker or the defender, counted into _tick's minute.
  void Passed(std::uint32_t _tick, std::string_view _structure, bool _attacker);
  void Spent(std::uint32_t _tick, std::uint8_t _player, std::int32_t _hundredthsCubicMeters, bool _timber, bool _repair);
  void Stopped(std::uint32_t _tick, std::string_view _structure);
  void Works(std::uint32_t _tick, std::string_view _structure);
  void Died(std::uint32_t _tick, std::uint8_t _player, Protocol::UnitKind _kind);
  void Command(const CommandRecord& _record);
  /// Where _structure was on the frame that drew _tick, the tick it opened or a collapse of it began its warning; and,
  /// framed there or not, the wait for it to be framed.
  void Onscreen(std::uint32_t _tick, std::string_view _structure, bool _warning, const Coverage& _coverage);
  /// The structures still waiting to be framed.
  [[nodiscard]] std::vector<std::string> Unframed() const;
  /// _structure's centre lies in the uncovered area on the frame that drew _tick: each of its waits ends.
  void Framed(std::uint32_t _tick, std::string_view _structure);
  /// The run's end at _tick: every minute's passed rows, `never` for each wait, and the outcome when there is one.
  void End(std::uint32_t _tick, std::optional<Protocol::TrialOutcome> _outcome);

  /// The rows made since the last call, in the order they were made, which is the ticks' order.
  [[nodiscard]] std::vector<std::string> TakeRows();

private:
  struct Wait
  {
    std::string structure;
    bool warning;
    std::uint32_t tick;
  };

  void WritePassed(std::uint32_t _beforeMinute);

  std::vector<std::string> m_rows;
  std::vector<Wait> m_waits;
  std::map<std::tuple<std::uint32_t, std::string, bool>, std::int32_t> m_passed; // minute, structure, defender
  bool m_ended = false;
};

} // namespace Client
