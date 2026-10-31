#include "pch.h"

#include "VoxelGridView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace EngineTests
{

namespace
{

bool NothingOutside(std::int32_t, std::int32_t, std::int32_t)
{
  return false;
}

bool EverythingOutside(std::int32_t, std::int32_t, std::int32_t)
{
  return true;
}

std::uint32_t CellOf(const Engine::VoxelRecord& _record, std::uint32_t _shift)
{
  return (_record.cellAndPalette >> _shift) & Engine::VOXEL_RECORD_MAX_CELL;
}

} // namespace

TEST_CLASS(VoxelGridViewTests)
{
public:
  TEST_METHOD(LeavesOutBuriedCells)
  {
    // A solid 3 x 3 x 3 block: only its centre is buried.
    const std::vector<std::uint8_t> cells(27, 4);
    const std::vector<Engine::VoxelRecord> records =
      Engine::BuildVisibleRecords({.paletteIndices = cells, .xVoxels = 3, .yVoxels = 3, .zVoxels = 3}, NothingOutside);
    Assert::AreEqual(std::size_t{26}, records.size());
    for (const Engine::VoxelRecord& record : records)
    {
      Assert::IsFalse(CellOf(record, 0) == 1 && CellOf(record, 6) == 1 && CellOf(record, 12) == 1, L"the centre is buried");
    }
  }

  TEST_METHOD(AsksTheNeighborsAtTheEdge)
  {
    // One layer of ground whose neighbours are all solid: only its top faces are exposed.
    const std::vector<std::uint8_t> ground(std::size_t{2} * 1 * 2, 2);
    const std::vector<std::uint8_t> withAir = {2, 2, 2, 2, 0, 0, 0, 0}; // two layers: ground, then air
    Assert::AreEqual(
      std::size_t{4},
      Engine::BuildVisibleRecords({.paletteIndices = withAir, .xVoxels = 2, .yVoxels = 2, .zVoxels = 2}, EverythingOutside).size());
    Assert::AreEqual(
      std::size_t{0},
      Engine::BuildVisibleRecords({.paletteIndices = ground, .xVoxels = 2, .yVoxels = 1, .zVoxels = 2}, EverythingOutside).size(),
      L"a layer with solid neighbours on every side shows nothing");
  }

  TEST_METHOD(KeepsTheCellAndPaletteIndex)
  {
    std::vector<std::uint8_t> cells(std::size_t{4} * 3 * 2, 0);
    cells[(2 * 2 + 1) * 4 + 3] = 9; // x 3, y 2, z 1
    const std::vector<Engine::VoxelRecord> records =
      Engine::BuildVisibleRecords({.paletteIndices = cells, .xVoxels = 4, .yVoxels = 3, .zVoxels = 2}, NothingOutside);
    Assert::AreEqual(std::size_t{1}, records.size());
    Assert::AreEqual(std::uint32_t{3}, CellOf(records[0], 0));
    Assert::AreEqual(std::uint32_t{2}, CellOf(records[0], 6));
    Assert::AreEqual(std::uint32_t{1}, CellOf(records[0], 12));
    Assert::AreEqual(std::uint32_t{9}, (records[0].cellAndPalette >> 18) & 255);
  }

  TEST_METHOD(RefusesAGridItsSizeDoesNotDescribe)
  {
    const std::vector<std::uint8_t> cells(8, 1);
    Assert::ExpectException<std::invalid_argument>(
      [&cells] { (void)Engine::BuildVisibleRecords({.paletteIndices = cells, .xVoxels = 3, .yVoxels = 2, .zVoxels = 2}, NothingOutside); });
    const std::vector<std::uint8_t> tooLong(65, 1);
    Assert::ExpectException<std::invalid_argument>(
      [&tooLong]
      { (void)Engine::BuildVisibleRecords({.paletteIndices = tooLong, .xVoxels = 65, .yVoxels = 1, .zVoxels = 1}, NothingOutside); });
  }
};

} // namespace EngineTests
