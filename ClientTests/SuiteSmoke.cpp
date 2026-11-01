#include "pch.h"

#include "Catalog.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The suite's placeholder (AGENTS.md §3): vstest reports an empty suite as a pass, so ClientTests starts with one test,
// deleted when the first real test lands. It checks the suite links against the client's content.

namespace ClientTests
{

TEST_CLASS(SuiteSmoke)
{
public:
  TEST_METHOD(FindsTheModelTheClientDrawsTheSquadWith)
  {
    Assert::IsNotNull(Content::FindModel("U5"));
  }
};

} // namespace ClientTests
