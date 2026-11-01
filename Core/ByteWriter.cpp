#include "pch.h"

#include "ByteWriter.h"

namespace Core
{

void ByteWriter::WriteU8(std::uint8_t _value)
{
  WriteLittleEndian(_value, 1);
}

void ByteWriter::WriteU16(std::uint16_t _value)
{
  WriteLittleEndian(_value, 2);
}

void ByteWriter::WriteU32(std::uint32_t _value)
{
  WriteLittleEndian(_value, 4);
}

void ByteWriter::WriteU64(std::uint64_t _value)
{
  WriteLittleEndian(_value, 8);
}

void ByteWriter::WriteI32(std::int32_t _value)
{
  WriteLittleEndian(static_cast<std::uint32_t>(_value), 4);
}

void ByteWriter::WriteI64(std::int64_t _value)
{
  WriteLittleEndian(static_cast<std::uint64_t>(_value), 8);
}

void ByteWriter::WriteBytes(std::span<const std::byte> _bytes)
{
  m_bytes.insert(m_bytes.end(), _bytes.begin(), _bytes.end());
}

std::span<const std::byte> ByteWriter::Bytes() const noexcept
{
  return m_bytes;
}

std::vector<std::byte> ByteWriter::TakeBytes() noexcept
{
  return std::exchange(m_bytes, {});
}

void ByteWriter::WriteLittleEndian(std::uint64_t _value, std::size_t _bytes)
{
  for (std::size_t index = 0; index < _bytes; ++index)
  {
    m_bytes.push_back(static_cast<std::byte>((_value >> (8 * index)) & 0xffU));
  }
}

} // namespace Core
