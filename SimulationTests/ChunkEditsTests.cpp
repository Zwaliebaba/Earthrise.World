#include "pch.h"

#include "ChunkEdits.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// A chunk's edits as a save writes them (ADR-014, decision 3).

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t EARTH = 2;
constexpr std::uint8_t MASONRY = 6;
constexpr std::int32_t EDGE = Protocol::Chunk::EDGE_VOXELS;

/// A base like the generator's: earth up to y 10.
std::unique_ptr<Protocol::Chunk> Ground()
{
  auto chunk = std::make_unique<Protocol::Chunk>();
  for (std::int32_t y = 0; y < 10; ++y)
  {
    for (std::int32_t z = 0; z < EDGE; ++z)
    {
      for (std::int32_t x = 0; x < EDGE; ++x)
      {
        chunk->Set(x, y, z, EARTH);
      }
    }
  }
  return chunk;
}

/// _edited's edits against _base, which a test expects there to be.
std::vector<std::byte> Encoded(const Protocol::Chunk& _base, const Protocol::Chunk& _edited)
{
  std::optional<std::vector<std::byte>> bytes = Simulation::EncodeChunkEdits(_base, _edited);
  if (!bytes.has_value())
  {
    throw std::logic_error("the chunk was expected to differ from its base");
  }
  return std::move(*bytes);
}

/// _edited rebuilt from _base through its encoding.
std::unique_ptr<Protocol::Chunk> RoundTrip(const Protocol::Chunk& _base, const Protocol::Chunk& _edited, std::uint8_t& _form)
{
  const std::vector<std::byte> bytes = Encoded(_base, _edited);
  _form = static_cast<std::uint8_t>(bytes.front());
  auto chunk = std::make_unique<Protocol::Chunk>(_base);
  Core::ByteReader reader(bytes);
  Simulation::ApplyChunkEdits(reader, *chunk);
  Assert::IsTrue(reader.AtEnd());
  return chunk;
}

bool Same(const Protocol::Chunk& _a, const Protocol::Chunk& _b)
{
  return std::ranges::equal(_a.Voxels(), _b.Voxels());
}

} // namespace

TEST_CLASS(ChunkEditsTests)
{
public:
  TEST_METHOD(WritesAFewChangesAsAList)
  {
    const auto base = Ground();
    auto edited = std::make_unique<Protocol::Chunk>(*base);
    edited->Set(3, 9, 4, 0);
    edited->Set(3, 10, 4, MASONRY);
    edited->Set(31, 31, 31, MASONRY);
    const std::vector<std::byte> bytes = Encoded(*base, *edited);
    Assert::AreEqual(Simulation::CHUNK_EDITS_LIST, static_cast<std::uint8_t>(bytes.front()));
    Assert::AreEqual(std::size_t{1 + 2 + 3 * 3}, bytes.size());
  }

  TEST_METHOD(WritesAChunkReplacedThroughoutAsRuns)
  {
    const auto base = Ground();
    Protocol::Chunk edited;
    for (std::int32_t y = 0; y < EDGE; ++y)
    {
      for (std::int32_t z = 0; z < EDGE; ++z)
      {
        for (std::int32_t x = 0; x < EDGE; ++x)
        {
          edited.Set(x, y, z, MASONRY);
        }
      }
    }
    const std::vector<std::byte> bytes = Encoded(*base, edited);
    Assert::AreEqual(Simulation::CHUNK_EDITS_RUNS, static_cast<std::uint8_t>(bytes.front()));
    Assert::AreEqual(std::size_t{1 + 3}, bytes.size()); // one run of 32,768
  }

  TEST_METHOD(WritesNothingForAChunkBackToItsBase)
  {
    const auto base = Ground();
    auto edited = std::make_unique<Protocol::Chunk>(*base);
    edited->Set(5, 5, 5, 0);
    edited->Set(5, 5, 5, EARTH);
    Assert::IsFalse(Simulation::EncodeChunkEdits(*base, *edited).has_value());
  }

  TEST_METHOD(RoundTripsBothForms)
  {
    const auto base = Ground();
    std::uint8_t form = 0;
    auto few = std::make_unique<Protocol::Chunk>(*base);
    few->Set(0, 0, 0, 0);
    few->Set(17, 12, 30, MASONRY);
    Assert::IsTrue(Same(*few, *RoundTrip(*base, *few, form)));
    Assert::AreEqual(Simulation::CHUNK_EDITS_LIST, form);

    auto many = std::make_unique<Protocol::Chunk>(*base);
    for (std::int32_t z = 0; z < EDGE; ++z)
    {
      for (std::int32_t x = 0; x < EDGE; ++x)
      {
        many->Set(x, 9, z, (x + z) % 3 == 0 ? MASONRY : std::uint8_t{0});
        many->Set(x, 20, z, MASONRY);
      }
    }
    Assert::IsTrue(Same(*many, *RoundTrip(*base, *many, form)));
    Assert::AreEqual(Simulation::CHUNK_EDITS_RUNS, form);
  }

  TEST_METHOD(TheTwoFormsNeverTie)
  {
    // A list is 2 + 3n bytes after its form and a run-length copy 3r: never a multiple of 3 against
    // always one, so ADR-014's tie rule never applies.
    for (std::size_t changed = 1; changed <= Protocol::Chunk::VOXEL_COUNT; ++changed)
    {
      Assert::AreNotEqual(std::size_t{0}, (2 + 3 * changed) % 3);
    }
  }

  TEST_METHOD(RefusesBrokenEdits)
  {
    Protocol::Chunk chunk;
    const std::array<std::byte, 3> unknownForm = {std::byte{7}, std::byte{0}, std::byte{0}};
    Core::ByteReader unknown(unknownForm);
    Assert::ExpectException<std::invalid_argument>([&] { Simulation::ApplyChunkEdits(unknown, chunk); });
    const std::array<std::byte, 4> shortRuns = {std::byte{1}, std::byte{4}, std::byte{0}, std::byte{6}};
    Core::ByteReader runs(shortRuns);
    Assert::ExpectException<std::out_of_range>([&] { Simulation::ApplyChunkEdits(runs, chunk); });
    const std::array<std::byte, 6> outside = {std::byte{0}, std::byte{1}, std::byte{0}, std::byte{0xff}, std::byte{0xff}, std::byte{6}};
    Core::ByteReader list(outside);
    Assert::ExpectException<std::invalid_argument>([&] { Simulation::ApplyChunkEdits(list, chunk); });
  }
};

} // namespace SimulationTests
