#include "pch.h"

#include "DescriptorHeap.h"
#include "FrameRing.h"
#include "GpuDevice.h"
#include "LinePass.h"
#include "ResourceBarrier.h"
#include "UploadRing.h"
#include "VoxelPass.h"
#include "VoxelRecord.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The line pass on WARP (ADR-012; ADR-018, decision 4): each test draws voxels, then lines, into a 64 x 64 offscreen
// target, and checks which pixels the lines take, in what colour, against values worked out from the scene.

namespace EngineTests
{

namespace
{

constexpr std::uint32_t TARGET_PIXELS = 64;
constexpr DXGI_FORMAT COLOR_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT DEPTH_FORMAT = DXGI_FORMAT_D32_FLOAT;
constexpr std::uint8_t ORANGE = 6; // linear (1, 0.5, 0.25) in these scenes' palette
constexpr int COLOR_TOLERANCE = 2;
constexpr float EYE_DISTANCE_METERS = 10.0F;
constexpr float NEAR_METERS = 0.1F;
constexpr float FAR_METERS = 20.0F;
constexpr float CLOSE_VIEW_METERS = 4.0F; // 16 pixels a metre
constexpr float WIDE_VIEW_METERS = 8.0F;  // 8 pixels a metre
constexpr DirectX::XMFLOAT4 RED = {1.0F, 0.0F, 0.0F, 1.0F};
constexpr DirectX::XMFLOAT4 GREEN = {0.0F, 1.0F, 0.0F, 1.0F};
constexpr DirectX::XMFLOAT4 BLUE = {0.0F, 0.0F, 1.0F, 1.0F};
constexpr DirectX::XMFLOAT3 EAST = {1.0F, 0.0F, 0.0F};
constexpr DirectX::XMFLOAT3 NORTH = {0.0F, 0.0F, 1.0F};

using Rgba = std::array<std::uint8_t, 4>;

struct Readback
{
  std::vector<Rgba> colors;

  [[nodiscard]] const Rgba& ColorAt(std::uint32_t _x, std::uint32_t _y) const
  {
    return colors.at(static_cast<std::size_t>(_y) * TARGET_PIXELS + _x);
  }
};

/// An orthographic camera at _eye looking along _forward, _viewMeters across, with reversed Z between the near and far
/// distances and flat light, so that a voxel face takes its albedo.
Engine::VoxelPass::FrameConstants MakeFrame(DirectX::FXMVECTOR _eye, DirectX::FXMVECTOR _forward, DirectX::FXMVECTOR _up, float _viewMeters)
{
  const DirectX::XMMATRIX view = DirectX::XMMatrixLookToLH(_eye, _forward, _up);
  // Near and far swapped: depth 1 at the near plane, 0 at the far one (ADR-011, decision 4).
  const DirectX::XMMATRIX projection = DirectX::XMMatrixOrthographicLH(_viewMeters, _viewMeters, FAR_METERS, NEAR_METERS);
  const DirectX::XMMATRIX viewProjection = DirectX::XMMatrixMultiply(view, projection);
  Engine::VoxelPass::FrameConstants frame{};
  DirectX::XMStoreFloat4x4(&frame.viewProjection, viewProjection);
  DirectX::XMStoreFloat4x4(&frame.inverseViewProjection, DirectX::XMMatrixInverse(nullptr, viewProjection));
  DirectX::XMStoreFloat4(&frame.rayDirection, DirectX::XMVector3Normalize(_forward));
  constexpr float PIXELS = static_cast<float>(TARGET_PIXELS);
  frame.viewportPixels = {PIXELS, PIXELS, 1.0F / PIXELS, 1.0F / PIXELS};
  frame.lightDirection = {0.0F, 1.0F, 0.0F, 0.0F};
  frame.sunRadiance = {0.0F, 0.0F, 0.0F, 0.0F};
  frame.skyRadiance = {1.0F, 1.0F, 1.0F, 0.0F};
  frame.groundRadiance = {1.0F, 1.0F, 1.0F, 0.0F};
  frame.cornerShades = {{{1.0F, 1.0F, 1.0F, 1.0F}, {1.0F, 1.0F, 1.0F, 1.0F}}};
  frame.shadowParameters = {0.0F, 0.0F, 0.0F, 0.0F}; // no shadows
  return frame;
}

// Looking straight down from 10 m over the origin, +x to the right and +z up the screen: the point (x, z) lies at pixel
// (32 + x * p, 32 - z * p), p the pixels a metre.
Engine::VoxelPass::FrameConstants TopDownFrame(float _viewMeters)
{
  return MakeFrame(DirectX::XMVectorSet(0.0F, EYE_DISTANCE_METERS, 0.0F, 1.0F), DirectX::XMVectorSet(0.0F, -1.0F, 0.0F, 0.0F),
                   DirectX::XMVectorSet(0.0F, 0.0F, 1.0F, 0.0F), _viewMeters);
}

// Looking north along +z from 10 m south of the origin, +x to the right and +y up the screen: the voxel [0, 1]^3's south
// face covers pixels x in [32, 48) and y in [16, 32).
Engine::VoxelPass::FrameConstants SouthFrame()
{
  return MakeFrame(DirectX::XMVectorSet(0.0F, 0.0F, -EYE_DISTANCE_METERS, 1.0F), DirectX::XMVectorSet(0.0F, 0.0F, 1.0F, 0.0F),
                   DirectX::XMVectorSet(0.0F, 1.0F, 0.0F, 0.0F), CLOSE_VIEW_METERS);
}

winrt::com_ptr<ID3D12Resource> MakeTexture(ID3D12Device* _device, DXGI_FORMAT _format, D3D12_RESOURCE_FLAGS _flags,
                                           D3D12_RESOURCE_STATES _state)
{
  const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_DEFAULT};
  const D3D12_RESOURCE_DESC desc{.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                 .Width = TARGET_PIXELS,
                                 .Height = TARGET_PIXELS,
                                 .DepthOrArraySize = 1,
                                 .MipLevels = 1,
                                 .Format = _format,
                                 .SampleDesc = {.Count = 1},
                                 .Flags = _flags};
  winrt::com_ptr<ID3D12Resource> texture;
  winrt::check_hresult(
    _device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, _state, nullptr, IID_PPV_ARGS(texture.put())));
  return texture;
}

