#pragma once

#include "framework.h"

#include <cstdint>

namespace Engine
{

class PointerInput;

/// A top-level window for the renderer: resizable, sized for the monitor's DPI, and closed by the
/// user or the system. Per-monitor DPI awareness comes from the executable's manifest (ADR-011).
class Window
{
public:
  struct Desc
  {
    const wchar_t* title;
    std::uint32_t clientWidthPixels; // at 96 DPI; scaled to the system's DPI
    std::uint32_t clientHeightPixels;
  };

  struct Size
  {
    std::uint32_t widthPixels;
    std::uint32_t heightPixels;

    friend constexpr bool operator==(const Size&, const Size&) = default;
  };

  /// Throws std::system_error when Windows refuses to create the window.
  explicit Window(const Desc& _desc);
  ~Window();
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
  Window(Window&&) = delete;
  Window& operator=(Window&&) = delete;

  [[nodiscard]] HWND Handle() const noexcept
  {
    return m_handle;
  }

  /// The client area in physical pixels; zero while the window is minimized.
  [[nodiscard]] Size ClientSize() const noexcept;

  /// Handles every pending message. Returns false once the application has been asked to quit.
  [[nodiscard]] static bool PumpMessages() noexcept;

  /// As above, and shows each message to _pointer before it is dispatched.
  [[nodiscard]] static bool PumpMessages(PointerInput& _pointer) noexcept;

private:
  [[nodiscard]] static bool Pump(PointerInput* _pointer) noexcept;
  static LRESULT CALLBACK Procedure(HWND _window, UINT _message, WPARAM _wParam, LPARAM _lParam) noexcept;

  HWND m_handle = nullptr;
};

} // namespace Engine
