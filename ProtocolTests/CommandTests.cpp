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
    const Protocol::VoxelPosition at{.xVoxels = -5, .yVoxels = 17, .zVoxels = 300};
    const Protocol::VoxelPosition away{.xVoxels = 100, .yVoxels = 0, .zVoxels = -1};
    const std::array<Protocol::Command, 15> commands = {Protocol::MoveSquad{.squad = 42, .goalVoxels = at},
                                                        Protocol::FireEngine{.engine = 9, .targetVoxels = away},
                                                        Protocol::AttackTarget{.squad = 3, .target = 0x0102030405060708ULL},
                                                        Protocol::StormTo{.squad = 4, .goalVoxels = away},
                                                        Protocol::StopSquad{.squad = 5},
                                                        Protocol::HoldSquad{.squad = 6},
                                                        Protocol::SplitSquad{.squad = 7},
                                                        Protocol::MergeSquads{.squads = {11, 12, 13}},
                                                        Protocol::GarrisonSquad{.squad = 8, .structure = 21},
                                                        Protocol::EjectGarrison{.structure = 22},
                                                        Protocol::RepairStructure{.laborer = 9, .structure = 23},
                                                        Protocol::DigTrench{.squad = 10, .minVoxels = at, .maxVoxels = away},
                                                        Protocol::PackEngine{.engine = 24},
                                                        Protocol::DeployEngine{.engine = 25, .quarterTurns = 3},
                                                        Protocol::SetGate{.structure = 26, .open = true}};
    for (std::size_t index = 0; index < commands.size(); ++index)
    {
      // Each command's tag is its place in the variant, from 1 (MvpPlan-M4.md, Contracts).
      Assert::AreEqual(static_cast<std::uint16_t>(index + 1), Protocol::CommandTag(commands.at(index)));
      Assert::IsTrue(commands.at(index) == Decoded(Encoded(commands.at(index))));
    }
    const Protocol::Command empty = Protocol::MergeSquads{.squads = {}};
    Assert::IsTrue(empty == Decoded(Encoded(empty)));
  }

  TEST_METHOD(EncodesAMergeExactly)
  {
    // Tag 8, version 1, a u8 count of 2, then 4 and 9 as u64.
    const std::vector<std::byte> bytes = Encoded(Protocol::MergeSquads{.squads = {4, 9}});
    const std::array<unsigned, 21> expected = {0x08, 0x00, 0x01, 0x00, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
                                               0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    Assert::AreEqual(expected.size(), bytes.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
      Assert::AreEqual(expected.at(index), static_cast<unsigned>(bytes.at(index)));
    }
  }

  TEST_METHOD(KeepsAFiresBytes)
  {
    // M2's FireEngine keeps its bytes: tag 2, version 1, the engine and the target.
    const std::vector<std::byte> bytes =
      Encoded(Protocol::FireEngine{.engine = 7, .targetVoxels = {.xVoxels = 1, .yVoxels = -2, .zVoxels = 3}});
    const std::array<unsigned, 24> expected = {0x02, 0x00, 0x01, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                               0x01, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00};
    Assert::AreEqual(expected.size(), bytes.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
      Assert::AreEqual(expected.at(index), static_cast<unsigned>(bytes.at(index)));
    }
  }

  TEST_METHOD(RefusesAGateThatIsNeitherOpenNorShut)
  {
    std::vector<std::byte> bytes = Encoded(Protocol::SetGate{.structure = 1, .open = false});
    bytes.back() = std::byte{2};
    Assert::ExpectException<std::invalid_argument>([&bytes] { static_cast<void>(Decoded(bytes)); });
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
    bytes[0] = std::byte{16}; // one past SetGate, M4's last
    Assert::ExpectException<std::invalid_argument>([&bytes] { static_cast<void>(Decoded(bytes)); });
    bytes[0] = std::byte{0};
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
