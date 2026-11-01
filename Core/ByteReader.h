#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace Core
{

/// Reads what a ByteWriter wrote: little-endian, unpadded, signed values in two's complement. It
/// refers to the bytes it is given, which must outlive it. Reading past the end throws
/// std::out_of_range and reads nothing, so a truncated command or file is refused rather than
/// read as zeros.
class ByteReader
{
public:
  explicit ByteReader(std::span<const std::byte> _bytes) noexcept;

  [[nodiscard]] std::uint8_t ReadU8();
  [[nodiscard]] std::uint16_t ReadU16();
  [[nodiscard]] std::uint32_t ReadU32();
  [[nodiscard]] std::uint64_t ReadU64();
  [[nodiscard]] std::int32_t ReadI32();
  [[nodiscard]] std::int64_t ReadI64();
  [[nodiscard]] std::span<const std::byte> ReadBytes(std::size_t _count);

  /// How many bytes are left to read.
  [[nodiscard]] std::size_t Remaining() const noexcept;
  [[nodiscard]] bool AtEnd() const noexcept;
  /// How many bytes have been read.
  [[nodiscard]] std::size_t Position() const noexcept;

private:
  [[nodiscard]] std::uint64_t ReadLittleEndian(std::size_t _bytes);

  std::span<const std::byte> m_bytes;
  std::size_t m_position = 0;
};

} // namespace Core
