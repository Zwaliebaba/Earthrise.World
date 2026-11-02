#include "pch.h"

#include "DescriptorHeap.h"
#include "FrameRing.h"
#include "GpuDevice.h"
#include "InterfacePass.h"
#include "ResourceBarrier.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The interface pass's render tests on WARP (ADR-012; ADR-019, decision 3): quads drawn into a 64 x 64 target cleared to
// a known colour, read back, and checked pixel by pixel against what the blend gives in linear light.

namespace EngineTests
{

namespace
{

constexpr DXGI_FORMAT TARGET_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
constexpr DXGI_FORMAT ATLAS_FORMAT = DXGI_FORMAT_R8_UNORM;
constexpr std::uint32_t TARGET_PIXELS = 64;
constexpr std::uint32_t ATLAS_TEXELS = 4;
constexpr int COLOR_TOLERANCE = 2;
// The clear colour, linear, through the target's sRGB view.
constexpr std::array<float, 4> CLEAR = {0.25F, 0.5F, 0.75F, 1.0F};
// A 4 x 4 atlas with two texels set, (1, 0) and (2, 3), and the rest clear.
constexpr std::array<std::uint8_t, 16> ATLAS = {0, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 0};

using Rgba = std::array<std::uint8_t, 4>;

struct Image
{
  std::vector<Rgba> pixels; // row by row, top first

