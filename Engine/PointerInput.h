#pragma once

#include "framework.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Engine
{

/// The mouse, as the mouse-and-keyboard scheme reads it (ADR-007): where it is in the client area,
/// which buttons are held, which went down this frame, and how far the wheel turned. It reads the
/// messages Window::PumpMessages takes from the queue, and holds no game concept (R9).
class PointerInput
{
public:
  enum class Button : std::uint8_t
  {
    Left,
    Right,
    Middle
  };

  /// Reads one queued message. Anything that is not about the mouse is ignored.
  void Observe(const MSG& _message) noexcept;

  /// Clears what lasts one frame, the presses and the wheel. Call once the frame has used them.
  void EndFrame() noexcept;

  [[nodiscard]] std::int32_t XPixels() const noexcept;
  [[nodiscard]] std::int32_t YPixels() const noexcept;
  [[nodiscard]] bool IsDown(Button _button) const noexcept;
  [[nodiscard]] bool WasPressed(Button _button) const noexcept;

  /// The wheel's turn since the last EndFrame, in notches, positive away from the user. Fine-grained
  /// wheels give fractions.
  [[nodiscard]] float WheelNotches() const noexcept;

private:
  static constexpr std::size_t BUTTON_COUNT = 3;

  void Press(Button _button, HWND _window) noexcept;
  void Release(Button _button) noexcept;

  std::array<bool, BUTTON_COUNT> m_down{};
  std::array<bool, BUTTON_COUNT> m_pressed{};
  std::int32_t m_xPixels = 0;
  std::int32_t m_yPixels = 0;
  std::int32_t m_wheelDelta = 0; // in Windows' units, WHEEL_DELTA to a notch
};

} // namespace Engine
