#include "pch.h"

#include "SplitMix64.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace CoreTests
{

// SplitMix64 is pinned (ADR-013, decision 4). The expected outputs were computed on 2026-11-01
// with Python's integers from the reference algorithm, so a change to the mixer fails here before
// it changes a generated world, a save or a replay.
TEST_CLASS(SplitMix64Tests)
{
public:
  TEST_METHOD(MatchesTheReferenceFromSeedZero)
  {
    Core::SplitMix64 generator(0);
    Assert::AreEqual(std::uint64_t{0xe220a8397b1dcdafULL}, generator.Next());
    Assert::AreEqual(std::uint64_t{0x6e789e6aa1b965f4ULL}, generator.Next());
    Assert::AreEqual(std::uint64_t{0x06c45d188009454fULL}, generator.Next());
  }

  TEST_METHOD(MatchesTheReferenceFromAnotherSeed)
  {
    Core::SplitMix64 generator(1234567);
    Assert::AreEqual(std::uint64_t{0x599ed017fb08fc85ULL}, generator.Next());
    Assert::AreEqual(std::uint64_t{0x2c73f08458540fa5ULL}, generator.Next());
    Assert::AreEqual(std::uint64_t{0x883ebce5a3f27c77ULL}, generator.Next());
  }

  TEST_METHOD(MixesAsTheFirstOutput)
  {
    Assert::AreEqual(std::uint64_t{0xe220a8397b1dcdafULL}, Core::SplitMix64Mix(0));
    Core::SplitMix64 generator(42);
    Assert::AreEqual(Core::SplitMix64Mix(42), generator.Next());
  }

  TEST_METHOD(KeysADrawAsItsNestedForm)
  {
    constexpr std::uint64_t SEED = 0x1234;
    constexpr std::uint64_t SALT = 7;
    constexpr std::uint64_t KEY = 0xabcdef;
    Assert::AreEqual(Core::SplitMix64Mix(SEED ^ Core::SplitMix64Mix(SALT ^ Core::SplitMix64Mix(KEY))), Core::KeyedDraw(SEED, SALT, KEY));
    Assert::AreNotEqual(Core::KeyedDraw(SEED, SALT, KEY), Core::KeyedDraw(SEED, SALT + 1, KEY));
  }

  TEST_METHOD(DrawsBelowABound)
  {
    for (const std::uint32_t bound : {1U, 2U, 7U, 1000U, 0xffffffffU})
    {
      Assert::AreEqual(std::uint32_t{0}, Core::DrawBelow(0x123456789abcdefULL, 1));
      Assert::AreEqual(std::uint32_t{0}, Core::DrawBelow(0, bound));
      Assert::AreEqual(bound - 1, Core::DrawBelow(~std::uint64_t{0}, bound));
    }
  }
};

} // namespace CoreTests
