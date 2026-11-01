#include "pch.h"

#include "Percentile.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace CoreTests
{

namespace
{

std::vector<double> OneTo(std::size_t _count)
{
  std::vector<double> values;
  for (std::size_t value = _count; value >= 1; --value)
  {
    values.push_back(static_cast<double>(value)); // in falling order, so the function must sort
  }
  return values;
}

} // namespace

TEST_CLASS(PercentileTests)
{
public:
  TEST_METHOD(TakesTheValueAtTheNearestRank)
  {
    Assert::AreEqual(95.0, Core::NearestRankPercentile(OneTo(100), 95.0));
    Assert::AreEqual(19.0, Core::NearestRankPercentile(OneTo(20), 95.0));
    // 3540 frames, the measured run after its first second: rank 3363 exactly.
    Assert::AreEqual(3363.0, Core::NearestRankPercentile(OneTo(3540), 95.0));
  }

  TEST_METHOD(RoundsTheRankUp)
  {
    const std::vector<double> values = {5.0, 1.0, 3.0};
    Assert::AreEqual(3.0, Core::NearestRankPercentile(values, 50.0)); // rank ceil(1.5) = 2
    Assert::AreEqual(5.0, Core::NearestRankPercentile(values, 95.0)); // rank ceil(2.85) = 3
    Assert::AreEqual(1.0, Core::NearestRankPercentile(values, 1.0));  // rank ceil(0.03) = 1
  }

  TEST_METHOD(GivesTheLargestAtOneHundred)
  {
    Assert::AreEqual(100.0, Core::NearestRankPercentile(OneTo(100), 100.0));
  }

  TEST_METHOD(RefusesNoValuesAndPercentsOutOfRange)
  {
    const std::vector<double> values = {1.0, 2.0};
    Assert::ExpectException<std::invalid_argument>([] { (void)Core::NearestRankPercentile({}, 95.0); });
    Assert::ExpectException<std::invalid_argument>([&values] { (void)Core::NearestRankPercentile(values, 0.0); });
    Assert::ExpectException<std::invalid_argument>([&values] { (void)Core::NearestRankPercentile(values, 100.5); });
    Assert::ExpectException<std::invalid_argument>(
      [&values] { (void)Core::NearestRankPercentile(values, std::numeric_limits<double>::quiet_NaN()); });
  }
};

} // namespace CoreTests
