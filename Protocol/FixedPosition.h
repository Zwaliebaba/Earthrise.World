#pragma once

#include "Fixed.h"

namespace Protocol
{

/// A point in meters, held as Core::Fixed (ADR-013, decision 3). y is the height: for a unit, the
/// height of its feet. It crosses the boundary in state updates, so it lives here (ADR-020).
struct FixedPosition
{
  Core::Fixed xMeters;
  Core::Fixed yMeters;
  Core::Fixed zMeters;

  friend constexpr bool operator==(const FixedPosition&, const FixedPosition&) = default;
};

} // namespace Protocol
