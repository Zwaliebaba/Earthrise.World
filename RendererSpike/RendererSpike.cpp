#include "pch.h"

#include "Window.h"

// The renderer spike (MvpPlan.md, M1). For now it opens its window and waits; the device, the
// voxel pipeline and the test scene follow, one task at a time (MvpPlan.md, the spike's estimate).
int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int)
{
  try
  {
    const Engine::Window window({.title = L"Earthrise.World: renderer spike", .clientWidthPixels = 1280, .clientHeightPixels = 720});
    while (Engine::Window::PumpMessages())
    {
      WaitMessage();
    }
    return 0;
  }
  catch (const std::exception& error)
  {
    MessageBoxA(nullptr, error.what(), "Earthrise.World: renderer spike", MB_ICONERROR | MB_OK);
    return 1;
  }
}
