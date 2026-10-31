#include "pch.h"

#include "VoxelRecord.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace SampleTests
{

TEST_CLASS(SuiteSmoke)
{
public:
  TEST_METHOD(PacksAPosition)
  {
    Assert::AreEqual(0x030201u, Sample::PackPosition(1, 2, 3));
  }
};

} // namespace SampleTests
