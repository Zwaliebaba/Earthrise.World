#include "pch.h"

#include "Interpolation.h"

namespace Client
{

namespace
{

float MetersOf(Core::Fixed _value) noexcept
{
  // Through double, since a float's 24 bits hold a raw value only to about 256 m.
  return static_cast<float>(static_cast<double>(_value.Raw()) / static_cast<double>(Core::Fixed::ONE_RAW));
}

} // namespace

DirectX::XMFLOAT3 Meters(const Protocol::FixedPosition& _position) noexcept
{
  return {MetersOf(_position.xMeters), MetersOf(_position.yMeters), MetersOf(_position.zMeters)};
}

DirectX::XMFLOAT3 DrawnPosition(const Protocol::FixedPosition* _previous, const Protocol::FixedPosition& _last, double _tickFraction,
                                float _snapMeters) noexcept
{
  const DirectX::XMFLOAT3 last = Meters(_last);
  if (_previous == nullptr)
  {
    return last;
  }
  const DirectX::XMFLOAT3 previous = Meters(*_previous);
  const float dx = last.x - previous.x;
  const float dy = last.y - previous.y;
  const float dz = last.z - previous.z;
  if ((dx * dx) + (dy * dy) + (dz * dz) > _snapMeters * _snapMeters)
  {
    return last;
  }
  const auto fraction = static_cast<float>(std::clamp(_tickFraction, 0.0, 1.0));
  return {previous.x + (dx * fraction), previous.y + (dy * fraction), previous.z + (dz * fraction)};
}

} // namespace Client
