#include "pch.h"

#include "Hash.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace CoreTests
{

// FNV-1a 64 is pinned: content hashes, saves and replays depend on it (ADR-008). These are the
// published test vectors, so a change to the function fails here before it changes any hash.
TEST_CLASS(HashTests)
{
public:
  TEST_METHOD(MatchesThePublishedVectors)
  {
    Assert::AreEqual(std::uint64_t{0xcbf29ce484222325ULL}, Core::Fnv1a64(std::string_view{}));
    Assert::AreEqual(std::uint64_t{0xaf63dc4c8601ec8cULL}, Core::Fnv1a64(std::string_view{"a"}));
    Assert::AreEqual(std::uint64_t{0x85944171f73967e8ULL}, Core::Fnv1a64(std::string_view{"foobar"}));
  }

  TEST_METHOD(ContinuesAcrossBuffers)
  {
    const std::uint64_t first = Core::Fnv1a64(std::string_view{"foo"});
    Assert::AreEqual(Core::Fnv1a64(std::string_view{"foobar"}), Core::Fnv1a64(std::string_view{"bar"}, first));
  }
};

} // namespace CoreTests
