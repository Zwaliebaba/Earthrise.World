#include "pch.h"

#include "ByteReader.h"
#include "ByteWriter.h"
#include "HudAlerts.h"
#include "Look.h"
#include "OrthographicCamera.h"
#include "Overlays.h"
#include "Session.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The readability views and their alerts (M3-F1 to F3; ADR-021, decision 4; Hud.md §10).

namespace ClientTests
{

namespace
{

constexpr Protocol::EntityId W2 = 12;
constexpr Protocol::EntityId COLLAPSE = 40;

/// A state with W2 and one collapse of it: a piece of 9 voxels, 3 x 3 in a face, and a strip of 10 cells.
Protocol::StateUpdate Collapsing(std::uint32_t _warningTicksLeft, Protocol::EntityId _structure = W2)
{
  Protocol::StateUpdate state{};
  state.tick = 100;
  state.structures.push_back({.id = W2, .owner = 2, .templateId = "T3", .originVoxels = {375, 9, 180}, .quarterTurns = 0, .name = "W2"});
  Protocol::CollapseView collapse{};
  collapse.id = COLLAPSE;
  collapse.structure = _structure;
  collapse.warningTicksLeft = _warningTicksLeft;
  collapse.fallTicksLeft = 26;
  for (std::int32_t y = 11; y < 14; ++y)
  {
    for (std::int32_t x = 379; x < 382; ++x)
    {
      collapse.piece.push_back({x, y, 180});
    }
  }
  for (std::int32_t x = 376; x < 386; ++x)
  {
    collapse.strip.push_back({x, 9, 179});
  }
  state.collapses.push_back(collapse);
  return state;
}

/// Gives _session _state, as the executable does each tick. A session is built in place and never moved: with MSVC's
/// library a map's move may allocate, so Session's move may throw (bugprone-exception-escape).
void Tell(Client::Session& _session, const Protocol::StateUpdate& _state)
{
  _session.Receive(_state, {}, {});
}

/// A reading with nothing open and nothing to undermine, which each test changes where it needs to.
Protocol::IntegrityReading Sound(Protocol::EntityId _structure)
{
  return {.structure = _structure,
          .ground = Protocol::GroundKind::Rock,
          .canBeUndermined = false,
          .holeWidthVoxels = 0,
          .holeHeightVoxels = 0,
          .holeFallWidthVoxels = 5,
          .hasOpening = false,
          .openingFloorVoxels = 0,
          .openingFloor = Protocol::OpeningFloor::Ground,
          .openingClearVoxels = 0,
          .routeNow = false,
          .gapRunVoxels = 0,
          .gapFallRunVoxels = 5,
          .falling = false,
          .weakMinVoxels = {380, 9, 180},
          .weakMaxVoxels = {382, 10, 181}};
}

std::vector<std::string> Texts(const std::vector<Client::IntegrityRow>& _rows)
{
  std::vector<std::string> texts;
  texts.reserve(_rows.size());
  for (const Client::IntegrityRow& row : _rows)
  {
    texts.push_back(row.text);
  }
  return texts;
}

/// Answers every integrity query the session asked, each with _readings' reading for its structure.
void AnswerIntegrity(Client::Session& _session, const Protocol::StateUpdate& _state,
                     const std::vector<Protocol::IntegrityReading>& _readings)
{
  std::vector<std::vector<std::byte>> answers;
  for (const std::vector<std::byte>& bytes : _session.TakeQueries())
  {
    Core::ByteReader reader(bytes);
    const Protocol::QueryRequest request = Protocol::DecodeQueryRequest(reader);
    const auto* asked = std::get_if<Protocol::SectionIntegrity>(&request.query);
    Assert::IsNotNull(asked);
    if (asked == nullptr)
    {
      continue;
    }
    const auto reading = std::ranges::find(_readings, asked->structure, &Protocol::IntegrityReading::structure);
    if (reading != _readings.end())
    {
      Core::ByteWriter writer;
      Protocol::Encode(Protocol::Answer{.queryId = request.id, .tick = _state.tick, .reading = *reading}, writer);
      answers.push_back(writer.TakeBytes());
    }
  }
  _session.Receive(_state, {}, answers);
}

/// A view-projection that maps world x and y from 0 to 100 m onto the whole target, z ignored, at one depth.
DirectX::XMFLOAT4X4 FlatProjection()
{
  constexpr float SCALE = 1.0F / 50.0F;
  return {SCALE, 0.0F, 0.0F, 0.0F, 0.0F, SCALE, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, -1.0F, -1.0F, 0.5F, 1.0F};
}

constexpr std::uint8_t PLAYER = 1;
constexpr Protocol::VoxelPosition GOAL{.xVoxels = 380, .yVoxels = 9, .zVoxels = 190}; // tile (11, 5), cell (28, 30)

/// A state with the player's squad of one figure, standing at (380, 9, 170).
Protocol::StateUpdate WithSquad()
{
  Protocol::StateUpdate state{};
  state.tick = 10;
  state.squads.push_back({.id = 3, .owner = PLAYER, .units = {4}, .hasGoal = false, .goalVoxels = {}, .shortOfGoal = false});
  state.units.push_back({.id = 4,
                         .squad = 3,
                         .owner = PLAYER,
                         .kind = Protocol::UnitKind::ManAtArms,
                         .position = {},
                         .cellVoxels = {.xVoxels = 380, .yVoxels = 9, .zVoxels = 170}});
  return state;
}

/// The tiles the session asked the reach of, in order.
std::vector<Client::Tile> AskedTiles(const std::vector<std::vector<std::byte>>& _queries)
{
  std::vector<Client::Tile> tiles;
  for (const std::vector<std::byte>& bytes : _queries)
  {
    Core::ByteReader reader(bytes);
    const Protocol::QueryRequest request = Protocol::DecodeQueryRequest(reader);
    if (const auto* reach = std::get_if<Protocol::TileReach>(&request.query))
    {
      tiles.emplace_back(reach->tileX, reach->tileZ);
    }
  }
  return tiles;
}

/// Answers every reach query the session asked with _reading, its tile set to the query's.
void AnswerReach(Client::Session& _session, const Protocol::StateUpdate& _state, Protocol::TileReachReading _reading)
{
  std::vector<std::vector<std::byte>> answers;
  for (const std::vector<std::byte>& bytes : _session.TakeQueries())
  {
    Core::ByteReader reader(bytes);
    const Protocol::QueryRequest request = Protocol::DecodeQueryRequest(reader);
    if (const auto* reach = std::get_if<Protocol::TileReach>(&request.query))
    {
      _reading.tileX = reach->tileX;
      _reading.tileZ = reach->tileZ;
      Core::ByteWriter writer;
      Protocol::Encode(Protocol::Answer{.queryId = request.id, .tick = _state.tick, .reading = _reading}, writer);
      answers.push_back(writer.TakeBytes());
    }
  }
  _session.Receive(_state, {}, answers);
}

void SetBit(std::array<std::uint8_t, Protocol::TILE_REACH_BYTES>& _bits, std::int32_t _x, std::int32_t _z)
{
  const std::int32_t cell = (_z * 32) + _x;
  const auto index = static_cast<std::size_t>(cell);
  _bits.at(index / 8) = static_cast<std::uint8_t>(_bits.at(index / 8) | (1U << (index % 8)));
}

Protocol::TileReachReading NothingReached()
{
  Protocol::TileReachReading reading{.tileX = 0, .tileZ = 0, .usable = {}, .reached = {}, .surfaceVoxels = {}};
  reading.surfaceVoxels.fill(Protocol::NO_SURFACE_VOXELS);
  return reading;
}

/// The default framing at 1280 x 720, looking at _target.
DirectX::XMFLOAT4X4 Framing(const DirectX::XMFLOAT3& _target)
{
  const Engine::OrthographicCamera camera({.elevationRadians = DirectX::XMConvertToRadians(Client::ELEVATION_DEGREES),
                                           .azimuthRadians = DirectX::XMConvertToRadians(Client::AZIMUTH_DEGREES),
                                           .viewHeightMeters = Client::DEFAULT_VIEW_HEIGHT_METERS,
                                           .minViewHeightMeters = Client::MIN_VIEW_HEIGHT_METERS,
                                           .maxViewHeightMeters = Client::MAX_VIEW_HEIGHT_METERS,
                                           .rotationSeconds = 0.0F,
                                           .targetMeters = _target});
  DirectX::XMFLOAT4X4 viewProjection{};
  DirectX::XMStoreFloat4x4(&viewProjection, camera.ViewProjection(1280.0F / 720.0F));
  return viewProjection;
}

Protocol::StructureView Section(Protocol::EntityId _id, Protocol::VoxelPosition _origin, Protocol::VoxelPosition _extent)
{
  return {.id = _id,
          .owner = 2,
          .templateId = "T3",
          .originVoxels = _origin,
          .quarterTurns = 0,
          .extentVoxels = _extent,
          .name = "W" + std::to_string(_id)};
}

} // namespace

TEST_CLASS(OverlaysTests)
{
public:
  TEST_METHOD(DrawsACollapsesPieceAndStripInBad)
  {
    Client::Session session;
    Tell(session, Collapsing(135));
    const Client::Overlays overlays;
    const std::vector<Client::OverlayLayer> layers = overlays.Layers(session, 0.25, true);
    std::size_t records = 0;
    for (const Client::OverlayLayer& layer : layers)
    {
      records += layer.boxes.size();
      for (const Client::OverlayBox& box : layer.boxes)
      {
        Assert::AreEqual(Client::OVERLAY_BAD_INDEX, box.paletteIndex);
      }
    }
    Assert::AreEqual(std::size_t{19}, records); // 9 voxels of the piece and 10 cells of the strip

    // The strip lies on the ground, the voxel under each cell where units are hit.
    const auto strip = std::ranges::find_if(layers, [](const Client::OverlayLayer& _layer) { return _layer.boxes.size() == 10; });
    Assert::IsTrue(strip != layers.end());
    for (const Client::OverlayBox& box : strip->boxes)
    {
      Assert::AreEqual(8, box.cellVoxels.yVoxels);
    }

    // The piece pulses over a second, and the preview draws only with the integrity view on.
    const auto pieceAlpha = [&overlays, &session](double _seconds)
    {
      for (const Client::OverlayLayer& layer : overlays.Layers(session, _seconds, true))
      {
        if (layer.boxes.size() == 9)
        {
          return layer.alpha;
        }
      }
      return -1.0F;
    };
    Assert::AreEqual(Client::PIECE_MIN_ALPHA, pieceAlpha(0.0), 1.0e-5F);
    Assert::AreEqual(Client::PIECE_MAX_ALPHA, pieceAlpha(0.5), 1.0e-5F);
    Assert::AreEqual(pieceAlpha(0.2), pieceAlpha(1.2), 1.0e-5F);
    for (const Client::OverlayLayer& layer : overlays.Layers(session, 0.25, false))
    {
      Assert::IsTrue(layer.boxes.empty());
    }
    // A collapse that is falling has no preview.
    Client::Session falling;
    Tell(falling, Collapsing(0));
    for (const Client::OverlayLayer& layer : Client::Overlays{}.Layers(falling, 0.25, true))
    {
      Assert::IsTrue(layer.boxes.empty());
    }
  }

  TEST_METHOD(CountsTheAlertDownInWholeSecondsRoundingUp)
  {
    const Protocol::StateUpdate state = Collapsing(135);
    for (const auto& [ticks, text] :
         {std::pair{135U, "W2 WILL FALL IN 7 S"}, std::pair{121U, "W2 WILL FALL IN 7 S"}, std::pair{120U, "W2 WILL FALL IN 6 S"},
          std::pair{21U, "W2 WILL FALL IN 2 S"}, std::pair{20U, "W2 WILL FALL IN 1 S"}, std::pair{1U, "W2 WILL FALL IN 1 S"}})
    {
      Protocol::CollapseView collapse = state.collapses.front();
      collapse.warningTicksLeft = ticks;
      Assert::AreEqual(std::string(text), Client::Overlays::WillFallText(state, collapse));
    }

    // Raised each frame in place, urgent and at the piece, and replaced by the fall's alert, which then expires.
    Client::Overlays overlays;
    Client::HudAlerts alerts;
    Client::Session told;
    Tell(told, Collapsing(135));
    overlays.Update(told, 1.0, alerts);
    Tell(told, Collapsing(119));
    overlays.Update(told, 1.8, alerts);
    Assert::AreEqual(std::size_t{1}, alerts.Shown().size());
    const Client::HudAlert countdown = alerts.Shown().front();
    Assert::AreEqual(std::string("W2 WILL FALL IN 6 S"), countdown.text);
    Assert::IsTrue(countdown.urgent);
    Assert::IsTrue(countdown.location == std::optional<Protocol::VoxelPosition>(Protocol::VoxelPosition{380, 11, 180}));
    Tell(told, Collapsing(0));
    overlays.Update(told, 8.0, alerts);
    Assert::AreEqual(std::size_t{1}, alerts.Shown().size());
    Assert::AreEqual(std::string("W2 HAS FALLEN"), alerts.Shown().front().text);
    alerts.Expire(8.0 + Client::HudAlerts::LIFETIME_SECONDS);
    Assert::AreEqual(std::size_t{0}, alerts.Shown().size());
  }

  TEST_METHOD(ReadsAReadingAsItsRowsWordForWord)
  {
    // W01's slot: 4 m wide, falling at 5, an opening on the ground 1 m clear, on earth.
    Protocol::IntegrityReading slot = Sound(W2);
    slot.holeWidthVoxels = 4;
    slot.holeHeightVoxels = 1;
    slot.hasOpening = true;
    slot.openingClearVoxels = 1;
    slot.ground = Protocol::GroundKind::Earth;
    slot.canBeUndermined = true;
    const std::vector<Client::IntegrityRow> rows = Client::Overlays::IntegrityRows(slot);
    Assert::IsTrue(Texts(rows) == std::vector<std::string>{"HOLE 4 m · falls at 5 m", "OPENING ground 1 m · too low", "GAP none",
                                                           "GROUND earth · can be undermined"});
    Assert::AreEqual(Client::BAD_SRGB, rows.back().srgb);

    // W05's slit: its floor stone 1 m up, 4 m clear, on rock; a hole that never falls in the section holds.
    Protocol::IntegrityReading slit = Sound(W2);
    slit.holeWidthVoxels = 1;
    slit.holeFallWidthVoxels = 0;
    slit.hasOpening = true;
    slit.openingFloor = Protocol::OpeningFloor::Stone;
    slit.openingFloorVoxels = 1;
    slit.openingClearVoxels = 4;
    const std::vector<Client::IntegrityRow> slitRows = Client::Overlays::IntegrityRows(slit);
    Assert::IsTrue(Texts(slitRows) == std::vector<std::string>{"HOLE 1 m · holds", "OPENING stone 1 m up 4 m · no footing", "GAP none",
                                                               "GROUND rock · cannot be undermined"});
    Assert::AreEqual(Client::GOOD_SRGB, slitRows.back().srgb);

    // W07's hole, a route now; W10's gap against its fall run; a gap that never falls; and a section falling.
    Protocol::IntegrityReading hole = Sound(W2);
    hole.holeWidthVoxels = 1;
    hole.hasOpening = true;
    hole.openingClearVoxels = 2;
    hole.routeNow = true;
    hole.gapRunVoxels = 4;
    hole.falling = true;
    const std::vector<Client::IntegrityRow> holeRows = Client::Overlays::IntegrityRows(hole);
    Assert::IsTrue(Texts(holeRows) == std::vector<std::string>{"HOLE 1 m · falls at 5 m", "OPENING ground 2 m · a route now",
                                                               "GAP 4 m · falls at 5 m", "GROUND rock · cannot be undermined", "FALLS"});
    Assert::AreEqual(Client::BAD_SRGB, holeRows.back().srgb);
    hole.gapFallRunVoxels = 0;
    Assert::AreEqual(std::string("GAP 4 m · holds"), Client::Overlays::IntegrityRows(hole).at(2).text);
  }

  TEST_METHOD(TintsTheWeakPointsBoxAndTogglesWithTheView)
  {
    Protocol::StateUpdate state{};
    state.structures.push_back(Section(W2, {375, 9, 180}, {8, 6, 2}));
    Client::Session session;
    Tell(session, state);
    Client::Overlays overlays;
    overlays.Ask(session, {}, true);
    AnswerIntegrity(session, state, {Sound(W2)});
    std::size_t accent = 0;
    for (const Client::OverlayLayer& layer : overlays.Layers(session, 0.0, true))
    {
      accent += static_cast<std::size_t>(std::ranges::count(layer.boxes, Client::OVERLAY_ACCENT_INDEX, &Client::OverlayBox::paletteIndex));
    }
    Assert::AreEqual(std::size_t{12}, accent); // 3 x 2 x 2, both corners included

    // F1 off: no tint and no label.
    for (const Client::OverlayLayer& layer : overlays.Layers(session, 0.0, false))
    {
      Assert::IsTrue(layer.boxes.empty());
    }
    std::vector<Client::ScreenQuad> quads;
    const DirectX::XMFLOAT4X4 projection = FlatProjection();
    Assert::IsTrue(overlays.LayLabels(session, projection, 600, 600, 30, 1.0F, false, quads).empty());
    Assert::IsTrue(quads.empty());
  }

  TEST_METHOD(KeepsALabelOnScreenWhenItsAnchorIsNearAnEdge)
  {
    // At 1200 px, a world x of 1 m is 12 px from the left and a height of 97 m is 36 px from the top, just under the
    // top bar's 30; the other's anchor is 24 px from the right and 12 px from the bottom. A label at a UI scale of 2 is
    // about 820 px wide, as at 1280 px.
    Protocol::StateUpdate state{};
    state.structures.push_back(Section(1, {0, 0, 0}, {2, 96, 2}));
    state.structures.push_back(Section(2, {97, -1, 0}, {2, 1, 2}));
    Client::Session session;
    Tell(session, state);
    Client::Overlays overlays;
    overlays.Ask(session, {}, true);
    AnswerIntegrity(session, state, {Sound(1), Sound(2)});
    for (const float scale : {1.0F, 1.5F, 2.0F})
    {
      std::vector<Client::ScreenQuad> quads;
      const DirectX::XMFLOAT4X4 projection = FlatProjection();
      const std::vector<Client::PlacedLabel> labels = overlays.LayLabels(session, projection, 1200, 1200, 30, scale, true, quads);
      Assert::AreEqual(std::size_t{2}, labels.size());
      for (const Client::PlacedLabel& label : labels)
      {
        Assert::IsFalse(label.nameOnly);
        Assert::IsTrue(label.xPixels >= 0 && label.xPixels + label.widthPixels <= 1200);
        Assert::IsTrue(label.yPixels >= 30 && label.yPixels + label.heightPixels <= 1200);
      }
      for (const Client::ScreenQuad& quad : quads)
      {
        Assert::IsTrue(quad.xPixels >= 0.0F && quad.xPixels + quad.widthPixels <= 1200.0F);
        Assert::IsTrue(quad.yPixels >= 30.0F && quad.yPixels + quad.heightPixels <= 1200.0F);
        Assert::AreEqual(std::floor(quad.xPixels), quad.xPixels);
        Assert::AreEqual(std::floor(quad.yPixels), quad.yPixels);
      }
    }
  }

  TEST_METHOD(ShowsANameAloneWhereItsRowsWouldCoverANearerLabel)
  {
    // Two anchors 16 m apart up the screen, 96 px: the first's rows fill the 138 px above it, so the second's would
    // cover them, and it shows its name alone in the 42 px above its anchor.
    Protocol::StateUpdate state{};
    state.structures.push_back(Section(1, {49, 49, 0}, {2, 1, 2}));
    state.structures.push_back(Section(2, {49, 33, 0}, {2, 1, 2}));
    Client::Session session;
    Tell(session, state);
    Client::Overlays overlays;
    overlays.Ask(session, {}, true);
    AnswerIntegrity(session, state, {Sound(1), Sound(2)});
    std::vector<Client::ScreenQuad> quads;
    const DirectX::XMFLOAT4X4 projection = FlatProjection();
    const std::vector<Client::PlacedLabel> labels = overlays.LayLabels(session, projection, 600, 600, 30, 1.0F, true, quads);
    Assert::AreEqual(std::size_t{2}, labels.size());
    Assert::IsFalse(labels.at(0).nameOnly);
    Assert::IsTrue(labels.at(1).nameOnly);
  }

  TEST_METHOD(AsksEachSectionOnceAndAgainWhenItsGroundOrItsCollapseChanges)
  {
    Protocol::StateUpdate state = Collapsing(135);
    state.collapses.clear();
    Client::Session session;
    Tell(session, state);
    Client::Overlays overlays;
    overlays.Ask(session, {}, true);
    Assert::AreEqual(std::size_t{1}, session.TakeQueries().size());
    overlays.Ask(session, {}, true);
    Assert::AreEqual(std::size_t{0}, session.TakeQueries().size());
    // A chunk under it, and one far away.
    const auto snapshot = [](std::int32_t _xChunks, std::int32_t _zChunks)
    {
      return Protocol::ChunkSnapshot{
        .coordinate = {_xChunks, 0, _zChunks}, .version = 2, .voxels = std::make_shared<const Protocol::Chunk>()};
    };
    const std::array<Protocol::ChunkSnapshot, 1> far = {snapshot(2, 2)};
    overlays.Ask(session, far, true);
    Assert::AreEqual(std::size_t{0}, session.TakeQueries().size());
    const std::array<Protocol::ChunkSnapshot, 1> under = {snapshot(375 / 32, 180 / 32)};
    overlays.Ask(session, under, true);
    Assert::AreEqual(std::size_t{1}, session.TakeQueries().size());
    // A collapse of it starts, then falls.
    Client::Session asking;
    Tell(asking, Collapsing(135));
    overlays.Ask(asking, {}, true);
    Assert::AreEqual(std::size_t{1}, asking.TakeQueries().size());
    overlays.Ask(asking, {}, true);
    Assert::AreEqual(std::size_t{0}, asking.TakeQueries().size());
    asking.Receive(Collapsing(0), {}, {});
    overlays.Ask(asking, {}, true);
    Assert::AreEqual(std::size_t{1}, asking.TakeQueries().size());
    // Off, it asks nothing; turned on again, it asks again.
    overlays.Ask(asking, {}, false);
    Assert::AreEqual(std::size_t{0}, asking.TakeQueries().size());
    overlays.Ask(asking, {}, true);
    Assert::AreEqual(std::size_t{1}, asking.TakeQueries().size());
  }

  TEST_METHOD(DrawsReachedCellsInGoodAndTheRestOfTheUsableInDimAtTheirSurfaces)
  {
    const Protocol::StateUpdate state = WithSquad();
    Client::Session session;
    Tell(session, state);
    Client::Overlays overlays;
    const std::array<Client::Tile, 1> visible = {{{11, 5}}};
    overlays.AskReach(session, {}, visible, PLAYER, true);
    // 12 usable cells along row 3, at heights 9 and 10, of which the first 5 are reached.
    Protocol::TileReachReading reading = NothingReached();
    for (std::int32_t x = 0; x < 12; ++x)
    {
      SetBit(reading.usable, x, 3);
      const std::int32_t cell = (3 * 32) + x;
      reading.surfaceVoxels.at(static_cast<std::size_t>(cell)) = static_cast<std::int16_t>(9 + (x % 2));
      if (x < 5)
      {
        SetBit(reading.reached, x, 3);
      }
    }
    AnswerReach(session, state, reading);
    const Client::OverlayLayer layer = overlays.PassabilityLayer(session, true);
    Assert::AreEqual(std::size_t{12}, layer.boxes.size());
    Assert::AreEqual(std::ptrdiff_t{5}, std::ranges::count(layer.boxes, Client::OVERLAY_GOOD_INDEX, &Client::OverlayBox::paletteIndex));
    Assert::AreEqual(std::ptrdiff_t{7}, std::ranges::count(layer.boxes, Client::OVERLAY_DIM_INDEX, &Client::OverlayBox::paletteIndex));
    for (const Client::OverlayBox& box : layer.boxes)
    {
      const std::int32_t x = box.cellVoxels.xVoxels - (11 * 32);
      Assert::AreEqual(9 + (x % 2), box.cellVoxels.yVoxels);
      Assert::AreEqual((5 * 32) + 3, box.cellVoxels.zVoxels);
    }
    Assert::IsTrue(overlays.PassabilityLayer(session, false).boxes.empty());
  }

  TEST_METHOD(RaisesTheBreachAlertOnceOnTheFirstUpdateThatReachesTheGoal)
  {
    const Protocol::StateUpdate state = WithSquad();
    Client::Session session;
    Tell(session, state);
    Client::Overlays overlays;
    Client::HudAlerts alerts;
    overlays.WatchBreach(GOAL);
    // With the view off it still asks the goal's tile, and only that.
    overlays.AskReach(session, {}, {}, PLAYER, false);
    const std::vector<std::vector<std::byte>> asked = session.TakeQueries();
    Assert::IsTrue(AskedTiles(asked) == std::vector<Client::Tile>{{11, 5}});
    AnswerReach(session, state, NothingReached());
    overlays.Update(session, 1.0, alerts);
    Assert::AreEqual(std::size_t{0}, alerts.Shown().size());

    // The courtyard's middle reads reached: the alert, once.
    const std::array<Protocol::ChunkSnapshot, 1> breached = {
      Protocol::ChunkSnapshot{.coordinate = {11, 0, 5}, .version = 3, .voxels = std::make_shared<const Protocol::Chunk>()}};
    overlays.AskReach(session, breached, {}, PLAYER, false);
    Protocol::TileReachReading open = NothingReached();
    SetBit(open.usable, GOAL.xVoxels - (11 * 32), GOAL.zVoxels - (5 * 32));
    SetBit(open.reached, GOAL.xVoxels - (11 * 32), GOAL.zVoxels - (5 * 32));
    AnswerReach(session, state, open);
    overlays.Update(session, 2.0, alerts);
    overlays.Update(session, 2.1, alerts);
    Assert::AreEqual(std::size_t{1}, alerts.Shown().size());
    const Client::HudAlert alert = alerts.Shown().front();
    Assert::AreEqual(std::string("BREACH OPEN AT W2"), alert.text);
    Assert::IsFalse(alert.urgent);
    Assert::IsTrue(alert.location == std::optional<Protocol::VoxelPosition>(GOAL));
  }

  TEST_METHOD(AsksNoTileOffScreen)
  {
    // The default framing on W2's foot covers the tiles around (11, 5), and none far off.
    const std::vector<Client::Tile> visible = Client::Overlays::VisibleTiles(Framing({380.5F, 9.0F, 180.0F}), 1280, 720, 9.0F);
    Assert::IsTrue(std::ranges::find(visible, Client::Tile{11, 5}) != visible.end());
    for (const Client::Tile& far : {Client::Tile{0, 0}, Client::Tile{20, 20}, Client::Tile{11, 12}})
    {
      Assert::IsTrue(std::ranges::find(visible, far) == visible.end());
    }
    Assert::AreEqual(std::size_t{35}, visible.size());
    // Zoomed out the farthest, at most 36, around the middle.
    const Engine::OrthographicCamera far({.elevationRadians = DirectX::XMConvertToRadians(Client::ELEVATION_DEGREES),
                                          .azimuthRadians = DirectX::XMConvertToRadians(Client::AZIMUTH_DEGREES),
                                          .viewHeightMeters = Client::MAX_VIEW_HEIGHT_METERS,
                                          .minViewHeightMeters = Client::MIN_VIEW_HEIGHT_METERS,
                                          .maxViewHeightMeters = Client::MAX_VIEW_HEIGHT_METERS,
                                          .rotationSeconds = 0.0F,
                                          .targetMeters = {380.5F, 9.0F, 180.0F}});
    DirectX::XMFLOAT4X4 farProjection{};
    DirectX::XMStoreFloat4x4(&farProjection, far.ViewProjection(1280.0F / 720.0F));
    const std::vector<Client::Tile> wide = Client::Overlays::VisibleTiles(farProjection, 1280, 720, 9.0F);
    Assert::AreEqual(std::size_t{36}, wide.size());
    Assert::IsTrue(std::ranges::find(wide, Client::Tile{11, 5}) != wide.end());

    const Protocol::StateUpdate state = WithSquad();
    Client::Session session;
    Tell(session, state);
    Client::Overlays overlays;
    overlays.AskReach(session, {}, visible, PLAYER, true);
    const std::vector<Client::Tile> asked = AskedTiles(session.TakeQueries());
    Assert::AreEqual(visible.size(), asked.size());
    for (const Client::Tile& tile : asked)
    {
      Assert::IsTrue(std::ranges::find(visible, tile) != visible.end());
    }
    // Nothing again until the camera moves to new tiles, and then only those.
    overlays.AskReach(session, {}, visible, PLAYER, true);
    Assert::AreEqual(std::size_t{0}, session.TakeQueries().size());
    const std::vector<Client::Tile> moved = Client::Overlays::VisibleTiles(Framing({444.5F, 9.0F, 180.0F}), 1280, 720, 9.0F);
    overlays.AskReach(session, {}, moved, PLAYER, true);
    for (const Client::Tile& tile : AskedTiles(session.TakeQueries()))
    {
      Assert::IsTrue(std::ranges::find(visible, tile) == visible.end());
      Assert::IsTrue(std::ranges::find(moved, tile) != moved.end());
    }
  }

  TEST_METHOD(NamesACollapseWithNoStructureAWall)
  {
    const Protocol::StateUpdate state = Collapsing(40, Protocol::NO_ENTITY);
    Assert::AreEqual(std::string("A WALL WILL FALL IN 2 S"), Client::Overlays::WillFallText(state, state.collapses.front()));
    Assert::AreEqual(std::string("A WALL HAS FALLEN"), Client::Overlays::HasFallenText(state, state.collapses.front()));
  }
};

} // namespace ClientTests
