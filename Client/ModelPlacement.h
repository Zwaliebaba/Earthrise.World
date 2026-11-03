#pragma once

#include <cstdint>
#include <string_view>

namespace Client
{

/// A model drawn at one place: the minimum corner of its footprint after the turn, its quarter turns clockwise seen
/// from above (one turns its front, +z, towards +x), and the palette entry its faction colour is drawn in, or 0 for a
/// model that has none.
struct ModelPlacement
{
  std::string_view modelId;
  float xMeters;
  float yMeters;
  float zMeters;
  std::uint32_t quarterTurns;
  std::uint8_t sideColorIndex;
  /// Drawn as a silhouette in its faction entry where the world stands in front of it: a unit's or an engine's (M4-H3;
  /// ADR-021, decision 6, amended).
  bool silhouette = false;
};

} // namespace Client
