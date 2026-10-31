#pragma once

#include "framework.h"

#include <DirectXMath.h>

#include <cstdint>

namespace Engine
{

/// A strategy camera: orthographic, at a fixed elevation, turned about the vertical in quarter turns
/// that ease into place, zoomed by view height, and panned along the ground. The game sets its
/// numbers (RendererLook.md §7); the camera holds none of its own (R9).
class OrthographicCamera
{
public:
  struct Desc
  {
    float elevationRadians; // above the horizon
    float azimuthRadians;   // where the camera stands, measured from +z towards +x
    float viewHeightMeters; // how much of the world the screen's height shows
    float minViewHeightMeters;
    float maxViewHeightMeters;
    float rotationSeconds; // how long a quarter turn eases
    DirectX::XMFLOAT3 targetMeters;
  };

  explicit OrthographicCamera(const Desc& _desc) noexcept;

  /// Moves the target along the ground, in the screen's directions: right, and up the screen.
  void Pan(float _rightMeters, float _upMeters) noexcept;

  /// Zooms by whole or fractional notches; a positive notch moves closer. The view height is clamped.
  void Zoom(float _notches) noexcept;

  /// Starts a turn of _quarterTurns times 90 degrees; a positive turn is clockwise seen from above.
  void Rotate(std::int32_t _quarterTurns) noexcept;

  /// Advances the easing of a turn by _seconds.
  void Update(float _seconds) noexcept;

  /// World to clip for row vectors, with reversed Z (ADR-011, decision 4), for a screen of _aspect
  /// width over height.
  [[nodiscard]] DirectX::XMMATRIX ViewProjection(float _aspect) const noexcept;

  /// The direction the camera looks, unit length.
  [[nodiscard]] DirectX::XMVECTOR Forward() const noexcept;

  [[nodiscard]] float ViewHeightMeters() const noexcept;
  [[nodiscard]] float AzimuthRadians() const noexcept;
  [[nodiscard]] DirectX::XMFLOAT3 TargetMeters() const noexcept;

  /// The camera stands this far back from its target, and draws between these distances.
  static constexpr float EYE_DISTANCE_METERS = 1000.0F;
  static constexpr float NEAR_METERS = 1.0F;
  static constexpr float FAR_METERS = 2000.0F;
  /// Each notch scales the view height by this.
  static constexpr float ZOOM_STEP = 1.15F;

private:
  float m_elevationRadians;
  float m_azimuthRadians;       // where the turn is now
  float m_targetAzimuthRadians; // where it is heading
  float m_turnRadiansPerSecond;
  float m_viewHeightMeters;
  float m_minViewHeightMeters;
  float m_maxViewHeightMeters;
  DirectX::XMFLOAT3 m_targetMeters;
};

} // namespace Engine
