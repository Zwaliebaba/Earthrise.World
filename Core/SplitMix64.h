#pragma once

#include <cstdint>

namespace Core
{

// The one pinned PRNG of the whole tree (ADR-013, decision 4): SplitMix64, with the constants of
// its reference implementation (Steele, Lea and Flood, *Fast splittable pseudorandom number
// generators*, OOPSLA 2014). CoreTests pins it to outputs computed from that reference, so saves,
// replays and generated worlds that depend on it cannot drift.

inline constexpr std::uint64_t SPLITMIX64_GAMMA = 0x9e3779b97f4a7c15ULL;

/// SplitMix64's first output with _value as its seed: the gamma added, then the finalizer.
[[nodiscard]] constexpr std::uint64_t SplitMix64Mix(std::uint64_t _value) noexcept
{
  _value += SPLITMIX64_GAMMA;
  _value = (_value ^ (_value >> 30)) * 0xbf58476d1ce4e5b9ULL;
  _value = (_value ^ (_value >> 27)) * 0x94d049bb133111ebULL;
  return _value ^ (_value >> 31);
}

/// The sequential generator, for a stream whose order is fixed by its own rule. Most draws in the
/// simulation are keyed instead (KeyedDraw), so that adding one never shifts another.
class SplitMix64
{
public:
  explicit constexpr SplitMix64(std::uint64_t _seed) noexcept
    : m_state(_seed)
  {
  }

  /// The next output: SplitMix64Mix of the state, after which the state advances by the gamma.
  [[nodiscard]] constexpr std::uint64_t Next() noexcept
  {
    const std::uint64_t output = SplitMix64Mix(m_state);
    m_state += SPLITMIX64_GAMMA;
    return output;
  }

private:
  std::uint64_t m_state;
};

/// A keyed draw: the seed mixed with a salt naming the draw's purpose and a key naming what it is
/// for, such as an entity id, a tick or a packed chunk coordinate (ADR-013, decision 4). Equal to
/// SplitMix64Mix(_seed ^ SplitMix64Mix(_salt ^ SplitMix64Mix(_key))).
[[nodiscard]] constexpr std::uint64_t KeyedDraw(std::uint64_t _seed, std::uint64_t _salt, std::uint64_t _key) noexcept
{
  return SplitMix64Mix(_seed ^ SplitMix64Mix(_salt ^ SplitMix64Mix(_key)));
}

/// A draw reduced to [0, _bound) by its high 32 bits: ((_draw >> 32) × _bound) >> 32. Its bias is
/// below _bound / 2^32, which no bound the game uses can see.
[[nodiscard]] constexpr std::uint32_t DrawBelow(std::uint64_t _draw, std::uint32_t _bound) noexcept
{
  return static_cast<std::uint32_t>(((_draw >> 32) * _bound) >> 32);
}

} // namespace Core
