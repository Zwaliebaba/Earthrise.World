#include "pch.h"

#include "Query.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ProtocolTests
{

namespace
{

std::vector<std::byte> Encoded(const Protocol::QueryRequest& _request)
{
  Core::ByteWriter writer;
  Protocol::Encode(_request, writer);
  return writer.TakeBytes();
}

std::vector<std::byte> Encoded(const Protocol::Answer& _answer)
{
  Core::ByteWriter writer;
  Protocol::Encode(_answer, writer);
  return writer.TakeBytes();
}

Protocol::QueryRequest DecodedRequest(const std::vector<std::byte>& _bytes)
{
  Core::ByteReader reader(_bytes);
  Protocol::QueryRequest request = Protocol::DecodeQueryRequest(reader);
  Assert::IsTrue(reader.AtEnd());
  return request;
}

Protocol::Answer DecodedAnswer(const std::vector<std::byte>& _bytes)
{
  Core::ByteReader reader(_bytes);
  Protocol::Answer answer = Protocol::DecodeAnswer(reader);
  Assert::IsTrue(reader.AtEnd());
  return answer;
}

} // namespace

// Queries and answers as bytes (ADR-009, decision 6; ADR-020, decision 5): a u32 id, a u16 tag, a u16
// record version, then the fields; an answer adds the tick after its id.
TEST_CLASS(QueryTests)
{
public:
  TEST_METHOD(RoundTripsEachQuery)
  {
    const std::array<Protocol::QueryRequest, 3> requests = {{
      {.id = 1, .query = Protocol::AimCheck{.engine = 9, .targetVoxels = {.xVoxels = 100, .yVoxels = 9, .zVoxels = -3}}},
      {.id = 2, .query = Protocol::SectionIntegrity{.structure = 4}},
      {.id = 4000000000U,
       .query = Protocol::TileReach{.navigationClass = Protocol::NavigationClass::Cavalry,
                                    .startCellVoxels = {.xVoxels = 5, .yVoxels = 8, .zVoxels = 6},
                                    .tileX = 11,
                                    .tileZ = 5}},
    }};
    for (const Protocol::QueryRequest& request : requests)
    {
      Assert::IsTrue(request == DecodedRequest(Encoded(request)));
    }
  }

  TEST_METHOD(RoundTripsEachAnswer)
  {
    Protocol::TileReachReading reach{.tileX = 11, .tileZ = 5, .usable = {}, .reached = {}};
    reach.usable[0] = 0xff;
    reach.usable[127] = 0x81;
    reach.reached[3] = 0x10;
    const std::array<Protocol::Answer, 3> answers = {{
      {.queryId = 1,
       .tick = 77,
       .reading = Protocol::AimReading{.verdict = Protocol::AimVerdict::Blocked,
                                       .distanceMeters = 40,
                                       .strikes = true,
                                       .strikeVoxels = {.xVoxels = 381, .yVoxels = 10, .zVoxels = 180}}},
      {.queryId = 2,
       .tick = 78,
       .reading = Protocol::IntegrityReading{.structure = 2,
                                             .ground = Protocol::GroundKind::Rock,
                                             .canBeUndermined = true,
                                             .holeWidthVoxels = 5,
                                             .holeHeightVoxels = 1,
                                             .holeFallWidthVoxels = 5,
                                             .hasOpening = true,
                                             .openingFloorVoxels = 1,
                                             .openingFloor = Protocol::OpeningFloor::Stone,
                                             .openingClearVoxels = 1,
                                             .routeNow = false,
                                             .gapRunVoxels = 0,
                                             .gapFallRunVoxels = 5,
                                             .falling = true,
                                             .weakMinVoxels = {.xVoxels = 379, .yVoxels = 10, .zVoxels = 180},
                                             .weakMaxVoxels = {.xVoxels = 383, .yVoxels = 10, .zVoxels = 181}}},
      {.queryId = 3, .tick = 79, .reading = reach},
    }};
    for (const Protocol::Answer& answer : answers)
    {
      Assert::IsTrue(answer == DecodedAnswer(Encoded(answer)));
    }
  }

  TEST_METHOD(EncodesAnAimCheckExactly)
  {
    const std::vector<std::byte> bytes = Encoded(Protocol::QueryRequest{
      .id = 7, .query = Protocol::AimCheck{.engine = 3, .targetVoxels = {.xVoxels = 1, .yVoxels = 2, .zVoxels = 3}}});
    const std::array<unsigned, 28> expected = {0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
                                               0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00};
    Assert::AreEqual(expected.size(), bytes.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
      Assert::AreEqual(expected.at(index), static_cast<unsigned>(bytes.at(index)));
    }
  }

  TEST_METHOD(RefusesAnUnknownTagOrVersionOrValue)
  {
    std::vector<std::byte> bytes = Encoded(Protocol::QueryRequest{.id = 1, .query = Protocol::SectionIntegrity{.structure = 1}});
    bytes[4] = std::byte{4};
    Assert::ExpectException<std::invalid_argument>([&bytes] { static_cast<void>(DecodedRequest(bytes)); });
    bytes = Encoded(Protocol::QueryRequest{.id = 1, .query = Protocol::SectionIntegrity{.structure = 1}});
    bytes[6] = std::byte{2};
    Assert::ExpectException<std::invalid_argument>([&bytes] { static_cast<void>(DecodedRequest(bytes)); });
    bytes = Encoded(Protocol::QueryRequest{
      .id = 1,
      .query = Protocol::TileReach{.navigationClass = Protocol::NavigationClass::Foot, .startCellVoxels = {}, .tileX = 0, .tileZ = 0}});
    bytes[8] = std::byte{3}; // a fourth navigation class
    Assert::ExpectException<std::invalid_argument>([&bytes] { static_cast<void>(DecodedRequest(bytes)); });
  }

  TEST_METHOD(RefusesTruncatedBytes)
  {
    std::vector<std::byte> bytes = Encoded(Protocol::QueryRequest{.id = 1, .query = Protocol::AimCheck{.engine = 3, .targetVoxels = {}}});
    bytes.pop_back();
    Assert::ExpectException<std::out_of_range>([&bytes] { static_cast<void>(DecodedRequest(bytes)); });
    std::vector<std::byte> answer = Encoded(Protocol::Answer{.queryId = 1, .tick = 2, .reading = Protocol::TileReachReading{}});
    answer.resize(answer.size() - 5);
    Assert::ExpectException<std::out_of_range>([&answer] { static_cast<void>(DecodedAnswer(answer)); });
  }
};

} // namespace ProtocolTests
