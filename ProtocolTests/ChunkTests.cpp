#include "pch.h"

#include "Chunk.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ProtocolTests
{

TEST_CLASS(ChunkTests)
{
public:
  TEST_METHOD(StartsEmpty)
  {
    const Protocol::Chunk chunk;
    Assert::AreEqual(std::size_t{0}, chunk.SolidCount());
    Assert::AreEqual(Protocol::Chunk::EMPTY, chunk.At(31, 31, 31));
  }

  TEST_METHOD(KeepsWhatIsSetAndCountsIt)
  {
    Protocol::Chunk chunk;
    chunk.Set(1, 2, 3, 6);
    chunk.Set(1, 2, 3, 6);
    chunk.Set(31, 0, 0, 7);
    Assert::AreEqual(std::uint8_t{6}, chunk.At(1, 2, 3));
    Assert::AreEqual(std::uint8_t{7}, chunk.At(31, 0, 0));
    Assert::AreEqual(std::size_t{2}, chunk.SolidCount());
    chunk.Set(1, 2, 3, Protocol::Chunk::EMPTY);
    Assert::AreEqual(std::size_t{1}, chunk.SolidCount());
  }

  TEST_METHOD(RefusesVoxelsOutsideIt)
  {
    Protocol::Chunk chunk;
    Assert::ExpectException<std::out_of_range>([&chunk] { chunk.Set(32, 0, 0, 1); });
    Assert::ExpectException<std::out_of_range>([&chunk] { (void)chunk.At(0, -1, 0); });
  }
};

} // namespace ProtocolTests
