#include "pch.h"

#include "CommandQueue.h"
#include "DescriptorHeap.h"
#include "FrameRing.h"
#include "GpuDevice.h"
#include "OrientedVoxel.h"
#include "ResourceBarrier.h"
#include "ShadowMap.h"
#include "SunProjection.h"
#include "UploadRing.h"
#include "VoxelPass.h"
#include "VoxelRecord.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Render tests on WARP (ADR-012): each draws a scene it builds itself into a 64 x 64 offscreen
// target and checks coverage, colour and depth against values worked out from the scene.

namespace EngineTests
{

namespace
{

constexpr std::uint32_t TARGET_PIXELS = 64;
constexpr float VIEW_METERS = 4.0F; // the orthographic view is 4 m across, so a pixel is 1/16 m
constexpr DXGI_FORMAT COLOR_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT DEPTH_FORMAT = DXGI_FORMAT_D32_FLOAT;
constexpr std::uint8_t ORANGE = 6;   // linear (1, 0.5, 0.25) in these scenes' palette
constexpr std::uint8_t GREEN = 7;    // linear (0, 1, 0)
constexpr std::uint8_t SWAPPED = 16; // no colour of its own in these scenes' palette: drawn only through a swap
constexpr int COLOR_TOLERANCE = 2;
constexpr float DEPTH_TOLERANCE = 1.0e-4F;

using Rgba = std::array<std::uint8_t, 4>;

struct Readback
{
  std::vector<Rgba> colors;
  std::vector<float> depths;

  [[nodiscard]] const Rgba& ColorAt(std::uint32_t _x, std::uint32_t _y) const
  {
    return colors.at(static_cast<std::size_t>(_y) * TARGET_PIXELS + _x);
  }

  [[nodiscard]] float DepthAt(std::uint32_t _x, std::uint32_t _y) const
  {
    return depths.at(static_cast<std::size_t>(_y) * TARGET_PIXELS + _x);
  }
};

/// The light the scenes are lit by unless a test says otherwise: sky and ground alike and no sun,
/// so that a face takes its albedo times its corner shade, whichever way it faces.
constexpr DirectX::XMFLOAT4 FLAT_LIGHT = {1.0F, 1.0F, 1.0F, 0.0F};
constexpr DirectX::XMFLOAT4 NO_LIGHT = {0.0F, 0.0F, 0.0F, 0.0F};
constexpr std::array<DirectX::XMFLOAT4, 2> CORNER_SHADES = {{{0.52F, 0.68F, 0.84F, 1.0F}, {0.74F, 0.84F, 0.93F, 1.0F}}};

/// An orthographic camera at _eye looking along _forward, with reversed Z between the near and far
/// distances, the sun straight overhead, and the flat light.
Engine::VoxelPass::FrameConstants MakeFrame(DirectX::FXMVECTOR _eye, DirectX::FXMVECTOR _forward, DirectX::FXMVECTOR _up, float _nearMeters,
                                            float _farMeters)
{
  const DirectX::XMMATRIX view = DirectX::XMMatrixLookToLH(_eye, _forward, _up);
  // Near and far swapped: depth 1 at the near plane, 0 at the far one (ADR-011, decision 4).
  const DirectX::XMMATRIX projection = DirectX::XMMatrixOrthographicLH(VIEW_METERS, VIEW_METERS, _farMeters, _nearMeters);
  const DirectX::XMMATRIX viewProjection = DirectX::XMMatrixMultiply(view, projection);
  Engine::VoxelPass::FrameConstants frame{};
  DirectX::XMStoreFloat4x4(&frame.viewProjection, viewProjection);
  DirectX::XMStoreFloat4x4(&frame.inverseViewProjection, DirectX::XMMatrixInverse(nullptr, viewProjection));
  DirectX::XMStoreFloat4(&frame.rayDirection, DirectX::XMVector3Normalize(_forward));
  constexpr float PIXELS = static_cast<float>(TARGET_PIXELS);
  frame.viewportPixels = {PIXELS, PIXELS, 1.0F / PIXELS, 1.0F / PIXELS};
  frame.lightDirection = {0.0F, 1.0F, 0.0F, 0.0F};
  frame.sunRadiance = NO_LIGHT;
  frame.skyRadiance = FLAT_LIGHT;
  frame.groundRadiance = FLAT_LIGHT;
  frame.cornerShades = CORNER_SHADES;
  frame.shadowParameters = NO_LIGHT; // no shadows unless a test draws them
  return frame;
}

/// The depth the camera above at _eyeHeightMeters, looking straight down, gives a point at
/// _heightMeters: reversed Z between the near and far distances.
float DepthBelow(float _eyeHeightMeters, float _heightMeters, float _nearMeters, float _farMeters)
{
  const float viewMeters = _eyeHeightMeters - _heightMeters;
  return (viewMeters - _farMeters) / (_nearMeters - _farMeters);
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

/// A readback buffer shaped for _texture, and the copy into it.
struct TextureCopy
{
  winrt::com_ptr<ID3D12Resource> buffer;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
};

TextureCopy CopyOut(ID3D12Device* _device, ID3D12GraphicsCommandList* _commandList, ID3D12Resource* _texture)
{
  const D3D12_RESOURCE_DESC textureDesc = _texture->GetDesc();
  TextureCopy copy{};
  UINT64 totalBytes = 0;
  _device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &copy.footprint, nullptr, nullptr, &totalBytes);
  const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_READBACK};
  const D3D12_RESOURCE_DESC bufferDesc{.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                       .Width = totalBytes,
                                       .Height = 1,
                                       .DepthOrArraySize = 1,
                                       .MipLevels = 1,
                                       .Format = DXGI_FORMAT_UNKNOWN,
                                       .SampleDesc = {.Count = 1},
                                       .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR};
  winrt::check_hresult(_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST,
                                                        nullptr, IID_PPV_ARGS(copy.buffer.put())));
  // The copy locations' unions are filled by member: designators cannot name an anonymous union's.
  D3D12_TEXTURE_COPY_LOCATION destination{};
  destination.pResource = copy.buffer.get();
  destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  destination.PlacedFootprint = copy.footprint;
  D3D12_TEXTURE_COPY_LOCATION source{};
  source.pResource = _texture;
  source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  source.SubresourceIndex = 0;
  _commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
  return copy;
}

