#include "pch.h"

#include "LabModel.h"

#include "SplitMix64.h"
#include "Strings.h"
#include "TickClock.h"

#include <cmath>
#include <cstdlib>

namespace Client
{

namespace
{

constexpr std::string_view WATCH_COMMAND = "WATCH";
constexpr std::string_view QUESTION_COMMAND = "QUESTION";
constexpr std::string_view RESULT_COMMAND = "RESULT";
constexpr std::string_view FREE_FIRE_COMMAND = "FREE FIRE";
constexpr std::string_view FINISHED_COMMAND = "DONE";
constexpr std::string_view OPEN = "open";
constexpr std::string_view SHUT = "shut";

std::string_view Word(bool _open) noexcept
{
  return _open ? OPEN : SHUT;
}

} // namespace

bool LabItemRow::Correct() const noexcept
{
  return (answer == LabAnswer::Open) == keyOpen;
}

bool LabItemRow::Matched() const noexcept
{
  return liveHash == headlessHash;
}

LabModel::LabModel(std::vector<LabItemSpec> _items, std::uint64_t _seed)
  : m_items(std::move(_items)),
    m_order(Order(m_items.size(), _seed))
{
}

std::vector<std::size_t> LabModel::Order(std::size_t _count, std::uint64_t _seed)
{
  std::vector<std::size_t> order(_count);
  for (std::size_t index = 0; index < _count; ++index)
  {
    order.at(index) = index;
  }
  // Fisher and Yates over all but the first, from the last down: each position takes one of those at or before it.
  for (std::size_t position = _count; position-- > 2;)
  {
    const auto choices = static_cast<std::uint32_t>(position); // positions 1 to position
    const std::size_t chosen = 1 + Core::DrawBelow(Core::KeyedDraw(_seed, ORDER_SALT, position), choices);
    std::swap(order.at(position), order.at(chosen));
  }
  return order;
}

LabPhase LabModel::Phase() const noexcept
{
  return m_phase;
}

const LabItemSpec* LabModel::Current() const noexcept
{
  if (m_started == 0 || m_phase == LabPhase::Finished)
  {
    return nullptr;
  }
  return &m_items.at(m_order.at(m_started - 1));
}

std::size_t LabModel::Started() const noexcept
{
  return m_started;
}

void LabModel::FreeFireTicked(std::uint32_t _tick)
{
  if (m_phase == LabPhase::FreeFire && _tick >= FREE_FIRE_TICKS)
  {
    m_phase = m_items.empty() ? LabPhase::Finished : LabPhase::Watching;
    m_started = m_items.empty() ? 0 : 1;
  }
}

void LabModel::PriorShotsDone(double _nowSeconds)
{
  if (m_phase == LabPhase::Watching)
  {
    m_phase = LabPhase::Question;
    m_askedSeconds = _nowSeconds;
    const LabItemSpec& item = *Current();
    m_row = LabItemRow{.id = item.id,
                       .shotsAsked = item.shotsAsked,
                       .keyOpen = false,
                       .answer = LabAnswer::Shut,
                       .secondsToAnswer = 0.0,
                       .integrityOn = false,
                       .passabilityOn = false,
                       .cameraTurns = 0,
                       .zoomNotches = 0.0,
                       .liveHash = 0,
                       .headlessHash = 0};
  }
}

bool LabModel::Answer(LabAnswer _answer, double _nowSeconds, bool _integrityOn, bool _passabilityOn)
{
  if (m_phase != LabPhase::Question || !m_row.has_value())
  {
    return false;
  }
  m_row->answer = _answer;
  m_row->secondsToAnswer = _nowSeconds - m_askedSeconds;
  m_row->integrityOn = _integrityOn;
  m_row->passabilityOn = _passabilityOn;
  m_phase = LabPhase::Shooting;
  return true;
}

void LabModel::NoteCamera(std::int32_t _quarterTurns, double _zoomNotches)
{
  if (m_phase == LabPhase::Question && m_row.has_value())
  {
    m_row->cameraTurns += std::abs(_quarterTurns);
    m_row->zoomNotches += std::abs(_zoomNotches);
  }
}

void LabModel::AskedShotsDone(bool _open)
{
  if (m_phase == LabPhase::Shooting)
  {
    m_phase = LabPhase::Result;
    m_resultOpen = _open;
  }
}

std::optional<LabItemRow> LabModel::Next(std::uint64_t _liveHash, std::uint64_t _headlessHash, bool _keyOpen)
{
  if (m_phase != LabPhase::Result || !m_row.has_value())
  {
    return std::nullopt;
  }
  LabItemRow row = *m_row;
  row.liveHash = _liveHash;
  row.headlessHash = _headlessHash;
  row.keyOpen = _keyOpen;
  m_row.reset();
  if (m_started < m_order.size())
  {
    ++m_started;
    m_phase = LabPhase::Watching;
  }
  else
  {
    m_phase = LabPhase::Finished;
  }
  return row;
}

HudMode LabModel::Mode(std::uint32_t _tick) const
{
  switch (m_phase)
  {
  case LabPhase::FreeFire:
  {
    const std::uint32_t left = _tick >= FREE_FIRE_TICKS ? 0 : FREE_FIRE_TICKS - _tick;
    const std::uint32_t seconds = (left + TickClock::TICKS_PER_SECOND - 1) / TickClock::TICKS_PER_SECOND;
    return {.command = std::string(FREE_FIRE_COMMAND), .banner = std::format(FREE_FIRE_BANNER, seconds / 60, seconds % 60), .buttons = {}};
  }
  case LabPhase::Watching:
  case LabPhase::Shooting:
    return {.command = std::string(WATCH_COMMAND), .banner = std::string(WATCH_BANNER), .buttons = {}};
  case LabPhase::Question:
  {
    const std::uint32_t shots = Current() != nullptr ? Current()->shotsAsked : 0;
    return {.command = std::string(QUESTION_COMMAND),
            .banner = shots == 1 ? std::string(QUESTION_ONE_SHOT_BANNER) : std::format(QUESTION_BANNER, shots),
            .buttons = {{.control = HudControl::AnswerOpen, .label = std::string(OPEN_BUTTON)},
                        {.control = HudControl::AnswerShut, .label = std::string(SHUT_BUTTON)}}};
  }
  case LabPhase::Result:
    return {
      .command = std::string(RESULT_COMMAND), .banner = std::string(m_resultOpen ? RESULT_OPEN_BANNER : RESULT_SHUT_BANNER), .buttons = {}};
  case LabPhase::Finished:
    return {.command = std::string(FINISHED_COMMAND), .banner = std::string(FINISHED_BANNER), .buttons = {}};
  }
  return {};
}

std::optional<HudObjective> LabModel::Objective() const
{
  if (m_phase == LabPhase::FreeFire)
  {
    return std::nullopt;
  }
  if (m_phase == LabPhase::Finished)
  {
    return HudObjective{.title = std::string(FINISHED_OBJECTIVE), .detail = "", .detailOff = false};
  }
  // The practice item first, then the scored items counted from 1.
  if (m_started <= 1)
  {
    return HudObjective{.title = std::string(PRACTICE_OBJECTIVE), .detail = "", .detailOff = false};
  }
  return HudObjective{.title = std::format(ITEM_OBJECTIVE, m_started - 1, m_order.size() - 1), .detail = "", .detailOff = false};
}

bool LabModel::FireAllowed() const noexcept
{
  return m_phase == LabPhase::FreeFire;
}

bool LabModel::PassabilityForced() const noexcept
{
  return m_phase == LabPhase::Shooting || m_phase == LabPhase::Result;
}

bool LabModel::Paused() const noexcept
{
  return m_phase == LabPhase::Question || m_phase == LabPhase::Result || m_phase == LabPhase::Finished;
}

double LabModel::Speed() const noexcept
{
  return m_phase == LabPhase::Watching || m_phase == LabPhase::Shooting ? FAST_FORWARD_SPEED : 1.0;
}

bool LabModel::MarkShown() const noexcept
{
  return m_phase == LabPhase::Question || m_phase == LabPhase::Shooting || m_phase == LabPhase::Result;
}

std::string LabModel::LogHeader(const LabLogInfo& _info)
{
  std::string text;
  text += std::format("# participant: {}\n", _info.participant);
  text += std::format("# build id: {:016x}\n", _info.buildId);
  text += std::format("# server-data hash: {:016x}\n", _info.serverDataHash);
  text += std::format("# item-set hash: {:016x}\n", _info.itemsHash);
  text += std::format("# display size: {}x{}\n", _info.widthPixels, _info.heightPixels);
  text += std::format("# UI scale: {}\n", _info.uiScale);
  text += std::format("# start time: {}\n", _info.startTime);
  text += "kind,id,n,key,answer,correct,secondsToAnswer,integrityOn,passabilityOn,cameraTurns,zoomNotches,liveHash,headlessHash,matched\n";
  return text;
}

std::string LabModel::ItemRowText(const LabItemRow& _row)
{
  const auto flag = [](bool _value) { return _value ? 1 : 0; };
  return std::format("item,{},{},{},{},{},{:.2f},{},{},{},{:.1f},{:016x},{:016x},{}\n", _row.id, _row.shotsAsked, Word(_row.keyOpen),
                     Word(_row.answer == LabAnswer::Open), flag(_row.Correct()), _row.secondsToAnswer, flag(_row.integrityOn),
                     flag(_row.passabilityOn), _row.cameraTurns, _row.zoomNotches, _row.liveHash, _row.headlessHash, flag(_row.Matched()));
}

std::string LabModel::CommandRowText(std::uint32_t _tick, double _seconds, std::string_view _command, std::string_view _source)
{
  return std::format("command,{},{:.2f},{},{}\n", _tick, _seconds, _command, _source);
}

} // namespace Client
