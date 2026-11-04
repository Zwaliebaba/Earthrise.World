#include "pch.h"

#include "ModelTurn.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Models turned to any angle (ADR-030; M5-G3), with no GPU: the turn's arithmetic, and the oriented voxels a turned
// model is drawn as, against the records a batch draws at a quarter turn.

namespace ClientTests
{

namespace
{

constexpr float CLOSE = 1.0e-4F;
constexpr float EDGE_METERS = 0.5F;
constexpr std::uint8_t FACTION = 16;
constexpr std::uint8_t SIDE = 41;
constexpr std::uint8_t WOOD = 23;

/// A model 3 cells wide and 2 deep: three records, the faction entry at (2, 0, 1), with its top's corners shaded.
std::vector<Engine::VoxelRecord> Records()
{
  Engine::VoxelShade shaded = Engine::UNSHADED;
  shaded.cornerLevels[static_cast<std::size_t>(Engine::VoxelFace::PositiveY)] = {0, 1, 2, 3};
  shaded.shadeTable = 1;
  return {Engine::MakeVoxelRecord(0, 0, 0, WOOD), Engine::MakeVoxelRecord(1, 1, 0, WOOD, shaded),
          Engine::MakeVoxelRecord(2, 0, 1, FACTION)};
}

/// The centre of the record's cell (_x, _y, _z) as a batch draws it from _origin at _turns quarter turns, the footprint
/// _width by _depth cells (VoxelCommon.hlsli's TurnedCell).
DirectX::XMFLOAT3 BatchCenter(const DirectX::XMFLOAT3& _origin, std::uint32_t _turns, std::uint32_t _x, std::uint32_t _y, std::uint32_t _z,
                              std::uint32_t _width, std::uint32_t _depth)
{
  const std::uint32_t lastX = _width - 1;
  const std::uint32_t lastZ = _depth - 1;
  std::uint32_t x = _x;
  std::uint32_t z = _z;
  if (_turns == 1)
  {
    x = _z;
    z = lastX - _x;
  }
  else if (_turns == 2)
  {
    x = lastX - _x;
    z = lastZ - _z;
  }
  else if (_turns == 3)
  {
    x = lastZ - _z;
    z = _x;
  }
  return {_origin.x + ((static_cast<float>(x) + 0.5F) * EDGE_METERS), _origin.y + ((static_cast<float>(_y) + 0.5F) * EDGE_METERS),
          _origin.z + ((static_cast<float>(z) + 0.5F) * EDGE_METERS)};
}

} // namespace

TEST_CLASS(ModelTurnTests)
{
public:
  TEST_METHOD(AYawIsAWholeQuarterTurnOnlyCloseToOne)
  {
    Assert::AreEqual(0U, Client::WholeQuarterTurns(0.0F).value_or(9));
    Assert::AreEqual(1U, Client::WholeQuarterTurns(Client::QUARTER_TURN_RADIANS).value_or(9));
    Assert::AreEqual(3U, Client::WholeQuarterTurns(-Client::QUARTER_TURN_RADIANS).value_or(9));
    Assert::AreEqual(0U, Client::WholeQuarterTurns(Client::FULL_TURN_RADIANS).value_or(9));
    Assert::IsFalse(Client::WholeQuarterTurns(0.01F).has_value());
    Assert::IsFalse(Client::WholeQuarterTurns(Client::QUARTER_TURN_RADIANS * 0.5F).has_value());
    Assert::AreEqual(0.5F, Client::NormalizedYaw(0.5F + (2.0F * Client::FULL_TURN_RADIANS)), CLOSE);
    Assert::AreEqual(Client::FULL_TURN_RADIANS - 0.5F, Client::NormalizedYaw(-0.5F), CLOSE);
  }

  TEST_METHOD(ATurnGoesTheShorterWay)
  {
    const float tenDegrees = std::numbers::pi_v<float> / 18.0F;
    // From 350° to 10° is 20° on through 0, not 340° back.
    Assert::AreEqual(2.0F * tenDegrees, Client::ShorterTurn(Client::FULL_TURN_RADIANS - tenDegrees, tenDegrees), CLOSE);
    Assert::AreEqual(-2.0F * tenDegrees, Client::ShorterTurn(tenDegrees, Client::FULL_TURN_RADIANS - tenDegrees), CLOSE);
    Assert::AreEqual(0.0F, Client::ShorterTurn(1.0F, 1.0F + Client::FULL_TURN_RADIANS), CLOSE);
  }

  TEST_METHOD(AQuarterTurnTakesPlusZTowardsPlusX)
  {
    const auto [x, z] = Client::TurnedOffset(0.0F, 1.0F, Client::QUARTER_TURN_RADIANS);
    Assert::AreEqual(1.0F, x);
    Assert::AreEqual(0.0F, z);
    const auto [eastX, eastZ] = Client::TurnedOffset(1.0F, 0.0F, Client::QUARTER_TURN_RADIANS);
    Assert::AreEqual(0.0F, eastX);
    Assert::AreEqual(-1.0F, eastZ);
    // The quaternion turns the same way: its rotation of +z, v + 2w(q × v) + 2q × (q × v), comes to +x.
    const DirectX::XMFLOAT4 rotation = Client::YawRotation(Client::QUARTER_TURN_RADIANS);
    const float s = rotation.y;
    const float w = rotation.w;
    Assert::AreEqual(1.0F, 2.0F * w * s, CLOSE);
    Assert::AreEqual(0.0F, 1.0F - (2.0F * s * s), CLOSE);
  }

  TEST_METHOD(TheBoxThatBoundsATurnedFootprint)
  {
    const auto [width, depth] = Client::TurnedExtent(2.0F, 1.0F, 0.0F);
    Assert::AreEqual(2.0F, width);
    Assert::AreEqual(1.0F, depth);
    const auto [across, along] = Client::TurnedExtent(2.0F, 1.0F, Client::QUARTER_TURN_RADIANS);
    Assert::AreEqual(1.0F, across);
    Assert::AreEqual(2.0F, along);
    const auto [diagonalX, diagonalZ] = Client::TurnedExtent(2.0F, 1.0F, std::numbers::pi_v<float> / 4.0F);
    Assert::AreEqual(3.0F / std::numbers::sqrt2_v<float>, diagonalX, CLOSE);
    Assert::AreEqual(3.0F / std::numbers::sqrt2_v<float>, diagonalZ, CLOSE);
  }

  TEST_METHOD(TurnedVoxelsAtWholeQuarterTurnsLieWhereTheBatchDrawsTheRecords)
  {
    // The batch draws from the minimum corner of the turned footprint; the turned voxels turn about its middle.
    const std::vector<Engine::VoxelRecord> records = Records();
    constexpr std::uint32_t WIDTH = 3;
    constexpr std::uint32_t DEPTH = 2;
    const DirectX::XMFLOAT3 origin{10.0F, 4.0F, 20.0F};
    for (std::uint32_t turns = 0; turns < 4; ++turns)
    {
      const float yaw = static_cast<float>(turns) * Client::QUARTER_TURN_RADIANS;
      const auto [boundWidth, boundDepth] =
        Client::TurnedExtent(static_cast<float>(WIDTH) * EDGE_METERS, static_cast<float>(DEPTH) * EDGE_METERS, yaw);
      const Client::ModelTurn turn{.middleMeters = {origin.x + (0.5F * boundWidth), origin.y, origin.z + (0.5F * boundDepth)},
                                   .yawRadians = yaw,
                                   .voxelEdgeMeters = EDGE_METERS,
                                   .widthCells = WIDTH,
                                   .depthCells = DEPTH};
      std::vector<Engine::OrientedVoxel> voxels;
      Client::TurnedVoxels(records, turn, 0, 0, voxels);
      Assert::AreEqual(records.size(), voxels.size());
      for (std::size_t index = 0; index < records.size(); ++index)
      {
        const std::uint32_t cell = records.at(index).cellAndPalette;
        const DirectX::XMFLOAT3 expected = BatchCenter(origin, turns, cell & 63U, (cell >> 6) & 63U, (cell >> 12) & 63U, WIDTH, DEPTH);
        const DirectX::XMFLOAT3& center = voxels.at(index).centerMeters;
        const std::wstring which = std::format(L"turn {} voxel {}", turns, index);
        Assert::AreEqual(expected.x, center.x, CLOSE, which.c_str());
        Assert::AreEqual(expected.y, center.y, CLOSE, which.c_str());
        Assert::AreEqual(expected.z, center.z, CLOSE, which.c_str());
        Assert::AreEqual(0.5F * EDGE_METERS, voxels.at(index).halfEdgeMeters);
      }
    }
  }

  TEST_METHOD(TurnedVoxelsCarryTheirRecordsShadingAndSwapTheFactionEntry)
  {
    const std::vector<Engine::VoxelRecord> records = Records();
    const Client::ModelTurn turn{
      .middleMeters = {0.0F, 0.0F, 0.0F}, .yawRadians = 0.6F, .voxelEdgeMeters = EDGE_METERS, .widthCells = 3, .depthCells = 2};
    std::vector<Engine::OrientedVoxel> voxels;
    Client::TurnedVoxels(records, turn, FACTION, SIDE, voxels);
    Assert::AreEqual(std::uint32_t{WOOD}, voxels.at(0).paletteIndex);
    Assert::AreEqual(std::uint32_t{SIDE}, voxels.at(2).paletteIndex);
    Assert::AreEqual(records.at(1).cornerLevels, voxels.at(1).cornerLevels);
    Assert::AreEqual(records.at(1).lastFaceAndJitter & 0xffU, voxels.at(1).lastFaceAndShade & 0xffU);
    Assert::IsTrue((voxels.at(1).lastFaceAndShade & Engine::ORIENTED_SHADE_TABLE) != 0);
    Assert::IsTrue((voxels.at(1).lastFaceAndShade & Engine::ORIENTED_CARRIES_LEVELS) != 0);
    const DirectX::XMFLOAT4 rotation = Client::YawRotation(0.6F);
    Assert::AreEqual(rotation.y, voxels.at(1).rotation.y);
    Assert::AreEqual(rotation.w, voxels.at(1).rotation.w);
    // Each voxel keeps its distance from the middle across the ground as it turns.
    const float ownX = (2.5F * EDGE_METERS) - (1.5F * EDGE_METERS);
    const float ownZ = (1.5F * EDGE_METERS) - (1.0F * EDGE_METERS);
    Assert::AreEqual(std::hypot(ownX, ownZ), std::hypot(voxels.at(2).centerMeters.x, voxels.at(2).centerMeters.z), CLOSE);
  }

  TEST_METHOD(ATurnedSilhouetteIsItsVoxelsFlatInOneEntry)
  {
    const std::vector<Engine::VoxelRecord> records = Records();
    const Client::ModelTurn turn{
      .middleMeters = {1.0F, 2.0F, 3.0F}, .yawRadians = 2.0F, .voxelEdgeMeters = EDGE_METERS, .widthCells = 3, .depthCells = 2};
    std::vector<Engine::OrientedVoxel> voxels;
    Client::TurnedVoxels(records, turn, FACTION, SIDE, voxels);
    std::vector<Engine::OrientedVoxel> silhouette;
    Client::TurnedSilhouette(records, turn, SIDE, silhouette);
    Assert::AreEqual(voxels.size(), silhouette.size());
    for (std::size_t index = 0; index < voxels.size(); ++index)
    {
      Assert::AreEqual(voxels.at(index).centerMeters.x, silhouette.at(index).centerMeters.x);
      Assert::AreEqual(voxels.at(index).centerMeters.z, silhouette.at(index).centerMeters.z);
      Assert::AreEqual(std::uint32_t{SIDE}, silhouette.at(index).paletteIndex);
      Assert::AreEqual(0U, silhouette.at(index).lastFaceAndShade);
      Assert::AreEqual(0.0F, silhouette.at(index).lightnessOffset);
    }
  }
};

} // namespace ClientTests
