#include "pch.h"

#include "VoxelPick.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The CPU's pick along a ray (M3-E3; ADR-001, *Consequences*), with no GPU.

namespace EngineTests
{

namespace
{

constexpr float TOLERANCE = 1.0e-4F;
constexpr float FAR_METERS = 100.0F;

/// A 4 x 4 x 4 grid of 1 m cells from the origin, solid where _solid says.
Engine::SolidAt Grid(std::function<bool(std::int32_t, std::int32_t, std::int32_t)> _solid)
{
  return [solid = std::move(_solid)](std::int32_t _x, std::int32_t _y, std::int32_t _z)
  {
    const auto inside = [](std::int32_t _value) { return _value >= 0 && _value < 4; };
    return inside(_x) && inside(_y) && inside(_z) && solid(_x, _y, _z);
  };
}

/// The hit, after asserting there is one.
Engine::PickHit HitOf(const std::optional<Engine::PickHit>& _hit)
{
  Assert::IsTrue(_hit.has_value());
  return _hit.value_or(Engine::PickHit{});
}

void AreEqualCells(const std::array<std::int32_t, 3>& _expected, const std::array<std::int32_t, 3>& _actual)
{
  for (std::size_t axis = 0; axis < _expected.size(); ++axis)
  {
    Assert::AreEqual(_expected.at(axis), _actual.at(axis));
  }
}

} // namespace

TEST_CLASS(VoxelPickTests)
{
public:
  TEST_METHOD(HitsTheTopFaceOfTheCellStraightBelow)
  {
    const Engine::SolidAt solid = Grid([](std::int32_t, std::int32_t, std::int32_t) { return true; });
    const Engine::PickHit hit = HitOf(Engine::PickVoxel({{1.5F, 10.0F, 2.5F}, {0.0F, -1.0F, 0.0F}}, 1.0F, FAR_METERS, solid));
    AreEqualCells({1, 3, 2}, hit.cell);
    Assert::IsTrue(hit.face == Engine::VoxelFace::PositiveY);
    Assert::AreEqual(6.0F, hit.distanceMeters, TOLERANCE); // from 10 m down to the grid's top at 4 m
  }

  TEST_METHOD(HitsTheFirstSolidCellOnARayAt34Degrees)
  {
    // From the middle of cell (0, 3, 0), down at 34 degrees along +x. Stepped by hand, with cos 34 = 0.8290 and
    // sin 34 = 0.5592: x crosses 1 at 0.603 m into (1, 3, 0), y crosses 3 at 0.894 m into (1, 2, 0), and x crosses 2 at
    // 1.809 m into (2, 2, 0), which is solid, by its -x face. The floor, and a cell under the ray's path, are solid too.
    const Engine::SolidAt solid =
      Grid([](std::int32_t _x, std::int32_t _y, std::int32_t) { return _y == 0 || (_x == 2 && _y <= 2) || (_x == 1 && _y == 1); });
    const float elevation = DirectX::XMConvertToRadians(34.0F);
    const Engine::PickRay ray{{0.5F, 3.5F, 0.5F}, {std::cos(elevation), -std::sin(elevation), 0.0F}};
    const Engine::PickHit hit = HitOf(Engine::PickVoxel(ray, 1.0F, FAR_METERS, solid));
    AreEqualCells({2, 2, 0}, hit.cell);
    Assert::IsTrue(hit.face == Engine::VoxelFace::NegativeX);
    Assert::AreEqual(1.5F / std::cos(elevation), hit.distanceMeters, TOLERANCE);

    // At half-meter cells, over a floor half a meter thick, the same ray meets the floor's top.
    const Engine::SolidAt halves = [](std::int32_t _x, std::int32_t _y, std::int32_t _z) { return _z == 1 && _y == 0 && _x >= 0; };
    const Engine::PickHit onTheFloor = HitOf(Engine::PickVoxel(ray, 0.5F, FAR_METERS, halves));
    Assert::IsTrue(onTheFloor.face == Engine::VoxelFace::PositiveY);
    // It reaches y = 0.5 m, the half-meter floor's top, after 3 m of fall: 3 / sin 34.
    Assert::AreEqual(3.0F / std::sin(elevation), onTheFloor.distanceMeters, TOLERANCE);
  }

  TEST_METHOD(ReturnsNothingForARayThroughNothing)
  {
    const Engine::SolidAt solid = Grid([](std::int32_t, std::int32_t, std::int32_t) { return false; });
    Assert::IsFalse(Engine::PickVoxel({{1.5F, 10.0F, 2.5F}, {0.0F, -1.0F, 0.0F}}, 1.0F, FAR_METERS, solid).has_value());
    // Nor one that would reach a solid grid, but beyond its reach.
    const Engine::SolidAt full = Grid([](std::int32_t, std::int32_t, std::int32_t) { return true; });
    Assert::IsFalse(Engine::PickVoxel({{1.5F, 10.0F, 2.5F}, {0.0F, -1.0F, 0.0F}}, 1.0F, 5.9F, full).has_value());
    // Nor a ray with no direction.
    Assert::IsFalse(Engine::PickVoxel({{1.5F, 10.0F, 2.5F}, {0.0F, 0.0F, 0.0F}}, 1.0F, FAR_METERS, full).has_value());
  }

  TEST_METHOD(HitsTheCellARayStartsIn)
  {
    const Engine::SolidAt solid = Grid([](std::int32_t _x, std::int32_t _y, std::int32_t _z) { return _x == 1 && _y == 1 && _z == 1; });
    const Engine::PickHit hit = HitOf(Engine::PickVoxel({{1.25F, 1.75F, 1.5F}, {0.0F, -1.0F, 0.0F}}, 1.0F, FAR_METERS, solid));
    AreEqualCells({1, 1, 1}, hit.cell);
    Assert::AreEqual(0.0F, hit.distanceMeters);
    Assert::IsTrue(hit.face == Engine::VoxelFace::PositiveY);
  }

  TEST_METHOD(RefusesARisingRay)
  {
    const Engine::SolidAt solid = Grid([](std::int32_t, std::int32_t, std::int32_t) { return true; });
    Assert::ExpectException<std::invalid_argument>(
      [&solid] { static_cast<void>(Engine::PickVoxel({{0.5F, -1.0F, 0.5F}, {0.0F, 1.0F, 0.0F}}, 1.0F, FAR_METERS, solid)); });
  }
};

} // namespace EngineTests
