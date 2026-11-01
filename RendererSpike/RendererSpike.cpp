#include "pch.h"

#include "CameraPath.h"
#include "ChunkStore.h"
#include "ColorTarget.h"
#include "CommandQueue.h"
#include "DepthTarget.h"
#include "FrameRing.h"
#include "FrameTimer.h"
#include "GpuDevice.h"
#include "GpuTimer.h"
#include "KeyboardInput.h"
#include "Measurement.h"
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
// It starts borderless over the whole monitor (ADR-011, decision 1), and Alt+Enter switches between
// that and a window. `--windowed` starts it in a window.
//
// `--render-scale 0.75` draws the scene at three quarters of the window's size in each direction;
// the default is 1 (ADR-011, decision 3).
//
// `--measure` flies the scripted camera path once without waiting for the vertical blank, with an
// edit published each second, and writes what it measured as CSV beside the executable, in
// Measurements\, before it exits (MvpPlan.md, M1 task 10; GameDesign.md, *Starting the MVP*). Its
// scene draws at the pass mark's 1920 x 1080 times the render scale, whatever the display's size,
// and is scaled to the display (the owner's choice of 2026-11-01).

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

// The camera target's height above the ground's datum, as the spike has always stood it.
constexpr float TARGET_HEIGHT_METERS = 6.0F;

// The measured run's camera path (GameDesign.md, *Starting the MVP*, the owner's choice of
// 2026-11-01): round a 48 m square about the scene at the default zoom, a quarter turn at each
// corner, four legs of 15 s stepped at 60 frames a second, and the first second left out as warm-up.
constexpr std::uint32_t PATH_FRAMES_PER_SECOND = 60;
constexpr float PATH_STEP_SECONDS = 1.0F / static_cast<float>(PATH_FRAMES_PER_SECOND);
constexpr std::uint32_t PATH_LEG_FRAMES = 15 * PATH_FRAMES_PER_SECOND;
constexpr float PATH_SIDE_METERS = 48.0F;
constexpr std::uint32_t WARM_UP_FRAMES = PATH_FRAMES_PER_SECOND;
// The scripted edit: until task 9's quarrying, the run digs out and puts back one ground voxel each
// second, half a second into it, so that edit-to-present latency has something to time. It is the
// top voxel of the ground at this column, inside the walls.
constexpr std::uint32_t EDIT_INTERVAL_FRAMES = PATH_FRAMES_PER_SECOND;
constexpr std::uint32_t EDIT_OFFSET_FRAMES = PATH_FRAMES_PER_SECOND / 2;
constexpr std::int32_t EDIT_X_VOXELS = 40;
constexpr std::int32_t EDIT_Z_VOXELS = 44;
constexpr std::int32_t EDIT_TOP_SEARCH_VOXELS = 64;

// The GPU timer's spans.
constexpr std::uint32_t VOXEL_PASS_SPAN = 0;
constexpr std::uint32_t SPAN_COUNT = 1;

struct Options
{
  float renderScale;
  bool measure;
  bool windowed;
};

/// The scene's size for a display _display pixels across: the render scale of the display, or for
/// a measured run of the pass mark's 1920 x 1080, so that the voxel pass does the pass mark's work
/// on any monitor.
Engine::Window::Size RenderSize(const Options& _options, const Engine::Window::Size& _display) noexcept
{
  if (_options.measure)
  {
    return {.widthPixels = Engine::ScaledPixels(Spike::Measurement::PASS_MARK_WIDTH_PIXELS, _options.renderScale),
            .heightPixels = Engine::ScaledPixels(Spike::Measurement::PASS_MARK_HEIGHT_PIXELS, _options.renderScale)};
  }
  return {.widthPixels = Engine::ScaledPixels(_display.widthPixels, _options.renderScale),
          .heightPixels = Engine::ScaledPixels(_display.heightPixels, _options.renderScale)};
}

/// An edit published but not yet seen on screen.
struct PendingEdit
{
  std::uint32_t frame;
  std::optional<std::uint32_t> presentCount; // DXGI's number for the present that first carried it
  std::int64_t refreshAtPublish;             // DXGI's last vertical blank when it was published; -1 if unknown
  std::int64_t publishedTicks;               // QueryPerformanceCounter
};

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

/// Whether the command line holds _flag as a word of its own.
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

std::int64_t PerformanceTicks() noexcept
{
  LARGE_INTEGER ticks{};
  QueryPerformanceCounter(&ticks);
  return ticks.QuadPart;
}