  [[nodiscard]] const Rgba& At(std::uint32_t _x, std::uint32_t _y) const
  {
    return pixels.at(static_cast<std::size_t>(_y) * TARGET_PIXELS + _x);
  }
};

double LinearToSrgb(double _value) noexcept
{
  return _value <= 0.0031308 ? _value * 12.92 : 1.055 * std::pow(_value, 1.0 / 2.4) - 0.055;
}

/// A linear colour as the target's sRGB view stores it.
Rgba Encoded(double _red, double _green, double _blue)
{
  const auto byte = [](double _linear) { return static_cast<std::uint8_t>(std::lround(LinearToSrgb(_linear) * 255.0)); };
  return {byte(_red), byte(_green), byte(_blue), 255};
}

/// The clear colour as the target stores it.
Rgba ClearPixel()
{
  return Encoded(CLEAR[0], CLEAR[1], CLEAR[2]);
}

bool Near(const Rgba& _actual, const Rgba& _expected)
{
  for (std::size_t channel = 0; channel < 3; ++channel)
  {
    if (std::abs(_actual[channel] - _expected[channel]) > COLOR_TOLERANCE)
    {
      return false;
    }
  }
  return true;
}

Engine::InterfaceQuad Solid(float _x, float _y, float _width, float _height, DirectX::XMFLOAT4 _color)
{
  return {.xPixels = _x,
          .yPixels = _y,
          .widthPixels = _width,
          .heightPixels = _height,
          .u0 = 0.0F,
          .v0 = 0.0F,
          .u1 = 0.0F,
          .v1 = 0.0F,
          .color = _color,
          .textured = 0,
          .reserved = {}};
}

winrt::com_ptr<ID3D12Resource> MakeTexture(ID3D12Device* _device, std::uint32_t _edgePixels, DXGI_FORMAT _format,
                                           D3D12_RESOURCE_FLAGS _flags, D3D12_RESOURCE_STATES _state)
{
  const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_DEFAULT};
  const D3D12_RESOURCE_DESC desc{.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                 .Width = _edgePixels,
                                 .Height = _edgePixels,
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

winrt::com_ptr<ID3D12Resource> MakeBuffer(ID3D12Device* _device, std::uint64_t _bytes, D3D12_HEAP_TYPE _heapType,
                                          D3D12_RESOURCE_STATES _state)
{
  const D3D12_HEAP_PROPERTIES heapProperties{.Type = _heapType};
  const D3D12_RESOURCE_DESC desc{.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                 .Width = _bytes,
                                 .Height = 1,
                                 .DepthOrArraySize = 1,
                                 .MipLevels = 1,
                                 .Format = DXGI_FORMAT_UNKNOWN,
                                 .SampleDesc = {.Count = 1},
                                 .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR};
  winrt::com_ptr<ID3D12Resource> buffer;
  winrt::check_hresult(
    _device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, _state, nullptr, IID_PPV_ARGS(buffer.put())));
  return buffer;
}

/// Records the copy between _texture and _buffer laid out as _footprint, in the direction _toTexture says.
void RecordCopy(ID3D12GraphicsCommandList* _commandList, ID3D12Resource* _texture, ID3D12Resource* _buffer,
                const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& _footprint, bool _toTexture)
{
  // The copy locations' unions are filled by member: designators cannot name an anonymous union's.
  D3D12_TEXTURE_COPY_LOCATION buffer{};
  buffer.pResource = _buffer;
  buffer.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  buffer.PlacedFootprint = _footprint;
  D3D12_TEXTURE_COPY_LOCATION texture{};
  texture.pResource = _texture;
  texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  texture.SubresourceIndex = 0;
  if (_toTexture)
  {
    _commandList->CopyTextureRegion(&texture, 0, 0, 0, &buffer, nullptr);
  }
  else
  {
    _commandList->CopyTextureRegion(&buffer, 0, 0, 0, &texture, nullptr);
  }
}

/// A texture's one subresource's footprint, and how many bytes a buffer holding it needs.
std::pair<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, std::uint64_t> Footprint(ID3D12Device* _device, ID3D12Resource* _texture)
{
  const D3D12_RESOURCE_DESC desc = _texture->GetDesc();
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  UINT64 totalBytes = 0;
  _device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &totalBytes);
  return {footprint, totalBytes};
}

/// Draws _quads through the pass over the target cleared to CLEAR, with ATLAS as the atlas, and reads the target back.
Image Draw(Engine::GpuDevice& _device, std::span<const Engine::InterfaceQuad> _quads)
{
  ID3D12Device* device = _device.Device();
  const winrt::com_ptr<ID3D12Resource> atlas =
    MakeTexture(device, ATLAS_TEXELS, ATLAS_FORMAT, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
  const winrt::com_ptr<ID3D12Resource> target =
    MakeTexture(device, TARGET_PIXELS, TARGET_FORMAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET);
  Engine::DescriptorHeap targetViews(device, {.type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV, .capacityDescriptors = 1, .shaderVisible = false});
  const D3D12_CPU_DESCRIPTOR_HANDLE targetView = targetViews.Allocate().cpu;
  device->CreateRenderTargetView(target.get(), nullptr, targetView);

  // The atlas's texels, row by row at the footprint's pitch.
  const auto [atlasFootprint, atlasBytes] = Footprint(device, atlas.get());
  const winrt::com_ptr<ID3D12Resource> atlasUpload =
    MakeBuffer(device, atlasBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
  void* mapped = nullptr;
  winrt::check_hresult(atlasUpload->Map(0, nullptr, &mapped));
  for (std::uint32_t row = 0; row < ATLAS_TEXELS; ++row)
  {
    std::memcpy(static_cast<std::byte*>(mapped) + static_cast<std::size_t>(row) * atlasFootprint.Footprint.RowPitch,
                &ATLAS.at(static_cast<std::size_t>(row) * ATLAS_TEXELS), ATLAS_TEXELS);
  }
  atlasUpload->Unmap(0, nullptr);
  // The quads, in a buffer the vertex shader reads as a root shader-resource view.
  const winrt::com_ptr<ID3D12Resource> quads =
    MakeBuffer(device, std::max<std::uint64_t>(_quads.size_bytes(), sizeof(Engine::InterfaceQuad)), D3D12_HEAP_TYPE_UPLOAD,
               D3D12_RESOURCE_STATE_GENERIC_READ);
  winrt::check_hresult(quads->Map(0, nullptr, &mapped));
  std::memcpy(mapped, _quads.data(), _quads.size_bytes());
  quads->Unmap(0, nullptr);
  const auto [targetFootprint, targetBytes] = Footprint(device, target.get());
  const winrt::com_ptr<ID3D12Resource> readback = MakeBuffer(device, targetBytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);

  Engine::InterfacePass pass(device, {.targetFormat = TARGET_FORMAT});
  pass.SetAtlas(atlas.get(), ATLAS_TEXELS, ATLAS_TEXELS);
  Engine::FrameRing frames(device, _device.DirectQueue());
  ID3D12GraphicsCommandList* commandList = frames.Begin();
  RecordCopy(commandList, atlas.get(), atlasUpload.get(), atlasFootprint, true);
  const D3D12_RESOURCE_BARRIER toRead =
    Engine::TransitionBarrier(atlas.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  commandList->ResourceBarrier(1, &toRead);
  commandList->OMSetRenderTargets(1, &targetView, FALSE, nullptr);
  commandList->ClearRenderTargetView(targetView, CLEAR.data(), 0, nullptr);
  const D3D12_VIEWPORT viewport{.TopLeftX = 0.0F,
                                .TopLeftY = 0.0F,
                                .Width = static_cast<float>(TARGET_PIXELS),
                                .Height = static_cast<float>(TARGET_PIXELS),
                                .MinDepth = 0.0F,
                                .MaxDepth = 1.0F};
  const D3D12_RECT scissor{.left = 0, .top = 0, .right = static_cast<LONG>(TARGET_PIXELS), .bottom = static_cast<LONG>(TARGET_PIXELS)};
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  pass.Draw(commandList, quads->GetGPUVirtualAddress(), static_cast<std::uint32_t>(_quads.size()), TARGET_PIXELS, TARGET_PIXELS);
  const D3D12_RESOURCE_BARRIER toCopy =
    Engine::TransitionBarrier(target.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
  commandList->ResourceBarrier(1, &toCopy);
  RecordCopy(commandList, target.get(), readback.get(), targetFootprint, false);
  _device.DirectQueue().WaitFor(frames.End());

  Image result;
  result.pixels.resize(static_cast<std::size_t>(TARGET_PIXELS) * TARGET_PIXELS);
  const D3D12_RANGE readRange{.Begin = 0, .End = static_cast<SIZE_T>(targetBytes)};
  winrt::check_hresult(readback->Map(0, &readRange, &mapped));
  for (std::uint32_t row = 0; row < TARGET_PIXELS; ++row)
  {
    std::memcpy(&result.pixels[static_cast<std::size_t>(row) * TARGET_PIXELS],
                static_cast<const std::byte*>(mapped) + static_cast<std::size_t>(row) * targetFootprint.Footprint.RowPitch,
                sizeof(Rgba) * TARGET_PIXELS);
  }
  const D3D12_RANGE noWrite{.Begin = 0, .End = 0};
  readback->Unmap(0, &noWrite);
  return result;
}

/// Checks every pixel against _expected, which gives the colour each pixel should hold.
template <typename Expected> void AssertPixels(const Image& _image, const Expected& _expected)
{
  for (std::uint32_t y = 0; y < TARGET_PIXELS; ++y)
  {
    for (std::uint32_t x = 0; x < TARGET_PIXELS; ++x)
    {
      const Rgba expected = _expected(x, y);
      const Rgba& actual = _image.At(x, y);
      if (!Near(actual, expected))
      {
        Assert::Fail(std::format(L"pixel ({}, {}) is ({}, {}, {}), and should be ({}, {}, {})", x, y, actual[0], actual[1], actual[2],
                                 expected[0], expected[1], expected[2])
                       .c_str());
      }
    }
  }
}

void AssertNoDebugErrors(Engine::GpuDevice& _device)
{
  const std::vector<std::string> errors = _device.TakeDebugErrors();
  if (!errors.empty())
  {
    Assert::Fail(std::wstring(errors.front().begin(), errors.front().end()).c_str());
  }
}

} // namespace

TEST_CLASS(InterfacePassTests)
{
public:
  TEST_METHOD(DrawsAGlyphAtScaleTwoOnItsSetTexelsAlone)
  {
    // The 4 x 4 atlas drawn 8 x 8 at (8, 16): each texel covers 2 x 2 pixels, red where it is set and nothing where it is
    // clear.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::InterfaceQuad, 1> quads = {{{.xPixels = 8.0F,
                                                          .yPixels = 16.0F,
                                                          .widthPixels = 8.0F,
                                                          .heightPixels = 8.0F,
                                                          .u0 = 0.0F,
                                                          .v0 = 0.0F,
                                                          .u1 = 4.0F,
                                                          .v1 = 4.0F,
                                                          .color = {1.0F, 0.0F, 0.0F, 1.0F},
                                                          .textured = 1,
                                                          .reserved = {}}}};
    const Image image = Draw(device, quads);
    const Rgba red = Encoded(1.0, 0.0, 0.0);
    AssertPixels(image,
                 [&red](std::uint32_t _x, std::uint32_t _y)
                 {
                   const bool inside = _x >= 8 && _x < 16 && _y >= 16 && _y < 24;
                   const bool set = inside && ATLAS.at(((_y - 16) / 2 * ATLAS_TEXELS) + ((_x - 8) / 2)) != 0;
                   return set ? red : ClearPixel();
                 });
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(BlendsASolidQuadInLinearLight)
  {
    // White at 0.92 over the clear colour: 0.92 + 0.08 of the clear colour in each channel, linear.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    constexpr float ALPHA = 0.92F;
    const std::array<Engine::InterfaceQuad, 1> quads = {Solid(10.0F, 20.0F, 30.0F, 12.0F, {1.0F, 1.0F, 1.0F, ALPHA})};
    const Image image = Draw(device, quads);
    const Rgba blended =
      Encoded(ALPHA + ((1.0 - ALPHA) * CLEAR[0]), ALPHA + ((1.0 - ALPHA) * CLEAR[1]), ALPHA + ((1.0 - ALPHA) * CLEAR[2]));
    AssertPixels(image, [&blended](std::uint32_t _x, std::uint32_t _y)
                 { return _x >= 10 && _x < 40 && _y >= 20 && _y < 32 ? blended : ClearPixel(); });
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(DrawsTheLaterOfTwoOverlappingQuadsOnTop)
  {
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const std::array<Engine::InterfaceQuad, 2> quads = {Solid(4.0F, 4.0F, 20.0F, 20.0F, {1.0F, 0.0F, 0.0F, 1.0F}),
                                                        Solid(14.0F, 14.0F, 20.0F, 20.0F, {0.0F, 0.0F, 1.0F, 1.0F})};
    const Image image = Draw(device, quads);
    const Rgba red = Encoded(1.0, 0.0, 0.0);
    const Rgba blue = Encoded(0.0, 0.0, 1.0);
    AssertPixels(image,
                 [&red, &blue](std::uint32_t _x, std::uint32_t _y)
                 {
                   if (_x >= 14 && _x < 34 && _y >= 14 && _y < 34)
                   {
                     return blue;
                   }
                   return _x >= 4 && _x < 24 && _y >= 4 && _y < 24 ? red : ClearPixel();
                 });
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(ChangesNothingOutsideItsQuads)
  {
    // No quad at all leaves every pixel the clear colour.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const Image image = Draw(device, {});
    AssertPixels(image, [](std::uint32_t, std::uint32_t) { return ClearPixel(); });
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(RefusesToDrawWithoutAnAtlas)
  {
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const Engine::InterfacePass pass(device.Device(), {.targetFormat = TARGET_FORMAT});
    Engine::FrameRing frames(device.Device(), device.DirectQueue());
    ID3D12GraphicsCommandList* commandList = frames.Begin();
    Assert::ExpectException<std::logic_error>([&pass, commandList] { pass.Draw(commandList, 0, 1, TARGET_PIXELS, TARGET_PIXELS); });
    device.DirectQueue().WaitFor(frames.End());
  }
};

} // namespace EngineTests
