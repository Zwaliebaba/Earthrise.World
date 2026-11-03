#include "pch.h"

#include "DeckCoverage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Where a breach is on screen against layout A's top bar and deck (M4-J2; MvpPlan.md, M4, input 25), with no GPU.

namespace ClientTests
{

namespace
{

constexpr std::uint32_t WIDTH = 1000;
constexpr std::uint32_t HEIGHT = 1000;
constexpr float TOP_BAR_PIXELS = 50.0F;
constexpr float DECK_PIXELS = 200.0F; // its top at y 800

/// Seen from straight above, 100 m square onto the 1000 pixels: pixel x is 10 x, and pixel y is 1000 - 10 z.
DirectX::XMFLOAT4X4 TopDown()
{
  DirectX::XMFLOAT4X4 matrix{};
  matrix.m[0][0] = 1.0F / 50.0F;
  matrix.m[2][1] = 1.0F / 50.0F;
  matrix.m[3][0] = -1.0F;
  matrix.m[3][1] = -1.0F;
  matrix.m[3][3] = 1.0F;
  return matrix;
}

Client::Coverage At(float _minZ, float _maxZ, float _minX = 40.0F, float _maxX = 44.0F)
{
  return Client::CoverageOf(TopDown(), WIDTH, HEIGHT, TOP_BAR_PIXELS, DECK_PIXELS, {_minX, 9.0F, _minZ}, {_maxX, 19.0F, _maxZ});
}

} // namespace

TEST_CLASS(DeckCoverageTests)
{
public:
  TEST_METHOD(ACenterBelowTheDecksTopReadsDeckAndAboveTheTopBarsFootTopbar)
  {
    Assert::IsTrue(At(50.0F, 54.0F).place == Client::ScreenPlace::Uncovered); // y 460 to 500
    Assert::AreEqual(0, At(50.0F, 54.0F).deckSharePercent);
    Assert::IsTrue(At(8.0F, 12.0F).place == Client::ScreenPlace::Deck); // centre at y 900
    Assert::AreEqual(100, At(8.0F, 12.0F).deckSharePercent);
    Assert::IsTrue(At(96.0F, 98.0F).place == Client::ScreenPlace::TopBar); // centre at y 30
    Assert::AreEqual(std::string("topbar"), std::string(Client::PlaceName(Client::ScreenPlace::TopBar)));
  }

  TEST_METHOD(OneOutsideTheScreenReadsOff)
  {
    Assert::IsTrue(At(50.0F, 54.0F, 120.0F, 124.0F).place == Client::ScreenPlace::Off);
    Assert::IsTrue(At(-30.0F, -26.0F).place == Client::ScreenPlace::Off);
    Assert::AreEqual(std::string("off"), std::string(Client::PlaceName(Client::ScreenPlace::Off)));
  }

  TEST_METHOD(TheDecksShareOfAFootprintHalfUnderItIsFiftyPercent)
  {
    // z 16 to 24 projects to y 760 to 840, about the deck's top at 800.
    const Client::Coverage half = At(16.0F, 24.0F);
    Assert::AreEqual(50, half.deckSharePercent);
    Assert::IsTrue(half.place == Client::ScreenPlace::Uncovered); // its centre, y 800, on the line, is not below it
  }
};

} // namespace ClientTests
