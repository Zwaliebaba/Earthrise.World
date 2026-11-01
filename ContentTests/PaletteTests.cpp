#include "pch.h"

#include "Palette.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The look the bake carries for each palette index (RendererLook.md, sections 3 to 5), against
// GameData/Server/Palette.json's values as RendererLook.md's table states them.

namespace ContentTests
{

namespace
{

constexpr std::uint8_t GRASS = 1;
constexpr std::uint8_t MASONRY = 6;
constexpr std::uint8_t LEAVES_A = 10;
constexpr std::uint8_t ROAD = 15;
constexpr std::uint8_t RUBBLE_OF_STONE = 8;
constexpr std::uint8_t UNUSED = 200;

} // namespace

TEST_CLASS(PaletteTests)
{
public:
  TEST_METHOD(CarriesEachMaterialsJitter)
  {
    Assert::AreEqual(0.05F, Content::PaletteJitter(GRASS));
    Assert::AreEqual(0.07F, Content::PaletteJitter(MASONRY));
    Assert::AreEqual(0.10F, Content::PaletteJitter(LEAVES_A));
    Assert::AreEqual(0.04F, Content::PaletteJitter(ROAD));
    Assert::AreEqual(0.0F, Content::PaletteJitter(UNUSED));
  }

  TEST_METHOD(GivesTerrainItsReducedShades)
  {
    Assert::IsTrue(Content::PaletteCornerShadeSet(GRASS) == Content::CornerShadeSet::Terrain);
    Assert::IsTrue(Content::PaletteCornerShadeSet(ROAD) == Content::CornerShadeSet::Terrain);
    Assert::IsTrue(Content::PaletteCornerShadeSet(MASONRY) == Content::CornerShadeSet::Structure);
    Assert::IsTrue(Content::PaletteCornerShadeSet(RUBBLE_OF_STONE) == Content::CornerShadeSet::Structure);
    const std::array<float, Content::CORNER_SHADE_LEVELS> structure = Content::CornerShades(Content::CornerShadeSet::Structure);
    const std::array<float, Content::CORNER_SHADE_LEVELS> terrain = Content::CornerShades(Content::CornerShadeSet::Terrain);
    Assert::IsTrue(structure == std::array{0.52F, 0.68F, 0.84F, 1.0F});
    Assert::IsTrue(terrain == std::array{0.74F, 0.84F, 0.93F, 1.0F});
  }
};

} // namespace ContentTests
