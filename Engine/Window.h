#pragma once

#include "framework.h"

#include <cstdint>

namespace Engine
{

class KeyboardInput;
class PointerInput;

/// A top-level window for the renderer: resizable, sized for the monitor's DPI, and closed by the
/// user or the system. It is borderless over the whole monitor at full screen, never exclusive
/// (ADR-011, decision 1). Per-monitor DPI awareness comes from the executable's manifest.
class Window
{
public:
  struct Desc
  {
    const wchar_t* title;
    std::uint32_t clientWidthPixels; // at 96 DPI; scaled to the system's DPI
    std::uint32_t clientHeightPixels;
    bool fullScreen; // starts borderless over its monitor; the client size is then the window's when it leaves full screen
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

  /// Covers the monitor the window is on, borderless, or goes back to the frame and place it had.
  /// Throws std::system_error when Windows will not say where the window or its monitor is.
  void SetFullScreen(bool _fullScreen);
  [[nodiscard]] bool IsFullScreen() const noexcept;

  /// Handles every pending message. Returns false once the application has been asked to quit.
  [[nodiscard]] static bool PumpMessages() noexcept;

  /// As above, and shows each message to _pointer before it is dispatched.
  [[nodiscard]] static bool PumpMessages(PointerInput& _pointer) noexcept;

  /// As above, and shows each message to _pointer and _keyboard.
  [[nodiscard]] static bool PumpMessages(PointerInput& _pointer, KeyboardInput& _keyboard) noexcept;

private:
  [[nodiscard]] static bool Pump(PointerInput* _pointer, KeyboardInput* _keyboard) noexcept;
  static LRESULT CALLBACK Procedure(HWND _window, UINT _message, WPARAM _wParam, LPARAM _lParam) noexcept;

  HWND m_handle = nullptr;
  WINDOWPLACEMENT m_windowedPlacement{.length = sizeof(WINDOWPLACEMENT)}; // where to go back to from full screen
  bool m_fullScreen = false;
};

} // namespace Engine