/// _values copied into the ring, at 256 bytes, or a byte of nothing for none.
template <typename T> D3D12_GPU_VIRTUAL_ADDRESS Held(Engine::UploadRing& _ring, std::span<const T> _values)
{
  const std::optional<Engine::UploadRing::Allocation> held = _ring.Allocate(std::max<std::size_t>(_values.size_bytes(), 1), 256);
  if (!held)
  {
    throw std::runtime_error("the upload ring refused a test scene");
  }
  if (!_values.empty())
  {
    std::memcpy(held->bytes.data(), _values.data(), _values.size_bytes());
  }
  return held->gpuAddress;
}

/// What a test draws: voxels, one batch at the origin with 1 m voxels, then lines.
struct Scene
{
  std::span<const Engine::VoxelRecord> voxels;
  std::span<const Engine::LineModelRecord> models;
  std::span<const Engine::LineSegmentRecord> segments;
  std::span<const Engine::LineInstance> lines;
};

/// Draws _scene through the voxel pass, then the line pass, and reads the colour back.
Readback Render(Engine::GpuDevice& _device, const Engine::VoxelPass::FrameConstants& _frame, const Scene& _scene)
{
  ID3D12Device* device = _device.Device();
  winrt::com_ptr<ID3D12Resource> color =
    MakeTexture(device, COLOR_FORMAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET);
  winrt::com_ptr<ID3D12Resource> depth =
    MakeTexture(device, DEPTH_FORMAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE);
  Engine::DescriptorHeap colorViews(device, {.type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV, .capacityDescriptors = 1, .shaderVisible = false});
  Engine::DescriptorHeap depthViews(device, {.type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV, .capacityDescriptors = 1, .shaderVisible = false});
  const D3D12_CPU_DESCRIPTOR_HANDLE colorView = colorViews.Allocate().cpu;
  const D3D12_CPU_DESCRIPTOR_HANDLE depthView = depthViews.Allocate().cpu;
  device->CreateRenderTargetView(color.get(), nullptr, colorView);
  device->CreateDepthStencilView(depth.get(), nullptr, depthView);

  Engine::UploadRing ring(device, std::uint64_t{64} * 1024);
  const std::optional<Engine::UploadRing::Allocation> frame =
    ring.Allocate(sizeof(Engine::VoxelPass::FrameConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
  const std::optional<Engine::UploadRing::Allocation> palette =
    ring.Allocate(sizeof(DirectX::XMFLOAT4) * Engine::VoxelPass::PALETTE_ENTRIES, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
  if (!frame || !palette)
  {
    throw std::runtime_error("the upload ring refused a test scene");
  }
  std::memcpy(frame->bytes.data(), &_frame, sizeof(_frame));
  std::array<DirectX::XMFLOAT4, Engine::VoxelPass::PALETTE_ENTRIES> colors{};
  colors[ORANGE] = {1.0F, 0.5F, 0.25F, 1.0F};
  std::memcpy(palette->bytes.data(), colors.data(), sizeof(colors));
  const D3D12_GPU_VIRTUAL_ADDRESS voxels = Held(ring, _scene.voxels);
  const D3D12_GPU_VIRTUAL_ADDRESS models = Held(ring, _scene.models);
  const D3D12_GPU_VIRTUAL_ADDRESS segments = Held(ring, _scene.segments);
  const D3D12_GPU_VIRTUAL_ADDRESS lines = Held(ring, _scene.lines);

  Engine::VoxelPass voxelPass(device, {.colorFormat = COLOR_FORMAT, .depthFormat = DEPTH_FORMAT});
  Engine::LinePass linePass(device, {.colorFormat = COLOR_FORMAT, .depthFormat = DEPTH_FORMAT});
  Engine::FrameRing frames(device, _device.DirectQueue());
  ID3D12GraphicsCommandList* commandList = frames.Begin();
  constexpr std::array<float, 4> CLEAR = {0.0F, 0.0F, 0.0F, 1.0F};
  commandList->OMSetRenderTargets(1, &colorView, FALSE, &depthView);
  commandList->ClearRenderTargetView(colorView, CLEAR.data(), 0, nullptr);
  commandList->ClearDepthStencilView(depthView, D3D12_CLEAR_FLAG_DEPTH, 0.0F, 0, 0, nullptr); // reversed Z clears to 0
  const D3D12_VIEWPORT viewport{.TopLeftX = 0.0F,
                                .TopLeftY = 0.0F,
                                .Width = static_cast<float>(TARGET_PIXELS),
                                .Height = static_cast<float>(TARGET_PIXELS),
                                .MinDepth = 0.0F,
                                .MaxDepth = 1.0F};
  const D3D12_RECT scissor{.left = 0, .top = 0, .right = TARGET_PIXELS, .bottom = TARGET_PIXELS};
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  if (!_scene.voxels.empty())
  {
    voxelPass.Begin(commandList, frame->gpuAddress, palette->gpuAddress);
    voxelPass.Draw(commandList, voxels, static_cast<std::uint32_t>(_scene.voxels.size()),
                   {.originMeters = {0.0F, 0.0F, 0.0F},
                    .voxelEdgeMeters = 1.0F,
                    .quarterTurns = 0,
                    .widthCells = 0,
                    .depthCells = 0,
                    .swapFromIndex = 0,
                    .swapToIndex = 0});
  }
  linePass.Begin(commandList, frame->gpuAddress, models, segments);
  linePass.Draw(commandList, lines, static_cast<std::uint32_t>(_scene.lines.size()));

  const D3D12_RESOURCE_BARRIER toCopy =
    Engine::TransitionBarrier(color.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
  commandList->ResourceBarrier(1, &toCopy);
  const D3D12_RESOURCE_DESC textureDesc = color->GetDesc();
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  UINT64 totalBytes = 0;
  device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, nullptr, nullptr, &totalBytes);
  const D3D12_HEAP_PROPERTIES readbackHeap{.Type = D3D12_HEAP_TYPE_READBACK};
  const D3D12_RESOURCE_DESC bufferDesc{.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                       .Width = totalBytes,
                                       .Height = 1,
                                       .DepthOrArraySize = 1,
                                       .MipLevels = 1,
                                       .Format = DXGI_FORMAT_UNKNOWN,
                                       .SampleDesc = {.Count = 1},
                                       .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR};
  winrt::com_ptr<ID3D12Resource> readback;
  winrt::check_hresult(device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST,
                                                       nullptr, IID_PPV_ARGS(readback.put())));
  // The copy locations' unions are filled by member: designators cannot name an anonymous union's.
  D3D12_TEXTURE_COPY_LOCATION destination{};
  destination.pResource = readback.get();
  destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  destination.PlacedFootprint = footprint;
  D3D12_TEXTURE_COPY_LOCATION source{};
  source.pResource = color.get();
  source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  source.SubresourceIndex = 0;
  commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
  const std::uint64_t fenceValue = frames.End();
  ring.Retire(fenceValue);
  _device.DirectQueue().WaitFor(fenceValue);

  const D3D12_RANGE readRange{.Begin = 0, .End = static_cast<SIZE_T>(footprint.Footprint.RowPitch) * TARGET_PIXELS};
  void* mapped = nullptr;
  winrt::check_hresult(readback->Map(0, &readRange, &mapped));
  const auto* bytes = static_cast<const std::byte*>(mapped);
  Readback result;
  result.colors.resize(static_cast<std::size_t>(TARGET_PIXELS) * TARGET_PIXELS);
  for (std::uint32_t row = 0; row < TARGET_PIXELS; ++row)
  {
    std::memcpy(&result.colors[static_cast<std::size_t>(row) * TARGET_PIXELS],
                bytes + static_cast<std::size_t>(row) * footprint.Footprint.RowPitch, sizeof(Rgba) * TARGET_PIXELS);
  }
  const D3D12_RANGE noWrite{.Begin = 0, .End = 0};
  readback->Unmap(0, &noWrite);
  return result;
}

bool Near(const Rgba& _color, int _red, int _green, int _blue)
{
  return std::abs(_color[0] - _red) <= COLOR_TOLERANCE && std::abs(_color[1] - _green) <= COLOR_TOLERANCE &&
         std::abs(_color[2] - _blue) <= COLOR_TOLERANCE;
}

bool IsRed(const Rgba& _color)
{
  return Near(_color, 255, 0, 0);
}

std::wstring Describe(const Rgba& _color)
{
  return std::format(L"({}, {}, {})", _color[0], _color[1], _color[2]);
}

void AssertNoDebugErrors(Engine::GpuDevice& _device)
{
  const std::vector<std::string> errors = _device.TakeDebugErrors();
  if (!errors.empty())
  {
    Assert::Fail(std::wstring(errors.front().begin(), errors.front().end()).c_str());
  }
}

/// A segment from _fromMeters to _toMeters along its model, _widthPixels wide, in _color.
Engine::LineSegmentRecord Segment(float _fromMeters, float _toMeters, DirectX::XMFLOAT4 _color, float _widthPixels, bool _tapers = false,
                                  bool _faction = false)
{
  return {.fromMeters = _fromMeters,
          .toMeters = _toMeters,
          .color = _color,
          .widthPixels = _widthPixels,
          .factionColor = _faction ? 1U : 0U,
          .tapers = _tapers ? 1U : 0U,
          .reserved = 0.0F};
}

/// A model of _segmentCount segments from _firstSegment, _lengthMeters long, a 2 px dot below 6 px, as the arrow's.
Engine::LineModelRecord Model(std::uint32_t _firstSegment, std::uint32_t _segmentCount, float _lengthMeters)
{
  return {.firstSegment = _firstSegment,
          .segmentCount = _segmentCount,
          .drawnLengthMeters = _lengthMeters,
          .dotBelowPixels = 6.0F,
          .dotWidthPixels = 2.0F,
          .reserved = {}};
}

Engine::LineInstance Line(DirectX::XMFLOAT3 _middleMeters, DirectX::XMFLOAT3 _direction, std::uint32_t _model = 0,
                          DirectX::XMFLOAT4 _faction = BLUE)
{
  return {.positionMeters = _middleMeters, .lineModel = _model, .direction = _direction, .reserved = 0.0F, .factionColor = _faction};
}

/// The rows of column _x that are red.
std::uint32_t RedRows(const Readback& _frame, std::uint32_t _x)
{
  std::uint32_t rows = 0;
  for (std::uint32_t y = 0; y < TARGET_PIXELS; ++y)
  {
    rows += IsRed(_frame.ColorAt(_x, y)) ? 1 : 0;
  }
  return rows;
}

} // namespace

TEST_CLASS(LinePassTests)
{
public:
  TEST_METHOD(IsHiddenWhereAWallIsInFrontAndDrawnWhereItIsNot)
  {
    // Seen from the south, the voxel [0, 1]^3 covers pixels x 32 to 47 and y 16 to 31. A 2.5 m red line along x at height
    // 0.5 m, 1 m behind the wall, runs from pixel 20 to 60 on rows 23 and 24: the wall hides it from 32 to 47 and it shows
    // either side. Another at height 0.25 m, 1 m in front of the wall, on rows 27 and 28, is drawn across it.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::VoxelRecord, 1> wall = {Engine::MakeVoxelRecord(0, 0, 0, ORANGE)};
    const std::array<Engine::LineSegmentRecord, 1> segments = {Segment(0.0F, 2.5F, RED, 2.0F)};
    const std::array<Engine::LineModelRecord, 1> models = {Model(0, 1, 2.5F)};
    const std::array<Engine::LineInstance, 2> lines = {Line({0.5F, 0.5F, 2.0F}, EAST), Line({0.5F, 0.25F, -1.0F}, EAST)};
    const Readback frame = Render(device, SouthFrame(), {.voxels = wall, .models = models, .segments = segments, .lines = lines});
    for (const std::uint32_t y : {23U, 24U})
    {
      Assert::IsTrue(IsRed(frame.ColorAt(24, y)), Describe(frame.ColorAt(24, y)).c_str());
      Assert::IsTrue(Near(frame.ColorAt(40, y), 255, 128, 64), Describe(frame.ColorAt(40, y)).c_str());
      Assert::IsTrue(IsRed(frame.ColorAt(55, y)), Describe(frame.ColorAt(55, y)).c_str());
    }
    for (const std::uint32_t y : {27U, 28U})
    {
      Assert::IsTrue(IsRed(frame.ColorAt(40, y)), Describe(frame.ColorAt(40, y)).c_str());
    }
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(DrawsTheShaftTwoPixelsWideAtTwoViewHeights)
  {
    // A 2 px line along x through z = 0, looked down on with the view 4 m and 8 m across: in each, the column at x = 0.5 m
    // has exactly its two rows, 31 and 32, either side of the line's centre on the pixel edge at 32.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::LineSegmentRecord, 1> segments = {Segment(0.0F, 2.0F, RED, 2.0F)};
    const std::array<Engine::LineModelRecord, 1> models = {Model(0, 1, 2.0F)};
    const std::array<Engine::LineInstance, 1> lines = {Line({0.5F, 0.0F, 0.0F}, EAST)};
    for (const auto& [viewMeters, column] : {std::pair{CLOSE_VIEW_METERS, 40U}, std::pair{WIDE_VIEW_METERS, 36U}})
    {
      const Readback frame = Render(device, TopDownFrame(viewMeters), {.models = models, .segments = segments, .lines = lines});
      Assert::AreEqual(2U, RedRows(frame, column));
      Assert::IsTrue(IsRed(frame.ColorAt(column, 31)) && IsRed(frame.ColorAt(column, 32)));
    }
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(IsADotOfTwoPixelsWhenShorterThanSixOnScreen)
  {
    // A 0.25 m line is 4 px long seen from above at 16 px a metre, under its model's 6: it is drawn as a 2 px square about
    // its middle, (0.5, 0.5) m, whose corner is the pixel corner (40, 24), in its longest segment's colour, the shaft's red,
    // and its short green tail is not drawn.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::LineSegmentRecord, 2> segments = {Segment(0.0F, 0.05F, GREEN, 3.0F), Segment(0.05F, 0.25F, RED, 2.0F)};
    const std::array<Engine::LineModelRecord, 1> models = {Model(0, 2, 0.25F)};
    const std::array<Engine::LineInstance, 1> lines = {Line({0.5F, 0.0F, 0.5F}, EAST)};
    const Readback frame = Render(device, TopDownFrame(CLOSE_VIEW_METERS), {.models = models, .segments = segments, .lines = lines});
    std::uint32_t red = 0;
    std::uint32_t green = 0;
    for (std::uint32_t y = 0; y < TARGET_PIXELS; ++y)
    {
      for (std::uint32_t x = 0; x < TARGET_PIXELS; ++x)
      {
        red += IsRed(frame.ColorAt(x, y)) ? 1 : 0;
        green += Near(frame.ColorAt(x, y), 0, 255, 0) ? 1 : 0;
      }
    }
    Assert::AreEqual(4U, red);
    Assert::AreEqual(0U, green);
    for (const auto& [x, y] : {std::pair{39U, 23U}, std::pair{40U, 23U}, std::pair{39U, 24U}, std::pair{40U, 24U}})
    {
      Assert::IsTrue(IsRed(frame.ColorAt(x, y)), Describe(frame.ColorAt(x, y)).c_str());
    }
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(TwoLinesCrossingOccludeByDepth)
  {
    // Seen from above, a red line along x at 1 m and a green one along z at 0.5 m cross over pixels 31 to 32 both ways.
    // The red one is drawn first, so only the depth test keeps the green one, farther from the eye, from covering it there.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::LineSegmentRecord, 2> segments = {Segment(0.0F, 2.0F, RED, 2.0F), Segment(0.0F, 2.0F, GREEN, 2.0F)};
    const std::array<Engine::LineModelRecord, 2> models = {Model(0, 1, 2.0F), Model(1, 1, 2.0F)};
    const std::array<Engine::LineInstance, 2> lines = {Line({0.0F, 1.0F, 0.0F}, EAST, 0), Line({0.0F, 0.5F, 0.0F}, NORTH, 1)};
    const Readback frame = Render(device, TopDownFrame(CLOSE_VIEW_METERS), {.models = models, .segments = segments, .lines = lines});
    for (const auto& [x, y] : {std::pair{31U, 31U}, std::pair{32U, 31U}, std::pair{31U, 32U}, std::pair{32U, 32U}})
    {
      Assert::IsTrue(IsRed(frame.ColorAt(x, y)), Describe(frame.ColorAt(x, y)).c_str());
    }
    Assert::IsTrue(Near(frame.ColorAt(31, 20), 0, 255, 0), Describe(frame.ColorAt(31, 20)).c_str());
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(DrawsAFactionSegmentInItsInstancesColor)
  {
    // A 2 m line, its first metre in its instance's faction colour and its second red, along x through z = 0 from x = -1:
    // blue at x = -0.5 m, pixel 24, and red at 0.5 m, pixel 40.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::LineSegmentRecord, 2> segments = {Segment(0.0F, 1.0F, GREEN, 2.0F, false, true),
                                                               Segment(1.0F, 2.0F, RED, 2.0F)};
    const std::array<Engine::LineModelRecord, 1> models = {Model(0, 2, 2.0F)};
    const std::array<Engine::LineInstance, 1> lines = {Line({0.0F, 0.0F, 0.0F}, EAST, 0, BLUE)};
    const Readback frame = Render(device, TopDownFrame(CLOSE_VIEW_METERS), {.models = models, .segments = segments, .lines = lines});
    Assert::IsTrue(Near(frame.ColorAt(24, 31), 0, 0, 255), Describe(frame.ColorAt(24, 31)).c_str());
    Assert::IsTrue(IsRed(frame.ColorAt(40, 31)), Describe(frame.ColorAt(40, 31)).c_str());
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(NarrowsATaperingSegmentToAPoint)
  {
    // A 1 m segment 8 px wide that tapers, along x through z = 0 from x = 0 to 1, seen from above at 16 px a metre: near its
    // tail, at pixel 33, it covers about 8 rows; near its point, at pixel 47, at most 1.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::LineSegmentRecord, 1> segments = {Segment(0.0F, 1.0F, RED, 8.0F, true)};
    const std::array<Engine::LineModelRecord, 1> models = {Model(0, 1, 1.0F)};
    const std::array<Engine::LineInstance, 1> lines = {Line({0.5F, 0.0F, 0.0F}, EAST)};
    const Readback frame = Render(device, TopDownFrame(CLOSE_VIEW_METERS), {.models = models, .segments = segments, .lines = lines});
    Assert::IsTrue(RedRows(frame, 33) >= 7);
    Assert::IsTrue(RedRows(frame, 47) <= 1);
    AssertNoDebugErrors(device);
  }
};

} // namespace EngineTests
