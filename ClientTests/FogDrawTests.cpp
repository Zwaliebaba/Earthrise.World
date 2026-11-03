#include "pch.h"

#include "FireTally.h"
#include "FogMask.h"
#include "WorldModels.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The fog and the silhouettes as the client hands them to the renderer (M4-H3; ADR-025, decision 8; ADR-021,
// decision 6, amended), with no GPU: the column mask's texels, and which models are silhouetted.

namespace ClientTests
{

namespace
{

constexpr std::int32_t WIDTH = 70; // across a 64-bit word's edge
constexpr std::int32_t DEPTH = 3;

/// An update of a WIDTH by DEPTH map with _seen's cells visible.
Protocol::StateUpdate Seeing(std::span<const std::pair<std::int32_t, std::int32_t>> _seen)
{
  Protocol::StateUpdate state{};
  state.widthCells = WIDTH;
  state.depthCells = DEPTH;
  state.visibleCells.assign(((WIDTH * DEPTH) + 63) / 64, std::uint64_t{0});
  for (const auto& [x, z] : _seen)
  {
    const auto bit = static_cast<std::size_t>((z * WIDTH) + x);
    state.visibleCells.at(bit / 64) |= std::uint64_t{1} << (bit % 64);
  }
  return state;
}

} // namespace

TEST_CLASS(FogDrawTests)
{
public:
  TEST_METHOD(TheMasksTexelsFollowTheUpdatesBits)
  {
    const std::array<std::pair<std::int32_t, std::int32_t>, 4> seen = {{{0, 0}, {63, 0}, {64, 0}, {5, 2}}};
    const std::vector<std::uint8_t> mask = Client::ColumnMask(Seeing(seen));
    Assert::AreEqual(static_cast<std::size_t>(WIDTH * DEPTH), mask.size());
    for (std::int32_t z = 0; z < DEPTH; ++z)
    {
      for (std::int32_t x = 0; x < WIDTH; ++x)
      {
        const bool visible = std::ranges::find(seen, std::pair{x, z}) != seen.end();
        Assert::AreEqual(visible ? Client::SEEN_TEXEL : std::uint8_t{0}, mask.at(static_cast<std::size_t>((z * WIDTH) + x)));
      }
    }
  }

  TEST_METHOD(AnUpdateWithNoVisibilityHasNoMask)
  {
    // A world without players has no fog, and its update no visibility.
    Protocol::StateUpdate state{};
    state.widthCells = WIDTH;
    state.depthCells = DEPTH;
    Assert::IsTrue(Client::ColumnMask(state).empty());
  }

  TEST_METHOD(UnitsAndEnginesAreSilhouettedAndStonesAndGatesAreNot)
  {
    Protocol::StateUpdate state{};
    state.tick = 10;
    state.player = 1;
    const auto at = [](std::int32_t _meters) { return Core::Fixed::FromRaw(_meters * Core::Fixed::ONE_RAW); };
    state.units.push_back({.id = 1,
                           .squad = 101,
                           .owner = 1,
                           .kind = Protocol::UnitKind::Spearman,
                           .position = {.xMeters = at(10), .yMeters = at(9), .zMeters = at(20)},
                           .cellVoxels = {.xVoxels = 10, .yVoxels = 9, .zVoxels = 20}});
    state.engines.push_back({.id = 5,
                             .owner = 1,
                             .kind = Protocol::EngineKind::Ram,
                             .cellVoxels = {.xVoxels = 30, .yVoxels = 9, .zVoxels = 20},
                             .quarterTurns = 0,
                             .reloadTicks = 0,
                             .state = Protocol::EngineState::Packed,
                             .squad = 105,
                             .stateTicksLeft = 0,
                             .position = {.xMeters = at(30), .yMeters = at(9), .zMeters = at(20)},
                             .hitPoints = 1,
                             .maxHitPoints = 1,
                             .footprintVoxels = 3});
    state.shots.push_back({.id = 9,
                           .engine = 5,
                           .position = {.xMeters = at(50), .yMeters = at(20), .zMeters = at(20)},
                           .kind = Protocol::ShotKind::Stone,
                           .owner = 1});
    Protocol::StructureView gatehouse{.id = 7,
                                      .owner = 2,
                                      .templateId = "T5",
                                      .originVoxels = {.xVoxels = 100, .yVoxels = 9, .zVoxels = 200},
                                      .quarterTurns = 0,
                                      .extentVoxels = {.xVoxels = 9, .yVoxels = 12, .zVoxels = 7},
                                      .name = "GH"};
    gatehouse.gateOpen = true;
    gatehouse.gateVoxels = {{.xVoxels = 103, .yVoxels = 9, .zVoxels = 200}};
    state.structures.push_back(gatehouse);
    Client::WorldModels models;
    std::vector<Client::ModelPlacement> placements;
    std::vector<Protocol::VoxelPosition> hidden;
    models.Place(Protocol::StateUpdate{}, state, Client::FireTally{}, 0.0, placements, hidden);
    Assert::AreEqual(std::size_t{4}, placements.size());
    for (const Client::ModelPlacement& placement : placements)
    {
      const std::string_view id = placement.modelId;
      Assert::AreEqual(id == "U2" || id == "S1", placement.silhouette);
    }
  }
};

} // namespace ClientTests
