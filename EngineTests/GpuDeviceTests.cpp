#include "pch.h"

#include "CommandQueue.h"
#include "DescriptorHeap.h"
#include "FrameRing.h"
#include "GpuDevice.h"
#include "UploadRing.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// These run on WARP, the software adapter Windows ships, so that CI exercises the engine base on
// runners without a GPU. With the debug layer installed (Graphics Tools), every test also asserts
// that it reported no error.

namespace EngineTests
{

namespace
{

Engine::GpuDevice MakeWarpDevice()
{
  return Engine::GpuDevice({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
}

void AssertNoDebugErrors(Engine::GpuDevice& _device)
{
  const std::vector<std::string> errors = _device.TakeDebugErrors();
  if (!errors.empty())
  {
    Assert::Fail(std::wstring(errors.front().begin(), errors.front().end()).c_str());
  }
}

winrt::com_ptr<ID3D12Resource> MakeBuffer(ID3D12Device* _device, D3D12_HEAP_TYPE _heap, std::uint64_t _sizeBytes,
                                          D3D12_RESOURCE_STATES _state)
{
  const D3D12_HEAP_PROPERTIES heapProperties{.Type = _heap};
  const D3D12_RESOURCE_DESC bufferDesc{.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                       .Width = _sizeBytes,
                                       .Height = 1,
                                       .DepthOrArraySize = 1,
                                       .MipLevels = 1,
                                       .Format = DXGI_FORMAT_UNKNOWN,
                                       .SampleDesc = {.Count = 1},
                                       .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR};
  winrt::com_ptr<ID3D12Resource> buffer;
  winrt::check_hresult(
    _device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, _state, nullptr, IID_PPV_ARGS(buffer.put())));
  return buffer;
}

} // namespace

TEST_CLASS(GpuDeviceTests)
{
public:
  TEST_METHOD(CreatesAWarpDeviceAtFeatureLevel12)
  {
    Engine::GpuDevice device = MakeWarpDevice();
    Assert::IsNotNull(device.Device());
    Assert::IsFalse(device.AdapterName().empty());
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(SignalsAndWaitsOnTheDirectQueue)
  {
    Engine::GpuDevice device = MakeWarpDevice();
    Engine::CommandQueue& queue = device.DirectQueue();
    const std::uint64_t first = queue.Signal();
    const std::uint64_t second = queue.Signal();
    Assert::IsTrue(second > first);
    queue.WaitFor(second);
    Assert::IsTrue(queue.CompletedValue() >= second);
    queue.Flush();
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(HandsOutDescriptorsUntilTheHeapIsFull)
  {
    Engine::GpuDevice device = MakeWarpDevice();
    Engine::DescriptorHeap heap(device.Device(),
                                {.type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV, .capacityDescriptors = 4, .shaderVisible = false});
    const Engine::DescriptorHeap::Handle first = heap.Allocate(3);
    const Engine::DescriptorHeap::Handle last = heap.Allocate();
    const SIZE_T incrementBytes = device.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    Assert::AreEqual(std::uint32_t{3}, last.index);
    Assert::IsTrue(last.cpu.ptr == first.cpu.ptr + 3 * incrementBytes);
    Assert::IsTrue(last.gpu.ptr == 0, L"a render-target heap is not shader-visible");
    Assert::ExpectException<std::length_error>([&heap] { (void)heap.Allocate(); });
    heap.Reset();
    Assert::AreEqual(std::uint32_t{0}, heap.Allocate().index);
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(GivesShaderVisibleDescriptorsAGpuHandle)
  {
    Engine::GpuDevice device = MakeWarpDevice();
    Engine::DescriptorHeap heap(device.Device(),
                                {.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, .capacityDescriptors = 8, .shaderVisible = true});
    Assert::IsTrue(heap.Allocate().gpu.ptr != 0);
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(UploadsBytesTheGpuCopiesBack)
  {
    constexpr std::uint64_t SIZE_BYTES = 256;
    Engine::GpuDevice device = MakeWarpDevice();
    Engine::UploadRing ring(device.Device(), std::uint64_t{64} * 1024);
    const std::optional<Engine::UploadRing::Allocation> allocation = ring.Allocate(SIZE_BYTES, 256);
    if (!allocation)
    {
      Assert::Fail(L"an empty ring refused 256 bytes");
      return;
    }
    for (std::size_t index = 0; index < allocation->bytes.size(); ++index)
    {
      allocation->bytes[index] = static_cast<std::byte>(index * 7 + 3);
    }

    winrt::com_ptr<ID3D12Resource> readback =
      MakeBuffer(device.Device(), D3D12_HEAP_TYPE_READBACK, SIZE_BYTES, D3D12_RESOURCE_STATE_COPY_DEST);
    Engine::FrameRing frames(device.Device(), device.DirectQueue());
    ID3D12GraphicsCommandList* commandList = frames.Begin();
    commandList->CopyBufferRegion(readback.get(), 0, allocation->buffer, allocation->offsetBytes, SIZE_BYTES);
    const std::uint64_t fenceValue = frames.End();
    ring.Retire(fenceValue);
    device.DirectQueue().WaitFor(fenceValue);
    ring.Reclaim(device.DirectQueue().CompletedValue());
    Assert::AreEqual(std::uint64_t{0}, ring.UsedBytes(), L"the upload is reclaimed once its copy has finished");

    const D3D12_RANGE readRange{.Begin = 0, .End = SIZE_BYTES};
    void* mapped = nullptr;
    winrt::check_hresult(readback->Map(0, &readRange, &mapped));
    const std::span<const std::byte> copied(static_cast<const std::byte*>(mapped), SIZE_BYTES);
    bool same = true;
    for (std::size_t index = 0; index < copied.size(); ++index)
    {
      same = same && copied[index] == static_cast<std::byte>(index * 7 + 3);
    }
    const D3D12_RANGE noWrite{.Begin = 0, .End = 0};
    readback->Unmap(0, &noWrite);
    Assert::IsTrue(same, L"the GPU's copy differs from what was uploaded");
    AssertNoDebugErrors(device);
  }

  TEST_METHOD(KeepsTwoFramesInFlight)
  {
    Engine::GpuDevice device = MakeWarpDevice();
    Engine::FrameRing frames(device.Device(), device.DirectQueue());
    std::uint64_t previous = 0;
    for (std::uint32_t frame = 0; frame < 2 * Engine::FrameRing::FRAMES_IN_FLIGHT + 1; ++frame)
    {
      Assert::AreEqual(frame % Engine::FrameRing::FRAMES_IN_FLIGHT, frames.Slot());
      (void)frames.Begin();
      const std::uint64_t fenceValue = frames.End();
      Assert::IsTrue(fenceValue > previous);
      previous = fenceValue;
    }
    device.DirectQueue().Flush();
    AssertNoDebugErrors(device);
  }
};

} // namespace EngineTests
