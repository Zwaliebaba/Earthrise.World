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

/// The record of the cell at (_x, _y, _z).
const Engine::VoxelRecord& RecordAt(const std::vector<Engine::VoxelRecord>& _records, std::uint32_t _x, std::uint32_t _y, std::uint32_t _z)
{
  const auto found = std::ranges::find_if(_records, [&](const Engine::VoxelRecord& _record)
                                          { return CellOf(_record, 0) == _x && CellOf(_record, 6) == _y && CellOf(_record, 12) == _z; });
  if (found == _records.end())
  {
    throw std::runtime_error("no record for that cell");
  }
  return *found;
}

/// A face's four corner levels, corner 0 to 3.
std::array<std::uint8_t, 4> Levels(const Engine::VoxelRecord& _record, Engine::VoxelFace _face)
{
  return {Engine::CornerLevel(_record, _face, 0), Engine::CornerLevel(_record, _face, 1), Engine::CornerLevel(_record, _face, 2),
          Engine::CornerLevel(_record, _face, 3)};
}

constexpr std::array<std::uint8_t, 4> OPEN = {3, 3, 3, 3};

/// Every index jittered by _jitter and read through table 1.
Engine::PaletteShading Shading(float _jitter)
{
  Engine::PaletteShading shading{};
  shading.jitter.fill(_jitter);
  shading.shadeTable.fill(1);
  return shading;
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

  TEST_METHOD(ShadesACornerByItsThreeNeighbors)
  {
    // A voxel at (1, 0, 1) with walls above it to the west, at (0, 1, 1), and to the south, at
    // (1, 1, 0). On its top face, x is u and z is w. The south-west corner has both sides solid,
    // so it is fully occluded whatever its diagonal; the corners with one side solid lose a level;
    // the north-east corner sees nothing.
    std::vector<std::uint8_t> cells(std::size_t{3} * 2 * 3, 0);
    const auto at = [&cells](std::size_t _x, std::size_t _y, std::size_t _z) -> std::uint8_t& { return cells[(_y * 3 + _z) * 3 + _x]; };
    at(1, 0, 1) = 6;
    at(0, 1, 1) = 6;
    at(1, 1, 0) = 6;
    const std::vector<Engine::VoxelRecord> records =
      Engine::BuildVisibleRecords({.paletteIndices = cells, .xVoxels = 3, .yVoxels = 2, .zVoxels = 3}, NothingOutside);
    const Engine::VoxelRecord& voxel = RecordAt(records, 1, 0, 1);
    Assert::IsTrue(Levels(voxel, Engine::VoxelFace::PositiveY) == std::array<std::uint8_t, 4>{0, 2, 2, 3});
    // Its south face looks at (1, 0, 0), with x as u and y as w: the south wall stands above that,
    // so both upper corners lose a level, and its east face sees nothing.
    Assert::IsTrue(Levels(voxel, Engine::VoxelFace::NegativeZ) == std::array<std::uint8_t, 4>{3, 3, 2, 2});
    Assert::IsTrue(Levels(voxel, Engine::VoxelFace::PositiveX) == OPEN);
    // Its west face looks into the wall's foot at (0, 0, 1), which is empty, with the wall above it.
    Assert::IsTrue(Levels(voxel, Engine::VoxelFace::NegativeX) == std::array<std::uint8_t, 4>{3, 2, 3, 2});
  }

  TEST_METHOD(LeavesAHiddenFaceAndALoneVoxelOpen)
  {
    // Two voxels side by side along x: the faces they share are hidden, and with nothing else
    // about, every corner is open.
    const std::vector<std::uint8_t> cells = {7, 7};
    const std::vector<Engine::VoxelRecord> records =
      Engine::BuildVisibleRecords({.paletteIndices = cells, .xVoxels = 2, .yVoxels = 1, .zVoxels = 1}, NothingOutside);
    const Engine::VoxelRecord& west = RecordAt(records, 0, 0, 0);
    Assert::IsTrue(Levels(west, Engine::VoxelFace::PositiveX) == OPEN, L"hidden by its neighbour");
    Assert::IsTrue(Levels(west, Engine::VoxelFace::NegativeZ) == OPEN);
    // Ground everywhere below, as a chunk sees the world's floor: below a side face's lower corners
    // are the cell under the one in front and its diagonal, so they lose two levels.
    const std::vector<Engine::VoxelRecord> grounded =
      Engine::BuildVisibleRecords({.paletteIndices = cells, .xVoxels = 2, .yVoxels = 1, .zVoxels = 1},
                                  [](std::int32_t, std::int32_t _y, std::int32_t) { return _y < 0; });
    Assert::IsTrue(Levels(RecordAt(grounded, 0, 0, 0), Engine::VoxelFace::NegativeZ) == std::array<std::uint8_t, 4>{1, 1, 3, 3});
  }

  TEST_METHOD(CountsWhatElseIsSolidAtTheCorners)
  {
    // A lone model voxel standing on the world: the world's ground below it shades the lower
    // corners of its sides, as the cell under the one in front and its diagonal (RendererLook.md
    // §5), though the model has no voxel there; its top sees nothing; and the faces it shows are
    // its own, so the bottom, against the ground, still counts as exposed.
    const std::vector<std::uint8_t> cell = {19};
    const Engine::SolidOutside ground = [](std::int32_t, std::int32_t _y, std::int32_t) { return _y < 0; };
    const std::vector<Engine::VoxelRecord> records =
      Engine::BuildVisibleRecords({.paletteIndices = cell, .xVoxels = 1, .yVoxels = 1, .zVoxels = 1}, NothingOutside,
                                  {.palette = nullptr, .originCells = {0, 0, 0}, .jitterCellShift = 0, .alsoSolid = &ground});
    Assert::AreEqual(std::size_t{1}, records.size());
    Assert::IsTrue(Levels(records[0], Engine::VoxelFace::NegativeZ) == std::array<std::uint8_t, 4>{1, 1, 3, 3});
    Assert::IsTrue(Levels(records[0], Engine::VoxelFace::PositiveX) == std::array<std::uint8_t, 4>{1, 3, 1, 3});
    Assert::IsTrue(Levels(records[0], Engine::VoxelFace::PositiveY) == OPEN);
    // Without it, the same voxel is open all round.
    const std::vector<Engine::VoxelRecord> alone =
      Engine::BuildVisibleRecords({.paletteIndices = cell, .xVoxels = 1, .yVoxels = 1, .zVoxels = 1}, NothingOutside);
    Assert::IsTrue(Levels(alone[0], Engine::VoxelFace::NegativeZ) == OPEN);
  }

  TEST_METHOD(JittersByTheHashOfTheWorldCell)
  {
    // RendererLook.md §4's hash, against values worked out independently from the prototype's
    // JavaScript: the cell (3, 5, 7) in masonry hashes to 0.23749, so a jitter of 0.07 gives an
    // offset of -0.0183755, or -19/1024 in the record's steps.
    Assert::AreEqual(0.2374932934F, Engine::JitterHash(3, 5, 7, 6), 1.0e-7F);
    const Engine::PaletteShading shading = Shading(0.07F);
    const std::vector<std::uint8_t> cell = {6};
    const std::vector<Engine::VoxelRecord> records =
      Engine::BuildVisibleRecords({.paletteIndices = cell, .xVoxels = 1, .yVoxels = 1, .zVoxels = 1}, NothingOutside,
                                  {.palette = &shading, .originCells = {3, 5, 7}, .jitterCellShift = 0, .alsoSolid = nullptr});
    Assert::AreEqual(-19.0F / 1024.0F, Engine::LightnessOffset(records.at(0)));
    Assert::AreEqual(1U, (records.at(0).cellAndPalette >> 26) & 1U, L"the palette's shade table");
  }

  TEST_METHOD(FloorsCellsIntoJitterCells)
  {
    // At 0.5 m the jitter cell is the 1 m voxel: cell -1 halves to -1, not to 0. The hash of (-1, 0,
    // 0) in earth with a jitter of 0.04 gives -10/1024; truncating to (0, 0, 0) would give -3/1024.
    const Engine::PaletteShading shading = Shading(0.04F);
    const std::vector<std::uint8_t> cell = {2};
    const std::vector<Engine::VoxelRecord> records =
      Engine::BuildVisibleRecords({.paletteIndices = cell, .xVoxels = 1, .yVoxels = 1, .zVoxels = 1}, NothingOutside,
                                  {.palette = &shading, .originCells = {-1, 1, 1}, .jitterCellShift = 1, .alsoSolid = nullptr});
    Assert::AreEqual(-10.0F / 1024.0F, Engine::LightnessOffset(records.at(0)));
  }

  TEST_METHOD(PacksAndClampsTheShade)
  {
    Engine::VoxelShade shade = Engine::UNSHADED;
    shade.cornerLevels[4] = {0, 1, 2, 3};
    shade.cornerLevels[0] = {3, 2, 1, 0};
    shade.lightnessOffset = 0.5F;
    const Engine::VoxelRecord record = Engine::MakeVoxelRecord(63, 0, 63, 255, shade);
    Assert::IsTrue(Levels(record, Engine::VoxelFace::PositiveZ) == std::array<std::uint8_t, 4>{0, 1, 2, 3});
    Assert::IsTrue(Levels(record, Engine::VoxelFace::NegativeX) == std::array<std::uint8_t, 4>{3, 2, 1, 0});
    Assert::IsTrue(Levels(record, Engine::VoxelFace::PositiveY) == OPEN);
    Assert::AreEqual(Engine::MAX_LIGHTNESS_OFFSET, Engine::LightnessOffset(record), L"clamped");
    Assert::AreEqual(255U, (record.cellAndPalette >> 18) & 255U, L"the palette index is untouched by the shade table's bit");
    shade.lightnessOffset = -0.5F;
    Assert::AreEqual(-Engine::MAX_LIGHTNESS_OFFSET, Engine::LightnessOffset(Engine::MakeVoxelRecord(0, 0, 0, 1, shade)));
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
