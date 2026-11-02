#pragma once

#include <cstdint>
#include <string>

namespace Simulation
{

/// A template's values as a structure at work, ServerData's STRC section, from GameData/Server/Structures.json (ADR-024,
/// decision 7).
struct TemplateRules
{
  std::string templateId;
  std::uint8_t criticalThresholdPercent; // works while this share of its critical part stands; 0 when it names none
  std::uint8_t garrisonCapacity;
  std::int32_t standHeightVoxels; // where a garrison looses from, above the structure's base
  std::int32_t visionVoxels;      // from its footprint
  std::int32_t garrisonedVisionVoxels;
  std::int32_t controlHitPoints; // the control's (ADR-028)
  std::int32_t controlHitPointsPerCubicMeter;
};

} // namespace Simulation
