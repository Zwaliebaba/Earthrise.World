#include "pch.h"

#include "Look.h"
#include "OrthographicCamera.h"
#include "WorldPicker.h"

#include <memory>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Picking in the world as the client was told it (M3-E3; Hud.md §5).

namespace ClientTests
{

namespace
{

constexpr std::uint32_t WIDTH_PIXELS = 1280;
constexpr std::uint32_t HEIGHT_PIXELS = 720;
constexpr Protocol::EntityId TREBUCHET = 7;
constexpr std::int32_t GROUND_VOXELS = 2; // the ground's top
constexpr std::int32_t WALL_SOUTH_VOXELS = 20;

/// One chunk: ground two voxels deep, and a wall along x from 10 to 20, from the ground up to 7, at z = 20.
Protocol::ChunkSnapshot Site()
{
  Protocol::Chunk chunk;
  for (std::int32_t z = 0; z < Protocol::CHUNK_EDGE_VOXELS; ++z)
  {
    for (std::int32_t x = 0; x < Protocol::CHUNK_EDGE_VOXELS; ++x)
    {
      for (std::int32_t y = 0; y < GROUND_VOXELS; ++y)
      {
        chunk.Set(x, y, z, 1);
      }
    }
  }
  for (std::int32_t x = 10; x <= 20; ++x)
  {
    for (std::int32_t y = GROUND_VOXELS; y < 7; ++y)
    {
      chunk.Set(x, y, WALL_SOUTH_VOXELS, 2);
    }
  }
  return {.coordinate = {0, 0, 0}, .version = 1, .voxels = std::make_shared<const Protocol::Chunk>(chunk)};
}

/// The camera at its default framing, looking at the middle of the chunk's ground.
DirectX::XMFLOAT4X4 ViewProjection()
{
  const Engine::OrthographicCamera camera({.elevationRadians = DirectX::XMConvertToRadians(Client::ELEVATION_DEGREES),
                                           .azimuthRadians = DirectX::XMConvertToRadians(Client::AZIMUTH_DEGREES),
                                           .viewHeightMeters = Client::DEFAULT_VIEW_HEIGHT_METERS,
                                           .minViewHeightMeters = Client::MIN_VIEW_HEIGHT_METERS,
                                           .maxViewHeightMeters = Client::MAX_VIEW_HEIGHT_METERS,
                                           .rotationSeconds = 0.0F,
                                           .targetMeters = {16.0F, static_cast<float>(GROUND_VOXELS), 16.0F}});
  DirectX::XMFLOAT4X4 viewProjection{};
  DirectX::XMStoreFloat4x4(&viewProjection, camera.ViewProjection(static_cast<float>(WIDTH_PIXELS) / static_cast<float>(HEIGHT_PIXELS)));
  return viewProjection;
}

/// The pixel a world point is drawn at.
std::pair<std::int32_t, std::int32_t> PixelOf(const DirectX::XMFLOAT4X4& _viewProjection, float _x, float _y, float _z)
{
  DirectX::XMFLOAT4 clip{};
  DirectX::XMStoreFloat4(&clip,
                         DirectX::XMVector4Transform(DirectX::XMVectorSet(_x, _y, _z, 1.0F), DirectX::XMLoadFloat4x4(&_viewProjection)));
  return {static_cast<std::int32_t>(std::floor((clip.x + 1.0F) * 0.5F * static_cast<float>(WIDTH_PIXELS))),
          static_cast<std::int32_t>(std::floor((1.0F - clip.y) * 0.5F * static_cast<float>(HEIGHT_PIXELS)))};
}

/// The voxel a pick met, after asserting it met one.
Engine::PickHit VoxelOf(const Client::WorldPick& _pick)
{
  Assert::IsTrue(_pick.voxel.has_value());
  return _pick.voxel.value_or(Engine::PickHit{});
}

} // namespace

TEST_CLASS(WorldPickerTests)
{
public:
  TEST_METHOD(SelectsTheTrebuchetUnderAPixelOverItsFootprint)
  {
    Client::WorldPicker picker;
    const Protocol::ChunkSnapshot site = Site();
    picker.TakeSnapshots(std::span(&site, 1));
    const std::array<Client::EngineFootprint, 1> engines = {
      {{.engine = TREBUCHET, .xMeters = 4.0F, .zMeters = 6.0F, .widthMeters = 5.0F, .depthMeters = 5.0F, .groundMeters = 2.0F}}};
    const DirectX::XMFLOAT4X4 viewProjection = ViewProjection();

    const auto [x, y] = PixelOf(viewProjection, 6.5F, 2.0F, 8.5F);
    const Engine::PickRay ray = Client::WorldPicker::RayThrough(viewProjection, x, y, WIDTH_PIXELS, HEIGHT_PIXELS);
    const Client::WorldPick pick = picker.Pick(ray, engines);
    Assert::AreEqual(TREBUCHET, pick.engine);
    // The ground under it is the voxel the ray meets.
    const Engine::PickHit ground = VoxelOf(pick);
    Assert::AreEqual(GROUND_VOXELS - 1, ground.cell[1]);
    Assert::IsTrue(ground.face == Engine::VoxelFace::PositiveY);

    // A pixel beside the footprint picks the ground and no engine.
    const auto [besideX, besideY] = PixelOf(viewProjection, 14.5F, 2.0F, 8.5F);
    const Client::WorldPick beside =
      picker.Pick(Client::WorldPicker::RayThrough(viewProjection, besideX, besideY, WIDTH_PIXELS, HEIGHT_PIXELS), engines);
    Assert::AreEqual(Protocol::NO_ENTITY, beside.engine);
    Assert::IsTrue(beside.voxel.has_value());
  }

  TEST_METHOD(PicksTheWallsVoxelAndFaceUnderAPixelOverIt)
  {
    Client::WorldPicker picker;
    const Protocol::ChunkSnapshot site = Site();
    picker.TakeSnapshots(std::span(&site, 1));
    const DirectX::XMFLOAT4X4 viewProjection = ViewProjection();

    // The middle of the south face of the wall's voxel at (15, 4, 20): the camera stands to the south-west.
    const auto [x, y] = PixelOf(viewProjection, 15.5F, 4.5F, static_cast<float>(WALL_SOUTH_VOXELS));
    const Engine::PickRay ray = Client::WorldPicker::RayThrough(viewProjection, x, y, WIDTH_PIXELS, HEIGHT_PIXELS);
    const Client::WorldPick pick = picker.Pick(ray, {});
    Assert::AreEqual(Protocol::NO_ENTITY, pick.engine);
    const Engine::PickHit wall = VoxelOf(pick);
    const std::array<std::int32_t, 3> expected = {15, 4, WALL_SOUTH_VOXELS};
    for (std::size_t axis = 0; axis < expected.size(); ++axis)
    {
      Assert::AreEqual(expected.at(axis), wall.cell.at(axis));
    }
    Assert::IsTrue(wall.face == Engine::VoxelFace::NegativeZ);

    // The ray runs along the view, from the near plane.
    const Engine::OrthographicCamera camera({.elevationRadians = DirectX::XMConvertToRadians(Client::ELEVATION_DEGREES),
                                             .azimuthRadians = DirectX::XMConvertToRadians(Client::AZIMUTH_DEGREES),
                                             .viewHeightMeters = Client::DEFAULT_VIEW_HEIGHT_METERS,
                                             .minViewHeightMeters = Client::MIN_VIEW_HEIGHT_METERS,
                                             .maxViewHeightMeters = Client::MAX_VIEW_HEIGHT_METERS,
                                             .rotationSeconds = 0.0F,
                                             .targetMeters = {}});
    DirectX::XMFLOAT3 forward{};
    DirectX::XMStoreFloat3(&forward, camera.Forward());
    Assert::AreEqual(forward.x, ray.direction.x, 1.0e-4F);
    Assert::AreEqual(forward.y, ray.direction.y, 1.0e-4F);
    Assert::AreEqual(forward.z, ray.direction.z, 1.0e-4F);
    // A wall hides an engine's footprint behind it.
    const std::array<Client::EngineFootprint, 1> hidden = {
      {{.engine = TREBUCHET, .xMeters = 10.0F, .zMeters = 21.0F, .widthMeters = 12.0F, .depthMeters = 8.0F, .groundMeters = 2.0F}}};
    Assert::AreEqual(Protocol::NO_ENTITY, picker.Pick(ray, hidden).engine);
  }
};

} // namespace ClientTests
