#include "pch.h"

#include "Window.h"

namespace Engine
{

namespace
{

constexpr const wchar_t* CLASS_NAME = L"EarthriseWindow";
constexpr UINT DEFAULT_DPI = 96;

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
  constexpr DWORD STYLE = WS_OVERLAPPEDWINDOW;
  RECT frame{.left = 0,
             .top = 0,
             .right = MulDiv(static_cast<int>(_desc.clientWidthPixels), static_cast<int>(dpi), DEFAULT_DPI),
             .bottom = MulDiv(static_cast<int>(_desc.clientHeightPixels), static_cast<int>(dpi), DEFAULT_DPI)};
  if (!AdjustWindowRectExForDpi(&frame, STYLE, FALSE, 0, dpi))
  {
    ThrowLastError("AdjustWindowRectExForDpi");
  }

  m_handle = CreateWindowExW(0, CLASS_NAME, _desc.title, STYLE, CW_USEDEFAULT, CW_USEDEFAULT, frame.right - frame.left,
                             frame.bottom - frame.top, nullptr, nullptr, instance, nullptr);
  if (m_handle == nullptr)
  {
    ThrowLastError("CreateWindowExW");
  }
  ShowWindow(m_handle, SW_SHOWDEFAULT);
}

Window::~Window()
{
  if (m_handle != nullptr && IsWindow(m_handle))
  {
    DestroyWindow(m_handle);
  }
}

bool Window::PumpMessages() noexcept
{
  MSG message{};
  while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
  {
    if (message.message == WM_QUIT)
    {
      return false;
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
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  default:
    return DefWindowProcW(_window, _message, _wParam, _lParam);
  }
}

} // namespace Engine