/// Reads every pixel of a copy as a Pixel.
template <typename Pixel> std::vector<Pixel> ReadPixels(const TextureCopy& _copy)
{
  const D3D12_RANGE readRange{.Begin = 0, .End = static_cast<SIZE_T>(_copy.footprint.Footprint.RowPitch) * TARGET_PIXELS};
  void* mapped = nullptr;
  winrt::check_hresult(_copy.buffer->Map(0, &readRange, &mapped));
  const auto* bytes = static_cast<const std::byte*>(mapped);
  std::vector<Pixel> pixels(static_cast<std::size_t>(TARGET_PIXELS) * TARGET_PIXELS);
  for (std::uint32_t row = 0; row < TARGET_PIXELS; ++row)
  {
    std::memcpy(&pixels[static_cast<std::size_t>(row) * TARGET_PIXELS],
                bytes + static_cast<std::size_t>(row) * _copy.footprint.Footprint.RowPitch, sizeof(Pixel) * TARGET_PIXELS);
  }
  const D3D12_RANGE noWrite{.Begin = 0, .End = 0};
  _copy.buffer->Unmap(0, &noWrite);
  return pixels;
}

/// One batch at the origin with 1 m voxels, drawn as it is.
constexpr Engine::VoxelPass::Batch PLAIN_BATCH{.originMeters = {0.0F, 0.0F, 0.0F},
                                               .voxelEdgeMeters = 1.0F,
                                               .quarterTurns = 0,
                                               .widthCells = 0,
                                               .depthCells = 0,
                                               .swapFromIndex = 0,
                                               .swapToIndex = 0};

/// What a test casts shadows with: the sun's frame, and what each shadow map holds, drawn as one
/// batch each, like the scene's.
struct ShadowScene
{
  Engine::VoxelPass::FrameConstants sunFrame;
  std::span<const Engine::VoxelRecord> staticRecords;
  std::span<const Engine::VoxelRecord> dynamicRecords;
  std::span<const Engine::OrientedVoxel> dynamicOriented; // drawn into the dynamic map too
};

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

constexpr std::uint32_t SHADOW_MAP_TEXELS = 256;

