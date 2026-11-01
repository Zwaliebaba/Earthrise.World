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
#include "ShadowMap.h"
#include "SunProjection.h"
#include "SwapChain.h"
#include "UploadRing.h"
#include "UpscalePass.h"
#include "VoxelPass.h"
#include "Window.h"
#include "WorldView.h"

// The renderer spike (MvpPlan.md, M1). It draws the v5 test scene through the voxel pass: world
// chunks built from the simulation's snapshots and 0.5 m models in the same pass, at a render scale
// of its own, scaled to the window. For the first 60 s the scene runs its scripted edits, quarrying
// and a wall collapse; its units walk for as long as it runs (MvpPlan.md, M1 task 9). The look
// follows.
//
// It starts borderless over the whole monitor (ADR-011, decision 1), and Alt+Enter switches between
// that and a window. `--windowed` starts it in a window.
//
// `--render-scale 0.75` draws the scene at three quarters of the window's size in each direction;
// the default is 1 (ADR-011, decision 3).
//
// `--measure` flies the scripted camera path once without waiting for the vertical blank, while the
// scene's edits run, and writes what it measured as CSV beside the executable, in
// Measurements\, before it exits (MvpPlan.md, M1 task 10; GameDesign.md, *Starting the MVP*). Its
// scene draws at the pass mark's 1920 x 1080 times the render scale, whatever the display's size,
// and is scaled to the display (the owner's choice of 2026-11-01).
//
// `--half-meter-world` draws the 0.5 m world of ADR-006's comparison instead: the same world with
// each voxel split into eight, its edits split with it (GameDesign.md, M1's open values, item 8;
// MvpPlan.md, M1 task 12). The units and the camera are the same, so the two runs differ only in
// the world's voxel size.

