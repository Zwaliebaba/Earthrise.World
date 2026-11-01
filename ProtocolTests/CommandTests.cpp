#include "pch.h"

#include "Command.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ProtocolTests
{

namespace
{

std::vector<std::byte> Encoded(const Protocol::Command& _command)
{
  Core::ByteWriter writer;
  Protocol::Encode(_command, writer);
  return writer.TakeBytes();
}

Protocol::Command Decoded(const std::vector<std::byte>& _bytes)
{
  Core::ByteReader reader(_bytes);
  Protocol::Command command = Protocol::DecodeCommand(reader);
  Assert::IsTrue(reader.AtEnd());
  return command;
}

} // namespace

// Commands as bytes (ADR-002, decision 3): a u16 tag, a u16 record version, then the fields.
TEST_CLASS(CommandTests)
{
public:
  TEST_METHOD(RoundTripsEachCommand)
  {
    const Protocol::Command move = Protocol::MoveSquad{.squad = 42, .goalVoxels = {.xVoxels = -5, .yVoxels = 17, .zVoxels = 300}};
    const Protocol::Command fire = Protocol::FireEngine{.engine = 9, .targetVoxels = {.xVoxels = 100, .yVoxels = 0, .zVoxels = -1}};
    Assert::IsTrue(move == Decoded(Encoded(move)));
    Assert::IsTrue(fire == Decoded(Encoded(fire)));
  }

  TEST_METHOD(EncodesAMoveExactly)
  {
    const std::vector<std::byte> bytes =
      Encoded(Protocol::MoveSquad{.squad = 7, .goalVoxels = {.xVoxels = 1, .yVoxels = -2, .zVoxels = 3}});
    const std::array<unsigned, 24> expected = {0x01, 0x00, 0x01, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                               0x01, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00};
    Assert::AreEqual(expected.size(), bytes.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
      Assert::AreEqual(expected.at(index), static_cast<unsigned>(bytes.at(index)));
    }
  }

  TEST_METHOD(RefusesAnUnknownTagOrVersion)
  {
    std::vector<std::byte> bytes = Encoded(Protocol::FireEngine{.engine = 1, .targetVoxels = {}});
    bytes[0] = std::byte{3};
    Assert::ExpectException<std::invalid_argument>([&bytes] { static_cast<void>(Decoded(bytes)); });
    bytes = Encoded(Protocol::FireEngine{.engine = 1, .targetVoxels = {}});
    bytes[2] = std::byte{2};
    Assert::ExpectException<std::invalid_argument>([&bytes] { static_cast<void>(Decoded(bytes)); });
  }

  TEST_METHOD(RefusesTruncatedBytes)
  {
    std::vector<std::byte> bytes = Encoded(Protocol::MoveSquad{.squad = 1, .goalVoxels = {}});
    for (std::size_t length = 0; length < bytes.size(); ++length)
    {
      const std::vector<std::byte> truncated(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(length));
      Assert::ExpectException<std::out_of_range>([&truncated] { static_cast<void>(Decoded(truncated)); });
    }
  }
};

} // namespace ProtocolTests
