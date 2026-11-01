#include "pch.h"

#include "Catalog.h"
#include "Palette.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ContentTests
{

namespace
{

// What GameData/Manifest.json lists: nine templates, and eight models, one figure for each unit type and two engines.
constexpr std::array<std::string_view, 9> TEMPLATE_IDS = {"T1", "T2", "T3", "T4", "T5", "T6", "T7", "T8", "T9"};
constexpr std::array<std::string_view, 10> MODEL_IDS = {"P1", "P2", "S1", "S2", "S3", "U1", "U2", "U3", "U4", "U5"};

constexpr std::uint32_t WORLD_VOXEL_EDGE_MILLIMETERS = 1000; // ADR-006
constexpr std::uint32_t MODEL_VOXEL_EDGE_MILLIMETERS = 500;

std::size_t SolidCount(const Content::VoxelGrid& _grid)
{
  return static_cast<std::size_t>(
    std::ranges::count_if(_grid.indices, [](std::uint8_t _index) { return _index != Content::VoxelGrid::EMPTY; }));
}

/// Whether every index the grid holds is one the palette defines.
bool UsesOnlyThePalette(const Content::VoxelGrid& _grid)
{
  return std::ranges::all_of(_grid.indices,
                             [](std::uint8_t _index) { return _index == Content::VoxelGrid::EMPTY || Content::PaletteHasEntry(_index); });
}

/// Whether the occupied voxels span the whole grid, from the origin to its far corner, which is what trimming the
/// empty padding leaves.
bool IsTrimmed(const Content::VoxelGrid& _grid)
{
  std::array<std::uint32_t, 3> lowVoxels = {_grid.widthVoxels, _grid.heightVoxels, _grid.depthVoxels};
  std::array<std::uint32_t, 3> highVoxels = {0, 0, 0};
  for (std::uint32_t y = 0; y < _grid.heightVoxels; ++y)
  {
    for (std::uint32_t z = 0; z < _grid.depthVoxels; ++z)
    {
      for (std::uint32_t x = 0; x < _grid.widthVoxels; ++x)
      {
        if (_grid.At(x, y, z) == Content::VoxelGrid::EMPTY)
        {
          continue;
        }
        const std::array<std::uint32_t, 3> position = {x, y, z};
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
          lowVoxels[axis] = std::min(lowVoxels[axis], position[axis]);
          highVoxels[axis] = std::max(highVoxels[axis], position[axis] + 1);
        }
      }
    }
  }
  return lowVoxels == std::array<std::uint32_t, 3>{0, 0, 0} &&
         highVoxels == std::array<std::uint32_t, 3>{_grid.widthVoxels, _grid.heightVoxels, _grid.depthVoxels};
}

/// Whether the grid's dimensions account for every index it holds.
bool IsWhole(const Content::VoxelGrid& _grid)
{
  const std::size_t voxels = static_cast<std::size_t>(_grid.widthVoxels) * _grid.heightVoxels * _grid.depthVoxels;
  return voxels > 0 && _grid.indices.size() == voxels;
}

} // namespace

