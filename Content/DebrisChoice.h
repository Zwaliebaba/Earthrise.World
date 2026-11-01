#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace Content
{

/// The debris a collapse scatters over one rubble (GameDesign.md, M3's values, item 16), baked from
/// GameData/Manifest.json's debrisByRubble (MvpAssets.md, *Poses, projectiles, gates and debris*).
struct DebrisChoice
{
  std::uint8_t rubbleIndex;                 // the palette index of the rubble it lies on
  std::span<const std::string_view> models; // the models scattered over it, each in Models()
  std::uint8_t tintIndex;                   // every voxel of a piece drawn in this index, or 0 to keep the model's own
};

/// Every choice, in ascending rubble index.
[[nodiscard]] std::span<const DebrisChoice> DebrisChoices() noexcept;

/// The choice for the rubble at _rubbleIndex, or nullptr when no debris lies on it.
[[nodiscard]] const DebrisChoice* FindDebrisChoice(std::uint8_t _rubbleIndex) noexcept;

} // namespace Content
