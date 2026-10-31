#include "pch.h"

#include "KeyboardInput.h"

namespace Engine
{

namespace
{

// Bit 30 of a key message's lParam: the key was already down, so this is an auto-repeat.
constexpr LPARAM WAS_DOWN_BIT = LPARAM{1} << 30;

} // namespace

void KeyboardInput::Observe(const MSG& _message) noexcept
{
  const auto key = static_cast<std::size_t>(_message.wParam & 0xffU);
  switch (_message.message)
  {
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    if ((_message.lParam & WAS_DOWN_BIT) == 0)
    {
      m_pressed.set(key);
    }
    m_down.set(key);
    break;
  case WM_KEYUP:
  case WM_SYSKEYUP:
    m_down.reset(key);
    break;
  default:
    break;
  }
}

void KeyboardInput::EndFrame() noexcept
{
  m_pressed.reset();
}

void KeyboardInput::ReleaseAll() noexcept
{
  m_down.reset();
}

bool KeyboardInput::IsDown(std::uint8_t _virtualKey) const noexcept
{
  return m_down.test(_virtualKey);
}

bool KeyboardInput::WasPressed(std::uint8_t _virtualKey) const noexcept
{
  return m_pressed.test(_virtualKey);
}

} // namespace Engine
