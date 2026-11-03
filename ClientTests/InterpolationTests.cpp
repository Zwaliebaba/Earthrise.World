#include "pch.h"

#include "Interpolation.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Where an entity is drawn between ticks (M4-H1; MvpPlan.md, M4, input 20): between its last two positions at the clock's
// fraction, a tick behind, snapping past 1.5 m for a unit, and never for a shot.

namespace ClientTests
{

namespace
{

constexpr float CLOSE_METERS = 0.0001F;
constexpr std::int32_t FOOT_RAW_A_TICK = 4587; // foot's 1.4 m/s at 20 ticks a second, floored (ADR-015)

Protocol::FixedPosition At(std::int32_t _xRaw, std::int32_t _yRaw, std::int32_t _zRaw)
{
  return {.xMeters = Core::Fixed::FromRaw(_xRaw), .yMeters = Core::Fixed::FromRaw(_yRaw), .zMeters = Core::Fixed::FromRaw(_zRaw)};
}

Protocol::FixedPosition AtMeters(std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return At(_x * Core::Fixed::ONE_RAW, _y * Core::Fixed::ONE_RAW, _z * Core::Fixed::ONE_RAW);
}

void AssertAt(float _x, float _y, float _z, const DirectX::XMFLOAT3& _drawn)
{
  Assert::AreEqual(_x, _drawn.x, CLOSE_METERS);
  Assert::AreEqual(_y, _drawn.y, CLOSE_METERS);
  Assert::AreEqual(_z, _drawn.z, CLOSE_METERS);
}

} // namespace

TEST_CLASS(InterpolationTests)
{
public:
  TEST_METHOD(HalfwayBetweenTicksAFigureIsDrawnHalfway)
  {
    const Protocol::FixedPosition before = AtMeters(10, 9, 20);
    const Protocol::FixedPosition after =
      At((10 * Core::Fixed::ONE_RAW) + FOOT_RAW_A_TICK, 9 * Core::Fixed::ONE_RAW, 20 * Core::Fixed::ONE_RAW);
    const float step = static_cast<float>(FOOT_RAW_A_TICK) / static_cast<float>(Core::Fixed::ONE_RAW);
    AssertAt(10.0F + (0.5F * step), 9.0F, 20.0F, Client::DrawnPosition(&before, after, 0.5, Client::UNIT_SNAP_METERS));
    // A tick behind: at the tick it is where the previous update had it, and it reaches the last as the tick ends.
    AssertAt(10.0F, 9.0F, 20.0F, Client::DrawnPosition(&before, after, 0.0, Client::UNIT_SNAP_METERS));
    AssertAt(10.0F + step, 9.0F, 20.0F, Client::DrawnPosition(&before, after, 1.0, Client::UNIT_SNAP_METERS));
  }

  TEST_METHOD(AFigureThatDroppedFourMetersIsDrawnAtItsNewPlaceAtOnce)
  {
    const Protocol::FixedPosition before = AtMeters(10, 13, 20);
    const Protocol::FixedPosition after = AtMeters(10, 9, 20);
    AssertAt(10.0F, 9.0F, 20.0F, Client::DrawnPosition(&before, after, 0.25, Client::UNIT_SNAP_METERS));
  }

  TEST_METHOD(AnEntityNewInTheLastUpdateIsDrawnThere)
  {
    AssertAt(10.0F, 9.0F, 20.0F, Client::DrawnPosition(nullptr, AtMeters(10, 9, 20), 0.25, Client::UNIT_SNAP_METERS));
  }

  TEST_METHOD(AShotNeverSnaps)
  {
    // A stone 12 m along in a tick is drawn on its line between ticks.
    const Protocol::FixedPosition before = AtMeters(0, 10, 0);
    const Protocol::FixedPosition after = AtMeters(12, 10, 0);
    AssertAt(6.0F, 10.0F, 0.0F, Client::DrawnPosition(&before, after, 0.5, Client::NEVER_SNAP_METERS));
  }

  TEST_METHOD(TheFractionIsClampedToTheTick)
  {
    const Protocol::FixedPosition before = AtMeters(10, 9, 20);
    const Protocol::FixedPosition after = AtMeters(11, 9, 20);
    AssertAt(11.0F, 9.0F, 20.0F, Client::DrawnPosition(&before, after, 1.5, Client::UNIT_SNAP_METERS));
    AssertAt(10.0F, 9.0F, 20.0F, Client::DrawnPosition(&before, after, -0.5, Client::UNIT_SNAP_METERS));
  }
};

} // namespace ClientTests
