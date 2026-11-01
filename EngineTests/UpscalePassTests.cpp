#include "pch.h"

#include "DescriptorHeap.h"
#include "FrameRing.h"
#include "GpuDevice.h"
#include "ResourceBarrier.h"
#include "UpscalePass.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The upscale's render tests on WARP (ADR-012): a source of known texels is scaled into a target,
// and every pixel read back is checked against the bilinear filter worked out here, in linear light.

namespace EngineTests
{

namespace
{

constexpr DXGI_FORMAT SRGB_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
constexpr int COLOR_TOLERANCE = 2;

using Rgba = std::array<std::uint8_t, 4>;

struct Image
{
  std::uint32_t widthPixels;
  std::uint32_t heightPixels;
  std::vector<Rgba> pixels; // row by row, top first

  [[nodiscard]] const Rgba& At(std::uint32_t _x, std::uint32_t _y) const
  {
    return pixels.at(static_cast<std::size_t>(_y) * widthPixels + _x);
  }
};

double SrgbToLinear(double _value) noexcept
{
  return _value <= 0.04045 ? _value / 12.92 : std::pow((_value + 0.055) / 1.055, 2.4);
}

double LinearToSrgb(double _value) noexcept
{
  return _value <= 0.0031308 ? _value * 12.92 : 1.055 * std::pow(_value, 1.0 / 2.4) - 0.055;
}

/// The bilinear filter with clamped edges, as the pass samples _source for the target pixel (_x,
/// _y) of a target _widthPixels by _heightPixels: in linear light, encoded back to sRGB bytes.
Rgba ExpectedPixel(const Image& _source, std::uint32_t _widthPixels, std::uint32_t _heightPixels, std::uint32_t _x, std::uint32_t _y)
{
  // The pixel's centre in the source's texels, less half a texel: the filter's sample position.
  const double sourceX = (static_cast<double>(_x) + 0.5) * _source.widthPixels / _widthPixels - 0.5;
  const double sourceY = (static_cast<double>(_y) + 0.5) * _source.heightPixels / _heightPixels - 0.5;
  const double floorX = std::floor(sourceX);
  const double floorY = std::floor(sourceY);
  const double weightX = sourceX - floorX;
  const double weightY = sourceY - floorY;
  const auto clampTo = [](double _texel, std::uint32_t _countTexels)
  { return static_cast<std::uint32_t>(std::clamp(_texel, 0.0, static_cast<double>(_countTexels - 1))); };
  const std::uint32_t left = clampTo(floorX, _source.widthPixels);
  const std::uint32_t right = clampTo(floorX + 1.0, _source.widthPixels);
  const std::uint32_t top = clampTo(floorY, _source.heightPixels);
  const std::uint32_t bottom = clampTo(floorY + 1.0, _source.heightPixels);

  Rgba expected{};
  for (std::size_t channel = 0; channel < 3; ++channel)
  {
    const auto linear = [&_source, channel](std::uint32_t _texelX, std::uint32_t _texelY)
    { return SrgbToLinear(static_cast<double>(_source.At(_texelX, _texelY)[channel]) / 255.0); };
    const double upper = linear(left, top) * (1.0 - weightX) + linear(right, top) * weightX;
    const double lower = linear(left, bottom) * (1.0 - weightX) + linear(right, bottom) * weightX;
    const double blended = upper * (1.0 - weightY) + lower * weightY;
    expected[channel] = static_cast<std::uint8_t>(std::lround(LinearToSrgb(blended) * 255.0));
  }
  expected[3] = 255;
  return expected;
}

winrt::com_ptr<ID3D12Resource> MakeTexture(ID3D12Device* _device, std::uint32_t _widthPixels, std::uint32_t _heightPixels,
                                           D3D12_RESOURCE_FLAGS _flags, D3D12_RESOURCE_STATES _state)
{
  const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_DEFAULT};
  const D3D12_RESOURCE_DESC desc{.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                 .Width = _widthPixels,
                                 .Height = _heightPixels,
                                 .DepthOrArraySize = 1,
                                 .MipLevels = 1,
                                 .Format = SRGB_FORMAT,
                                 .SampleDesc = {.Count = 1},
                                 .Flags = _flags};
  winrt::com_ptr<ID3D12Resource> texture;
  winrt::check_hresult(
    _device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, _state, nullptr, IID_PPV_ARGS(texture.put())));
  return texture;
}

/// A buffer in _heapType shaped for _texture's one subresource, and that footprint.
struct Staging
{
  winrt::com_ptr<ID3D12Resource> buffer;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
};

