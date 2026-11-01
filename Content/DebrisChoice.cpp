#include "pch.h"

#include "DebrisChoice.h"

#include "DebrisTable.h"

namespace Content
{

std::span<const DebrisChoice> DebrisChoices() noexcept
{
  return Generated::DEBRIS_CHOICES;
}

const DebrisChoice* FindDebrisChoice(std::uint8_t _rubbleIndex) noexcept
{
  const std::span<const DebrisChoice> choices = DebrisChoices();
  const auto found = std::ranges::find(choices, _rubbleIndex, &DebrisChoice::rubbleIndex);
  return found == choices.end() ? nullptr : &*found;
}

} // namespace Content
