#pragma once

#include "framework.h"

#include <bitset>
#include <cstdint>

namespace Engine
{

/// The keyboard, as the mouse-and-keyboard scheme reads it (ADR-007): which virtual keys are held,
/// and which went down this frame. It reads the messages Window::PumpMessages takes from the queue.
class KeyboardInput
{
public:
  /// Reads one queued message. Anything that is not a key going down or up is ignored, and so is
  /// a key's auto-repeat.
  void Observe(const MSG& _message) noexcept;

  /// Clears this frame's presses. Call once the frame has used them.
  void EndFrame() noexcept;

  /// Forgets every held key, for when the window loses the keyboard and will not see the releases.
  void ReleaseAll() noexcept;

  [[nodiscard]] bool IsDown(std::uint8_t _virtualKey) const noexcept;
  [[nodiscard]] bool WasPressed(std::uint8_t _virtualKey) const noexcept;

private:
  std::bitset<256> m_down;
  std::bitset<256> m_pressed;
};

} // namespace Engine
