#pragma once

#include <array>
#include <cstdint>

namespace Client
{

/// A quad the interface pass draws (ADR-019, decision 3), as the client builds it: a rectangle in the target's pixels,
/// top-left origin, either a solid linear colour or a rectangle of the glyph atlas whose red channel is the alpha. The
/// renderer copies each into an Engine::InterfaceQuad, field for field.
struct ScreenQuad
{
  float xPixels;
  float yPixels;
  float widthPixels;
  float heightPixels;
  float u0; // the atlas rectangle, in texels; unused when solid
  float v0;
  float u1;
  float v1;
  std::array<float, 4> color; // linear RGBA
  bool textured;
};

/// An sRGB colour, 0xrrggbb, as linear RGBA at _alpha.
[[nodiscard]] std::array<float, 4> LinearColor(std::uint32_t _srgb, float _alpha = 1.0F) noexcept;

} // namespace Client
