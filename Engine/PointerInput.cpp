#include "pch.h"

#include <windowsx.h>

#include "PointerInput.h"

namespace Engine
{

namespace
{

constexpr std::size_t IndexOf(PointerInput::Button _button) noexcept
{
  return static_cast<std::size_t>(_button);
}

} // namespace

void PointerInput::Observe(const MSG& _message) noexcept
{
  switch (_message.message)
  {
  case WM_MOUSEMOVE:
  case WM_LBUTTONDOWN:
  case WM_LBUTTONUP:
  case WM_RBUTTONDOWN:
  case WM_RBUTTONUP:
  case WM_MBUTTONDOWN:
  case WM_MBUTTONUP:
    // These carry the position in client pixels; the wheel's carries it in screen pixels.
    m_xPixels = GET_X_LPARAM(_message.lParam);
    m_yPixels = GET_Y_LPARAM(_message.lParam);
    break;
  default:
    break;
  }

  switch (_message.message)
  {
  case WM_LBUTTONDOWN:
    Press(Button::Left, _message.hwnd);
    break;
  case WM_RBUTTONDOWN:
    Press(Button::Right, _message.hwnd);
    break;
  case WM_MBUTTONDOWN:
    Press(Button::Middle, _message.hwnd);
    break;
  case WM_LBUTTONUP:
    Release(Button::Left);
    break;
  case WM_RBUTTONUP:
    Release(Button::Right);
    break;
  case WM_MBUTTONUP:
    Release(Button::Middle);
    break;
  case WM_MOUSEWHEEL:
    m_wheelDelta += GET_WHEEL_DELTA_WPARAM(_message.wParam);
    break;
  default:
    break;
  }
}

void PointerInput::EndFrame() noexcept
{
  m_pressed = {};
  m_wheelDelta = 0;
}

std::int32_t PointerInput::XPixels() const noexcept
{
  return m_xPixels;
}

std::int32_t PointerInput::YPixels() const noexcept
{
  return m_yPixels;
}

bool PointerInput::IsDown(Button _button) const noexcept
{
  return m_down[IndexOf(_button)];
}

bool PointerInput::WasPressed(Button _button) const noexcept
{
  return m_pressed[IndexOf(_button)];
}

float PointerInput::WheelNotches() const noexcept
{
  return static_cast<float>(m_wheelDelta) / static_cast<float>(WHEEL_DELTA);
}

void PointerInput::Press(Button _button, HWND _window) noexcept
{
  m_down[IndexOf(_button)] = true;
  m_pressed[IndexOf(_button)] = true;
  // Capture keeps a drag's release coming to this window when the pointer leaves it.
  if (_window != nullptr)
  {
    SetCapture(_window);
  }
}

void PointerInput::Release(Button _button) noexcept
{
  m_down[IndexOf(_button)] = false;
  if (m_down == std::array<bool, BUTTON_COUNT>{})
  {
    ReleaseCapture();
  }
}

} // namespace Engine
