#include "pch.h"

#include "IntegerMath.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace CoreTests
{

TEST_CLASS(IntegerMathTests)
{
public:
  TEST_METHOD(TakesTheSquareRootOfSmallNumbers)
  {
    Assert::AreEqual(std::uint32_t{0}, Core::IntegerSquareRoot(0));
    Assert::AreEqual(std::uint32_t{1}, Core::IntegerSquareRoot(1));
    Assert::AreEqual(std::uint32_t{1}, Core::IntegerSquareRoot(2));
    Assert::AreEqual(std::uint32_t{2}, Core::IntegerSquareRoot(4));
  }

  TEST_METHOD(TakesTheSquareRootOfTheLargest)
  {
    Assert::AreEqual(std::uint32_t{4294967295U}, Core::IntegerSquareRoot(UINT64_MAX));
  }

  TEST_METHOD(FloorsAroundExactSquares)
  {
    for (const std::uint64_t n : {std::uint64_t{2}, std::uint64_t{65535}, std::uint64_t{4294967295U}})
    {
      Assert::AreEqual(static_cast<std::uint32_t>(n), Core::IntegerSquareRoot(n * n));
      Assert::AreEqual(static_cast<std::uint32_t>(n - 1), Core::IntegerSquareRoot(n * n - 1));
    }
  }

  TEST_METHOD(DividesTowardNegativeInfinity)
  {
    Assert::AreEqual(std::int64_t{-4}, Core::FloorDivide(-7, 2));
    Assert::AreEqual(std::int64_t{-4}, Core::FloorDivide(7, -2));
    Assert::AreEqual(std::int64_t{3}, Core::FloorDivide(-7, -2));
    Assert::AreEqual(std::int64_t{3}, Core::FloorDivide(7, 2));
    Assert::AreEqual(std::int64_t{-3}, Core::FloorDivide(-6, 2));
  }

  TEST_METHOD(RefusesAZeroDenominator)
  {
    Assert::ExpectException<std::domain_error>([] { static_cast<void>(Core::FloorDivide(1, 0)); });
    Assert::ExpectException<std::overflow_error>([] { static_cast<void>(Core::FloorDivide(INT64_MIN, -1)); });
  }
};

} // namespace CoreTests