namespace
{

constexpr const wchar_t* TITLE = L"Earthrise.World: renderer spike";
constexpr std::uint64_t SCENE_SEED = 0x5eed2026ULL;
constexpr std::uint64_t UPLOAD_RING_BYTES = std::uint64_t{32} * 1024 * 1024;

// The Golden preset (RendererLook.md §6): its clear colour #d6ccba in linear values, because the
// view that is cleared is sRGB and encodes on write, its sun direction, towards the light, and its
// lights. Their intensities are three.js r160's physical ones: r160 divides its Lambert term by pi
// and no longer scales its lights by pi (WebGLRenderer.useLegacyLights is false), so they are
// divided by pi here, as RendererLook.md expected.
constexpr std::array<float, 4> CLEAR_COLOR = {0.6724F, 0.6038F, 0.4910F, 1.0F};
constexpr std::array<float, 3> SUN_TOWARDS = {-90.0F, 75.0F, 55.0F};
constexpr std::uint32_t SUN_SRGB = 0xffd8a6;
constexpr float SUN_INTENSITY = 2.7F;
constexpr std::uint32_t SKY_SRGB = 0xd4dbe6;
constexpr std::uint32_t GROUND_SRGB = 0x5a4a38;
constexpr float HEMISPHERE_INTENSITY = 1.0F;
constexpr float PI = 3.14159265F;
constexpr std::uint8_t WATER_INDEX = 9;

// The sun's shadow maps (ADR-011, decision 9; RendererLook.md §6): the world's, drawn when it
// changes, and the models', drawn each frame, both over the whole world with a margin. Over the
// 128 m scene a static-map texel is about 5 cm. Under the Golden sun, 35 degrees up, ground's depth
// in the sun's view changes by about 1.4 texels' worth a texel, so the filter's 1.5-texel reach spans
// about 10 cm of depth. A hit is moved 10 cm out along its normal before it reads the maps, which
// puts it 17 cm above the ground along the sun's ray, and its depth gets no further bias. These are
// first values, tuned against check 11 (ADR-011, *Left open*).
constexpr std::uint32_t STATIC_SHADOW_TEXELS = 4096;
constexpr std::uint32_t DYNAMIC_SHADOW_TEXELS = 2048;
constexpr float SHADOW_MARGIN_METERS = 2.0F;
constexpr float SHADOW_NORMAL_OFFSET_METERS = 0.1F;
constexpr float SHADOW_DEPTH_BIAS = 0.0F;

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

// The measured run's camera path (GameDesign.md, *Starting the MVP*, the owner's choice of
// 2026-11-01): round a 48 m square about the scene at the default zoom, a quarter turn at each
// corner, four legs of 15 s stepped at 60 frames a second, and the first second left out as warm-up.
constexpr std::uint32_t PATH_FRAMES_PER_SECOND = 60;
constexpr float PATH_STEP_SECONDS = 1.0F / static_cast<float>(PATH_FRAMES_PER_SECOND);
constexpr std::uint32_t PATH_LEG_FRAMES = 15 * PATH_FRAMES_PER_SECOND;
constexpr float PATH_SIDE_METERS = 48.0F;
constexpr std::uint32_t WARM_UP_FRAMES = PATH_FRAMES_PER_SECOND;
// The scene's scripted edits publish every frame. Edit-to-present latency is timed on one
// publication a second, half a second into it.
constexpr std::uint32_t LATENCY_INTERVAL_FRAMES = PATH_FRAMES_PER_SECOND;
constexpr std::uint32_t LATENCY_OFFSET_FRAMES = PATH_FRAMES_PER_SECOND / 2;
static_assert(PATH_FRAMES_PER_SECOND == Simulation::TestWorldEdits::FRAMES_PER_SECOND, "the path and the scene's edits step together");

// The GPU timer's spans. Every frame opens and closes each of them, the static shadow map's empty
// on a frame that does not redraw it, because a span left out reads as nonsense (GpuTimer.h).
constexpr std::uint32_t VOXEL_PASS_SPAN = 0;
constexpr std::uint32_t STATIC_SHADOW_SPAN = 1;
constexpr std::uint32_t DYNAMIC_SHADOW_SPAN = 2;
constexpr std::uint32_t SPAN_COUNT = 3;

struct Options
{
  float renderScale;
  bool measure;
  bool windowed;
  bool halfMeterWorld;
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

DirectX::XMFLOAT3 LinearOf(std::uint32_t _srgb) noexcept
{
  const auto channel = [_srgb](int _shift) { return SrgbToLinear(static_cast<float>((_srgb >> _shift) & 0xffU) / 255.0F); };
  return {channel(16), channel(8), channel(0)};
}

/// A light's radiance as the voxel pass takes it: linear, times its intensity, over pi.
DirectX::XMFLOAT4 Radiance(std::uint32_t _srgb, float _intensity) noexcept
{
  const DirectX::XMFLOAT3 color = LinearOf(_srgb);
  const float scale = _intensity / PI;
  return {color.x * scale, color.y * scale, color.z * scale, 0.0F};
}

/// A linear colour with its HSL lightness scaled, hue and saturation kept: three.js r160's getHSL
/// and setHSL, as the voxel pass's jitter uses them (Shader/VoxelCommon.hlsli).
DirectX::XMFLOAT4 ScaledLightness(DirectX::XMFLOAT4 _color, float _scale) noexcept
{
  const float maxChannel = std::max({_color.x, _color.y, _color.z});
  const float minChannel = std::min({_color.x, _color.y, _color.z});
  const float lightness = std::clamp(0.5F * (maxChannel + minChannel) * _scale, 0.0F, 1.0F);
  if (maxChannel == minChannel)
  {
    return {lightness, lightness, lightness, _color.w};
  }
  const float delta = maxChannel - minChannel;
  const float oldLightness = 0.5F * (maxChannel + minChannel);
  const float saturation = oldLightness <= 0.5F ? delta / (maxChannel + minChannel) : delta / (2.0F - maxChannel - minChannel);
  float hue = 0.0F;
  if (maxChannel == _color.x)
  {
    hue = (_color.y - _color.z) / delta + (_color.y < _color.z ? 6.0F : 0.0F);
  }
  else if (maxChannel == _color.y)
  {
    hue = (_color.z - _color.x) / delta + 2.0F;
  }
  else
  {
    hue = (_color.x - _color.y) / delta + 4.0F;
  }
  hue /= 6.0F;
  const float p = lightness <= 0.5F ? lightness * (1.0F + saturation) : lightness + saturation - lightness * saturation;
  const float q = 2.0F * lightness - p;
  const auto channel = [p, q](float _t)
  {
    const float t = _t < 0.0F ? _t + 1.0F : (_t > 1.0F ? _t - 1.0F : _t);
    if (t < 1.0F / 6.0F)
    {
      return q + (p - q) * 6.0F * t;
    }
    if (t < 0.5F)
    {
      return p;
    }
    if (t < 2.0F / 3.0F)
    {
      return q + (p - q) * 6.0F * (2.0F / 3.0F - t);
    }
    return q;
  };
  return {channel(hue + 1.0F / 3.0F), channel(hue), channel(hue - 1.0F / 3.0F), _color.w};
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
  // The two sides' faction colours, in entries beyond the materials'.
  for (const auto& [index, srgb] : {std::pair{Spike::ATTACKER_COLOR_INDEX, Spike::ATTACKER_COLOR_SRGB},
                                    std::pair{Spike::DEFENDER_COLOR_INDEX, Spike::DEFENDER_COLOR_SRGB}})
  {
    const std::uint32_t side = srgb;
    const auto channel = [side](int _shift) { return SrgbToLinear(static_cast<float>((side >> _shift) & 0xffU) / 255.0F); };
    colors[index] = {channel(16), channel(8), channel(0), 1.0F};
  }
  // Fords: water one world voxel deep, drawn lighter (RendererLook.md §3).
  colors[Spike::FORD_COLOR_INDEX] = ScaledLightness(colors[WATER_INDEX], Spike::FORD_LIGHTNESS_SCALE);
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

DirectX::XMVECTOR SunTowards() noexcept
{
  return DirectX::XMVector3Normalize(DirectX::XMVectorSet(SUN_TOWARDS[0], SUN_TOWARDS[1], SUN_TOWARDS[2], 0.0F));
}

/// The sun's frame for a shadow map _edgeTexels across: its view-projection, the ray away from the
/// sun, and the map as the viewport.
Engine::VoxelPass::FrameConstants MakeSunFrame(const DirectX::XMFLOAT4X4& _sunViewProjection, std::uint32_t _edgeTexels) noexcept
{
  Engine::VoxelPass::FrameConstants frame{};
  frame.viewProjection = _sunViewProjection;
  DirectX::XMStoreFloat4x4(&frame.inverseViewProjection, DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&_sunViewProjection)));
  DirectX::XMStoreFloat4(&frame.rayDirection, DirectX::XMVectorNegate(SunTowards()));
  const auto edge = static_cast<float>(_edgeTexels);
  frame.viewportPixels = {edge, edge, 1.0F / edge, 1.0F / edge};
  return frame;
}

/// The frame's constants for a target _widthPixels by _heightPixels that the display shows at
/// _displayAspect, width over height. The two aspects differ by the rounding of the render scale,
/// and the display's is the one the picture keeps.
Engine::VoxelPass::FrameConstants MakeFrame(const Engine::OrthographicCamera& _camera, float _displayAspect, std::uint32_t _widthPixels,
                                            std::uint32_t _heightPixels, const DirectX::XMFLOAT4X4& _sunViewProjection)
{
  const auto width = static_cast<float>(_widthPixels);
  const auto height = static_cast<float>(_heightPixels);
  const DirectX::XMMATRIX viewProjection = _camera.ViewProjection(_displayAspect);
  Engine::VoxelPass::FrameConstants frame{};
  DirectX::XMStoreFloat4x4(&frame.viewProjection, viewProjection);
  DirectX::XMStoreFloat4x4(&frame.inverseViewProjection, DirectX::XMMatrixInverse(nullptr, viewProjection));
  DirectX::XMStoreFloat4(&frame.rayDirection, _camera.Forward());
  frame.viewportPixels = {width, height, 1.0F / width, 1.0F / height};
  DirectX::XMStoreFloat4(&frame.lightDirection, SunTowards());
  frame.shadowViewProjection = _sunViewProjection;
  frame.shadowParameters = {SHADOW_NORMAL_OFFSET_METERS, SHADOW_DEPTH_BIAS, 1.0F / static_cast<float>(STATIC_SHADOW_TEXELS),
                            1.0F / static_cast<float>(DYNAMIC_SHADOW_TEXELS)};
  frame.sunRadiance = Radiance(SUN_SRGB, SUN_INTENSITY);
  frame.skyRadiance = Radiance(SKY_SRGB, HEMISPHERE_INTENSITY);
  frame.groundRadiance = Radiance(GROUND_SRGB, HEMISPHERE_INTENSITY);
  // The record's shade-table bit is Content's corner-shade set: 0 structure, 1 terrain.
  for (const Content::CornerShadeSet set : {Content::CornerShadeSet::Structure, Content::CornerShadeSet::Terrain})
  {
    const std::array<float, Content::CORNER_SHADE_LEVELS> shades = Content::CornerShades(set);
    frame.cornerShades.at(static_cast<std::size_t>(set)) = {shades[0], shades[1], shades[2], shades[3]};
  }
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

/// Takes the row waiting in a frame slot, if there is one, with the voxel pass's GPU time from the
/// slot's timestamps, which are back once the slot comes round again or the queue is flushed.
std::optional<Spike::Measurement::FrameRow> TakeWaitingRow(std::optional<Spike::Measurement::FrameRow>& _waiting,
                                                           const Engine::GpuTimer& _timer, std::uint32_t _slot)
{
  std::optional<Spike::Measurement::FrameRow> row = std::exchange(_waiting, std::nullopt);
  if (row.has_value())
  {
    const std::vector<double> spans = _timer.ReadMilliseconds(_slot);
    row->voxelPassMilliseconds = spans[VOXEL_PASS_SPAN];
    row->shadowPassMilliseconds = spans[STATIC_SHADOW_SPAN] + spans[DYNAMIC_SHADOW_SPAN];
  }
  return row;
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
  Engine::VoxelPass pass(device.Device(), {.colorFormat = SCENE_FORMAT, .depthFormat = Engine::DepthTarget::FORMAT});
  Engine::ShadowMap staticShadow(device.Device(), STATIC_SHADOW_TEXELS);
  Engine::ShadowMap dynamicShadow(device.Device(), DYNAMIC_SHADOW_TEXELS);
  pass.SetShadowMaps(staticShadow.Texture(), dynamicShadow.Texture());
  Engine::UpscalePass upscale(device.Device(), {.targetFormat = Engine::SwapChain::VIEW_FORMAT});
  upscale.SetSource(sceneColor.Texture());
  const Engine::GpuTimer gpuTimer(device.Device(), device.DirectQueue().Queue(),
                                  {.slotCount = Engine::FrameRing::FRAMES_IN_FLIGHT, .spanCount = SPAN_COUNT});
  const float worldVoxelMeters = _options.halfMeterWorld ? 0.5F : 1.0F;
  Spike::WorldView worldView(device.Device(), worldVoxelMeters);
  const FlushOnExit flushOnExit{device.DirectQueue()};

  // The simulation's side: the scene goes into the chunk store, and the client draws only from what
  // the store publishes (ADR-002, decision 3). For the 0.5 m world the client draws from a second
  // store, the first split eight ways, which takes every edit the first makes.
  Simulation::ChunkStore store;
  const Spike::TestScene scene = Spike::BuildTestScene(store, SCENE_SEED);
  Simulation::ChunkStore halfMeterStore;
  if (_options.halfMeterWorld)
  {
    Simulation::SplitToHalfMeters(store, 0, 0, scene.world.edgeVoxels - 1, scene.world.edgeVoxels - 1, halfMeterStore);
  }
  Simulation::ChunkStore& shownStore = _options.halfMeterWorld ? halfMeterStore : store;
  std::vector<Protocol::ChunkSnapshot> published = shownStore.Publish();
  Simulation::TestWorldEdits edits(store, scene.world, SCENE_SEED);
  // The sun's view over the whole world, its height included, for both shadow maps.
  const auto worldEdgeMeters = static_cast<float>(scene.world.edgeVoxels);
  DirectX::XMFLOAT4X4 sunViewProjection{};
  DirectX::XMStoreFloat4x4(
    &sunViewProjection,
    Engine::SunViewProjection(SunTowards(), DirectX::XMFLOAT3{0.0F, 0.0F, 0.0F},
                              DirectX::XMFLOAT3{worldEdgeMeters, static_cast<float>(Simulation::TestWorld::HEIGHT_VOXELS), worldEdgeMeters},
                              SHADOW_MARGIN_METERS));
  std::vector<Spike::ModelPlacement> placements;
  // The frame the scene's edits and walkers are at: the path's in a measured run, else counted from
  // the start.
  std::uint32_t sceneFrame = 0;
  const std::array<DirectX::XMFLOAT4, Engine::VoxelPass::PALETTE_ENTRIES> palette = LinearPalette();

  Engine::OrthographicCamera camera({.elevationRadians = DirectX::XMConvertToRadians(ELEVATION_DEGREES),
                                     .azimuthRadians = DirectX::XMConvertToRadians(AZIMUTH_DEGREES),
                                     .viewHeightMeters = DEFAULT_VIEW_HEIGHT_METERS,
                                     .minViewHeightMeters = MIN_VIEW_HEIGHT_METERS,
                                     .maxViewHeightMeters = MAX_VIEW_HEIGHT_METERS,
                                     .rotationSeconds = ROTATION_SECONDS,
                                     .targetMeters = {scene.centerXMeters, scene.centerYMeters, scene.centerZMeters}});
  const Engine::CameraPath path({.centerMeters = {scene.centerXMeters, scene.centerYMeters, scene.centerZMeters},
                                 .sideMeters = PATH_SIDE_METERS,
                                 .legFrames = PATH_LEG_FRAMES});
  Spike::Measurement measurement;
  std::vector<PendingEdit> pendingEdits;
  std::uint32_t pathFrame = 0;
  bool resizedDuringRun = false;
  // Each frame slot's row, waiting for its timestamps, which come back when the slot is next used.
  std::array<std::optional<Spike::Measurement::FrameRow>, Engine::FrameRing::FRAMES_IN_FLIGHT> waitingRows{};
  double voxelPassMilliseconds = 0.0;
  double shadowPassMilliseconds = 0.0;
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
      sceneFrame = pathFrame;
    }
    else
    {
      Steer(camera, keyboard, pointer, pointer.XPixels() - lastXPixels, pointer.YPixels() - lastYPixels, swapChain.HeightPixels(),
            static_cast<float>(lapSeconds));
    }
    lastXPixels = pointer.XPixels();
    lastYPixels = pointer.YPixels();

    // The scene's scripted edits: the quarrying, and the collapse (task 9). The client sees them in
    // this frame, from what the store publishes (ADR-002, decision 3).
    const auto editStart = std::chrono::steady_clock::now();
    const std::vector<Protocol::VoxelPosition> changedVoxels = edits.Apply(store, sceneFrame);
    if (!changedVoxels.empty())
    {
      if (_options.halfMeterWorld)
      {
        Simulation::SplitChanges(store, changedVoxels, halfMeterStore);
      }
      std::vector<Protocol::ChunkSnapshot> changed = shownStore.Publish();
      published.insert(published.end(), changed.begin(), changed.end());
      if (measure && sceneFrame >= WARM_UP_FRAMES && sceneFrame % LATENCY_INTERVAL_FRAMES == LATENCY_OFFSET_FRAMES)
      {
        const std::optional<DXGI_FRAME_STATISTICS> statistics = swapChain.FrameStatistics();
        pendingEdits.push_back({.frame = sceneFrame,
                                .presentCount = std::nullopt,
                                .refreshAtPublish = statistics ? static_cast<std::int64_t>(statistics->SyncRefreshCount) : -1,
                                .publishedTicks = PerformanceTicks()});
      }
    }
    const std::chrono::duration<double, std::milli> editTime = std::chrono::steady_clock::now() - editStart;
    placements = scene.fixedModels;
    Spike::PlaceWalkers(store, scene, sceneFrame, placements);
    const std::uint64_t completed = device.DirectQueue().CompletedValue();
    ring.Reclaim(completed);
    worldView.Release(completed);

    const std::uint32_t slot = frames.Slot();
    ID3D12GraphicsCommandList* commandList = frames.Begin();
    // Begin waited for this slot's last frame, so its timestamps are back.
    if (const std::optional<Spike::Measurement::FrameRow> row = TakeWaitingRow(waitingRows[slot], gpuTimer, slot))
    {
      voxelPassMilliseconds = row->voxelPassMilliseconds;
      shadowPassMilliseconds = row->shadowPassMilliseconds;
      if (measure)
      {
        measurement.AddFrame(*row);
      }
    }
    const auto rebuildStart = std::chrono::steady_clock::now();
    const bool worldChanged = !published.empty();
    if (worldChanged)
    {
      worldView.TakeSnapshots(commandList, ring, published);
      published.clear();
    }
    // What the edits cost the CPU: the store's edit and publication, and the chunks' rebuilt records
    // with their neighbours' (GameDesign.md, the renderer spike's question 5).
    const double rebuildMilliseconds =
      editTime.count() + std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - rebuildStart).count();
    worldView.PlaceModels(commandList, ring, placements);
    const float displayAspect = static_cast<float>(swapChain.WidthPixels()) / static_cast<float>(swapChain.HeightPixels());
    const D3D12_GPU_VIRTUAL_ADDRESS frameConstants =
      UploadConstants(ring, MakeFrame(camera, displayAspect, sceneColor.WidthPixels(), sceneColor.HeightPixels(), sunViewProjection));
    const D3D12_GPU_VIRTUAL_ADDRESS paletteConstants = UploadConstants(ring, palette);

