#include "pch.h"

#include "CommandQueue.h"
#include "DescriptorHeap.h"
#include "FrameRing.h"
#include "GpuDevice.h"
#include "ResourceBarrier.h"
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
constexpr std::uint8_t ORANGE = 6; // linear (1, 0.5, 0.25) in these scenes' palette
constexpr std::uint8_t GREEN = 7;  // linear (0, 1, 0)
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

/// An orthographic camera at _eye looking along _forward, with reversed Z between the near and far
/// distances, and the light straight overhead.
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

/// Draws _records as one batch at the origin with 1 m voxels, and reads the frame back.
Readback Render(Engine::GpuDevice& _device, const Engine::VoxelPass::FrameConstants& _frame, std::span<const Engine::VoxelRecord> _records)
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
  const std::optional<Engine::UploadRing::Allocation> records = ring.Allocate(_records.size_bytes(), 256);
  if (!frame || !palette || !records)
  {
    throw std::runtime_error("the upload ring refused a test scene");
  }
  std::memcpy(frame->bytes.data(), &_frame, sizeof(_frame));
  std::array<DirectX::XMFLOAT4, Engine::VoxelPass::PALETTE_ENTRIES> colors{};
  colors[ORANGE] = {1.0F, 0.5F, 0.25F, 1.0F};
  colors[GREEN] = {0.0F, 1.0F, 0.0F, 1.0F};
  std::memcpy(palette->bytes.data(), colors.data(), sizeof(colors));
  std::memcpy(records->bytes.data(), _records.data(), _records.size_bytes());

  const Engine::VoxelPass pass(device, {.colorFormat = COLOR_FORMAT, .depthFormat = DEPTH_FORMAT});
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
  pass.Begin(commandList, frame->gpuAddress, palette->gpuAddress);
  pass.Draw(commandList, records->gpuAddress, static_cast<std::uint32_t>(_records.size()), {0.0F, 0.0F, 0.0F}, 1.0F);

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
    // The light is overhead, so the top face takes its full albedo: (1, 0.5, 0.25).
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
};

} // namespace EngineTests