/// Draws _records as one batch, with shadows when _shadows says, and reads the frame back.
Readback Render(Engine::GpuDevice& _device, const Engine::VoxelPass::FrameConstants& _frame, std::span<const Engine::VoxelRecord> _records,
                const Engine::VoxelPass::Batch& _batch = PLAIN_BATCH, const ShadowScene* _shadows = nullptr,
                std::span<const Engine::OrientedVoxel> _oriented = {})
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
  const std::optional<Engine::UploadRing::Allocation> records = ring.Allocate(std::max<std::size_t>(_records.size_bytes(), 1), 256);
  if (!frame || !palette || !records)
  {
    throw std::runtime_error("the upload ring refused a test scene");
  }
  std::memcpy(frame->bytes.data(), &_frame, sizeof(_frame));
  std::array<DirectX::XMFLOAT4, Engine::VoxelPass::PALETTE_ENTRIES> colors{};
  colors[ORANGE] = {1.0F, 0.5F, 0.25F, 1.0F};
  colors[GREEN] = {0.0F, 1.0F, 0.0F, 1.0F};
  std::memcpy(palette->bytes.data(), colors.data(), sizeof(colors));
  if (!_records.empty())
  {
    std::memcpy(records->bytes.data(), _records.data(), _records.size_bytes());
  }

  Engine::VoxelPass pass(device, {.colorFormat = COLOR_FORMAT, .depthFormat = DEPTH_FORMAT});
  Engine::FrameRing frames(device, _device.DirectQueue());
  ID3D12GraphicsCommandList* commandList = frames.Begin();

  // The shadow maps first, through the shadow pipeline with the sun's frame.
  Engine::ShadowMap staticShadow(device, SHADOW_MAP_TEXELS);
  Engine::ShadowMap dynamicShadow(device, SHADOW_MAP_TEXELS);
  if (_shadows != nullptr)
  {
    const std::optional<Engine::UploadRing::Allocation> sunFrame =
      ring.Allocate(sizeof(Engine::VoxelPass::FrameConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    if (!sunFrame)
    {
      throw std::runtime_error("the upload ring refused the sun's frame");
    }
    std::memcpy(sunFrame->bytes.data(), &_shadows->sunFrame, sizeof(_shadows->sunFrame));
    for (const auto& [map, mapRecords] :
         {std::pair{&staticShadow, _shadows->staticRecords}, std::pair{&dynamicShadow, _shadows->dynamicRecords}})
    {
      const std::optional<Engine::UploadRing::Allocation> held = ring.Allocate(std::max<std::size_t>(mapRecords.size_bytes(), 1), 256);
      if (!held)
      {
        throw std::runtime_error("the upload ring refused a shadow map's records");
      }
      if (!mapRecords.empty())
      {
        std::memcpy(held->bytes.data(), mapRecords.data(), mapRecords.size_bytes());
      }
      map->BeginDrawing(commandList);
      pass.BeginShadow(commandList, sunFrame->gpuAddress, palette->gpuAddress);
      pass.Draw(commandList, held->gpuAddress, static_cast<std::uint32_t>(mapRecords.size()), _batch);
      if (map == &dynamicShadow && !_shadows->dynamicOriented.empty())
      {
        pass.BeginOrientedShadow(commandList, sunFrame->gpuAddress, palette->gpuAddress);
        pass.DrawOriented(commandList, Held(ring, _shadows->dynamicOriented), static_cast<std::uint32_t>(_shadows->dynamicOriented.size()));
      }
      map->EndDrawing(commandList);
    }
    pass.SetShadowMaps(staticShadow.Texture(), dynamicShadow.Texture());
  }
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
  pass.Begin(commandList, frame->gpuAddress, palette->gpuAddress);
  pass.Draw(commandList, records->gpuAddress, static_cast<std::uint32_t>(_records.size()), _batch);
  if (!_oriented.empty())
  {
    pass.BeginOriented(commandList, frame->gpuAddress, palette->gpuAddress);
    pass.DrawOriented(commandList, Held(ring, _oriented), static_cast<std::uint32_t>(_oriented.size()));
  }

  const std::array<D3D12_RESOURCE_BARRIER, 2> toCopy = {
    Engine::TransitionBarrier(color.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE),
    Engine::TransitionBarrier(depth.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_SOURCE)};
  commandList->ResourceBarrier(static_cast<UINT>(toCopy.size()), toCopy.data());
  const TextureCopy colorCopy = CopyOut(device, commandList, color.get());
  const TextureCopy depthCopy = CopyOut(device, commandList, depth.get());
  const std::uint64_t fenceValue = frames.End();
  ring.Retire(fenceValue);
  _device.DirectQueue().WaitFor(fenceValue);

  return {.colors = ReadPixels<Rgba>(colorCopy), .depths = ReadPixels<float>(depthCopy)};
}

bool Near(const Rgba& _color, int _red, int _green, int _blue)
{
  return std::abs(_color[0] - _red) <= COLOR_TOLERANCE && std::abs(_color[1] - _green) <= COLOR_TOLERANCE &&
         std::abs(_color[2] - _blue) <= COLOR_TOLERANCE;
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

constexpr float EYE_HEIGHT_METERS = 10.0F;
constexpr float NEAR_METERS = 0.1F;
constexpr float FAR_METERS = 20.0F;

// Looking straight down from 10 m over the origin, with +x to the right and +z up the screen: the
// voxel [0, 1]^3 covers pixels x in [32, 48) and y in [16, 32).
Engine::VoxelPass::FrameConstants TopDownFrame(float _eyeHeightMeters, float _nearMeters)
{
  return MakeFrame(DirectX::XMVectorSet(0.0F, _eyeHeightMeters, 0.0F, 1.0F), DirectX::XMVectorSet(0.0F, -1.0F, 0.0F, 0.0F),
                   DirectX::XMVectorSet(0.0F, 0.0F, 1.0F, 0.0F), _nearMeters, FAR_METERS);
}

// Looking north along +z from 10 m south of the origin, with +x to the right and +y up the screen:
// the voxel [0, 1]^3's south face covers the same pixels as its top does from above.
Engine::VoxelPass::FrameConstants SouthFrame()
{
  return MakeFrame(DirectX::XMVectorSet(0.0F, 0.0F, -EYE_HEIGHT_METERS, 1.0F), DirectX::XMVectorSet(0.0F, 0.0F, 1.0F, 0.0F),
                   DirectX::XMVectorSet(0.0F, 1.0F, 0.0F, 0.0F), NEAR_METERS, FAR_METERS);
}

/// A record whose top face is fully occluded at corner 0, its south-west corner, and open at the
/// other three, read through _shadeTable.
Engine::VoxelRecord ShadedAtSouthWest(std::uint32_t _x, std::uint8_t _paletteIndex, std::uint8_t _shadeTable)
{
  Engine::VoxelShade shade = Engine::UNSHADED;
  shade.cornerLevels[static_cast<std::size_t>(Engine::VoxelFace::PositiveY)] = {0, 3, 3, 3};
  shade.shadeTable = _shadeTable;
  return Engine::MakeVoxelRecord(_x, 0, 0, _paletteIndex, shade);
}

} // namespace

TEST_CLASS(VoxelPassTests)
{
public:
  TEST_METHOD(CoversExactlyTheVoxelsPixels)
  {
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::VoxelRecord, 1> records = {Engine::MakeVoxelRecord(0, 0, 0, ORANGE)};
    const Readback frame = Render(device, TopDownFrame(EYE_HEIGHT_METERS, NEAR_METERS), records);

    std::uint32_t covered = 0;
    for (std::uint32_t y = 0; y < TARGET_PIXELS; ++y)
    {
      for (std::uint32_t x = 0; x < TARGET_PIXELS; ++x)
      {
        const bool inside = x >= 32 && x < 48 && y >= 16 && y < 32;
        const bool drawn = !Near(frame.ColorAt(x, y), 0, 0, 0);
        if (inside != drawn)
        {
          Assert::Fail(
            std::format(L"pixel ({}, {}) is {} but should be {}", x, y, Describe(frame.ColorAt(x, y)), inside ? L"lit" : L"clear").c_str());
        }
        covered += drawn ? 1 : 0;
      }
    }
    Assert::AreEqual(std::uint32_t{256}, covered);
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(ShadesTheTopFaceInItsPaletteColor)
  {
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::VoxelRecord, 1> records = {Engine::MakeVoxelRecord(0, 0, 0, ORANGE)};
    const Readback frame = Render(device, TopDownFrame(EYE_HEIGHT_METERS, NEAR_METERS), records);
    // Under the flat light, with every corner open, the top face takes its full albedo: (1, 0.5, 0.25).
    Assert::IsTrue(Near(frame.ColorAt(40, 24), 255, 128, 64), Describe(frame.ColorAt(40, 24)).c_str());
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(WritesTheDepthOfTheHit)
  {
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::VoxelRecord, 1> records = {Engine::MakeVoxelRecord(0, 0, 0, ORANGE)};
    const Readback frame = Render(device, TopDownFrame(EYE_HEIGHT_METERS, NEAR_METERS), records);
    Assert::AreEqual(DepthBelow(EYE_HEIGHT_METERS, 1.0F, NEAR_METERS, FAR_METERS), frame.DepthAt(40, 24), DEPTH_TOLERANCE,
                     L"the top face, at 1 m");
    Assert::AreEqual(0.0F, frame.DepthAt(8, 8), DEPTH_TOLERANCE, L"the cleared background");
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(DrawsTheNearerVoxelWhateverTheOrder)
  {
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const Engine::VoxelRecord below = Engine::MakeVoxelRecord(0, 0, 0, ORANGE);
    const Engine::VoxelRecord above = Engine::MakeVoxelRecord(0, 2, 0, GREEN);
    for (const std::array<Engine::VoxelRecord, 2>& records : {std::array{below, above}, std::array{above, below}})
    {
      const Readback frame = Render(device, TopDownFrame(EYE_HEIGHT_METERS, NEAR_METERS), records);
      Assert::IsTrue(Near(frame.ColorAt(40, 24), 0, 255, 0), Describe(frame.ColorAt(40, 24)).c_str());
      Assert::AreEqual(DepthBelow(EYE_HEIGHT_METERS, 3.0F, NEAR_METERS, FAR_METERS), frame.DepthAt(40, 24), DEPTH_TOLERANCE);
    }
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(CutsAVoxelThatCrossesTheNearPlane)
  {
    // The eye is 1.5 m up and the near plane 1 m below it, at 0.5 m: halfway through the voxel.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::VoxelRecord, 1> records = {Engine::MakeVoxelRecord(0, 0, 0, ORANGE)};
    const Readback frame = Render(device, TopDownFrame(1.5F, 1.0F), records);
    Assert::IsTrue(Near(frame.ColorAt(40, 24), 255, 128, 64), L"the cut is drawn, facing the camera");
    Assert::AreEqual(1.0F, frame.DepthAt(40, 24), DEPTH_TOLERANCE, L"at the near plane");
    Assert::IsTrue(Near(frame.ColorAt(31, 24), 0, 0, 0), L"and nothing beyond the voxel");
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(TurnsABatchAQuarterAboutItsFootprint)
  {
    // Orange at x 0 and green at x 1, a 2 x 1 footprint. A quarter turn clockwise seen from above
    // takes +z to +x: green comes to (0, 0) and orange to (0, 1), up the screen, in the same
    // footprint corner. Each 1 m voxel covers 16 x 16 pixels, z 0 at y [16, 32) and z 1 at y [0, 16).
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::VoxelRecord, 2> records = {Engine::MakeVoxelRecord(0, 0, 0, ORANGE), Engine::MakeVoxelRecord(1, 0, 0, GREEN)};
    Engine::VoxelPass::Batch turned = PLAIN_BATCH;
    turned.quarterTurns = 1;
    turned.widthCells = 2;
    turned.depthCells = 1;
    const Readback frame = Render(device, TopDownFrame(EYE_HEIGHT_METERS, NEAR_METERS), records, turned);
    Assert::IsTrue(Near(frame.ColorAt(40, 24), 0, 255, 0), (L"green at (0, 0): " + Describe(frame.ColorAt(40, 24))).c_str());
    Assert::IsTrue(Near(frame.ColorAt(40, 8), 255, 128, 64), (L"orange at (0, 1): " + Describe(frame.ColorAt(40, 8))).c_str());
    Assert::IsTrue(Near(frame.ColorAt(56, 24), 0, 0, 0), (L"nothing left at (1, 0): " + Describe(frame.ColorAt(56, 24))).c_str());

    // Unturned, it is drawn as it was built: orange at (0, 0) and green at (1, 0).
    turned.quarterTurns = 0;
    const Readback unturned = Render(device, TopDownFrame(EYE_HEIGHT_METERS, NEAR_METERS), records, turned);
    Assert::IsTrue(Near(unturned.ColorAt(40, 24), 255, 128, 64) && Near(unturned.ColorAt(56, 24), 0, 255, 0));
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(SwapsOnePaletteIndex)
  {
    // The swapped index draws as green, and orange beside it is left alone.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::VoxelRecord, 2> records = {Engine::MakeVoxelRecord(0, 0, 0, SWAPPED),
                                                        Engine::MakeVoxelRecord(1, 0, 0, ORANGE)};
    Engine::VoxelPass::Batch swapped = PLAIN_BATCH;
    swapped.swapFromIndex = SWAPPED;
    swapped.swapToIndex = GREEN;
    const Readback frame = Render(device, TopDownFrame(EYE_HEIGHT_METERS, NEAR_METERS), records, swapped);
    Assert::IsTrue(Near(frame.ColorAt(40, 24), 0, 255, 0), Describe(frame.ColorAt(40, 24)).c_str());
    Assert::IsTrue(Near(frame.ColorAt(56, 24), 255, 128, 64), Describe(frame.ColorAt(56, 24)).c_str());
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(LightsByTheSunAndTheHemisphere)
  {
    // The sun overhead at 0.5, the sky at 0.25 and the ground dark (RendererLook.md §6). The top
    // face takes all of the sun and all of the sky: 0.75 of its albedo. The south face takes none
    // of the sun, and half sky, half ground: 0.125.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::VoxelRecord, 1> records = {Engine::MakeVoxelRecord(0, 0, 0, ORANGE)};
    const auto lit = [](Engine::VoxelPass::FrameConstants _frame)
    {
      _frame.sunRadiance = {0.5F, 0.5F, 0.5F, 0.0F};
      _frame.skyRadiance = {0.25F, 0.25F, 0.25F, 0.0F};
      _frame.groundRadiance = NO_LIGHT;
      return _frame;
    };
    const Readback top = Render(device, lit(TopDownFrame(EYE_HEIGHT_METERS, NEAR_METERS)), records);
    Assert::IsTrue(Near(top.ColorAt(40, 24), 191, 96, 48), (L"the top face: " + Describe(top.ColorAt(40, 24))).c_str());
    const Readback south = Render(device, lit(SouthFrame()), records);
    Assert::IsTrue(Near(south.ColorAt(40, 24), 32, 16, 8), (L"the south face: " + Describe(south.ColorAt(40, 24))).c_str());
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(BlendsCornerShadesAcrossAFace)
  {
    // Two voxels whose top faces are fully occluded at their south-west corners, the first through
    // the structure table, where that is 0.52, the second through the terrain table, where it is
    // 0.74 (RendererLook.md §5). The pixel nearest that corner sits 1/32 of the face in from it
    // along both edges, so its shade is 1 - (1 - 0.52) x (31/32)^2 = 0.5495, or for terrain 0.7560.
    // The opposite corner's pixel is all but open.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::VoxelRecord, 2> records = {ShadedAtSouthWest(0, ORANGE, 0), ShadedAtSouthWest(1, ORANGE, 1)};
    const Readback frame = Render(device, TopDownFrame(EYE_HEIGHT_METERS, NEAR_METERS), records);
    Assert::IsTrue(Near(frame.ColorAt(32, 31), 140, 70, 35), (L"structure's corner: " + Describe(frame.ColorAt(32, 31))).c_str());
    Assert::IsTrue(Near(frame.ColorAt(47, 16), 255, 128, 64), (L"the open corner: " + Describe(frame.ColorAt(47, 16))).c_str());
    Assert::IsTrue(Near(frame.ColorAt(48, 31), 193, 96, 48), (L"terrain's corner: " + Describe(frame.ColorAt(48, 31))).c_str());
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(TurnsCornerShadesWithTheBatch)
  {
    // A 2 x 1 footprint turned a quarter, as in TurnsABatchAQuarterAboutItsFootprint: orange comes
    // to (0, 1). Its occluded corner, the batch's own south-west one, turns with it to the world's
    // north-west corner of that cell, at pixel (32, 0); the cell's own south-west pixel, (32, 15),
    // is all but open. Read unturned, the two would swap.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::VoxelRecord, 2> records = {ShadedAtSouthWest(0, ORANGE, 0), Engine::MakeVoxelRecord(1, 0, 0, GREEN)};
    Engine::VoxelPass::Batch turned = PLAIN_BATCH;
    turned.quarterTurns = 1;
    turned.widthCells = 2;
    turned.depthCells = 1;
    const Readback frame = Render(device, TopDownFrame(EYE_HEIGHT_METERS, NEAR_METERS), records, turned);
    Assert::IsTrue(Near(frame.ColorAt(32, 0), 140, 70, 35), (L"the turned corner: " + Describe(frame.ColorAt(32, 0))).c_str());
    Assert::IsTrue(frame.ColorAt(32, 15)[0] >= 245, (L"the cell's own south-west: " + Describe(frame.ColorAt(32, 15))).c_str());
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(JittersOnlyTheLightness)
  {
    // Orange is (1, 0.5, 0.25), in HSL (1/18, 1, 0.625). Lightness 50/1024 lower, hue and saturation
    // kept, is (1, 0.4349, 0.1523) (RendererLook.md §4, three.js's offsetHSL on linear RGB).
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    Engine::VoxelShade shade = Engine::UNSHADED;
    shade.lightnessOffset = -50.0F / 1024.0F;
    const std::array<Engine::VoxelRecord, 1> records = {Engine::MakeVoxelRecord(0, 0, 0, ORANGE, shade)};
    const Readback frame = Render(device, TopDownFrame(EYE_HEIGHT_METERS, NEAR_METERS), records);
    Assert::IsTrue(Near(frame.ColorAt(40, 24), 255, 111, 39), Describe(frame.ColorAt(40, 24)).c_str());
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(CastsTheSunsShadowFromEitherMap)
  {
    // A floor of four voxels from x -2 to 2, and one above its east end at x 1, y 2. The sun stands
    // to the east at 45 degrees, so the raised voxel's shadow falls on the floor's top from x -1 to
    // 1: the cells at -1 and 0 take only the sky, the one at -2 the sun too. The sun at 0.5 and the
    // sky and ground at 0.25 give 0.25 in shadow and 0.25 + 0.5 x 0.7071 in the sun. The raised
    // voxel casts from the static map, from the dynamic map, and as an oriented voxel from the dynamic map.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::VoxelRecord, 4> floor = {Engine::MakeVoxelRecord(0, 0, 0, ORANGE), Engine::MakeVoxelRecord(1, 0, 0, ORANGE),
                                                      Engine::MakeVoxelRecord(2, 0, 0, ORANGE), Engine::MakeVoxelRecord(3, 0, 0, ORANGE)};
    const std::array<Engine::VoxelRecord, 1> raised = {Engine::MakeVoxelRecord(3, 2, 0, ORANGE)};
    std::vector<Engine::VoxelRecord> scene(floor.begin(), floor.end());
    scene.push_back(raised[0]);
    Engine::VoxelPass::Batch batch = PLAIN_BATCH;
    batch.originMeters = {-2.0F, 0.0F, 0.0F};

    const DirectX::XMVECTOR towardsSun = DirectX::XMVectorSet(1.0F, 1.0F, 0.0F, 0.0F);
    Engine::VoxelPass::FrameConstants sun{};
    const DirectX::XMMATRIX sunViewProjection =
      Engine::SunViewProjection(towardsSun, DirectX::XMFLOAT3{-2.0F, 0.0F, 0.0F}, DirectX::XMFLOAT3{2.0F, 3.0F, 1.0F}, 0.5F);
    DirectX::XMStoreFloat4x4(&sun.viewProjection, sunViewProjection);
    DirectX::XMStoreFloat4x4(&sun.inverseViewProjection, DirectX::XMMatrixInverse(nullptr, sunViewProjection));
    DirectX::XMStoreFloat4(&sun.rayDirection, DirectX::XMVector3Normalize(DirectX::XMVectorNegate(towardsSun)));
    constexpr float MAP = static_cast<float>(SHADOW_MAP_TEXELS);
    sun.viewportPixels = {MAP, MAP, 1.0F / MAP, 1.0F / MAP};

    Engine::VoxelPass::FrameConstants frame = TopDownFrame(EYE_HEIGHT_METERS, NEAR_METERS);
    DirectX::XMStoreFloat4(&frame.lightDirection, DirectX::XMVector3Normalize(towardsSun));
    frame.sunRadiance = {0.5F, 0.5F, 0.5F, 0.0F};
    frame.skyRadiance = {0.25F, 0.25F, 0.25F, 0.0F};
    frame.groundRadiance = frame.skyRadiance;
    DirectX::XMStoreFloat4x4(&frame.shadowViewProjection, sunViewProjection);
    // A hit is moved 0.1 m out along its normal before the maps are read, enough to clear the filter's
    // reach on a floor lit at 45 degrees and far less than the 0.5 m from any probed pixel to the
    // shadow's edge.
    frame.shadowParameters = {0.1F, 0.0F, 1.0F / MAP, 1.0F / MAP};

    // The raised voxel as an oriented one, unturned, where the batch puts the record: x 1, y 2.
    const std::array<Engine::OrientedVoxel, 1> raisedOriented = {{{.centerMeters = {1.5F, 2.5F, 0.5F},
                                                                   .halfEdgeMeters = 0.5F,
                                                                   .rotation = {0.0F, 0.0F, 0.0F, 1.0F},
                                                                   .paletteIndex = ORANGE,
                                                                   .lightnessOffset = 0.0F,
                                                                   .reserved = {}}}};
    enum class Caster
    {
      StaticMap,
      DynamicMap,
      OrientedInTheDynamicMap
    };
    for (const Caster caster : {Caster::StaticMap, Caster::DynamicMap, Caster::OrientedInTheDynamicMap})
    {
      const bool oriented = caster == Caster::OrientedInTheDynamicMap;
      const ShadowScene shadows{.sunFrame = sun,
                                .staticRecords = caster == Caster::StaticMap ? std::span<const Engine::VoxelRecord>(scene)
                                                                             : std::span<const Engine::VoxelRecord>(floor),
                                .dynamicRecords = caster == Caster::DynamicMap ? std::span<const Engine::VoxelRecord>(raised)
                                                                               : std::span<const Engine::VoxelRecord>{},
                                .dynamicOriented = oriented ? std::span<const Engine::OrientedVoxel>(raisedOriented)
                                                            : std::span<const Engine::OrientedVoxel>{}};
      const Readback image =
        oriented ? Render(device, frame, floor, batch, &shadows, raisedOriented) : Render(device, frame, scene, batch, &shadows);
      const std::wstring which =
        caster == Caster::StaticMap ? L" (static map)" : (caster == Caster::DynamicMap ? L" (dynamic map)" : L" (oriented, dynamic map)");
      Assert::IsTrue(Near(image.ColorAt(8, 24), 154, 77, 38), (L"in the sun at x -2: " + Describe(image.ColorAt(8, 24)) + which).c_str());
      Assert::IsTrue(Near(image.ColorAt(24, 24), 64, 32, 16), (L"in shadow at x -1: " + Describe(image.ColorAt(24, 24)) + which).c_str());
      Assert::IsTrue(Near(image.ColorAt(40, 24), 64, 32, 16), (L"in shadow at x 0: " + Describe(image.ColorAt(40, 24)) + which).c_str());
      Assert::IsTrue(Near(image.ColorAt(56, 24), 154, 77, 38),
                     (L"the raised voxel's top: " + Describe(image.ColorAt(56, 24)) + which).c_str());
    }
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(DrawsAnUnturnedOrientedVoxelAsAnAxisAlignedOne)
  {
    // The unit voxel [0, 1]^3 as an oriented voxel with no turn: the same pixels, colour and depth
    // as CoversExactlyTheVoxelsPixels and WritesTheDepthOfTheHit.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::OrientedVoxel, 1> oriented = {{{.centerMeters = {0.5F, 0.5F, 0.5F},
                                                             .halfEdgeMeters = 0.5F,
                                                             .rotation = {0.0F, 0.0F, 0.0F, 1.0F},
                                                             .paletteIndex = ORANGE,
                                                             .lightnessOffset = 0.0F,
                                                             .reserved = {}}}};
    const Readback frame = Render(device, TopDownFrame(EYE_HEIGHT_METERS, NEAR_METERS), {}, PLAIN_BATCH, nullptr, oriented);
    std::uint32_t covered = 0;
    for (std::uint32_t y = 0; y < TARGET_PIXELS; ++y)
    {
      for (std::uint32_t x = 0; x < TARGET_PIXELS; ++x)
      {
        const bool inside = x >= 32 && x < 48 && y >= 16 && y < 32;
        const bool drawn = !Near(frame.ColorAt(x, y), 0, 0, 0);
        if (inside != drawn)
        {
          Assert::Fail(std::format(L"pixel ({}, {}) is {}", x, y, Describe(frame.ColorAt(x, y))).c_str());
        }
        covered += drawn ? 1 : 0;
      }
    }
    Assert::AreEqual(std::uint32_t{256}, covered);
    Assert::IsTrue(Near(frame.ColorAt(40, 24), 255, 128, 64), Describe(frame.ColorAt(40, 24)).c_str());
    Assert::AreEqual(DepthBelow(EYE_HEIGHT_METERS, 1.0F, NEAR_METERS, FAR_METERS), frame.DepthAt(40, 24), DEPTH_TOLERANCE);
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(TurnsAnOrientedVoxelAboutItsCenter)
  {
    // The same voxel turned 45 degrees about y: from above, its top is a diamond about (0.5, 0.5)
    // reaching 0.707 m along each axis. Pixel (49, 24), 0.594 m east of the middle, is inside it and
    // outside the unturned square; pixel (47, 16), near the square's north-east corner, is the
    // other way round. The top still faces up, so it takes its full albedo.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const float half = 0.5F * DirectX::XM_PIDIV4;
    const std::array<Engine::OrientedVoxel, 1> oriented = {{{.centerMeters = {0.5F, 0.5F, 0.5F},
                                                             .halfEdgeMeters = 0.5F,
                                                             .rotation = {0.0F, std::sin(half), 0.0F, std::cos(half)},
                                                             .paletteIndex = ORANGE,
                                                             .lightnessOffset = 0.0F,
                                                             .reserved = {}}}};
    const Readback frame = Render(device, TopDownFrame(EYE_HEIGHT_METERS, NEAR_METERS), {}, PLAIN_BATCH, nullptr, oriented);
    Assert::IsTrue(Near(frame.ColorAt(40, 24), 255, 128, 64), (L"the middle: " + Describe(frame.ColorAt(40, 24))).c_str());
    Assert::IsTrue(Near(frame.ColorAt(49, 24), 255, 128, 64), (L"the east tip: " + Describe(frame.ColorAt(49, 24))).c_str());
    Assert::IsTrue(Near(frame.ColorAt(47, 16), 0, 0, 0), (L"the square's corner: " + Describe(frame.ColorAt(47, 16))).c_str());
    Assert::AreEqual(DepthBelow(EYE_HEIGHT_METERS, 1.0F, NEAR_METERS, FAR_METERS), frame.DepthAt(40, 24), DEPTH_TOLERANCE);
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(RefusesATurnItCannotMake)
  {
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const Engine::VoxelPass pass(device.Device(), {.colorFormat = COLOR_FORMAT, .depthFormat = DEPTH_FORMAT});
    Engine::FrameRing frames(device.Device(), device.DirectQueue());
    ID3D12GraphicsCommandList* commandList = frames.Begin();
    Engine::VoxelPass::Batch batch = PLAIN_BATCH;
    batch.quarterTurns = 4;
    Assert::ExpectException<std::invalid_argument>([&pass, commandList, batch] { pass.Draw(commandList, 0, 1, batch); });
    batch.quarterTurns = 1;
    batch.widthCells = 65;
    batch.depthCells = 1;
    Assert::ExpectException<std::invalid_argument>([&pass, commandList, batch] { pass.Draw(commandList, 0, 1, batch); });
    device.DirectQueue().WaitFor(frames.End());
  }
};

} // namespace EngineTests
