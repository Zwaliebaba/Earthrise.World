#pragma once

#include <span>

namespace Core
{

/// The nearest-rank percentile of _values: the value at rank ceil(_percent / 100 x count) once they
/// are sorted, so the smallest value that at least _percent percent of them do not exceed. It is
/// always one of the values, never a blend of two, which is how the renderer's pass mark is read
/// (GameDesign.md, *Starting the MVP*). Throws std::invalid_argument for no values, or for a
/// _percent that is not above 0 and at most 100.
[[nodiscard]] double NearestRankPercentile(std::span<const double> _values, double _percent);

} // namespace Core
