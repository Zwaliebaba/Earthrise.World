#include "pch.h"

#include "SiegeLog.h"

#include "TickClock.h"

namespace Client
{

namespace
{

std::string Voxel(const Protocol::VoxelPosition& _voxels)
{
  return std::format("{} {} {}", _voxels.xVoxels, _voxels.yVoxels, _voxels.zVoxels);
}

std::string Number(std::int64_t _value)
{
  return std::format("{}", _value);
}

/// Hud.md §5's names in the log's words, by UnitKind.
constexpr std::array<std::string_view, 8> KIND_NAMES = {"laborer",     "spearman", "archer", "outrider",
                                                        "men-at-arms", "sapper",   "ram",    "trebuchet"};

std::string_view OutcomeName(Protocol::TrialOutcome _outcome) noexcept
{
  switch (_outcome)
  {
  case Protocol::TrialOutcome::Won:
    return "won";
  case Protocol::TrialOutcome::LostToClock:
    return "clock";
  case Protocol::TrialOutcome::LostArmy:
    return "army";
  case Protocol::TrialOutcome::Running:
    break;
  }
  return "running";
}

std::string Seconds(std::uint32_t _ticks)
{
  return std::format("{:.2f}", static_cast<double>(_ticks) / static_cast<double>(TickClock::TICKS_PER_SECOND));
}

} // namespace

std::string SiegeLog::FileStem(std::string_view _participant, bool _control, std::string_view _scenario, std::string_view _stamp)
{
  return std::format("Siege-{}-{}-{}-{}", _participant, _control ? "control" : "voxel", _scenario, _stamp);
}

std::vector<std::string> SiegeLog::Head(const SiegeLogHeader& _header)
{
  return {std::format("# participant: {}", _header.participant),
          std::format("# version: {}", _header.control ? "control" : "voxel"),
          std::format("# build id: {:016x}", _header.buildId),
          std::format("# server-data hash: {:016x}", _header.serverDataHash),
          std::format("# scenario: {}", _header.scenario),
          std::format("# nearest: {}", _header.nearest),
          std::format("# weak: {}", _header.weak),
          std::format("# gate: {}", _header.gate),
          std::format("# display: {} x {}", _header.widthPixels, _header.heightPixels),
          std::format("# ui scale: {}", _header.uiScale),
          std::format("# start time: {}", _header.startTime),
          Row({"kind", "tick", "a", "b", "c", "d", "e"})};
}

std::string SiegeLog::Row(std::initializer_list<std::string_view> _fields)
{
  std::string row;
  std::size_t count = 0;
  for (const std::string_view field : _fields)
  {
    if (count != 0)
    {
      row += ',';
    }
    row += field;
    ++count;
  }
  for (; count < COLUMNS; ++count)
  {
    row += ',';
  }
  return row;
}

void SiegeLog::Advance(std::uint32_t _tick)
{
  WritePassed(_tick / TICKS_PER_MINUTE);
}

void SiegeLog::WritePassed(std::uint32_t _beforeMinute)
{
  while (!m_passed.empty() && std::get<0>(m_passed.begin()->first) < _beforeMinute)
  {
    const auto& [key, figures] = *m_passed.begin();
    const auto& [minute, structure, defender] = key;
    m_rows.push_back(Row({"passed", Number(minute), structure, defender ? "defender" : "attacker", Number(figures)}));
    m_passed.erase(m_passed.begin());
  }
}

void SiegeLog::Shot(std::uint32_t _tick, std::uint8_t _player, Protocol::EntityId _engine, const Protocol::VoxelPosition& _target,
                    const std::optional<Protocol::VoxelPosition>& _strike, std::string_view _structure)
{
  m_rows.push_back(Row({"shot", Number(_tick), Number(_player), Number(_engine), Voxel(_target),
                        _strike.has_value() ? Voxel(*_strike) : "-", _structure.empty() ? "-" : _structure}));
}

void SiegeLog::Opened(std::uint32_t _tick, std::string_view _structure)
{
  m_rows.push_back(Row({"opened", Number(_tick), _structure}));
}

void SiegeLog::Passed(std::uint32_t _tick, std::string_view _structure, bool _attacker)
{
  ++m_passed[{_tick / TICKS_PER_MINUTE, std::string(_structure), !_attacker}];
}

void SiegeLog::Spent(std::uint32_t _tick, std::uint8_t _player, std::int32_t _hundredthsCubicMeters, bool _timber, bool _repair)
{
  m_rows.push_back(
    Row({_timber ? "timber" : "stone", Number(_tick), Number(_player), Number(_hundredthsCubicMeters), _repair ? "repair" : "shot"}));
}

void SiegeLog::Stopped(std::uint32_t _tick, std::string_view _structure)
{
  m_rows.push_back(Row({"stopped", Number(_tick), _structure}));
}

void SiegeLog::Works(std::uint32_t _tick, std::string_view _structure)
{
  m_rows.push_back(Row({"works", Number(_tick), _structure}));
}

void SiegeLog::Died(std::uint32_t _tick, std::uint8_t _player, Protocol::UnitKind _kind)
{
  const auto index = static_cast<std::size_t>(_kind);
  m_rows.push_back(Row({"died", Number(_tick), Number(_player), index < KIND_NAMES.size() ? KIND_NAMES.at(index) : "unknown"}));
}

void SiegeLog::Command(const CommandRecord& _record)
{
  m_rows.push_back(Row({"command", Number(_record.tick), std::format("{:.3f}", _record.seconds), _record.command, _record.source}));
}

void SiegeLog::Onscreen(std::uint32_t _tick, std::string_view _structure, bool _warning, const Coverage& _coverage)
{
  m_rows.push_back(Row({"onscreen", Number(_tick), _structure, _warning ? "warning" : "opened", PlaceName(_coverage.place),
                        Number(_coverage.deckSharePercent)}));
  m_waits.push_back({.structure = std::string(_structure), .warning = _warning, .tick = _tick});
  if (_coverage.place == ScreenPlace::Uncovered)
  {
    Framed(_tick, _structure);
  }
}

std::vector<std::string> SiegeLog::Unframed() const
{
  std::vector<std::string> names;
  for (const Wait& wait : m_waits)
  {
    if (std::ranges::find(names, wait.structure) == names.end())
    {
      names.push_back(wait.structure);
    }
  }
  return names;
}

void SiegeLog::Framed(std::uint32_t _tick, std::string_view _structure)
{
  std::erase_if(m_waits,
                [this, _tick, _structure](const Wait& _wait)
                {
                  if (_wait.structure != _structure)
                  {
                    return false;
                  }
                  m_rows.push_back(Row(
                    {"framed", Number(_wait.tick), _wait.structure, _wait.warning ? "warning" : "opened", Seconds(_tick - _wait.tick)}));
                  return true;
                });
}

void SiegeLog::End(std::uint32_t _tick, std::optional<Protocol::TrialOutcome> _outcome)
{
  if (m_ended)
  {
    return;
  }
  m_ended = true;
  WritePassed(std::numeric_limits<std::uint32_t>::max());
  for (const Wait& wait : m_waits)
  {
    m_rows.push_back(Row({"framed", Number(wait.tick), wait.structure, wait.warning ? "warning" : "opened", "never"}));
  }
  m_waits.clear();
  if (_outcome.has_value() && *_outcome != Protocol::TrialOutcome::Running)
  {
    m_rows.push_back(Row({"outcome", Number(_tick), OutcomeName(*_outcome), Seconds(_tick)}));
  }
}

std::vector<std::string> SiegeLog::TakeRows()
{
  return std::exchange(m_rows, {});
}

} // namespace Client
