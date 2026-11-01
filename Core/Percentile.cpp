#include "pch.h"

#include "Percentile.h"

namespace Core
{

double NearestRankPercentile(std::span<const double> _values, double _percent)
{
  // Written so that a NaN fails it too.
  const bool inRange = _percent > 0.0 && _percent <= 100.0;
  if (_values.empty() || !inRange)
  {
    throw std::invalid_argument("a percentile needs values, and a percent above 0 and at most 100");
  }
  // The product comes first, so that a whole percent of a whole count is exact: 95 x 3540 / 100 is
  // 3363 exactly, where 0.95 x 3540 is not.
  const auto rank = static_cast<std::size_t>(std::ceil(_percent * static_cast<double>(_values.size()) / 100.0));
  std::vector<double> sorted(_values.begin(), _values.end());
  const auto nth = sorted.begin() + static_cast<std::ptrdiff_t>(rank - 1);
  std::nth_element(sorted.begin(), nth, sorted.end());
  return *nth;
}

} // namespace Core