/// The folder the measured run writes into: Measurements\ beside the executable.
std::filesystem::path MeasurementFolder()
{
  std::wstring path(32768, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  if (length == 0 || length >= path.size())
  {
    throw std::runtime_error("the spike could not find where its executable is");
  }
  path.resize(length);
  return std::filesystem::path(path).parent_path() / L"Measurements";
}

/// The top voxel of the ground at the scripted edit's column, and what it holds.
std::pair<Protocol::VoxelPosition, std::uint8_t> EditVoxel(const Simulation::ChunkStore& _store)
{
  for (std::int32_t y = EDIT_TOP_SEARCH_VOXELS; y >= 0; --y)
  {
    const Protocol::VoxelPosition position{.xVoxels = EDIT_X_VOXELS, .yVoxels = y, .zVoxels = EDIT_Z_VOXELS};
    const std::uint8_t index = _store.At(position);
    if (index != 0)
    {
      return {position, index};
    }
  }
  throw std::runtime_error("the scripted edit's column holds no ground");
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

int Run(const Options& _options)
{
  const float renderScale = _options.renderScale;
  const bool measure = _options.measure;
  Engine::Window window({.title = TITLE, .clientWidthPixels = 1280, .clientHeightPixels = 720, .fullScreen = !_options.windowed});
  Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = false});
  Engine::SwapChain swapChain(device, window.Handle());
  // The scene draws at the render scale, and its depth with it (ADR-011, decision 3).
  const Engine::Window::Size firstRenderSize =
    RenderSize(_options, {.widthPixels = swapChain.WidthPixels(), .heightPixels = swapChain.HeightPixels()});
  Engine::ColorTarget sceneColor(device.Device(), {.format = SCENE_FORMAT, .clearColor = CLEAR_COLOR}, firstRenderSize.widthPixels,
                                 firstRenderSize.heightPixels);
  Engine::DepthTarget depth(device.Device(), sceneColor.WidthPixels(), sceneColor.HeightPixels());
  Engine::FrameRing frames(device.Device(), device.DirectQueue());
  Engine::UploadRing ring(device.Device(), UPLOAD_RING_BYTES);
  const Engine::VoxelPass pass(device.Device(), {.colorFormat = SCENE_FORMAT, .depthFormat = Engine::DepthTarget::FORMAT});
  Engine::UpscalePass upscale(device.Device(), {.targetFormat = Engine::SwapChain::VIEW_FORMAT});
  upscale.SetSource(sceneColor.Texture());
  const Engine::GpuTimer gpuTimer(device.Device(), device.DirectQueue().Queue(),
                                  {.slotCount = Engine::FrameRing::FRAMES_IN_FLIGHT, .spanCount = SPAN_COUNT});
  Spike::WorldView worldView(device.Device());
  const FlushOnExit flushOnExit{device.DirectQueue()};

  // The simulation's side: the scene goes into the chunk store, and the client draws only from what
  // the store publishes (ADR-002, decision 3).
  Simulation::ChunkStore store;
  const Spike::TestScene scene = Spike::BuildTestScene(store, SCENE_SEED);
  std::vector<Protocol::ChunkSnapshot> published = store.Publish();
  bool modelsPlaced = false;
  const std::array<DirectX::XMFLOAT4, Engine::VoxelPass::PALETTE_ENTRIES> palette = LinearPalette();

  Engine::OrthographicCamera camera({.elevationRadians = DirectX::XMConvertToRadians(ELEVATION_DEGREES),
                                     .azimuthRadians = DirectX::XMConvertToRadians(AZIMUTH_DEGREES),
                                     .viewHeightMeters = DEFAULT_VIEW_HEIGHT_METERS,
                                     .minViewHeightMeters = MIN_VIEW_HEIGHT_METERS,
                                     .maxViewHeightMeters = MAX_VIEW_HEIGHT_METERS,
                                     .rotationSeconds = ROTATION_SECONDS,
                                     .targetMeters = {scene.centerXMeters, TARGET_HEIGHT_METERS, scene.centerZMeters}});
  const Engine::CameraPath path({.centerMeters = {scene.centerXMeters, TARGET_HEIGHT_METERS, scene.centerZMeters},
                                 .sideMeters = PATH_SIDE_METERS,
                                 .legFrames = PATH_LEG_FRAMES});
  const auto [editPosition, editGround] = EditVoxel(store);
  Spike::Measurement measurement;
  std::vector<PendingEdit> pendingEdits;
  std::uint32_t pathFrame = 0;
  bool resizedDuringRun = false;
  // Each frame slot's row, waiting for its timestamps, which come back when the slot is next used.
  std::array<std::optional<Spike::Measurement::FrameRow>, Engine::FrameRing::FRAMES_IN_FLIGHT> waitingRows{};
  double voxelPassMilliseconds = 0.0;
  LARGE_INTEGER ticksPerSecond{};
  QueryPerformanceFrequency(&ticksPerSecond);
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
      const Engine::Window::Size renderSize = RenderSize(_options, size);
      sceneColor.Resize(renderSize.widthPixels, renderSize.heightPixels);
      depth.Resize(sceneColor.WidthPixels(), sceneColor.HeightPixels());
      upscale.SetSource(sceneColor.Texture());
      resizedDuringRun = resizedDuringRun || (measure && pathFrame > 0);
    }

    swapChain.WaitForFrame();
    const double lapSeconds = timer.LapSeconds();
    if (measure)
    {
      // The path, stepped by frame, and no input: every run sees the same views.
      const Engine::CameraPath::Pose pose = path.At(pathFrame);
      if (pose.turnStarts)
      {
        camera.Rotate(1);
      }
      camera.SetTargetMeters(pose.targetMeters);
      camera.Update(PATH_STEP_SECONDS);
      if (pathFrame >= WARM_UP_FRAMES && pathFrame % EDIT_INTERVAL_FRAMES == EDIT_OFFSET_FRAMES)
      {
        // Dig the voxel out, or put it back, and publish: the client sees it in this frame.
        store.Set(editPosition, store.At(editPosition) == 0 ? editGround : std::uint8_t{0});
        published = store.Publish();
        const std::optional<DXGI_FRAME_STATISTICS> statistics = swapChain.FrameStatistics();
        pendingEdits.push_back({.frame = pathFrame,
                                .presentCount = std::nullopt,
                                .refreshAtPublish = statistics ? static_cast<std::int64_t>(statistics->SyncRefreshCount) : -1,
                                .publishedTicks = PerformanceTicks()});
      }
    }
    else
    {
      Steer(camera, keyboard, pointer, pointer.XPixels() - lastXPixels, pointer.YPixels() - lastYPixels, swapChain.HeightPixels(),
            static_cast<float>(lapSeconds));
    }
    lastXPixels = pointer.XPixels();
    lastYPixels = pointer.YPixels();
    const std::uint64_t completed = device.DirectQueue().CompletedValue();
    ring.Reclaim(completed);
    worldView.Release(completed);

    const std::uint32_t slot = frames.Slot();
    ID3D12GraphicsCommandList* commandList = frames.Begin();
    // Begin waited for this slot's last frame, so its timestamps are back.
    if (waitingRows[slot])
    {
      waitingRows[slot]->voxelPassMilliseconds = gpuTimer.ReadMilliseconds(slot)[VOXEL_PASS_SPAN];
      voxelPassMilliseconds = waitingRows[slot]->voxelPassMilliseconds;
      if (measure)
      {
        measurement.AddFrame(*waitingRows[slot]);
      }
      waitingRows[slot].reset();
    }
    if (!published.empty())
    {
      worldView.TakeSnapshots(commandList, ring, published);
      published.clear();
    }
    if (!modelsPlaced)
    {
      worldView.PlaceModels(commandList, ring, scene.models);
      modelsPlaced = true;
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
    gpuTimer.Begin(commandList, slot, VOXEL_PASS_SPAN);
    pass.Begin(commandList, frameConstants, paletteConstants);
    worldView.Draw(commandList, pass);
    gpuTimer.End(commandList, slot, VOXEL_PASS_SPAN);

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
    gpuTimer.Resolve(commandList, slot);
    waitingRows[slot] = Spike::Measurement::FrameRow{.frame = pathFrame,
                                                     .azimuthDegrees = DirectX::XMConvertToDegrees(camera.AzimuthRadians()),
                                                     .voxelsDrawn = worldView.RecordsDrawn(),
                                                     .voxelPassMilliseconds = 0.0,
                                                     .frameMilliseconds = 1000.0 * lapSeconds};
    const std::uint64_t fenceValue = frames.End();
    ring.Retire(fenceValue);
    worldView.Retire(fenceValue);
    // A measured run does not wait for the vertical blank (the owner's choice of 2026-11-01).
    swapChain.Present(!measure);

    if (measure)
    {
      // Edit to present (the owner's choice of 2026-11-01): an edit is on screen once DXGI says a
      // present at or after the first one that carried it was shown.
      for (PendingEdit& edit : pendingEdits)
      {
        if (!edit.presentCount)
        {
          edit.presentCount = swapChain.LastPresentCount();
        }
      }
      const std::optional<DXGI_FRAME_STATISTICS> statistics = swapChain.FrameStatistics();
      if (statistics)
      {
        const std::int64_t now = PerformanceTicks();
        std::erase_if(
          pendingEdits,
          [&](const PendingEdit& _edit)
          {
            if (statistics->PresentCount < *_edit.presentCount)
            {
              return false;
            }
            measurement.AddEdit(
              {.frame = _edit.frame,
               .displayRefreshes =
                 _edit.refreshAtPublish < 0 ? -1 : static_cast<std::int64_t>(statistics->PresentRefreshCount) - _edit.refreshAtPublish,
               .milliseconds = 1000.0 * static_cast<double>(now - _edit.publishedTicks) / static_cast<double>(ticksPerSecond.QuadPart)});
            return true;
          });
      }
      ++pathFrame;
      if (pathFrame == path.FrameCount())
      {
        // The last two frames' timestamps, then the files.
        device.DirectQueue().Flush();
        for (std::uint32_t waitingSlot = 0; waitingSlot < Engine::FrameRing::FRAMES_IN_FLIGHT; ++waitingSlot)
        {
          if (waitingRows[waitingSlot])
          {
            waitingRows[waitingSlot]->voxelPassMilliseconds = gpuTimer.ReadMilliseconds(waitingSlot)[VOXEL_PASS_SPAN];
            measurement.AddFrame(*waitingRows[waitingSlot]);
          }
        }
        for (const PendingEdit& edit : pendingEdits)
        {
          measurement.AddEdit({.frame = edit.frame, .displayRefreshes = -1, .milliseconds = -1.0});
        }
        const std::wstring stem =
          std::format(L"RendererSpike-{:%Y%m%d-%H%M%S}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
        const std::wstring results = measurement.Write(
          MeasurementFolder(), stem,
          {.adapter = device.AdapterName(),
           .driverVersion = device.DriverVersion(),
           .displayWidthPixels = swapChain.WidthPixels(),
           .displayHeightPixels = swapChain.HeightPixels(),
           .renderWidthPixels = sceneColor.WidthPixels(),
           .renderHeightPixels = sceneColor.HeightPixels(),
           .renderScale = renderScale,
           .tearing = device.TearingSupported(),
           .resizedDuringRun = resizedDuringRun,
           .path = std::format(L"round a {:.0f} m square at a view height of {:.0f} m, a quarter turn at each corner, {} legs of {} frames "
                               L"stepped at {} a second",
                               PATH_SIDE_METERS, DEFAULT_VIEW_HEIGHT_METERS, Engine::CameraPath::LEG_COUNT, PATH_LEG_FRAMES,
                               PATH_FRAMES_PER_SECOND),
           .firstMeasuredFrame = WARM_UP_FRAMES});
        MessageBoxW(window.Handle(), results.c_str(), TITLE, MB_ICONINFORMATION | MB_OK);
        return 0;
      }
    }

    titleSeconds += lapSeconds;
    ++titleFrames;
    if (titleSeconds >= TITLE_REFRESH_SECONDS)
    {
      const std::wstring title =
        std::format(L"{} | {} | {:.2f} ms a frame, voxel pass {:.2f} ms on the GPU | {} voxels | render {} x {} at {:.2f} | view {:.0f} m, "
                    L"azimuth {:.0f}{}",
                    TITLE, device.AdapterName(), 1000.0 * titleSeconds / titleFrames, voxelPassMilliseconds, worldView.RecordsDrawn(),
                    sceneColor.WidthPixels(), sceneColor.HeightPixels(), renderScale, camera.ViewHeightMeters(),
                    DirectX::XMConvertToDegrees(camera.AzimuthRadians()),
                    measure ? std::format(L" | measuring, frame {} of {}", pathFrame, path.FrameCount()) : std::wstring{});
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
    const std::wstring_view commandLine = _commandLine != nullptr ? _commandLine : L"";
    return Run({.renderScale = RenderScaleFrom(commandLine),
                .measure = HasFlag(commandLine, L"--measure"),
                .windowed = HasFlag(commandLine, L"--windowed")});
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