    // The sun's shadow maps: the world's when it changed, the models' every frame (ADR-011,
    // decision 9). Both are timed, and the pass mark counts them with the voxel pass.
    gpuTimer.Begin(commandList, slot, STATIC_SHADOW_SPAN);
    if (worldChanged || !staticShadow.Readable())
    {
      const D3D12_GPU_VIRTUAL_ADDRESS staticSunFrame = UploadConstants(ring, MakeSunFrame(sunViewProjection, STATIC_SHADOW_TEXELS));
      staticShadow.BeginDrawing(commandList);
      pass.BeginShadow(commandList, staticSunFrame, paletteConstants);
      worldView.DrawChunks(commandList, pass);
      staticShadow.EndDrawing(commandList);
    }
    gpuTimer.End(commandList, slot, STATIC_SHADOW_SPAN);
    gpuTimer.Begin(commandList, slot, DYNAMIC_SHADOW_SPAN);
    const D3D12_GPU_VIRTUAL_ADDRESS dynamicSunFrame = UploadConstants(ring, MakeSunFrame(sunViewProjection, DYNAMIC_SHADOW_TEXELS));
    dynamicShadow.BeginDrawing(commandList);
    pass.BeginShadow(commandList, dynamicSunFrame, paletteConstants);
    worldView.DrawModels(commandList, pass);
    dynamicShadow.EndDrawing(commandList);
    gpuTimer.End(commandList, slot, DYNAMIC_SHADOW_SPAN);

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
                                                     .shadowPassMilliseconds = 0.0,
                                                     .frameMilliseconds = 1000.0 * lapSeconds,
                                                     .rebuildMilliseconds = rebuildMilliseconds};
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
          if (const std::optional<Spike::Measurement::FrameRow> row = TakeWaitingRow(waitingRows[waitingSlot], gpuTimer, waitingSlot))
          {
            measurement.AddFrame(*row);
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
           .worldVoxelMeters = worldVoxelMeters,
           .tearing = device.TearingSupported(),
           .resizedDuringRun = resizedDuringRun,
           .path = std::format(L"round a {:.0f} m square at a view height of {:.0f} m, a quarter turn at each corner, {} legs of {} frames "
                               L"stepped at {} a second",
                               PATH_SIDE_METERS, DEFAULT_VIEW_HEIGHT_METERS, Engine::CameraPath::LEG_COUNT, PATH_LEG_FRAMES,
                               PATH_FRAMES_PER_SECOND),
           .edits = std::format(
             L"quarrying {} voxels a second, the west curtain's northern section collapsing at frame {}, {} units walking "
             L"2 m circles at 1.4 m/s; latency timed on one publication a second",
             Simulation::TestWorldEdits::QUARRY_VOXELS_PER_SECOND, Simulation::TestWorldEdits::COLLAPSE_FRAME, scene.walkers.size()),
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
        std::format(L"{} | {} | {:.2f} ms a frame, voxel pass {:.2f} ms and shadows {:.2f} ms on the GPU | {} voxels, world in {} m | "
                    L"render {} x {} at {:.2f} | view {:.0f} m, azimuth {:.0f}{}",
                    TITLE, device.AdapterName(), 1000.0 * titleSeconds / titleFrames, voxelPassMilliseconds, shadowPassMilliseconds,
                    worldView.RecordsDrawn(), worldVoxelMeters, sceneColor.WidthPixels(), sceneColor.HeightPixels(), renderScale,
                    camera.ViewHeightMeters(), DirectX::XMConvertToDegrees(camera.AzimuthRadians()),
                    measure ? std::format(L" | measuring, frame {} of {}", pathFrame, path.FrameCount()) : std::wstring{});
      SetWindowTextW(window.Handle(), title.c_str());
      titleSeconds = 0.0;
      titleFrames = 0;
    }
    if (!measure)
    {
      ++sceneFrame;
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
                .windowed = HasFlag(commandLine, L"--windowed"),
                .halfMeterWorld = HasFlag(commandLine, L"--half-meter-world")});
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
