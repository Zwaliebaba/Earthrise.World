#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace Client
{

/// The figures' portraits (Hud.md §5; M4-I2): each model rendered once through the voxel pass into a cell of the
/// interface pass's image atlas, PORTRAIT_TEXELS square, side by side in this order.
inline constexpr std::uint32_t PORTRAIT_TEXELS = 96;
inline constexpr std::array<std::string_view, 8> PORTRAIT_MODELS = {"U1", "U2", "U3", "U4", "U5", "S2", "S1", "S3p"};
inline constexpr std::uint32_t PORTRAIT_ATLAS_WIDTH_TEXELS = PORTRAIT_TEXELS * static_cast<std::uint32_t>(PORTRAIT_MODELS.size());

/// The atlas cell _modelId is rendered in, or nothing for a model with no portrait.
[[nodiscard]] constexpr std::optional<std::uint32_t> PortraitCell(std::string_view _modelId) noexcept
{
  for (std::uint32_t cell = 0; cell < PORTRAIT_MODELS.size(); ++cell)
  {
    if (PORTRAIT_MODELS.at(cell) == _modelId)
    {
      return cell;
    }
  }
  return std::nullopt;
}

} // namespace Client
