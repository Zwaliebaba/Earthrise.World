#include "pch.h"

#include "MaterialRules.h"

namespace Simulation
{

MaterialRules ScaleSpans(const MaterialRules& _rules, std::uint32_t _voxelsPerMeter)
{
  if (_voxelsPerMeter == 0 || _voxelsPerMeter > 4)
  {
    throw std::invalid_argument("material rules take 1 to 4 voxels a meter");
  }
  MaterialRules scaled = _rules;
  for (MaterialRule& rule : scaled)
  {
    const std::uint32_t span = rule.spanVoxels * _voxelsPerMeter;
    if (span >= UINT8_MAX)
    {
      throw std::invalid_argument("a scaled span does not fit in a byte");
    }
    rule.spanVoxels = static_cast<std::uint8_t>(span);
  }
  return scaled;
}

std::uint32_t LongestSpanVoxels(const MaterialRules& _rules) noexcept
{
  std::uint32_t longest = 0;
  for (const MaterialRule& rule : _rules)
  {
    if (rule.support == Support::Spans)
    {
      longest = std::max<std::uint32_t>(longest, rule.spanVoxels);
    }
  }
  return longest;
}

} // namespace Simulation
