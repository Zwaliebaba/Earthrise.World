#include "pch.h"

#include "SunProjection.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The sun's view-projection over a box of the world (ADR-011, decision 9), on the CPU.

namespace EngineTests
{

namespace
{

constexpr float EDGE_TOLERANCE = 1.0e-4F;

/// Where a world point lands: x and y in [-1, 1] across the map, z its reversed depth.
DirectX::XMFLOAT3 Project(DirectX::FXMMATRIX _viewProjection, float _x, float _y, float _z)
{
  DirectX::XMFLOAT3 projected{};
  DirectX::XMStoreFloat3(&projected, DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(_x, _y, _z, 1.0F), _viewProjection));
  return projected;
}

bool InsideTheMap(const DirectX::XMFLOAT3& _point)
{
  return std::abs(_point.x) <= 1.0F + EDGE_TOLERANCE && std::abs(_point.y) <= 1.0F + EDGE_TOLERANCE && _point.z >= -EDGE_TOLERANCE &&
         _point.z <= 1.0F + EDGE_TOLERANCE;
}

} // namespace

TEST_CLASS(SunProjectionTests)
{
public:
  TEST_METHOD(FitsTheBoxInTheMap)
  {
    // The Golden preset's sun over the test scene's 128 m holding, 48 m high (RendererLook.md §6).
    const DirectX::XMMATRIX sun =
      Engine::SunViewProjection(DirectX::XMVectorSet(-90.0F, 75.0F, 55.0F, 0.0F), {0.0F, 0.0F, 0.0F}, {128.0F, 48.0F, 128.0F}, 0.0F);
    float widest = 0.0F;
    for (std::uint32_t corner = 0; corner < 8; ++corner)
    {
      const DirectX::XMFLOAT3 projected =
        Project(sun, (corner & 1U) != 0 ? 128.0F : 0.0F, (corner & 2U) != 0 ? 48.0F : 0.0F, (corner & 4U) != 0 ? 128.0F : 0.0F);
      Assert::IsTrue(InsideTheMap(projected),
                     std::format(L"corner {} lands at ({}, {}, {})", corner, projected.x, projected.y, projected.z).c_str());
      widest = std::max({widest, std::abs(projected.x), std::abs(projected.y)});
    }
    Assert::AreEqual(1.0F, widest, EDGE_TOLERANCE, L"and fills it, with no margin asked for");
  }

  TEST_METHOD(PutsTheSunsSideNearest)
  {
    // Reversed depth: a point moved towards the sun is deeper, and the box's corner nearest the sun
    // is at depth 1, the farthest at 0.
    const DirectX::XMVECTOR towardsSun = DirectX::XMVectorSet(1.0F, 1.0F, 0.0F, 0.0F);
    const DirectX::XMMATRIX sun = Engine::SunViewProjection(towardsSun, {-2.0F, 0.0F, 0.0F}, {2.0F, 3.0F, 1.0F}, 0.0F);
    Assert::AreEqual(1.0F, Project(sun, 2.0F, 3.0F, 0.5F).z, EDGE_TOLERANCE, L"the corner nearest the sun");
    Assert::AreEqual(0.0F, Project(sun, -2.0F, 0.0F, 0.5F).z, EDGE_TOLERANCE, L"the corner farthest from it");
    Assert::IsTrue(Project(sun, 0.1F, 1.1F, 0.5F).z > Project(sun, 0.0F, 1.0F, 0.5F).z, L"a step towards the sun is deeper");
  }

  TEST_METHOD(LooksStraightDownFromAnOverheadSun)
  {
    // With the sun overhead, the world's up is the direction of view, so the map is oriented by
    // north instead; it must still be a finite, fitted projection.
    const DirectX::XMMATRIX sun =
      Engine::SunViewProjection(DirectX::XMVectorSet(0.0F, 1.0F, 0.0F, 0.0F), {0.0F, 0.0F, 0.0F}, {4.0F, 2.0F, 4.0F}, 1.0F);
    const DirectX::XMFLOAT3 top = Project(sun, 2.0F, 2.0F, 2.0F);
    const DirectX::XMFLOAT3 bottom = Project(sun, 2.0F, 0.0F, 2.0F);
    Assert::IsTrue(std::isfinite(top.x) && std::isfinite(top.y) && std::isfinite(top.z));
    Assert::IsTrue(InsideTheMap(top) && InsideTheMap(bottom));
    Assert::IsTrue(top.z > bottom.z, L"the top is nearer the sun");
    Assert::AreEqual(0.0F, top.x, EDGE_TOLERANCE, L"the box's middle is the map's");
  }
};

} // namespace EngineTests
