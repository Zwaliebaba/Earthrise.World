#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace Core
{

// The pinned 64-bit hash: FNV-1a. Content hashes, saves and the shared-content check of ADR-008
// depend on its results, so it never changes; CoreTests pin it to the published test vectors.
inline constexpr std::uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325ULL;
inline constexpr std::uint64_t FNV_PRIME = 0x100000001b3ULL;

/// Hashes bytes. Pass a previous result as the seed to continue a hash across several buffers.
[[nodiscard]] std::uint64_t Fnv1a64(std::span<const std::byte> _bytes, std::uint64_t _seed = FNV_OFFSET_BASIS) noexcept;

/// Hashes the characters of a string, without a terminator.
[[nodiscard]] std::uint64_t Fnv1a64(std::string_view _text, std::uint64_t _seed = FNV_OFFSET_BASIS) noexcept;

} // namespace Core
