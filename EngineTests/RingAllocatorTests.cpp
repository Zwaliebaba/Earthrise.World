#include "pch.h"

#include "RingAllocator.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace EngineTests
{

TEST_CLASS(RingAllocatorTests)
{
public:
  TEST_METHOD(HandsOutAlignedSpansFrontToBack)
  {
    Engine::RingAllocator ring(1024);
    Assert::IsTrue(ring.Allocate(10, 1) == std::uint64_t{0});
    Assert::IsTrue(ring.Allocate(16, 256) == std::uint64_t{256});
    Assert::AreEqual(std::uint64_t{272}, ring.UsedBytes(), L"the padding before the aligned span is held too");
  }

  TEST_METHOD(RefusesWhatDoesNotFitUntilTheGpuFinishes)
  {
    Engine::RingAllocator ring(100);
    Assert::IsTrue(ring.Allocate(60, 1) == std::uint64_t{0});
    ring.Retire(1);
    Assert::IsFalse(ring.Allocate(50, 1).has_value());
    ring.Reclaim(0);
    Assert::IsFalse(ring.Allocate(50, 1).has_value(), L"fence 1 has not completed");
    ring.Reclaim(1);
    Assert::AreEqual(std::uint64_t{0}, ring.UsedBytes());
    Assert::IsTrue(ring.Allocate(50, 1) == std::uint64_t{0}, L"an empty ring starts again at the front");
  }

  TEST_METHOD(WrapsToTheFrontAndHoldsTheSkippedEnd)
  {
    Engine::RingAllocator ring(100);
    Assert::IsTrue(ring.Allocate(60, 1) == std::uint64_t{0});
    ring.Retire(1);
    Assert::IsTrue(ring.Allocate(30, 1) == std::uint64_t{60});
    ring.Retire(2);
    ring.Reclaim(1);

    // 10 bytes are left at the end; a span of 50 wraps and holds them as well.
    Assert::IsTrue(ring.Allocate(50, 1) == std::uint64_t{0});
    Assert::AreEqual(std::uint64_t{90}, ring.UsedBytes());
    ring.Retire(3);
    Assert::IsFalse(ring.Allocate(20, 1).has_value(), L"only [50, 60) is free");

    ring.Reclaim(2);
    Assert::IsTrue(ring.Allocate(20, 1) == std::uint64_t{50}, L"[60, 90) came back");
    ring.Retire(4);
    ring.Reclaim(4);
    Assert::AreEqual(std::uint64_t{0}, ring.UsedBytes());
  }

  TEST_METHOD(KeepsUnretiredSpansThroughAReclaim)
  {
    Engine::RingAllocator ring(64);
    Assert::IsTrue(ring.Allocate(32, 1).has_value());
    ring.Reclaim(1000);
    Assert::AreEqual(std::uint64_t{32}, ring.UsedBytes(), L"a span is held until it is retired");
  }

  TEST_METHOD(RefusesBadArguments)
  {
    Assert::ExpectException<std::invalid_argument>([] { Engine::RingAllocator ring(0); });
    Engine::RingAllocator ring(64);
    Assert::ExpectException<std::invalid_argument>([&ring] { (void)ring.Allocate(0, 1); });
    Assert::ExpectException<std::invalid_argument>([&ring] { (void)ring.Allocate(65, 1); });
    Assert::ExpectException<std::invalid_argument>([&ring] { (void)ring.Allocate(8, 3); });
    ring.Retire(5);
    Assert::ExpectException<std::invalid_argument>([&ring] { ring.Retire(4); });
  }
};

} // namespace EngineTests
