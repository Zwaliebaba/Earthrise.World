#include "pch.h"

#include "ChunkStore.h"
#include "ColorTarget.h"
#include "CommandQueue.h"
#include "DepthTarget.h"
#include "FrameRing.h"
#include "FrameTimer.h"
#include "GpuDevice.h"
#include "KeyboardInput.h"
#include "OrthographicCamera.h"
#include "Palette.h"
#include "PointerInput.h"
#include "ResourceBarrier.h"
#include "SceneScript.h"
#include "SwapChain.h"
#include "UploadRing.h"
#include "UpscalePass.h"
#include "VoxelPass.h"
#include "Window.h"
#include "WorldView.h"

// The renderer spike (MvpPlan.md, M1). It draws a first scene through the voxel pass: world chunks
// built from the simulation's snapshots and 0.5 m models in the same pass, at a render scale of its
// own, scaled to the window. The look and the v5 test scene follow, one task at a time.
//
// `--render-scale 0.75` draws the scene at three quarters of the window's size in each direction;
// the default is 1 (ADR-011, decision 3).

namespace
{

constexpr const wchar_t* TITLE = L"Earthrise.World: renderer spike";
constexpr std::uint64_t SCENE_SEED = 0x5eed2026ULL;
constexpr std::uint64_t UPLOAD_RING_BYTES = std::uint64_t{32} * 1024 * 1024;

// The Golden preset (RendererLook.md): its clear colour #d6ccba in linear values, because the view
// that is cleared is sRGB and encodes on write, and its sun direction, towards the light.
constexpr std::array<float, 4> CLEAR_COLOR = {0.6724F, 0.6038F, 0.4910F, 1.0F};
constexpr std::array<float, 3> SUN_TOWARDS = {-90.0F, 75.0F, 55.0F};

// The scene's target: 8 bits a channel through an sRGB view, as the back buffer, because the output
// is sRGB with no tone mapping (RendererLook.md). The upscale reads it through the same view.
constexpr DXGI_FORMAT SCENE_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
// The render scales the spike takes: down to a quarter of the window in each direction, and never
// above it, because the bilinear upscale does not filter a larger source down.
constexpr float MIN_RENDER_SCALE = 0.25F;
constexpr float MAX_RENDER_SCALE = 1.0F;

// The camera of RendererLook.md §7: orthographic at 34 degrees, standing to the south-west, turning
// in quarter turns eased over 0.3 s, from 12 m to 260 m of view height, 84 m by default. Azimuth is
// measured from +z (north) towards +x (east), so the south-west is -135 degrees.
constexpr float ELEVATION_DEGREES = 34.0F;
constexpr float AZIMUTH_DEGREES = -135.0F;
constexpr float DEFAULT_VIEW_HEIGHT_METERS = 84.0F;
constexpr float MIN_VIEW_HEIGHT_METERS = 12.0F;
constexpr float MAX_VIEW_HEIGHT_METERS = 260.0F;
constexpr float ROTATION_SECONDS = 0.3F;
// Keys pan the view by this many view heights a second.
constexpr float PAN_VIEWS_PER_SECOND = 0.8F;

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

float SrgbToLinear(float _value) noexcept
{
  return _value <= 0.04045F ? _value / 12.92F : std::pow((_value + 0.055F) / 1.055F, 2.4F);
}

std::array<DirectX::XMFLOAT4, Engine::VoxelPass::PALETTE_ENTRIES> LinearPalette() noexcept
{
  std::array<DirectX::XMFLOAT4, Engine::VoxelPass::PALETTE_ENTRIES> colors{};
  for (std::size_t index = 0; index < colors.size(); ++index)
  {
    const std::uint32_t srgb = Content::PaletteColorSrgb(static_cast<std::uint8_t>(index));
    const auto channel = [srgb](int _shift) { return SrgbToLinear(static_cast<float>((srgb >> _shift) & 0xffU) / 255.0F); };
    colors[index] = {channel(16), channel(8), channel(0), 1.0F};
  }
  return colors;
}

/// The render scale the command line asks for, as `--render-scale 0.75` or `--render-scale=0.75`,
/// and 1 when it asks for none. Throws std::invalid_argument for anything but a number from
/// MIN_RENDER_SCALE to MAX_RENDER_SCALE.
float RenderScaleFrom(std::wstring_view _commandLine)
{
  constexpr std::wstring_view OPTION = L"--render-scale";
  const std::size_t at = _commandLine.find(OPTION);
  if (at == std::wstring_view::npos)
  {
    return 1.0F;
  }
  std::wstring_view rest = _commandLine.substr(at + OPTION.size());
  if (rest.starts_with(L'='))
  {
    rest.remove_prefix(1);
  }
  while (rest.starts_with(L' '))
  {
    rest.remove_prefix(1);
  }
  std::string value;
  for (const wchar_t character : rest.substr(0, rest.find(L' ')))
  {
    value.push_back(character < 128 ? static_cast<char>(character) : '?');
  }
  float scale = 0.0F;
  const std::from_chars_result parsed = std::from_chars(value.data(), value.data() + value.size(), scale);
  // Written so that a NaN fails it too.
  const bool inRange = scale >= MIN_RENDER_SCALE && scale <= MAX_RENDER_SCALE;
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !inRange)
  {
    throw std::invalid_argument(
      std::format("--render-scale takes a number from {} to {}, and was given \"{}\".", MIN_RENDER_SCALE, MAX_RENDER_SCALE, value));
  }
  return scale;
}

