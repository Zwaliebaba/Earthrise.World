#pragma once

#include <cstdint>

namespace Core
{

// Integer arithmetic the simulation needs and C++ does not give (ADR-013, decision 3). Portable
// 64-bit arithmetic only, with no intrinsics, so ARM64 needs no second path (AGENTS.md §3).

/// The integer square root: the largest whole number whose square is not above _value.
[[nodiscard]] std::uint32_t IntegerSquareRoot(std::uint64_t _value) noexcept;

/// _numerator / _denominator, rounded toward negative infinity, as C++'s / does not: −7 / 2 is −4.
/// Throws std::domain_error for a zero denominator, and std::overflow_error for the one quotient
/// that does not fit, INT64_MIN / −1.
[[nodiscard]] std::int64_t FloorDivide(std::int64_t _numerator, std::int64_t _denominator);

} // namespace Core
