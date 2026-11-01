#include "pch.h"

#include "HudModel.h"

#include "Strings.h"
#include "TickClock.h"

namespace Client
{

std::vector<HudResource> BreachResources(std::uint32_t _tick, std::uint32_t _shotsFired)
{
  constexpr std::uint32_t SECONDS_PER_MINUTE = 60;
  const std::uint32_t seconds = _tick / TickClock::TICKS_PER_SECOND;
  return {{.key = std::string(TIME_KEY),
           .value = std::format(TIME_VALUE, seconds / SECONDS_PER_MINUTE, seconds % SECONDS_PER_MINUTE),
           .warn = false},
          {.key = std::string(SHOTS_KEY), .value = std::format(SHOTS_VALUE, _shotsFired), .warn = false}};
}

} // namespace Client