/// The frame's constants for a target _widthPixels by _heightPixels that the display shows at
/// _displayAspect, width over height. The two aspects differ by the rounding of the render scale,
/// and the display's is the one the picture keeps.
Engine::VoxelPass::FrameConstants MakeFrame(const Engine::OrthographicCamera& _camera, float _displayAspect, std::uint32_t _widthPixels,
                                            std::uint32_t _heightPixels)
{
  const auto width = static_cast<float>(_widthPixels);
  const auto height = static_cast<float>(_heightPixels);
  const DirectX::XMMATRIX viewProjection = _camera.ViewProjection(_displayAspect);
  Engine::VoxelPass::FrameConstants frame{};
  DirectX::XMStoreFloat4x4(&frame.viewProjection, viewProjection);
  DirectX::XMStoreFloat4x4(&frame.inverseViewProjection, DirectX::XMMatrixInverse(nullptr, viewProjection));
  DirectX::XMStoreFloat4(&frame.rayDirection, _camera.Forward());
  frame.viewportPixels = {width, height, 1.0F / width, 1.0F / height};
  DirectX::XMStoreFloat4(&frame.lightDirection,
                         DirectX::XMVector3Normalize(DirectX::XMVectorSet(SUN_TOWARDS[0], SUN_TOWARDS[1], SUN_TOWARDS[2], 0.0F)));
  return frame;
}

/// The camera controls of Hud.md's global keys: WASD and the arrows or a middle-drag pan, the wheel
/// and + and - zoom, and [ ] or Page Up and Page Down turn a quarter.
void Steer(Engine::OrthographicCamera& _camera, const Engine::KeyboardInput& _keyboard, const Engine::PointerInput& _pointer,
           std::int32_t _dragXPixels, std::int32_t _dragYPixels, std::uint32_t _heightPixels, float _seconds)
{
  const auto held = [&_keyboard](std::uint8_t _first, std::uint8_t _second)
  { return _keyboard.IsDown(_first) || _keyboard.IsDown(_second) ? 1.0F : 0.0F; };
  const float speed = PAN_VIEWS_PER_SECOND * _camera.ViewHeightMeters() * _seconds;
  _camera.Pan(speed * (held('D', VK_RIGHT) - held('A', VK_LEFT)), speed * (held('W', VK_UP) - held('S', VK_DOWN)));
  if (_pointer.IsDown(Engine::PointerInput::Button::Middle))
  {
    // A drag moves the ground with the pointer. Up the screen covers more ground than across it,
    // by 1 / sin(elevation), because the view looks down at an angle.
    const float metersPerPixel = _camera.ViewHeightMeters() / static_cast<float>(_heightPixels);
    const float upScale = 1.0F / std::sin(DirectX::XMConvertToRadians(ELEVATION_DEGREES));
    _camera.Pan(-static_cast<float>(_dragXPixels) * metersPerPixel, static_cast<float>(_dragYPixels) * metersPerPixel * upScale);
  }
  const bool zoomIn = _keyboard.WasPressed(VK_OEM_PLUS) || _keyboard.WasPressed(VK_ADD);
  const bool zoomOut = _keyboard.WasPressed(VK_OEM_MINUS) || _keyboard.WasPressed(VK_SUBTRACT);
  _camera.Zoom(_pointer.WheelNotches() + (zoomIn ? 1.0F : 0.0F) - (zoomOut ? 1.0F : 0.0F));
  if (_keyboard.WasPressed(VK_OEM_4) || _keyboard.WasPressed(VK_PRIOR))
  {
    _camera.Rotate(-1);
  }
  if (_keyboard.WasPressed(VK_OEM_6) || _keyboard.WasPressed(VK_NEXT))
  {
    _camera.Rotate(1);
  }
  _camera.Update(_seconds);
}

