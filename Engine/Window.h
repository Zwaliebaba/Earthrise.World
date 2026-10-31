#pragma once

#include "framework.h"

#include <cstdint>

namespace Engine
{

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

  /// Handles every pending message. Returns false once the application has been asked to quit.
  [[nodiscard]] static bool PumpMessages() noexcept;

private:
  static LRESULT CALLBACK Procedure(HWND _window, UINT _message, WPARAM _wParam, LPARAM _lParam) noexcept;

  HWND m_handle = nullptr;
};

} // namespace Engine
