#include "pch.h"

#include "MaterialRules.h"

namespace Simulation
{

namespace
{

constexpr std::uint8_t STONE_SPAN_METERS = 2;  // Decided 2026-10-31
constexpr std::uint8_t TIMBER_SPAN_METERS = 5; // Decided 2026-11-01
constexpr std::uint8_t LEAVES_SPAN_METERS = 3;

} // namespace

MaterialRules StandInMaterialRules(std::uint32_t _voxelsPerMeter)
{
  if (_voxelsPerMeter == 0 || _voxelsPerMeter > 4)
  {
    throw std::invalid_argument("material rules take 1 to 4 voxels a meter");
  }
  const auto span = [_voxelsPerMeter](std::uint8_t _meters) { return static_cast<std::uint8_t>(_meters * _voxelsPerMeter); };
  const MaterialRule nothing{.support = Support::None, .spanVoxels = 0, .walkableGround = false, .liquid = false};
  const MaterialRule earth{.support = Support::Carries, .spanVoxels = 0, .walkableGround = true, .liquid = false};
  const MaterialRule rubble = earth;
  const MaterialRule rock{.support = Support::Spans, .spanVoxels = span(STONE_SPAN_METERS), .walkableGround = true, .liquid = false};
  const MaterialRule masonry{.support = Support::Spans, .spanVoxels = span(STONE_SPAN_METERS), .walkableGround = false, .liquid = false};
  const MaterialRule timber{.support = Support::Spans, .spanVoxels = span(TIMBER_SPAN_METERS), .walkableGround = false, .liquid = false};
  const MaterialRule leaves{.support = Support::Spans, .spanVoxels = span(LEAVES_SPAN_METERS), .walkableGround = false, .liquid = false};
  const MaterialRule bedrock{.support = Support::Anchors, .spanVoxels = 0, .walkableGround = true, .liquid = false};
  const MaterialRule water{.support = Support::None, .spanVoxels = 0, .walkableGround = false, .liquid = true};

  MaterialRules rules{};
  rules.fill(nothing);
  // By palette index, as GameData/Server/Palette.json gives each its behaviour.
  rules[1] = earth;   // grass
  rules[2] = earth;   // earth
  rules[3] = rock;    // common rock
  rules[4] = rock;    // building stone
  rules[5] = bedrock; // bedrock
  rules[6] = masonry; // masonry, the stone a template lays
  rules[7] = timber;  // timber
  rules[8] = rubble;  // stone rubble
  rules[9] = water;   // water
  rules[10] = leaves; // leaves A
  rules[11] = leaves; // leaves B
  rules[12] = timber; // roof, which behaves as timber
  rules[13] = earth;  // field A
  rules[14] = earth;  // field B
  rules[15] = earth;  // road
  rules[18] = timber; // iron, a decoration
  rules[20] = timber; // opening, a decoration
  rules[25] = rubble; // timber rubble
  rules[26] = rubble; // earth rubble
  rules[28] = rubble; // rock rubble
  return rules;
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
