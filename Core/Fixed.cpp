#include "pch.h"

#include "Fixed.h"
#include "IntegerMath.h"

namespace Core
{

namespace
{

Fixed Narrow(std::int64_t _raw, const char* _message)
{
  if (_raw < INT32_MIN || _raw > INT32_MAX)
  {
    throw std::overflow_error(_message);
  }
  return Fixed::FromRaw(static_cast<std::int32_t>(_raw));
}

} // namespace

Fixed Fixed::FromWhole(std::int32_t _whole)
{
  if (_whole < -MAX_WHOLE || _whole > MAX_WHOLE)
  {
    throw std::overflow_error("Core::Fixed holds whole numbers up to 32767 either way of zero");
  }
  return FromRaw(_whole * ONE_RAW);
}

Fixed operator+(Fixed _left, Fixed _right)
{
  return Narrow(std::int64_t{_left.Raw()} + _right.Raw(), "a Core::Fixed sum does not fit in 32 bits");
}

Fixed operator-(Fixed _left, Fixed _right)
{
  return Narrow(std::int64_t{_left.Raw()} - _right.Raw(), "a Core::Fixed difference does not fit in 32 bits");
}

Fixed operator-(Fixed _value)
{
  return Narrow(-std::int64_t{_value.Raw()}, "a Core::Fixed negation does not fit in 32 bits");
}

Fixed operator*(Fixed _left, Fixed _right)
{
  // The 64-bit product holds 32 fraction bits; the arithmetic shift drops 16 of them, flooring.
  return Narrow((std::int64_t{_left.Raw()} * _right.Raw()) >> Fixed::FRACTION_BITS, "a Core::Fixed product does not fit in 32 bits");
}

Fixed operator/(Fixed _left, Fixed _right)
{
  if (_right.Raw() == 0)
  {
    throw std::domain_error("Core::Fixed division by zero");
  }
  return Narrow(FloorDivide(std::int64_t{_left.Raw()} * Fixed::ONE_RAW, _right.Raw()), "a Core::Fixed quotient does not fit in 32 bits");
}

Fixed& Fixed::operator+=(Fixed _other)
{
  *this = *this + _other;
  return *this;
}

Fixed& Fixed::operator-=(Fixed _other)
{
  *this = *this - _other;
  return *this;
}

} // namespace Core