/// Sets the viewport and the scissor to the whole of a target _widthPixels by _heightPixels.
void SetViewport(ID3D12GraphicsCommandList* _commandList, std::uint32_t _widthPixels, std::uint32_t _heightPixels) noexcept
{
  const D3D12_VIEWPORT viewport{.TopLeftX = 0.0F,
                                .TopLeftY = 0.0F,
                                .Width = static_cast<float>(_widthPixels),
                                .Height = static_cast<float>(_heightPixels),
                                .MinDepth = 0.0F,
                                .MaxDepth = 1.0F};
  const D3D12_RECT scissor{.left = 0, .top = 0, .right = static_cast<LONG>(_widthPixels), .bottom = static_cast<LONG>(_heightPixels)};
  _commandList->RSSetViewports(1, &viewport);
  _commandList->RSSetScissorRects(1, &scissor);
}

/// Copies _value into the ring for this frame, at the alignment constant buffers need.
template <typename T> D3D12_GPU_VIRTUAL_ADDRESS UploadConstants(Engine::UploadRing& _ring, const T& _value)
{
  const std::optional<Engine::UploadRing::Allocation> allocation =
    _ring.Allocate(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
  if (!allocation)
  {
    throw std::runtime_error("the upload ring is full");
  }
  std::memcpy(allocation->bytes.data(), &_value, sizeof(T));
  return allocation->gpuAddress;
}

int Run(float _renderScale)
{
  Engine::Window window({.title = TITLE, .clientWidthPixels = 1280, .clientHeightPixels = 720});
  Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = false});
  Engine::SwapChain swapChain(device, window.Handle());
  // The scene draws at the render scale, and its depth with it (ADR-011, decision 3).
  Engine::ColorTarget sceneColor(device.Device(), {.format = SCENE_FORMAT, .clearColor = CLEAR_COLOR},
                                 Engine::ScaledPixels(swapChain.WidthPixels(), _renderScale),
                                 Engine::ScaledPixels(swapChain.HeightPixels(), _renderScale));
  Engine::DepthTarget depth(device.Device(), sceneColor.WidthPixels(), sceneColor.HeightPixels());
  Engine::FrameRing frames(device.Device(), device.DirectQueue());
  Engine::UploadRing ring(device.Device(), UPLOAD_RING_BYTES);
  const Engine::VoxelPass pass(device.Device(), {.colorFormat = SCENE_FORMAT, .depthFormat = Engine::DepthTarget::FORMAT});
  Engine::UpscalePass upscale(device.Device(), {.targetFormat = Engine::SwapChain::VIEW_FORMAT});
  upscale.SetSource(sceneColor.Texture());
  Spike::WorldView worldView(device.Device());
  const FlushOnExit flushOnExit{device.DirectQueue()};

  // The simulation's side: the scene goes into the chunk store, and the client draws only from what
  // the store publishes (ADR-002, decision 3).
  Simulation::ChunkStore store;
  const Spike::TestScene scene = Spike::BuildTestScene(store, SCENE_SEED);
  std::vector<Protocol::ChunkSnapshot> published = store.Publish();
  const std::array<DirectX::XMFLOAT4, Engine::VoxelPass::PALETTE_ENTRIES> palette = LinearPalette();

  Engine::OrthographicCamera camera({.elevationRadians = DirectX::XMConvertToRadians(ELEVATION_DEGREES),
                                     .azimuthRadians = DirectX::XMConvertToRadians(AZIMUTH_DEGREES),
                                     .viewHeightMeters = DEFAULT_VIEW_HEIGHT_METERS,
                                     .minViewHeightMeters = MIN_VIEW_HEIGHT_METERS,
                                     .maxViewHeightMeters = MAX_VIEW_HEIGHT_METERS,
                                     .rotationSeconds = ROTATION_SECONDS,
                                     .targetMeters = {scene.centerXMeters, 6.0F, scene.centerZMeters}});
  Engine::PointerInput pointer;
  Engine::KeyboardInput keyboard;
  Engine::FrameTimer timer;
  std::int32_t lastXPixels = 0;
  std::int32_t lastYPixels = 0;
  double titleSeconds = 0.0;
  std::uint32_t titleFrames = 0;
  while (Engine::Window::PumpMessages(pointer, keyboard))
  {
    // A window that is not in front does not see its keys and buttons released, so it forgets them.
    if (GetForegroundWindow() != window.Handle())
    {
      pointer.ReleaseAll();
      keyboard.ReleaseAll();
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
      sceneColor.Resize(Engine::ScaledPixels(size.widthPixels, _renderScale), Engine::ScaledPixels(size.heightPixels, _renderScale));
      depth.Resize(sceneColor.WidthPixels(), sceneColor.HeightPixels());
      upscale.SetSource(sceneColor.Texture());
    }

    swapChain.WaitForFrame();
    const double lapSeconds = timer.LapSeconds();
    Steer(camera, keyboard, pointer, pointer.XPixels() - lastXPixels, pointer.YPixels() - lastYPixels, swapChain.HeightPixels(),
          static_cast<float>(lapSeconds));
    lastXPixels = pointer.XPixels();
    lastYPixels = pointer.YPixels();
    const std::uint64_t completed = device.DirectQueue().CompletedValue();
    ring.Reclaim(completed);
    worldView.Release(completed);

    ID3D12GraphicsCommandList* commandList = frames.Begin();
    if (!published.empty())
    {
      worldView.TakeSnapshots(commandList, ring, published);
      worldView.PlaceModels(commandList, ring, scene.models);
      published.clear();
    }
    const float displayAspect = static_cast<float>(swapChain.WidthPixels()) / static_cast<float>(swapChain.HeightPixels());
    const D3D12_GPU_VIRTUAL_ADDRESS frameConstants =
      UploadConstants(ring, MakeFrame(camera, displayAspect, sceneColor.WidthPixels(), sceneColor.HeightPixels()));
    const D3D12_GPU_VIRTUAL_ADDRESS paletteConstants = UploadConstants(ring, palette);

    // The scene, at the render scale, into its own target.
    const D3D12_RESOURCE_BARRIER toScene =
      Engine::TransitionBarrier(sceneColor.Texture(), Engine::ColorTarget::IDLE_STATE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &toScene);
    const D3D12_CPU_DESCRIPTOR_HANDLE sceneView = sceneColor.View();
    const D3D12_CPU_DESCRIPTOR_HANDLE depthView = depth.View();
    commandList->OMSetRenderTargets(1, &sceneView, FALSE, &depthView);
    sceneColor.Clear(commandList);
    depth.Clear(commandList);
    SetViewport(commandList, sceneColor.WidthPixels(), sceneColor.HeightPixels());
    pass.Begin(commandList, frameConstants, paletteConstants);
    worldView.Draw(commandList, pass);

    // Then scaled to the back buffer, which the triangle covers whole, so it needs no clear. The
    // interface will draw after it, at the display's resolution.
    const std::array<D3D12_RESOURCE_BARRIER, 2> toUpscale = {
      Engine::TransitionBarrier(sceneColor.Texture(), D3D12_RESOURCE_STATE_RENDER_TARGET, Engine::ColorTarget::IDLE_STATE),
      Engine::TransitionBarrier(swapChain.BackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)};
    commandList->ResourceBarrier(static_cast<UINT>(toUpscale.size()), toUpscale.data());
    const D3D12_CPU_DESCRIPTOR_HANDLE backBufferView = swapChain.BackBufferView();
    commandList->OMSetRenderTargets(1, &backBufferView, FALSE, nullptr);
    SetViewport(commandList, swapChain.WidthPixels(), swapChain.HeightPixels());
    upscale.Draw(commandList);
    const D3D12_RESOURCE_BARRIER toPresent =
      Engine::TransitionBarrier(swapChain.BackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &toPresent);
    const std::uint64_t fenceValue = frames.End();
    ring.Retire(fenceValue);
    worldView.Retire(fenceValue);
    swapChain.Present(true);

    titleSeconds += lapSeconds;
    ++titleFrames;
    if (titleSeconds >= TITLE_REFRESH_SECONDS)
    {
      const std::wstring title = std::format(L"{} | {} | {:.2f} ms | {} voxels | render {} x {} at {:.2f} | view {:.0f} m, azimuth {:.0f}",
                                             TITLE, device.AdapterName(), 1000.0 * titleSeconds / titleFrames, worldView.RecordsDrawn(),
                                             sceneColor.WidthPixels(), sceneColor.HeightPixels(), _renderScale, camera.ViewHeightMeters(),
                                             DirectX::XMConvertToDegrees(camera.AzimuthRadians()));
      SetWindowTextW(window.Handle(), title.c_str());
      titleSeconds = 0.0;
      titleFrames = 0;
    }
    pointer.EndFrame();
    keyboard.EndFrame();
  }
  return 0;
}

} // namespace

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR _commandLine, _In_ int)
{
  try
  {
    return Run(RenderScaleFrom(_commandLine != nullptr ? _commandLine : L""));
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
