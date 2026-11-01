#include "pch.h"

#include "ByteWriter.h"
#include "Session.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The client's copy of what it was told, and what it asks (M3-C3; ADR-020, decision 2).

namespace ClientTests
{

namespace
{

constexpr Protocol::VoxelPosition TARGET{.xVoxels = 381, .yVoxels = 9, .zVoxels = 180};

std::vector<std::byte> Bytes(const Protocol::Command& _command)
{
  Core::ByteWriter writer;
  Protocol::Encode(_command, writer);
  return writer.TakeBytes();
}

std::vector<std::byte> Bytes(const Protocol::QueryRequest& _request)
{
  Core::ByteWriter writer;
  Protocol::Encode(_request, writer);
  return writer.TakeBytes();
}

std::vector<std::byte> AnswerBytes(std::uint32_t _id, std::uint32_t _tick, const Protocol::Reading& _reading)
{
  Core::ByteWriter writer;
  Protocol::Encode(Protocol::Answer{.queryId = _id, .tick = _tick, .reading = _reading}, writer);
  return writer.TakeBytes();
}

Protocol::IntegrityReading Integrity(Protocol::EntityId _structure, std::uint8_t _holeWidthVoxels)
{
  return {.structure = _structure,
          .ground = Protocol::GroundKind::Earth,
          .canBeUndermined = false,
          .holeWidthVoxels = _holeWidthVoxels,
          .holeHeightVoxels = 0,
          .holeFallWidthVoxels = 0,
          .hasOpening = false,
          .openingFloorVoxels = 0,
          .openingFloor = Protocol::OpeningFloor::Stone,
          .openingClearVoxels = 0,
          .routeNow = false,
          .gapRunVoxels = 0,
          .gapFallRunVoxels = 0,
          .falling = false,
          .weakMinVoxels = {},
          .weakMaxVoxels = {}};
}

Protocol::StateUpdate Update(std::uint32_t _tick)
{
  Protocol::StateUpdate update{};
  update.tick = _tick;
  return update;
}

} // namespace

TEST_CLASS(SessionTests)
{
public:
  TEST_METHOD(KeepsTheLastUpdateAndTheOneBefore)
  {
    Client::Session session;
    session.Receive(Update(1), {}, {});
    Assert::AreEqual(1U, session.State().tick);
    Assert::IsTrue(session.PreviousState() == Protocol::StateUpdate{});
    session.Receive(Update(2), {}, {});
    Assert::AreEqual(2U, session.State().tick);
    Assert::AreEqual(1U, session.PreviousState().tick);
  }

  TEST_METHOD(HandsOverCommandsAndQueriesAsTheirBytes)
  {
    Client::Session session;
    const Protocol::Command fire = Protocol::FireEngine{.engine = 3, .targetVoxels = TARGET};
    session.Submit(fire, "click");
    const Protocol::Query aim = Protocol::AimCheck{.engine = 3, .targetVoxels = TARGET};
    session.Ask(aim);
    session.Ask(aim);
    const std::vector<std::vector<std::byte>> commands = session.TakeCommands();
    Assert::AreEqual(std::size_t{1}, commands.size());
    Assert::IsTrue(commands.front() == Bytes(fire));
    Assert::AreEqual(std::string("click"), session.TakenSources().front());
    Assert::IsTrue(session.TakeCommands().empty());
    const std::vector<std::vector<std::byte>> queries = session.TakeQueries();
    Assert::AreEqual(std::size_t{2}, queries.size());
    Assert::IsTrue(queries[0] == Bytes(Protocol::QueryRequest{.id = 1, .query = aim}));
    Assert::IsTrue(queries[1] == Bytes(Protocol::QueryRequest{.id = 2, .query = aim}));
    Assert::IsTrue(session.TakeQueries().empty());
  }

  TEST_METHOD(KeepsTheNewestAnswerAboutEachThing)
  {
    Client::Session session;
    session.Ask(Protocol::SectionIntegrity{.structure = 5});
    session.Ask(Protocol::SectionIntegrity{.structure = 5});
    session.Ask(Protocol::AimCheck{.engine = 3, .targetVoxels = TARGET});
    session.Ask(Protocol::TileReach{.navigationClass = Protocol::NavigationClass::Foot, .startCellVoxels = {}, .tileX = 11, .tileZ = 5});
    Assert::IsNull(session.Integrity(5));
    const Protocol::AimReading aim{.verdict = Protocol::AimVerdict::Blocked, .distanceMeters = 40, .strikes = true, .strikeVoxels = TARGET};
    const Protocol::TileReachReading reach{.tileX = 11, .tileZ = 5, .usable = {}, .reached = {}};
    const std::vector<std::vector<std::byte>> answers = {AnswerBytes(1, 3, Integrity(5, 1)), AnswerBytes(2, 4, Integrity(5, 2)),
                                                         AnswerBytes(3, 4, aim), AnswerBytes(4, 4, reach),
                                                         AnswerBytes(99, 4, Integrity(6, 7))};
    session.Receive(Update(4), {}, answers);
    Assert::IsNotNull(session.Integrity(5));
    Assert::AreEqual(std::uint8_t{2}, session.Integrity(5)->holeWidthVoxels);
    Assert::IsNull(session.Integrity(6)); // an answer to nothing it asked
    Assert::IsNotNull(session.Aim(3, TARGET));
    Assert::IsTrue(*session.Aim(3, TARGET) == aim);
    Assert::IsNull(session.Aim(3, {.xVoxels = 1, .yVoxels = 2, .zVoxels = 3}));
    Assert::IsNotNull(session.Reach(11, 5));
    Assert::IsNull(session.Reach(12, 5));
  }

  TEST_METHOD(HandsOverEachChunkOnce)
  {
    Client::Session session;
    const std::vector<Protocol::ChunkSnapshot> snapshots(2);
    session.Receive(Update(1), snapshots, {});
    Assert::AreEqual(std::size_t{2}, session.TakeSnapshots().size());
    Assert::IsTrue(session.TakeSnapshots().empty());
  }
};

} // namespace ClientTests
