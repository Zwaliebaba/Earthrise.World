#include "pch.h"

#include "Camera.h"

#include "Look.h"

namespace Client
{

Camera::Camera(const DirectX::XMFLOAT3& _targetMeters)
  : m_camera({.elevationRadians = DirectX::XMConvertToRadians(ELEVATION_DEGREES),
              .azimuthRadians = DirectX::XMConvertToRadians(AZIMUTH_DEGREES),
              .viewHeightMeters = DEFAULT_VIEW_HEIGHT_METERS,
              .minViewHeightMeters = MIN_VIEW_HEIGHT_METERS,
              .maxViewHeightMeters = MAX_VIEW_HEIGHT_METERS,
              .rotationSeconds = ROTATION_SECONDS,
              .targetMeters = _targetMeters})
{
}

void Camera::Steer(const Engine::KeyboardInput& _keyboard, const Engine::PointerInput& _pointer, std::int32_t _dragXPixels,
                   std::int32_t _dragYPixels, std::uint32_t _heightPixels, float _seconds)
{
  const auto held = [&_keyboard](std::uint8_t _first, std::uint8_t _second)
  { return _keyboard.IsDown(_first) || _keyboard.IsDown(_second) ? 1.0F : 0.0F; };
  const float speed = PAN_VIEWS_PER_SECOND * m_camera.ViewHeightMeters() * _seconds;
  m_camera.Pan(speed * (held('D', VK_RIGHT) - held('A', VK_LEFT)), speed * (held('W', VK_UP) - held('S', VK_DOWN)));
  if (_pointer.IsDown(Engine::PointerInput::Button::Middle) && _heightPixels != 0)
  {
    // A drag moves the ground with the pointer. Up the screen covers more ground than across it, by
    // 1 / sin(elevation), because the view looks down at an angle.
    const float metersPerPixel = m_camera.ViewHeightMeters() / static_cast<float>(_heightPixels);
    const float upScale = 1.0F / std::sin(DirectX::XMConvertToRadians(ELEVATION_DEGREES));
    m_camera.Pan(-static_cast<float>(_dragXPixels) * metersPerPixel, static_cast<float>(_dragYPixels) * metersPerPixel * upScale);
  }
  const bool zoomIn = _keyboard.WasPressed(VK_OEM_PLUS) || _keyboard.WasPressed(VK_ADD);
  const bool zoomOut = _keyboard.WasPressed(VK_OEM_MINUS) || _keyboard.WasPressed(VK_SUBTRACT);
  m_camera.Zoom(_pointer.WheelNotches() + (zoomIn ? 1.0F : 0.0F) - (zoomOut ? 1.0F : 0.0F));
  if (_keyboard.WasPressed(VK_OEM_4) || _keyboard.WasPressed(VK_PRIOR))
  {
    m_camera.Rotate(-1);
  }
  if (_keyboard.WasPressed(VK_OEM_6) || _keyboard.WasPressed(VK_NEXT))
  {
    m_camera.Rotate(1);
  }
  m_camera.Update(_seconds);
}

void Camera::Turn(std::int32_t _quarterTurns) noexcept
{
  m_camera.Rotate(_quarterTurns);
}

void Camera::Zoom(float _notches) noexcept
{
  m_camera.Zoom(_notches);
}

void Camera::LookAt(const DirectX::XMFLOAT3& _targetMeters) noexcept
{
  m_camera.SetTargetMeters(_targetMeters);
}

DirectX::XMMATRIX Camera::ViewProjection(float _aspect, std::uint32_t _heightPixels, float _topBarPixels) const noexcept
{
  // The look-at point moves down by half the top bar, so it sits at the middle of the uncovered area: in clip space
  // that is -_topBarPixels / _heightPixels, the full height being 2.
  const float shift = _heightPixels == 0 ? 0.0F : -_topBarPixels / static_cast<float>(_heightPixels);
  return DirectX::XMMatrixMultiply(m_camera.ViewProjection(_aspect), DirectX::XMMatrixTranslation(0.0F, shift, 0.0F));
}

DirectX::XMVECTOR Camera::Forward() const noexcept
{
  return m_camera.Forward();
}

float Camera::ViewHeightMeters() const noexcept
{
  return m_camera.ViewHeightMeters();
}

float Camera::AzimuthRadians() const noexcept
{
  return m_camera.AzimuthRadians();
}

} // namespace Client
