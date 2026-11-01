#pragma once

#include <compare>
#include <cstdint>

namespace Core
{

/// A signed fixed-point number, Q16.16 in a 32-bit integer: the simulation's positions and
/// velocities (ADR-013, decision 3). Its range is about ±32,768 and its step 1/65536. The type says
/// how the number is stored and the name says what it counts, as in xMeters and speedMetersPerTick
/// (R6).
///
/// Arithmetic goes through 64-bit intermediates. A product or a quotient rounds toward negative
/// infinity, the one rounding rule of ADR-013. A result that does not fit in 32 bits throws
/// std::overflow_error, and a division by zero throws std::domain_error, so nothing wraps silently.
class Fixed
{
public:
  static constexpr std::int32_t FRACTION_BITS = 16;
  static constexpr std::int32_t ONE_RAW = 1 << FRACTION_BITS;
  /// The largest whole number FromWhole takes, either way of zero.
  static constexpr std::int32_t MAX_WHOLE = (1 << (31 - FRACTION_BITS)) - 1;

  constexpr Fixed() noexcept = default;

  /// The number whose raw value is _raw, in 65536ths.
  [[nodiscard]] static constexpr Fixed FromRaw(std::int32_t _raw) noexcept
  {
    Fixed value;
    value.m_raw = _raw;
    return value;
  }

  /// A whole number. Throws std::overflow_error past ±MAX_WHOLE.
  [[nodiscard]] static Fixed FromWhole(std::int32_t _whole);

  [[nodiscard]] constexpr std::int32_t Raw() const noexcept
  {
    return m_raw;
  }

  /// The largest whole number not above this one: −1.5 gives −2.
  [[nodiscard]] constexpr std::int32_t FloorWhole() const noexcept
  {
    // An arithmetic shift, which C++20 defines for negative values, rounds toward negative infinity.
    return m_raw >> FRACTION_BITS;
  }

  friend constexpr bool operator==(const Fixed&, const Fixed&) noexcept = default;
  friend constexpr std::strong_ordering operator<=>(const Fixed&, const Fixed&) noexcept = default;

  Fixed& operator+=(Fixed _other);
  Fixed& operator-=(Fixed _other);

private:
  std::int32_t m_raw = 0;
};

[[nodiscard]] Fixed operator+(Fixed _left, Fixed _right);
[[nodiscard]] Fixed operator-(Fixed _left, Fixed _right);
[[nodiscard]] Fixed operator-(Fixed _value);
[[nodiscard]] Fixed operator*(Fixed _left, Fixed _right);
[[nodiscard]] Fixed operator/(Fixed _left, Fixed _right);

} // namespace Core
