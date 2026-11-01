#include "pch.h"

#include "IntegerMath.h"

namespace Core
{

std::uint32_t IntegerSquareRoot(std::uint64_t _value) noexcept
{
  // Digit by digit, two bits of _value to one of the root: exact, and the same on every machine.
  std::uint64_t remainder = _value;
  std::uint64_t root = 0;
  std::uint64_t bit = std::uint64_t{1} << 62;
  while (bit > remainder)
  {
    bit >>= 2;
  }
  while (bit != 0)
  {
    if (remainder >= root + bit)
    {
      remainder -= root + bit;
      root = (root >> 1) + bit;
    }
    else
    {
      root >>= 1;
    }
    bit >>= 2;
  }
  return static_cast<std::uint32_t>(root);
}

std::int64_t FloorDivide(std::int64_t _numerator, std::int64_t _denominator)
{
  if (_denominator == 0)
  {
    throw std::domain_error("Core::FloorDivide by zero");
  }
  if (_numerator == INT64_MIN && _denominator == -1)
  {
    throw std::overflow_error("Core::FloorDivide of INT64_MIN by -1 does not fit in 64 bits");
  }
  const std::int64_t quotient = _numerator / _denominator;
  const bool inexact = quotient * _denominator != _numerator;
  return inexact && ((_numerator < 0) != (_denominator < 0)) ? quotient - 1 : quotient;
}

} // namespace Core
