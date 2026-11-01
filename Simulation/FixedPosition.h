#pragma once

#include "Fixed.h"

namespace Simulation
{

/// A point in meters, held as Core::Fixed (ADR-013, decision 3). y is the height: for a unit, the
/// height of its feet.
struct FixedPosition
{
  Core::Fixed xMeters;
  Core::Fixed yMeters;
  Core::Fixed zMeters;

  friend constexpr bool operator==(const FixedPosition&, const FixedPosition&) = default;
};

} // namespace Simulation
