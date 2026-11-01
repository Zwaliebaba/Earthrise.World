#include "pch.h"

#include "CommandQueue.h"
#include "FrameRing.h"
#include "GpuDevice.h"
#include "KeyboardInput.h"
#include "PointerInput.h"
#include "ResourceBarrier.h"
#include "SwapChain.h"
#include "Window.h"

// Earthrise.World (ADR-020, decision 1): the executable that joins the client and the simulation in one process. In
// M3-C1 it opens the window and presents a cleared frame a second until it is closed; M3-C3 runs the breach lab in it.
//
// It starts borderless over the whole monitor (ADR-011, decision 1), and Alt+Enter switches between that and a window.
// `--windowed` starts it in a window.

namespace
{

constexpr const wchar_t* TITLE = L"Earthrise.World";
// A frame a second, or sooner when a message comes, until the lab draws.
constexpr DWORD FRAME_INTERVAL_MILLISECONDS = 1000;
// The renderer spike's sky, linear RGBA, through the back buffer's sRGB view.
constexpr std::array<float, 4> CLEAR_COLOR = {0.6724F, 0.6038F, 0.4910F, 1.0F};

bool HasFlag(std::wstring_view _commandLine, std::wstring_view _flag)
{
  std::wstring_view rest = _commandLine;
  while (!rest.empty())
  {
    const std::size_t end = rest.find(L' ');
    if (rest.substr(0, end) == _flag)
    {
      return true;
    }
    rest.remove_prefix(end == std::wstring_view::npos ? rest.size() : end + 1);
  }
  return false;
}

/// Waits for the GPU before the resources it may still read are destroyed, also when the loop leaves by an exception.
struct FlushOnExit
{
  Engine::CommandQueue& queue;

  ~FlushOnExit()
  {
    try
    {
      queue.Flush();
    }
    catch (...)
    {
      // The device is lost, so there is nothing left to wait for; say so to a debugger.
      OutputDebugStringW(L"Earthrise.World: the GPU was lost before exit, so its work was not waited for.\n");
    }
  }
};

int Run(bool _windowed)
{
  Engine::Window window({.title = TITLE, .clientWidthPixels = 1280, .clientHeightPixels = 720, .fullScreen = !_windowed});
  Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = false});
  Engine::SwapChain swapChain(device, window.Handle());
  Engine::FrameRing frames(device.Device(), device.DirectQueue());
  const FlushOnExit flushOnExit{device.DirectQueue()};
  Engine::PointerInput pointer;
  Engine::KeyboardInput keyboard;
  while (Engine::Window::PumpMessages(pointer, keyboard))
  {
    // A window that is not in front does not see its keys released, so it forgets them.
    if (GetForegroundWindow() != window.Handle())
    {
      pointer.ReleaseAll();
      keyboard.ReleaseAll();
    }
    // Alt+Enter switches between borderless full screen and a window (ADR-011, decision 1).
    if (keyboard.WasPressed(VK_RETURN) && keyboard.IsDown(VK_MENU))
    {
      window.SetFullScreen(!window.IsFullScreen());
    }
    const Engine::Window::Size size = window.ClientSize();
    if (size.widthPixels == 0 || size.heightPixels == 0)
    {
      // Minimized: nothing to draw until the window comes back.
      WaitMessage();
      continue;
    }
    if (size.widthPixels != swapChain.WidthPixels() || size.heightPixels != swapChain.HeightPixels())
    {
      device.DirectQueue().Flush();
      swapChain.Resize(size.widthPixels, size.heightPixels);
    }

    swapChain.WaitForFrame();
    ID3D12GraphicsCommandList* commandList = frames.Begin();
    const D3D12_RESOURCE_BARRIER toClear =
      Engine::TransitionBarrier(swapChain.BackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &toClear);
    commandList->ClearRenderTargetView(swapChain.BackBufferView(), CLEAR_COLOR.data(), 0, nullptr);
    const D3D12_RESOURCE_BARRIER toPresent =
      Engine::TransitionBarrier(swapChain.BackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &toPresent);
    static_cast<void>(frames.End());
    swapChain.Present(true);
    pointer.EndFrame();
    keyboard.EndFrame();
    static_cast<void>(MsgWaitForMultipleObjects(0, nullptr, FALSE, FRAME_INTERVAL_MILLISECONDS, QS_ALLINPUT));
  }
  return 0;
}

} // namespace

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR _commandLine, _In_ int)
{
  try
  {
    const std::wstring_view commandLine = _commandLine != nullptr ? _commandLine : L"";
    return Run(HasFlag(commandLine, L"--windowed"));
  }
  catch (const winrt::hresult_error& error)
  {
    MessageBoxW(nullptr, error.message().c_str(), TITLE, MB_ICONERROR | MB_OK);
  }
  catch (const std::exception& error)
  {
    MessageBoxA(nullptr, error.what(), "Earthrise.World", MB_ICONERROR | MB_OK);
  }
  catch (...)
  {
    MessageBoxW(nullptr, L"An unknown error stopped Earthrise.World.", TITLE, MB_ICONERROR | MB_OK);
  }
  return 1;
}
