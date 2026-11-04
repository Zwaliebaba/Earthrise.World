#pragma once

#include <cstdint>
#include <string_view>

namespace Client
{

/// A model drawn at one place: the minimum corner of the box that bounds its footprint as turned, its turn about the
/// middle of that footprint in radians, clockwise seen from above, so that a quarter turn takes its front, +z, towards
/// +x (ModelTurn.h; ADR-030), and the palette entry its faction colour is drawn in, or 0 for a model that has none. At a
/// whole quarter turn the box is the turned footprint itself.
struct ModelPlacement
{
  std::string_view modelId;
  float xMeters;
  float yMeters;
  float zMeters;
  float yawRadians;
  std::uint8_t sideColorIndex;
  /// Drawn as a silhouette in its faction entry where the world stands in front of it: a unit's or an engine's (M4-H3;
  /// ADR-021, decision 6, amended).
  bool silhouette = false;
};

} // namespace Client
