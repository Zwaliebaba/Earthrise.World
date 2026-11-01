#include "pch.h"

#include "ScreenQuad.h"

namespace Client
{

namespace
{

float SrgbToLinear(float _value) noexcept
{
  return _value <= 0.04045F ? _value / 12.92F : std::pow((_value + 0.055F) / 1.055F, 2.4F);
}

} // namespace

std::array<float, 4> LinearColor(std::uint32_t _srgb, float _alpha) noexcept
{
  const auto channel = [_srgb](int _shift) { return SrgbToLinear(static_cast<float>((_srgb >> _shift) & 0xffU) / 255.0F); };
  return {channel(16), channel(8), channel(0), _alpha};
}

} // namespace Client
