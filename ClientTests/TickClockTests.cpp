#include "pch.h"

#include "TickClock.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The client's clock (M3-C3; ADR-020, decision 4).

namespace ClientTests
{

namespace
{

constexpr double FRAME_SECONDS = 1.0 / 60.0;

} // namespace

TEST_CLASS(TickClockTests)
{
public:
  TEST_METHOD(GivesATickEveryThirdFrameAtSixtyFramesASecond)
  {
    Client::TickClock clock;
    std::uint32_t ticks = 0;
    for (std::uint32_t frame = 1; frame <= 60; ++frame)
    {
      const std::uint32_t due = clock.Advance(FRAME_SECONDS, 1.0);
      Assert::AreEqual(frame % 3 == 0 ? 1U : 0U, due);
      ticks += due;
    }
    Assert::AreEqual(Client::TickClock::TICKS_PER_SECOND, ticks);
  }

  TEST_METHOD(GivesEightOfASecondsTicksAndKeepsTheRestForLater)
  {
    Client::TickClock clock;
    Assert::AreEqual(8U, clock.Advance(1.0, 1.0));
    Assert::AreEqual(8U, clock.Advance(0.0, 1.0));
    Assert::AreEqual(4U, clock.Advance(0.0, 1.0));
    Assert::AreEqual(0U, clock.Advance(0.0, 1.0));
  }

  TEST_METHOD(GivesNoneWhilePausedAndKeepsNoTime)
  {
    Client::TickClock clock;
    clock.SetPaused(true);
    Assert::IsTrue(clock.Paused());
    Assert::AreEqual(0U, clock.Advance(1.0, 1.0));
    clock.SetPaused(false);
    Assert::AreEqual(0U, clock.Advance(0.0, 1.0));
    Assert::AreEqual(0.0, clock.TickFraction());
  }

  TEST_METHOD(GivesTenTimesTheTicksAtTenTimesTheSpeed)
  {
    Client::TickClock clock;
    std::uint32_t ticks = 0;
    for (std::uint32_t frame = 0; frame < 60; ++frame)
    {
      ticks += clock.Advance(FRAME_SECONDS, 10.0);
    }
    Assert::AreEqual(10 * Client::TickClock::TICKS_PER_SECOND, ticks);
  }

  TEST_METHOD(KnowsHowFarItIsIntoTheNextTick)
  {
    Client::TickClock clock;
    Assert::AreEqual(0U, clock.Advance(0.025, 1.0));
    Assert::AreEqual(0.5, clock.TickFraction(), 1e-9);
    Assert::AreEqual(0U, clock.Advance(0.0249, 1.0));
    Assert::IsTrue(clock.TickFraction() < 1.0);
    Assert::AreEqual(1U, clock.Advance(0.0001, 1.0));
    Assert::AreEqual(0.0, clock.TickFraction(), 1e-9);
  }
};

} // namespace ClientTests
