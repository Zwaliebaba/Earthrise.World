#include "pch.h"

#include "ColorTarget.h"
#include "CommandQueue.h"
#include "FrameRing.h"
#include "GpuDevice.h"
#include "GpuTimer.h"
#include "ResourceBarrier.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The GPU timer on WARP (ADR-012): timestamps around real work come back as a time, and each frame
// slot keeps its own. WARP's times say nothing about speed (ADR-012, decision 5); these tests only
// check that the queries are recorded, resolved and read back where they belong.

namespace EngineTests
{

namespace
{

constexpr std::uint32_t TARGET_PIXELS = 512;
constexpr int CLEARS = 16;

/// Records _clears clears of a target, which WARP takes a measurable time over, inside _span.
void RecordWork(ID3D12GraphicsCommandList* _commandList, const Engine::GpuTimer& _timer, const Engine::ColorTarget& _target,
                std::uint32_t _slot, std::uint32_t _span, int _clears)
{
  const D3D12_RESOURCE_BARRIER toTarget =
    Engine::TransitionBarrier(_target.Texture(), Engine::ColorTarget::IDLE_STATE, D3D12_RESOURCE_STATE_RENDER_TARGET);
  _commandList->ResourceBarrier(1, &toTarget);
  _timer.Begin(_commandList, _slot, _span);
  for (int clear = 0; clear < _clears; ++clear)
  {
    _target.Clear(_commandList);
  }
  _timer.End(_commandList, _slot, _span);
  const D3D12_RESOURCE_BARRIER toIdle =
    Engine::TransitionBarrier(_target.Texture(), D3D12_RESOURCE_STATE_RENDER_TARGET, Engine::ColorTarget::IDLE_STATE);
  _commandList->ResourceBarrier(1, &toIdle);
}

} // namespace

TEST_CLASS(GpuTimerTests)
{
public:
  TEST_METHOD(TimesWorkOnTheGpu)
  {
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const Engine::GpuTimer timer(device.Device(), device.DirectQueue().Queue(),
                                 {.slotCount = Engine::FrameRing::FRAMES_IN_FLIGHT, .spanCount = 2});
    const Engine::ColorTarget target(device.Device(), {.format = DXGI_FORMAT_R8G8B8A8_UNORM, .clearColor = {0.2F, 0.4F, 0.6F, 1.0F}},
                                     TARGET_PIXELS, TARGET_PIXELS);
    Engine::FrameRing frames(device.Device(), device.DirectQueue());

    const std::uint32_t slot = frames.Slot();
    ID3D12GraphicsCommandList* commandList = frames.Begin();
    RecordWork(commandList, timer, target, slot, 0, CLEARS);
    timer.Begin(commandList, slot, 1); // a span with nothing in it
    timer.End(commandList, slot, 1);
    timer.Resolve(commandList, slot);
    device.DirectQueue().WaitFor(frames.End());

    const std::vector<double> milliseconds = timer.ReadMilliseconds(slot);
    Assert::AreEqual(std::size_t{2}, milliseconds.size());
    Assert::IsTrue(milliseconds[0] > 0.0 && milliseconds[0] < 10000.0, std::format(L"the clears took {} ms", milliseconds[0]).c_str());
    Assert::IsTrue(milliseconds[1] >= 0.0 && milliseconds[1] < 10000.0, std::format(L"the empty span took {} ms", milliseconds[1]).c_str());
    Assert::IsTrue(device.TakeDebugErrors().empty());
  }

  TEST_METHOD(KeepsEachSlotsSpansApart)
  {
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    const Engine::GpuTimer timer(device.Device(), device.DirectQueue().Queue(),
                                 {.slotCount = Engine::FrameRing::FRAMES_IN_FLIGHT, .spanCount = 1});
    const Engine::ColorTarget target(device.Device(), {.format = DXGI_FORMAT_R8G8B8A8_UNORM, .clearColor = {0.2F, 0.4F, 0.6F, 1.0F}},
                                     TARGET_PIXELS, TARGET_PIXELS);
    Engine::FrameRing frames(device.Device(), device.DirectQueue());

    const std::uint32_t firstSlot = frames.Slot();
    ID3D12GraphicsCommandList* commandList = frames.Begin();
    RecordWork(commandList, timer, target, firstSlot, 0, CLEARS);
    timer.Resolve(commandList, firstSlot);
    device.DirectQueue().WaitFor(frames.End());
    const std::vector<double> first = timer.ReadMilliseconds(firstSlot);

    // The next frame uses the other slot, and resolving it leaves the first slot's times alone.
    const std::uint32_t secondSlot = frames.Slot();
    Assert::AreNotEqual(firstSlot, secondSlot);
    commandList = frames.Begin();
    RecordWork(commandList, timer, target, secondSlot, 0, 1);
    timer.Resolve(commandList, secondSlot);
    device.DirectQueue().WaitFor(frames.End());

    Assert::AreEqual(first[0], timer.ReadMilliseconds(firstSlot)[0]);
    Assert::IsTrue(device.TakeDebugErrors().empty());
  }

  TEST_METHOD(RefusesCountsAndSpansItDoesNotHave)
  {
    Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = true});
    ID3D12CommandQueue* queue = device.DirectQueue().Queue();
    Assert::ExpectException<std::invalid_argument>([&device, queue]
                                                   { Engine::GpuTimer timer(device.Device(), queue, {.slotCount = 0, .spanCount = 1}); });
    Assert::ExpectException<std::invalid_argument>([&device, queue]
                                                   { Engine::GpuTimer timer(device.Device(), queue, {.slotCount = 2, .spanCount = 0}); });

    const Engine::GpuTimer timer(device.Device(), queue, {.slotCount = 2, .spanCount = 1});
    Engine::FrameRing frames(device.Device(), device.DirectQueue());
    ID3D12GraphicsCommandList* commandList = frames.Begin();
    Assert::ExpectException<std::out_of_range>([&timer, commandList] { timer.Begin(commandList, 0, 1); });
    Assert::ExpectException<std::out_of_range>([&timer, commandList] { timer.End(commandList, 2, 0); });
    Assert::ExpectException<std::out_of_range>([&timer] { (void)timer.ReadMilliseconds(2); });
    device.DirectQueue().WaitFor(frames.End());
  }
};

} // namespace EngineTests
