#include "pch.h"

#include "Fixed.h"
#include "IntegerMath.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace CoreTests
{

// Q16.16 and its one rounding rule, toward negative infinity (ADR-013, decision 3).
TEST_CLASS(FixedTests)
{
public:
  TEST_METHOD(RoundTripsWholeNumbers)
  {
    for (const std::int32_t whole : {-32767, -2, -1, 0, 1, 7, 32767})
    {
      Assert::AreEqual(whole, Core::Fixed::FromWhole(whole).FloorWhole());
      Assert::AreEqual(whole * Core::Fixed::ONE_RAW, Core::Fixed::FromWhole(whole).Raw());
    }
  }

  TEST_METHOD(FloorsMinusOneAndAHalfToMinusTwo)
  {
    const Core::Fixed minusOneAndAHalf = Core::Fixed::FromRaw(-Core::Fixed::ONE_RAW - Core::Fixed::ONE_RAW / 2);
    Assert::AreEqual(-2, minusOneAndAHalf.FloorWhole());
    Assert::AreEqual(1, Core::Fixed::FromRaw(Core::Fixed::ONE_RAW + Core::Fixed::ONE_RAW / 2).FloorWhole());
  }

  TEST_METHOD(FloorsAProductTowardNegativeInfinity)
  {
    // −1/65536 × 0.5 is −1/131072, which floors to −1 raw rather than truncating to 0.
    const Core::Fixed half = Core::Fixed::FromRaw(Core::Fixed::ONE_RAW / 2);
    Assert::AreEqual(-1, (Core::Fixed::FromRaw(-1) * half).Raw());
    Assert::AreEqual(0, (Core::Fixed::FromRaw(1) * half).Raw());
    Assert::IsTrue(Core::Fixed::FromWhole(6) == Core::Fixed::FromWhole(2) * Core::Fixed::FromWhole(3));
  }

  TEST_METHOD(FloorsAQuotientBelowMinusOneThird)
  {
    const Core::Fixed third = Core::Fixed::FromWhole(-1) / Core::Fixed::FromWhole(3);
    // −1/3 is −21845.33 raw, so the floor is −21846, below it.
    Assert::AreEqual(-21846, third.Raw());
    Assert::IsTrue(third.Raw() * 3 < -Core::Fixed::ONE_RAW);
    Assert::AreEqual(21845, (Core::Fixed::FromWhole(1) / Core::Fixed::FromWhole(3)).Raw());
  }

  TEST_METHOD(ThrowsPastThirtyTwoBits)
  {
    const Core::Fixed largest = Core::Fixed::FromRaw(INT32_MAX);
    const Core::Fixed smallest = Core::Fixed::FromRaw(INT32_MIN);
    Assert::ExpectException<std::overflow_error>([&] { static_cast<void>(largest + Core::Fixed::FromRaw(1)); });
    Assert::ExpectException<std::overflow_error>([&] { static_cast<void>(smallest - Core::Fixed::FromRaw(1)); });
    Assert::ExpectException<std::overflow_error>([&] { static_cast<void>(-smallest); });
    Assert::ExpectException<std::overflow_error>([] { static_cast<void>(Core::Fixed::FromWhole(200) * Core::Fixed::FromWhole(200)); });
    Assert::ExpectException<std::overflow_error>([] { static_cast<void>(Core::Fixed::FromWhole(32767) / Core::Fixed::FromRaw(1)); });
    Assert::ExpectException<std::overflow_error>([] { static_cast<void>(Core::Fixed::FromWhole(32768)); });
    Assert::ExpectException<std::overflow_error>([] { static_cast<void>(Core::Fixed::FromWhole(-32768)); });
  }

  TEST_METHOD(ThrowsOnDivisionByZero)
  {
    Assert::ExpectException<std::domain_error>([] { static_cast<void>(Core::Fixed::FromWhole(1) / Core::Fixed{}); });
  }

  TEST_METHOD(ConvertsAWalkingSpeedToATick)
  {
    // 1.4 m/s at 20 ticks a second is 0.07 m a tick: 4587.52 raw, floored once (ADR-016, decision 4).
    Assert::AreEqual(std::int64_t{4587}, Core::FloorDivide(std::int64_t{14} * Core::Fixed::ONE_RAW, std::int64_t{10} * 20));
    const Core::Fixed speedMetersPerTick = Core::Fixed::FromRaw(4587);
    Assert::AreEqual(4587, speedMetersPerTick.Raw());
  }

  TEST_METHOD(OrdersAndAccumulates)
  {
    Core::Fixed xMeters = Core::Fixed::FromWhole(-1);
    xMeters += Core::Fixed::FromRaw(Core::Fixed::ONE_RAW / 2);
    Assert::IsTrue(xMeters < Core::Fixed{});
    xMeters -= Core::Fixed::FromWhole(1);
    Assert::AreEqual(-2, xMeters.FloorWhole());
    Assert::IsTrue(Core::Fixed::FromRaw(1) > Core::Fixed::FromRaw(0));
  }
};

} // namespace CoreTests