Staging MakeStaging(ID3D12Device* _device, ID3D12Resource* _texture, D3D12_HEAP_TYPE _heapType, D3D12_RESOURCE_STATES _state)
{
  const D3D12_RESOURCE_DESC textureDesc = _texture->GetDesc();
  Staging staging{};
  UINT64 totalBytes = 0;
  _device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &staging.footprint, nullptr, nullptr, &totalBytes);
  const D3D12_HEAP_PROPERTIES heapProperties{.Type = _heapType};
  const D3D12_RESOURCE_DESC bufferDesc{.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                       .Width = totalBytes,
                                       .Height = 1,
                                       .DepthOrArraySize = 1,
                                       .MipLevels = 1,
                                       .Format = DXGI_FORMAT_UNKNOWN,
                                       .SampleDesc = {.Count = 1},
                                       .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR};
  winrt::check_hresult(_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, _state, nullptr,
                                                        IID_PPV_ARGS(staging.buffer.put())));
  return staging;
}

/// Records the copy between _texture and _staging, in the direction _toTexture says.
void RecordCopy(ID3D12GraphicsCommandList* _commandList, ID3D12Resource* _texture, const Staging& _staging, bool _toTexture)
{
  // The copy locations' unions are filled by member: designators cannot name an anonymous union's.
  D3D12_TEXTURE_COPY_LOCATION buffer{};
  buffer.pResource = _staging.buffer.get();
  buffer.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  buffer.PlacedFootprint = _staging.footprint;
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

/// Scales _source into a target _widthPixels by _heightPixels through the pass, and reads it back.
/// The target is first cleared to magenta, which no test's source holds, so a pixel the pass
/// missed shows.
Image Upscale(Engine::GpuDevice& _device, const Image& _source, std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  ID3D12Device* device = _device.Device();
  winrt::com_ptr<ID3D12Resource> source =
    MakeTexture(device, _source.widthPixels, _source.heightPixels, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
  winrt::com_ptr<ID3D12Resource> target =
    MakeTexture(device, _widthPixels, _heightPixels, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET);
  Engine::DescriptorHeap targetViews(device, {.type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV, .capacityDescriptors = 1, .shaderVisible = false});
  const D3D12_CPU_DESCRIPTOR_HANDLE targetView = targetViews.Allocate().cpu;
  device->CreateRenderTargetView(target.get(), nullptr, targetView);

  const Staging upload = MakeStaging(device, source.get(), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
  void* mapped = nullptr;
  winrt::check_hresult(upload.buffer->Map(0, nullptr, &mapped));
  for (std::uint32_t row = 0; row < _source.heightPixels; ++row)
  {
    std::memcpy(static_cast<std::byte*>(mapped) + static_cast<std::size_t>(row) * upload.footprint.Footprint.RowPitch,
                &_source.pixels[static_cast<std::size_t>(row) * _source.widthPixels], sizeof(Rgba) * _source.widthPixels);
  }
  upload.buffer->Unmap(0, nullptr);
  const Staging readback = MakeStaging(device, target.get(), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);

  Engine::UpscalePass pass(device, {.targetFormat = SRGB_FORMAT});
  pass.SetSource(source.get());
  Engine::FrameRing frames(device, _device.DirectQueue());
  ID3D12GraphicsCommandList* commandList = frames.Begin();
  RecordCopy(commandList, source.get(), upload, true);
  const D3D12_RESOURCE_BARRIER toRead =
    Engine::TransitionBarrier(source.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  commandList->ResourceBarrier(1, &toRead);

  constexpr std::array<float, 4> MAGENTA = {1.0F, 0.0F, 1.0F, 1.0F};
  commandList->OMSetRenderTargets(1, &targetView, FALSE, nullptr);
  commandList->ClearRenderTargetView(targetView, MAGENTA.data(), 0, nullptr);
  const D3D12_VIEWPORT viewport{.TopLeftX = 0.0F,
                                .TopLeftY = 0.0F,
                                .Width = static_cast<float>(_widthPixels),
                                .Height = static_cast<float>(_heightPixels),
                                .MinDepth = 0.0F,
                                .MaxDepth = 1.0F};
  const D3D12_RECT scissor{.left = 0, .top = 0, .right = static_cast<LONG>(_widthPixels), .bottom = static_cast<LONG>(_heightPixels)};
  commandList->RSSetViewports(1, &viewport);
  commandList->RSSetScissorRects(1, &scissor);
  pass.Draw(commandList);

  const D3D12_RESOURCE_BARRIER toCopy =
    Engine::TransitionBarrier(target.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
  commandList->ResourceBarrier(1, &toCopy);
  RecordCopy(commandList, target.get(), readback, false);
  _device.DirectQueue().WaitFor(frames.End());

  Image result{.widthPixels = _widthPixels, .heightPixels = _heightPixels, .pixels = {}};
  result.pixels.resize(static_cast<std::size_t>(_widthPixels) * _heightPixels);
  // The buffer is as long as the footprint needs: every row but the last is padded to the pitch.
  const D3D12_RANGE readRange{.Begin = 0, .End = static_cast<SIZE_T>(readback.buffer->GetDesc().Width)};
  winrt::check_hresult(readback.buffer->Map(0, &readRange, &mapped));
  for (std::uint32_t row = 0; row < _heightPixels; ++row)
  {
    std::memcpy(&result.pixels[static_cast<std::size_t>(row) * _widthPixels],
                static_cast<const std::byte*>(mapped) + static_cast<std::size_t>(row) * readback.footprint.Footprint.RowPitch,
                sizeof(Rgba) * _widthPixels);
  }
  const D3D12_RANGE noWrite{.Begin = 0, .End = 0};
  readback.buffer->Unmap(0, &noWrite);
  return result;
}

/// Checks every pixel of _result against the filter worked out from _source.
void AssertFiltered(const Image& _source, const Image& _result)
{
  for (std::uint32_t y = 0; y < _result.heightPixels; ++y)
  {
    for (std::uint32_t x = 0; x < _result.widthPixels; ++x)
    {
      const Rgba expected = ExpectedPixel(_source, _result.widthPixels, _result.heightPixels, x, y);
      const Rgba& actual = _result.At(x, y);
      for (std::size_t channel = 0; channel < 3; ++channel)
      {
        if (std::abs(actual[channel] - expected[channel]) > COLOR_TOLERANCE)
        {
          Assert::Fail(std::format(L"pixel ({}, {}) is ({}, {}, {}), and the filter gives ({}, {}, {})", x, y, actual[0], actual[1],
                                   actual[2], expected[0], expected[1], expected[2])
                         .c_str());
        }
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

/// A source whose every texel differs from its neighbours in each channel.
Image Gradient(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  Image image{.widthPixels = _widthPixels, .heightPixels = _heightPixels, .pixels = {}};
  for (std::uint32_t y = 0; y < _heightPixels; ++y)
  {
    for (std::uint32_t x = 0; x < _widthPixels; ++x)
    {
      image.pixels.push_back({static_cast<std::uint8_t>(20 + 60 * x), static_cast<std::uint8_t>(30 + 50 * y),
                              static_cast<std::uint8_t>(200 - 40 * x - 20 * y), 255});
    }
  }
  return image;
}

} // namespace

TEST_CLASS(UpscalePassTests)
{
public:
  TEST_METHOD(CopiesTheSourceAtTheSameSize)
  {
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const Image source = Gradient(4, 4);
    const Image result = Upscale(device, source, 4, 4);

    for (std::size_t index = 0; index < source.pixels.size(); ++index)
    {
      for (std::size_t channel = 0; channel < 3; ++channel)
      {
        Assert::IsTrue(std::abs(result.pixels[index][channel] - source.pixels[index][channel]) <= 1,
                       std::format(L"texel {} channel {} moved from {} to {}", index, channel, source.pixels[index][channel],
                                   result.pixels[index][channel])
                         .c_str());
      }
    }
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(BlendsInLinearLight)
  {
    // A black texel beside a white one, scaled 4x across: in linear light the blended pixels are
    // far brighter than a blend of the sRGB bytes would make them. At a quarter of the way the
    // filter gives 0.125 linear, which encodes to 99, where blending bytes would give 32.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const Image source{.widthPixels = 2, .heightPixels = 1, .pixels = {{0, 0, 0, 255}, {255, 255, 255, 255}}};
    const Image result = Upscale(device, source, 8, 1);

    AssertFiltered(source, result);
    Assert::IsTrue(std::abs(result.At(2, 0)[0] - 99) <= COLOR_TOLERANCE, std::format(L"got {}", result.At(2, 0)[0]).c_str());
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(FiltersEveryPixelOfALargerTarget)
  {
    // A render scale of 0.75: a 3 x 3 source to a 4 x 4 target, so no pixel falls on a texel's
    // centre, and the edge pixels sample beyond the outer texels and clamp.
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const Image source = Gradient(3, 3);
    const Image result = Upscale(device, source, 4, 4);

    AssertFiltered(source, result);
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(RefusesToDrawWithoutASource)
  {
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const Engine::UpscalePass pass(device.Device(), {.targetFormat = SRGB_FORMAT});
    Engine::FrameRing frames(device.Device(), device.DirectQueue());
    ID3D12GraphicsCommandList* commandList = frames.Begin();
    Assert::ExpectException<std::logic_error>([&pass, commandList] { pass.Draw(commandList); });
    device.DirectQueue().WaitFor(frames.End());
  }

  TEST_METHOD(ScalesPixelsToTheNearest)
  {
    Assert::AreEqual(std::uint32_t{1440}, Engine::ScaledPixels(1920, 0.75F));
    Assert::AreEqual(std::uint32_t{810}, Engine::ScaledPixels(1080, 0.75F));
    Assert::AreEqual(std::uint32_t{683}, Engine::ScaledPixels(1366, 0.5F));
    Assert::AreEqual(std::uint32_t{1920}, Engine::ScaledPixels(1920, 1.0F));
    Assert::AreEqual(std::uint32_t{1}, Engine::ScaledPixels(1, 0.25F));
  }
};

} // namespace EngineTests
