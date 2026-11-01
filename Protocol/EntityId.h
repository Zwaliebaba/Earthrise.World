#pragma once

#include <cstdint>

namespace Protocol
{

/// What names an entity: a squad, a unit, an engine, a shot, a collapse. A world gives ids out from
/// 1, rising, and never reuses one, so a pass in id order is a pass in the order entities were made
/// (ADR-013, decision 5).
using EntityId = std::uint64_t;

/// The id no entity has.
inline constexpr EntityId NO_ENTITY = 0;

} // namespace Protocol
