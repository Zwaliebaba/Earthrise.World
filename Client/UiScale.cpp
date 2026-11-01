#include "pch.h"

#include "UiScale.h"

namespace Client
{

namespace
{

constexpr std::uint32_t BASE_DPI = 96;
constexpr std::uint32_t MIDDLE_BAND_PERCENT = 125;
constexpr std::uint32_t TOP_BAND_PERCENT = 175;

} // namespace

float UiScaleForDpi(std::uint32_t _dpi) noexcept
{
  const std::uint32_t percent = _dpi == 0 ? 100 : _dpi * 100 / BASE_DPI;
  return percent >= TOP_BAND_PERCENT ? 2.0F : (percent >= MIDDLE_BAND_PERCENT ? 1.5F : 1.0F);
}

float UiScaleFromOption(std::string_view _value)
{
  if (_value == "1")
  {
    return 1.0F;
  }
  if (_value == "1.5")
  {
    return 1.5F;
  }
  if (_value == "2")
  {
    return 2.0F;
  }
  throw std::invalid_argument(std::format("--ui-scale takes 1, 1.5 or 2, and was given \"{}\".", _value));
}

std::int32_t ScaledPixels(float _logicalPixels, float _uiScale) noexcept
{
  return static_cast<std::int32_t>(std::lround(_logicalPixels * _uiScale));
}

} // namespace Client