TEST_CLASS(CatalogTests)
{
public:
  TEST_METHOD(HoldsEveryTemplateAndModelInIdOrder)
  {
    const std::span<const Content::TemplateView> templates = Content::Templates();
    Assert::AreEqual(TEMPLATE_IDS.size(), templates.size());
    for (std::size_t i = 0; i < TEMPLATE_IDS.size(); ++i)
    {
      Assert::IsTrue(templates[i].id == TEMPLATE_IDS[i]);
      Assert::IsFalse(templates[i].name.empty());
      Assert::AreEqual(WORLD_VOXEL_EDGE_MILLIMETERS, templates[i].grid.voxelEdgeMillimeters);
    }
    const std::span<const Content::ModelView> models = Content::Models();
    Assert::AreEqual(MODEL_IDS.size(), models.size());
    for (std::size_t i = 0; i < MODEL_IDS.size(); ++i)
    {
      Assert::IsTrue(models[i].id == MODEL_IDS[i]);
      Assert::IsFalse(models[i].name.empty());
      Assert::AreEqual(MODEL_VOXEL_EDGE_MILLIMETERS, models[i].grid.voxelEdgeMillimeters);
    }
  }

  // ADR-006, decision 3: the standard wall section is 8 x 2 x 6 m on the 1 m grid and costs exactly its volume, 96 m3.
  // Its crenellations step up over the top two layers, so the trimmed box is 7 voxels tall and 96 of its 112 are solid.
  TEST_METHOD(TrimsT3AndCountsItsStone)
  {
    const Content::TemplateView* wall = Content::FindTemplate("T3");
    Assert::IsNotNull(wall);
    Assert::AreEqual(std::uint32_t{8}, wall->grid.widthVoxels);
    Assert::AreEqual(std::uint32_t{7}, wall->grid.heightVoxels);
    Assert::AreEqual(std::uint32_t{2}, wall->grid.depthVoxels);
    Assert::AreEqual(std::size_t{96}, SolidCount(wall->grid));
    Assert::AreEqual(std::uint32_t{96}, wall->stoneM3);
    Assert::AreEqual(std::uint32_t{0}, wall->timberM3);
  }

  TEST_METHOD(EveryIndexIsInThePalette)
  {
    for (const Content::TemplateView& view : Content::Templates())
    {
      Assert::IsTrue(IsWhole(view.grid));
      Assert::IsTrue(UsesOnlyThePalette(view.grid));
    }
    for (const Content::ModelView& view : Content::Models())
    {
      Assert::IsTrue(IsWhole(view.grid));
      Assert::IsTrue(UsesOnlyThePalette(view.grid));
    }
  }

  // ADR-005, decision 2: the bake trims the empty padding, so the occupied voxels' minimum corner is the origin. T7
  // carries a metre of padding in front and the trebuchet's grid is larger than its voxels, so this is not vacuous.
  TEST_METHOD(TrimsEveryGridToItsVoxels)
  {
    for (const Content::TemplateView& view : Content::Templates())
    {
      Assert::IsTrue(IsTrimmed(view.grid));
    }
    for (const Content::ModelView& view : Content::Models())
    {
      Assert::IsTrue(IsTrimmed(view.grid));
    }
    Assert::AreEqual(std::uint32_t{1}, Content::FindTemplate("T7")->grid.depthVoxels);
  }

  TEST_METHOD(FindsByIdAndOnlyByIt)
  {
    const Content::TemplateView* keep = Content::FindTemplate("T2");
    Assert::IsNotNull(keep);
    Assert::IsTrue(keep->id == "T2");
    Assert::IsTrue(keep->name == "Keep");
    Assert::IsTrue(Content::FindTemplate("T10") == nullptr);
    Assert::IsTrue(Content::FindTemplate("U2") == nullptr);

    const Content::ModelView* spearman = Content::FindModel("U2");
    Assert::IsNotNull(spearman);
    Assert::IsTrue(spearman->name == "Spearman");
    Assert::IsTrue(Content::FindModel("T2") == nullptr);
  }

  TEST_METHOD(RefusesAPositionOutsideTheGrid)
  {
    const Content::VoxelGrid& grid = Content::FindTemplate("T3")->grid;
    Assert::ExpectException<std::out_of_range>([&grid] { (void)grid.At(8, 0, 0); });
    Assert::ExpectException<std::out_of_range>([&grid] { (void)grid.At(0, 0, 2); });
  }

  TEST_METHOD(MirrorsALayerFileInX)
  {
    // The trebuchet's layer file draws its counterweight (palette index 21) at the high-x end and its sling's cloth
    // (23) at the low-x end. The bake mirrors x, so that the shape looks as drawn in the world's frame (ADR-013).
    constexpr std::uint8_t COUNTERWEIGHT = 21;
    constexpr std::uint8_t CLOTH = 23;
    const Content::VoxelGrid& grid = Content::FindModel("S3")->grid;
    std::size_t counterweights = 0;
    std::size_t cloths = 0;
    for (std::uint32_t y = 0; y < grid.heightVoxels; ++y)
    {
      for (std::uint32_t z = 0; z < grid.depthVoxels; ++z)
      {
        for (std::uint32_t x = 0; x < grid.widthVoxels; ++x)
        {
          const std::uint8_t index = grid.At(x, y, z);
          const bool lowHalf = 2 * x < grid.widthVoxels;
          if (index == COUNTERWEIGHT)
          {
            Assert::IsTrue(lowHalf);
            ++counterweights;
          }
          else if (index == CLOTH)
          {
            Assert::IsFalse(lowHalf);
            ++cloths;
          }
        }
      }
    }
    Assert::IsTrue(counterweights > 0 && cloths > 0);
  }
};

} // namespace ContentTests
