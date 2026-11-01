#include "pch.h"

#include "Window.h"

#include "KeyboardInput.h"
#include "PointerInput.h"

namespace Engine
{

namespace
{

constexpr const wchar_t* CLASS_NAME = L"EarthriseWindow";
constexpr UINT DEFAULT_DPI = 96;
constexpr DWORD WINDOWED_STYLE = WS_OVERLAPPEDWINDOW;
constexpr DWORD FULL_SCREEN_STYLE = WS_POPUP;

[[noreturn]] void ThrowLastError(const char* _what)
{
  throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), _what);
}

} // namespace

Window::Window(const Desc& _desc)
{
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  const WNDCLASSEXW windowClass{.cbSize = sizeof(WNDCLASSEXW),
                                .style = CS_HREDRAW | CS_VREDRAW,
                                .lpfnWndProc = &Window::Procedure,
                                .hInstance = instance,
                                .hCursor = LoadCursorW(nullptr, IDC_ARROW),
                                .lpszClassName = CLASS_NAME};
  if (RegisterClassExW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
  {
    ThrowLastError("RegisterClassExW");
  }

  // The requested client size is at 96 DPI; scale it, then grow it by the frame for that DPI.
  const UINT dpi = GetDpiForSystem();
  RECT frame{.left = 0,
             .top = 0,
             .right = MulDiv(static_cast<int>(_desc.clientWidthPixels), static_cast<int>(dpi), DEFAULT_DPI),
             .bottom = MulDiv(static_cast<int>(_desc.clientHeightPixels), static_cast<int>(dpi), DEFAULT_DPI)};
  if (!AdjustWindowRectExForDpi(&frame, WINDOWED_STYLE, FALSE, 0, dpi))
  {
    ThrowLastError("AdjustWindowRectExForDpi");
  }

  m_handle = CreateWindowExW(0, CLASS_NAME, _desc.title, WINDOWED_STYLE, CW_USEDEFAULT, CW_USEDEFAULT, frame.right - frame.left,
                             frame.bottom - frame.top, nullptr, nullptr, instance, nullptr);
  if (m_handle == nullptr)
  {
    ThrowLastError("CreateWindowExW");
  }
  ShowWindow(m_handle, SW_SHOWDEFAULT);
  // Made windowed first, so that leaving full screen has a frame and a place to go back to.
  if (_desc.fullScreen)
  {
    SetFullScreen(true);
  }
}

Window::~Window()
{
  if (m_handle != nullptr && IsWindow(m_handle))
  {
    DestroyWindow(m_handle);
  }
}

Window::Size Window::ClientSize() const noexcept
{
  RECT client{};
  if (!GetClientRect(m_handle, &client))
  {
    return {.widthPixels = 0, .heightPixels = 0};
  }
  return {.widthPixels = static_cast<std::uint32_t>(client.right - client.left),
          .heightPixels = static_cast<std::uint32_t>(client.bottom - client.top)};
}

void Window::SetFullScreen(bool _fullScreen)
{
  if (_fullScreen == m_fullScreen)
  {
    return;
  }
  if (_fullScreen)
  {
    MONITORINFO monitor{.cbSize = sizeof(MONITORINFO)};
    if (!GetWindowPlacement(m_handle, &m_windowedPlacement))
    {
      ThrowLastError("GetWindowPlacement");
    }
    if (!GetMonitorInfoW(MonitorFromWindow(m_handle, MONITOR_DEFAULTTONEAREST), &monitor))
    {
      ThrowLastError("GetMonitorInfoW");
    }
    SetWindowLongPtrW(m_handle, GWL_STYLE, static_cast<LONG_PTR>(FULL_SCREEN_STYLE | WS_VISIBLE));
    const RECT& area = monitor.rcMonitor;
    SetWindowPos(m_handle, HWND_TOP, area.left, area.top, area.right - area.left, area.bottom - area.top,
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  }
  else
  {
    SetWindowLongPtrW(m_handle, GWL_STYLE, static_cast<LONG_PTR>(WINDOWED_STYLE | WS_VISIBLE));
    SetWindowPlacement(m_handle, &m_windowedPlacement);
    SetWindowPos(m_handle, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  }
  m_fullScreen = _fullScreen;
}

bool Window::IsFullScreen() const noexcept
{
  return m_fullScreen;
}

std::uint32_t Window::Dpi() const noexcept
{
  return GetDpiForWindow(m_handle);
}

bool Window::PumpMessages() noexcept
{
  return Pump(nullptr, nullptr);
}

bool Window::PumpMessages(PointerInput& _pointer) noexcept
{
  return Pump(&_pointer, nullptr);
}

bool Window::PumpMessages(PointerInput& _pointer, KeyboardInput& _keyboard) noexcept
{
  return Pump(&_pointer, &_keyboard);
}

bool Window::Pump(PointerInput* _pointer, KeyboardInput* _keyboard) noexcept
{
  MSG message{};
  while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
  {
    if (message.message == WM_QUIT)
    {
      return false;
    }
    if (_pointer != nullptr)
    {
      _pointer->Observe(message);
    }
    if (_keyboard != nullptr)
    {
      _keyboard->Observe(message);
    }
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  return true;
}

LRESULT CALLBACK Window::Procedure(HWND _window, UINT _message, WPARAM _wParam, LPARAM _lParam) noexcept
{
  switch (_message)
  {
  case WM_DPICHANGED:
  {
    // Windows suggests the rectangle that keeps the window's physical size on the new monitor, and
    // passes it as a pointer in lParam. The cast is the Win32 contract, and what the check guards,
    // optimization lost to an integer's pointer provenance, does not arise in a message handler.
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    const RECT& suggested = *reinterpret_cast<const RECT*>(_lParam);
    SetWindowPos(_window, nullptr, suggested.left, suggested.top, suggested.right - suggested.left, suggested.bottom - suggested.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    return 0;
  }
  case WM_SYSCHAR:
    // Alt+Enter is the game's to handle (the swap chain tells DXGI to leave it alone), and without
    // this Windows would beep for a menu the window does not have.
    if (_wParam == VK_RETURN)
    {
      return 0;
    }
    return DefWindowProcW(_window, _message, _wParam, _lParam);
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  default:
    return DefWindowProcW(_window, _message, _wParam, _lParam);
  }
}

} // namespace Engine
