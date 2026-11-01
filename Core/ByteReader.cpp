#include "pch.h"

#include "ByteReader.h"

namespace Core
{

ByteReader::ByteReader(std::span<const std::byte> _bytes) noexcept
  : m_bytes(_bytes)
{
}

std::uint8_t ByteReader::ReadU8()
{
  return static_cast<std::uint8_t>(ReadLittleEndian(1));
}

std::uint16_t ByteReader::ReadU16()
{
  return static_cast<std::uint16_t>(ReadLittleEndian(2));
}

std::uint32_t ByteReader::ReadU32()
{
  return static_cast<std::uint32_t>(ReadLittleEndian(4));
}

std::uint64_t ByteReader::ReadU64()
{
  return ReadLittleEndian(8);
}

std::int32_t ByteReader::ReadI32()
{
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(ReadLittleEndian(4)));
}

std::int64_t ByteReader::ReadI64()
{
  return static_cast<std::int64_t>(ReadLittleEndian(8));
}

std::span<const std::byte> ByteReader::ReadBytes(std::size_t _count)
{
  if (_count > Remaining())
  {
    throw std::out_of_range("a read past the end of the bytes");
  }
  const std::span<const std::byte> bytes = m_bytes.subspan(m_position, _count);
  m_position += _count;
  return bytes;
}

std::size_t ByteReader::Remaining() const noexcept
{
  return m_bytes.size() - m_position;
}

bool ByteReader::AtEnd() const noexcept
{
  return Remaining() == 0;
}

std::size_t ByteReader::Position() const noexcept
{
  return m_position;
}

std::uint64_t ByteReader::ReadLittleEndian(std::size_t _bytes)
{
  const std::span<const std::byte> bytes = ReadBytes(_bytes);
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < _bytes; ++index)
  {
    value |= static_cast<std::uint64_t>(bytes[index]) << (8 * index);
  }
  return value;
}

} // namespace Core
