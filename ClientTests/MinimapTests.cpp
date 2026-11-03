#include "pch.h"

#include "Minimap.h"
#include "Palette.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The minimap (M4-I5; Hud.md §6), with no GPU: the base reduced from the columns, the dots, the camera's view quad and
// a click's point.

namespace ClientTests
{

namespace
{

constexpr std::uint8_t ME = 1;
constexpr std::uint8_t GRASS = 2; // two palette entries with different colours
constexpr std::uint8_t MASONRY = 6;

/// A 64 x 64 map of grass at y 8, with a masonry column at (60, 12, 60), as the session was sent it.
Client::Session SessionWithMap()
{
  std::vector<Protocol::ChunkSnapshot> snapshots;
  for (std::int32_t zChunk = 0; zChunk < 2; ++zChunk)
  {
    for (std::int32_t xChunk = 0; xChunk < 2; ++xChunk)
    {
      auto chunk = std::make_shared<Protocol::Chunk>();
      for (std::int32_t z = 0; z < Protocol::CHUNK_EDGE_VOXELS; ++z)
      {
        for (std::int32_t x = 0; x < Protocol::CHUNK_EDGE_VOXELS; ++x)
        {
          chunk->Set(x, 8, z, GRASS);
        }
      }
      if (xChunk == 1 && zChunk == 1)
      {
        chunk->Set(60 - 32, 12, 60 - 32, MASONRY);
      }
      snapshots.push_back({.coordinate = {.xChunks = xChunk, .yChunks = 0, .zChunks = zChunk}, .version = 1, .voxels = chunk});
    }
  }
  Client::Session session;
  Protocol::StateUpdate state{};
  state.widthCells = 64;
  state.depthCells = 64;
  session.Receive(state, snapshots, {});
  return session;
}

} // namespace

TEST_CLASS(MinimapTests)
{
public:
  TEST_METHOD(FiveHundredTwelveMetersReduceTo188PixelsWithNoColumnLeftOut)
  {
    // Hud.md §6: built at the drawn size by reducing the 1 m columns. Every column falls to exactly one pixel.
    std::int32_t next = 0;
    for (std::uint32_t pixel = 0; pixel < 188; ++pixel)
    {
      const auto [first, last] = Client::Minimap::ColumnsOf(pixel, 188, 512);
      Assert::AreEqual(next, first);
      Assert::IsTrue(last - first == 2 || last - first == 3);
      next = last;
    }
    Assert::AreEqual(512, next);
    // North up: the map's origin is the bottom-left corner, and its far corner the top-right.
    const DirectX::XMFLOAT2 origin = Client::Minimap::ToMinimap(0.0F, 0.0F, 512, 512, 188);
    const DirectX::XMFLOAT2 farCorner = Client::Minimap::ToMinimap(512.0F, 512.0F, 512, 512, 188);
    Assert::AreEqual(0.0F, origin.x);
    Assert::AreEqual(188.0F, origin.y);
    Assert::AreEqual(188.0F, farCorner.x);
    Assert::AreEqual(0.0F, farCorner.y);
  }

  TEST_METHOD(TheBaseIsTheColumnsTopsShadedAndBuiltAgainOnlyWhenTheyChange)
  {
    const Client::Session session = SessionWithMap();
    Client::Minimap minimap;
    minimap.Reset(64, 64, 1);
    std::vector<Protocol::ChunkSnapshot> chunks;
    for (std::int32_t zChunk = 0; zChunk < 2; ++zChunk)
    {
      for (std::int32_t xChunk = 0; xChunk < 2; ++xChunk)
      {
        chunks.push_back(*session.Chunk({.xChunks = xChunk, .yChunks = 0, .zChunks = zChunk}));
      }
    }
    minimap.Take(chunks, session);
    Assert::IsTrue(minimap.BaseDirty());
    const std::vector<std::uint32_t> base = minimap.BuildBase(16); // 4 columns a pixel
    Assert::AreEqual(std::size_t{256}, base.size());
    Assert::IsFalse(minimap.BaseDirty());
    // Grass, shaded by its height, everywhere but the pixel holding the masonry column: the top-right one.
    const std::uint32_t grass = base.at((15 * 16) + 0); // the south-west corner
    Assert::AreEqual(0xFF000000U, grass & 0xFF000000U);
    Assert::AreEqual(grass, base.at((8 * 16) + 8));
    Assert::AreNotEqual(grass, base.at((0 * 16) + 15));
    Assert::AreEqual(12, minimap.GroundAt(60.5F, 60.5F));
    Assert::AreEqual(8, minimap.GroundAt(10.5F, 10.5F));
    // Taking the same chunks again changes nothing.
    minimap.Take(chunks, session);
    Assert::IsFalse(minimap.BaseDirty());
  }

  TEST_METHOD(ADotAppearsOnlyForASquadInTheUpdate)
  {
    Protocol::StateUpdate state{};
    state.player = ME;
    state.widthCells = 64;
    state.depthCells = 64;
    state.squads.push_back(
      {.id = 1, .owner = ME, .units = {10}, .hasGoal = false, .goalVoxels = {}, .shortOfGoal = false, .garrisonedIn = 0});
    state.squads.push_back(
      {.id = 2, .owner = 2, .units = {20}, .hasGoal = false, .goalVoxels = {}, .shortOfGoal = false, .garrisonedIn = 0});
    const auto at = [](std::int32_t _whole) { return Core::Fixed::FromRaw(_whole * Core::Fixed::ONE_RAW); };
    state.units.push_back(
      {.id = 10, .squad = 1, .owner = ME, .kind = Protocol::UnitKind::Spearman, .position = {at(16), at(9), at(48)}, .cellVoxels = {}});
    // Squad 2's figure is not in the update: unseen, it has no dot.
    state.structures.push_back(
      {.id = 70, .owner = 2, .templateId = "T4", .originVoxels = {30, 9, 30}, .quarterTurns = 0, .extentVoxels = {4, 10, 4}, .name = "NE"});
    const Client::Selection selection{.squads = {1}, .structure = Protocol::NO_ENTITY, .activeKind = std::nullopt};
    const std::vector<Client::HudDot> dots = Client::Minimap::Dots(state, selection, 64);
    Assert::AreEqual(std::size_t{2}, dots.size());
    const auto squad = std::ranges::find(dots, false, &Client::HudDot::structure);
    Assert::IsTrue(squad != dots.end());
    Assert::AreEqual(16.0F, squad->positionPixels.x, 0.001F);
    Assert::AreEqual(16.0F, squad->positionPixels.y, 0.001F);
    Assert::IsTrue(squad->selected);
    const auto tower = std::ranges::find(dots, true, &Client::HudDot::structure);
    Assert::AreEqual(32.0F, tower->positionPixels.x, 0.001F);
    Assert::AreEqual(std::uint8_t{1}, tower->team);
  }

  TEST_METHOD(TheViewQuadsCornersAreTheScreensCornersOnTheGround)
  {
    // Rays straight down onto four points of the ground at 9 m: the quad is those points on the minimap.
    const std::array<Engine::PickRay, 4> corners = {{{.originMeters = {10.0F, 50.0F, 50.0F}, .direction = {0.0F, -1.0F, 0.0F}},
                                                     {.originMeters = {40.0F, 50.0F, 50.0F}, .direction = {0.0F, -1.0F, 0.0F}},
                                                     {.originMeters = {40.0F, 50.0F, 20.0F}, .direction = {0.0F, -1.0F, 0.0F}},
                                                     {.originMeters = {10.0F, 50.0F, 20.0F}, .direction = {0.0F, -1.0F, 0.0F}}}};
    const std::array<DirectX::XMFLOAT2, 4> quad = Client::Minimap::ViewQuad(corners, 9.0F, 64, 64, 64);
    Assert::AreEqual(10.0F, quad.at(0).x, 0.001F);
    Assert::AreEqual(14.0F, quad.at(0).y, 0.001F);
    Assert::AreEqual(40.0F, quad.at(2).x, 0.001F);
    Assert::AreEqual(44.0F, quad.at(2).y, 0.001F);
  }

  TEST_METHOD(AClickAtTheMiddleIsTheMapsMiddle)
  {
    const DirectX::XMFLOAT2 point = Client::Minimap::ToMap(94.0F, 94.0F, 512, 512, 188);
    Assert::AreEqual(256.0F, point.x, 0.001F);
    Assert::AreEqual(256.0F, point.y, 0.001F);
  }
};

} // namespace ClientTests
