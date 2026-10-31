#include "pch.h"

#include "CommandQueue.h"
#include "FrameRing.h"
#include "FrameTimer.h"
#include "GpuDevice.h"
#include "PointerInput.h"
#include "ResourceBarrier.h"
#include "SwapChain.h"
#include "Window.h"

// The renderer spike (MvpPlan.md, M1). For now it brings up the engine base and clears each frame
// to the look's background; the voxel pipeline and the test scene follow, one task at a time
// (MvpPlan.md, the spike's estimate).

namespace
{

constexpr const wchar_t* TITLE = L"Earthrise.World: renderer spike";

// The Golden preset's clear colour, #d6ccba (RendererLook.md), in linear values, because the view
// that is cleared is sRGB and encodes on write.
constexpr float CLEAR_COLOR[4] = {0.6724F, 0.6038F, 0.4910F, 1.0F};

// The title shows the adapter, the mean frame time and the pointer, so the owner can see on screen
// that the right GPU is in use and that input arrives.
constexpr double TITLE_REFRESH_SECONDS = 0.5;

/// Waits for the GPU before the resources it may still read are destroyed, also when the loop
/// leaves by an exception.
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

int Run()
{
  Engine::Window window({.title = TITLE, .clientWidthPixels = 1280, .clientHeightPixels = 720});
  Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = false});
  Engine::SwapChain swapChain(device, window.Handle());
  Engine::FrameRing frames(device.Device(), device.DirectQueue());
  const FlushOnExit flushOnExit{device.DirectQueue()};
  Engine::PointerInput pointer;
  Engine::FrameTimer timer;

  double titleSeconds = 0.0;
  std::uint32_t titleFrames = 0;
  while (Engine::Window::PumpMessages(pointer))
  {
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
    const D3D12_RESOURCE_BARRIER toTarget =
      Engine::TransitionBarrier(swapChain.BackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &toTarget);
    commandList->ClearRenderTargetView(swapChain.BackBufferView(), CLEAR_COLOR, 0, nullptr);
    const D3D12_RESOURCE_BARRIER toPresent =
      Engine::TransitionBarrier(swapChain.BackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &toPresent);
    frames.End();
    swapChain.Present(true);

    titleSeconds += timer.LapSeconds();
    ++titleFrames;
    if (titleSeconds >= TITLE_REFRESH_SECONDS)
    {
      const std::wstring title =
        std::format(L"{} | {} | {:.2f} ms | pointer {}, {}{}", TITLE, device.AdapterName(), 1000.0 * titleSeconds / titleFrames,
                    pointer.XPixels(), pointer.YPixels(), pointer.IsDown(Engine::PointerInput::Button::Left) ? L" (held)" : L"");
      SetWindowTextW(window.Handle(), title.c_str());
      titleSeconds = 0.0;
      titleFrames = 0;
    }
    pointer.EndFrame();
  }
  return 0;
}

} // namespace

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int)
{
  try
  {
    return Run();
  }
  catch (const winrt::hresult_error& error)
  {
    MessageBoxW(nullptr, error.message().c_str(), TITLE, MB_ICONERROR | MB_OK);
  }
  catch (const std::exception& error)
  {
    MessageBoxA(nullptr, error.what(), "Earthrise.World: renderer spike", MB_ICONERROR | MB_OK);
  }
  catch (...)
  {
    MessageBoxW(nullptr, L"An unknown error stopped the renderer spike.", TITLE, MB_ICONERROR | MB_OK);
  }
  return 1;
}
