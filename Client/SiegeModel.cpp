#include "pch.h"

#include "SiegeModel.h"

#include "CommandCard.h"
#include "Strings.h"
#include "TickClock.h"

namespace Client
{

namespace
{

constexpr std::uint32_t SECONDS_PER_MINUTE = 60;

/// _ticks as m:ss, through _format, whole seconds rounded down.
std::string Clock(std::string_view _format, std::uint32_t _ticks)
{
  const std::uint32_t seconds = _ticks / TickClock::TICKS_PER_SECOND;
  const std::uint32_t minutes = seconds / SECONDS_PER_MINUTE;
  const std::uint32_t rest = seconds % SECONDS_PER_MINUTE;
  return std::vformat(_format, std::make_format_args(minutes, rest));
}

} // namespace

HudObjective SiegeModel::Objective(const Protocol::StateUpdate& _state)
{
  const Protocol::TrialView& trial = _state.trial;
  if (trial.holdTicks == 0 || trial.holdTicksNeeded == 0)
  {
    return {.title = std::string(SIEGE_OBJECTIVE_TITLE), .detail = {}, .detailOff = false, .progress = std::nullopt};
  }
  const std::uint32_t held = trial.holdTicks / TickClock::TICKS_PER_SECOND;
  const std::uint32_t needed = trial.holdTicksNeeded / TickClock::TICKS_PER_SECOND;
  return {.title = std::string(HOLD_OBJECTIVE_TITLE),
          .detail = std::format(HOLD_OBJECTIVE_DETAIL, held / SECONDS_PER_MINUTE, held % SECONDS_PER_MINUTE, needed / SECONDS_PER_MINUTE,
                                needed % SECONDS_PER_MINUTE),
          .detailOff = trial.holdContested,
          .progress = std::min(1.0F, static_cast<float>(trial.holdTicks) / static_cast<float>(trial.holdTicksNeeded))};
}

std::vector<HudResource> SiegeModel::Resources(const Protocol::StateUpdate& _state)
{
  const auto player = std::ranges::find(_state.players, _state.player, &Protocol::PlayerView::player);
  const std::int32_t stone = player == _state.players.end() ? 0 : std::max(0, player->stoneHundredthsCubicMeters);
  const auto army = std::ranges::count(_state.units, _state.player, &Protocol::UnitView::owner);
  return {{.key = std::string(STONE_KEY),
           .value = std::format(STONE_VALUE, stone / 100, (stone % 100) / 10),
           .warn = stone < SHOT_STONE_HUNDREDTHS},
          {.key = std::string(ARMY_KEY), .value = std::format("{}", army), .warn = false},
          {.key = std::string(TIME_KEY), .value = Clock(TIME_VALUE, _state.trial.ticksLeft), .warn = false}};
}

std::optional<HudMode> SiegeModel::EndBanner(const Protocol::StateUpdate& _state)
{
  std::string_view format;
  switch (_state.trial.outcome)
  {
  case Protocol::TrialOutcome::Running:
    return std::nullopt;
  case Protocol::TrialOutcome::Won:
    format = KEEP_FALLEN_BANNER;
    break;
  case Protocol::TrialOutcome::LostToClock:
    format = TIME_RUN_OUT_BANNER;
    break;
  case Protocol::TrialOutcome::LostArmy:
    format = ARMY_GONE_BANNER;
    break;
  }
  return HudMode{.command = "END", .banner = Clock(format, _state.tick), .buttons = {}};
}

void SiegeModel::TogglePause() noexcept
{
  m_paused = !m_paused;
}

bool SiegeModel::Paused() const noexcept
{
  return m_paused;
}

std::optional<HudMode> SiegeModel::PauseBanner() const
{
  if (!m_paused)
  {
    return std::nullopt;
  }
  return HudMode{.command = "PAUSED", .banner = std::string(PAUSED_BANNER), .buttons = {}};
}

} // namespace Client
