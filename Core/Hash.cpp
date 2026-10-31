#include "pch.h"

#include "Hash.h"

namespace Core
{

std::uint64_t Fnv1a64(std::span<const std::byte> _bytes, std::uint64_t _seed) noexcept
{
  std::uint64_t hash = _seed;
  for (const std::byte value : _bytes)
  {
    hash ^= static_cast<std::uint64_t>(value);
    hash *= FNV_PRIME;
  }
  return hash;
}

std::uint64_t Fnv1a64(std::string_view _text, std::uint64_t _seed) noexcept
{
  return Fnv1a64(std::as_bytes(std::span(_text.data(), _text.size())), _seed);
}

} // namespace Core
