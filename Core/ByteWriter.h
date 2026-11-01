#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Core
{

/// Writes the bytes that commands, saves, replays and the ServerData file are made of (ADR-002,
/// decision 3; ADR-014; ADR-016): little-endian by shifts, so the result does not depend on the
/// machine, and unpadded. A signed value is written as its two's complement.
class ByteWriter
{
public:
  void WriteU8(std::uint8_t _value);
  void WriteU16(std::uint16_t _value);
  void WriteU32(std::uint32_t _value);
  void WriteU64(std::uint64_t _value);
  void WriteI32(std::int32_t _value);
  void WriteI64(std::int64_t _value);
  void WriteBytes(std::span<const std::byte> _bytes);

  /// What has been written so far. Valid until the next write.
  [[nodiscard]] std::span<const std::byte> Bytes() const noexcept;

  /// Hands over what has been written and leaves the writer empty.
  [[nodiscard]] std::vector<std::byte> TakeBytes() noexcept;

private:
  void WriteLittleEndian(std::uint64_t _value, std::size_t _bytes);

  std::vector<std::byte> m_bytes;
};

} // namespace Core
