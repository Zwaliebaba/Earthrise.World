#include "pch.h"

#include "ByteReader.h"
#include "ByteWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace CoreTests
{

namespace
{

std::vector<std::byte> Bytes(std::initializer_list<unsigned> _values)
{
  std::vector<std::byte> bytes;
  for (const unsigned value : _values)
  {
    bytes.push_back(static_cast<std::byte>(value));
  }
  return bytes;
}

bool Same(std::span<const std::byte> _actual, const std::vector<std::byte>& _expected)
{
  return std::ranges::equal(_actual, _expected);
}

} // namespace

// The byte layout commands, saves and ServerData share (ADR-014, ADR-016): little-endian by
// shifts, unpadded, two's complement.
TEST_CLASS(ByteTests)
{
public:
  TEST_METHOD(WritesLittleEndian)
  {
    Core::ByteWriter writer;
    writer.WriteU64(0x0102030405060708ULL);
    Assert::IsTrue(Same(writer.Bytes(), Bytes({0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01})));
  }

  TEST_METHOD(WritesTwosComplement)
  {
    Core::ByteWriter writer;
    writer.WriteI32(-2);
    Assert::IsTrue(Same(writer.Bytes(), Bytes({0xfe, 0xff, 0xff, 0xff})));
  }

  TEST_METHOD(RoundTripsEveryWidthAtItsLimits)
  {
    Core::ByteWriter writer;
    writer.WriteU8(0);
    writer.WriteU8(UINT8_MAX);
    writer.WriteU16(0);
    writer.WriteU16(UINT16_MAX);
    writer.WriteU32(0);
    writer.WriteU32(UINT32_MAX);
    writer.WriteU64(0);
    writer.WriteU64(UINT64_MAX);
    writer.WriteI32(INT32_MIN);
    writer.WriteI32(INT32_MAX);
    writer.WriteI64(INT64_MIN);
    writer.WriteI64(INT64_MAX);
    const std::vector<std::byte> bytes = writer.TakeBytes();
    Assert::AreEqual(std::size_t{2 + 4 + 8 + 16 + 8 + 16}, bytes.size());
    Assert::IsTrue(writer.Bytes().empty());

    Core::ByteReader reader(bytes);
    Assert::AreEqual(std::uint8_t{0}, reader.ReadU8());
    Assert::AreEqual(std::uint8_t{UINT8_MAX}, reader.ReadU8());
    Assert::AreEqual(std::uint16_t{0}, reader.ReadU16());
    Assert::AreEqual(std::uint16_t{UINT16_MAX}, reader.ReadU16());
    Assert::AreEqual(std::uint32_t{0}, reader.ReadU32());
    Assert::AreEqual(std::uint32_t{UINT32_MAX}, reader.ReadU32());
    Assert::AreEqual(std::uint64_t{0}, reader.ReadU64());
    Assert::AreEqual(std::uint64_t{UINT64_MAX}, reader.ReadU64());
    Assert::AreEqual(std::int32_t{INT32_MIN}, reader.ReadI32());
    Assert::AreEqual(std::int32_t{INT32_MAX}, reader.ReadI32());
    Assert::AreEqual(std::int64_t{INT64_MIN}, reader.ReadI64());
    Assert::AreEqual(std::int64_t{INT64_MAX}, reader.ReadI64());
    Assert::IsTrue(reader.AtEnd());
  }

  TEST_METHOD(ThrowsOnAReadPastTheEnd)
  {
    const std::vector<std::byte> bytes = Bytes({1, 2, 3});
    Core::ByteReader reader(bytes);
    Assert::ExpectException<std::out_of_range>([&reader] { static_cast<void>(reader.ReadU32()); });
    // A refused read consumes nothing.
    Assert::AreEqual(std::size_t{3}, reader.Remaining());
    Assert::AreEqual(std::uint16_t{0x0201}, reader.ReadU16());
    Assert::ExpectException<std::out_of_range>([&reader] { static_cast<void>(reader.ReadBytes(2)); });
    Assert::AreEqual(std::uint8_t{3}, reader.ReadU8());
    Assert::ExpectException<std::out_of_range>([&reader] { static_cast<void>(reader.ReadU8()); });
  }

  TEST_METHOD(AgreesOnRemainingAndAtEnd)
  {
    Core::ByteWriter writer;
    writer.WriteU16(7);
    const std::array<std::byte, 3> more = {std::byte{1}, std::byte{2}, std::byte{3}};
    writer.WriteBytes(more);
    Core::ByteReader reader(writer.Bytes());
    Assert::AreEqual(std::size_t{5}, reader.Remaining());
    Assert::IsFalse(reader.AtEnd());
    static_cast<void>(reader.ReadU16());
    Assert::AreEqual(std::size_t{3}, reader.Remaining());
    Assert::AreEqual(std::size_t{2}, reader.Position());
    Assert::IsTrue(Same(reader.ReadBytes(3), {more.begin(), more.end()}));
    Assert::AreEqual(std::size_t{0}, reader.Remaining());
    Assert::IsTrue(reader.AtEnd());
  }
};

} // namespace CoreTests
