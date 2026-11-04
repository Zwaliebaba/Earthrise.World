#include "pch.h"

#include "ByteReader.h"
#include "ByteWriter.h"
#include "Hud.h"
#include "HudAlerts.h"
#include "Look.h"
#include "OrthographicCamera.h"
#include "Overlays.h"
#include "Session.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The readability views and their alerts (M3-F1 to F3 and H1; ADR-021, decision 4; Hud.md §10).

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

/// The lab's W2 and W5, as Simulation::BuildLabSite places them.
Protocol::StateUpdate LabSections()
{
  Protocol::StateUpdate state{};
  state.structures.push_back(Section(2, {377, 9, 180}, {8, 7, 2}));
  state.structures.push_back(Section(5, {391, 9, 190}, {2, 7, 8}));
  return state;
}

/// The default framing on a target _widthPixels by _heightPixels whose top bar is _topBarPixels high, as Client::Camera
/// draws it: looking at W2's foot, the look-at point moved down half the top bar.
DirectX::XMMATRIX DefaultFraming(float _widthPixels, float _heightPixels, float _topBarPixels)
{
  const Engine::OrthographicCamera camera({.elevationRadians = DirectX::XMConvertToRadians(Client::ELEVATION_DEGREES),
                                           .azimuthRadians = DirectX::XMConvertToRadians(Client::AZIMUTH_DEGREES),
                                           .viewHeightMeters = Client::DEFAULT_VIEW_HEIGHT_METERS,
                                           .minViewHeightMeters = Client::MIN_VIEW_HEIGHT_METERS,
                                           .maxViewHeightMeters = Client::MAX_VIEW_HEIGHT_METERS,
                                           .rotationSeconds = 0.0F,
                                           .targetMeters = {381.5F, 9.0F, 180.0F}});
  return DirectX::XMMatrixMultiply(camera.ViewProjection(_widthPixels / _heightPixels),
                                   DirectX::XMMatrixTranslation(0.0F, -_topBarPixels / _heightPixels, 0.0F));
}

/// The pixels a structure's box covers on screen: the bounds of its eight corners.
Client::PixelRect OnScreen(const Protocol::StructureView& _structure, const DirectX::XMMATRIX& _viewProjection, float _widthPixels,
                           float _heightPixels)
{
  float left = std::numeric_limits<float>::max();
  float top = std::numeric_limits<float>::max();
  float right = std::numeric_limits<float>::lowest();
  float bottom = std::numeric_limits<float>::lowest();
  for (std::uint32_t corner = 0; corner < 8; ++corner)
  {
    const auto along = [corner](std::uint32_t _bit, std::int32_t _origin, std::int32_t _extent)
    { return static_cast<float>((corner & _bit) != 0 ? _origin + _extent : _origin); };
    const DirectX::XMVECTOR point = DirectX::XMVectorSet(along(1, _structure.originVoxels.xVoxels, _structure.extentVoxels.xVoxels),
                                                         along(2, _structure.originVoxels.yVoxels, _structure.extentVoxels.yVoxels),
                                                         along(4, _structure.originVoxels.zVoxels, _structure.extentVoxels.zVoxels), 1.0F);
    DirectX::XMFLOAT4 clip{};
    DirectX::XMStoreFloat4(&clip, DirectX::XMVector4Transform(point, _viewProjection));
    const float x = (clip.x / clip.w + 1.0F) * 0.5F * _widthPixels;
    const float y = (1.0F - clip.y / clip.w) * 0.5F * _heightPixels;
    left = std::min(left, x);
    right = std::max(right, x);
    top = std::min(top, y);
    bottom = std::max(bottom, y);
  }
  return {static_cast<std::int32_t>(std::floor(left)), static_cast<std::int32_t>(std::floor(top)),
          static_cast<std::int32_t>(std::ceil(right - std::floor(left))), static_cast<std::int32_t>(std::ceil(bottom - std::floor(top)))};
}

/// Thirty sections W1 to W30, each 4 x 10 x 4 from y 9, in a grid of 6 by 5 cells 20 m by 14 m apart from (10, 40);
/// W15, the selected tests' 115, is locked 10 s.
Protocol::StateUpdate ThirtySections()
{
  Protocol::StateUpdate state{};
  state.tick = 10;
  state.player = PLAYER;
  for (std::int32_t row = 0; row < 5; ++row)
  {
    for (std::int32_t column = 0; column < 6; ++column)
    {
      const std::int32_t number = (row * 6) + column + 1;
      Protocol::StructureView structure{.id = static_cast<Protocol::EntityId>(100 + number),
                                        .owner = 2,
                                        .templateId = "T3",
                                        .originVoxels = {10 + (20 * column), 9, 40 + (14 * row)},
                                        .quarterTurns = 0,
                                        .extentVoxels = {4, 10, 4},
                                        .name = std::format("W{}", number)};
      structure.repairLockTicks = number == 15 ? 200 : 0;
      state.structures.push_back(structure);
    }
  }
  return state;
}

/// Seen from straight above, 128 m square onto the whole target: pixel x is x / 128 of the width, and pixel y is
/// (1 - z / 128) of the height.
DirectX::XMFLOAT4X4 Overhead()
{
  DirectX::XMFLOAT4X4 matrix{};
  matrix.m[0][0] = 2.0F / 128.0F;
  matrix.m[2][1] = 2.0F / 128.0F;
  matrix.m[3][0] = -1.0F;
  matrix.m[3][1] = -1.0F;
  matrix.m[3][3] = 1.0F;
  return matrix;
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
    Assert::AreEqual(Client::DIM_SRGB, rows.back().srgb); // no gap under it (M3-H1)

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
    Assert::AreEqual(Client::DIM_SRGB, slitRows.back().srgb);

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

    // F1 off: no tint and no readout.
    for (const Client::OverlayLayer& layer : overlays.Layers(session, 0.0, false))
    {
      Assert::IsTrue(layer.boxes.empty());
    }
    Assert::IsFalse(overlays.Readout(session, 0.0, false).has_value());
  }

  TEST_METHOD(FocusesW2UntilThePointerIsOverAnotherSectionOrOneIsClicked)
  {
    const Protocol::StateUpdate state = LabSections();
    Client::Session session;
    Tell(session, state);
    Client::Overlays overlays;
    Assert::AreEqual(Protocol::EntityId{2}, overlays.Focused(session)); // W2, with the pointer over nothing
    // The pointer over a voxel of W5's box.
    overlays.Hover(Client::Overlays::SectionAt(state, {392, 12, 195}));
    Assert::AreEqual(Protocol::EntityId{5}, overlays.Focused(session));
    overlays.Hover(Client::Overlays::SectionAt(state, {380, 9, 170})); // the ground south of the wall
    Assert::AreEqual(Protocol::EntityId{2}, overlays.Focused(session));
    // W5 clicked keeps it, the pointer over W2 shows W2 while it is there, and a click elsewhere lets W5 go.
    overlays.Select(5);
    Assert::AreEqual(Protocol::EntityId{5}, overlays.Focused(session));
    overlays.Hover(2);
    Assert::AreEqual(Protocol::EntityId{2}, overlays.Focused(session));
    overlays.Hover(Protocol::NO_ENTITY);
    Assert::AreEqual(Protocol::EntityId{5}, overlays.Focused(session));
    overlays.Select(Protocol::NO_ENTITY);
    Assert::AreEqual(Protocol::EntityId{2}, overlays.Focused(session));
    // A section the state no longer holds is not focused.
    overlays.Select(7);
    Assert::AreEqual(Protocol::EntityId{2}, overlays.Focused(session));
  }

  TEST_METHOD(ReadsTheFocusedSectionWithBarsOfItsWidthsOverTheirFallWidths)
  {
    const Protocol::StateUpdate state = LabSections();
    Client::Session session;
    Tell(session, state);
    Client::Overlays overlays;
    overlays.Ask(session, {}, true);
    Protocol::IntegrityReading w2 = Sound(2);
    w2.holeWidthVoxels = 2; // falls at 5
    w2.gapRunVoxels = 0;    // falls at 5
    Protocol::IntegrityReading w5 = Sound(5);
    w5.holeWidthVoxels = 1;
    w5.holeFallWidthVoxels = 0; // a hole that never falls in the section has no bar
    w5.gapRunVoxels = 6;        // past its fall run: full, in bad
    AnswerIntegrity(session, state, {w2, w5});
    Client::HudAlerts alerts;
    overlays.Update(session, 0.0, alerts);
    const std::optional<Client::HudReadout> readout = overlays.Readout(session, 0.0, true);
    Assert::IsTrue(readout.has_value());
    const Client::HudReadout shown = readout.value_or(Client::HudReadout{});
    Assert::AreEqual(std::string("W2"), shown.title);
    Assert::AreEqual(std::size_t{4}, shown.rows.size());
    Assert::AreEqual(std::string("HOLE"), shown.rows.at(0).key);
    Assert::AreEqual(std::string("2 m · falls at 5 m"), shown.rows.at(0).value);
    Assert::AreEqual(0.4F, shown.rows.at(0).fill.value_or(-1.0F));
    Assert::IsFalse(shown.rows.at(1).fill.has_value()); // OPENING
    Assert::AreEqual(0.0F, shown.rows.at(2).fill.value_or(-1.0F));
    Assert::IsFalse(shown.rows.at(3).fill.has_value()); // GROUND
    overlays.Hover(5);
    const Client::HudReadout other = overlays.Readout(session, 0.0, true).value_or(Client::HudReadout{});
    Assert::AreEqual(std::string("W5"), other.title);
    Assert::IsFalse(other.rows.at(0).fill.has_value());
    Assert::AreEqual(1.2F, other.rows.at(2).fill.value_or(-1.0F));
    // F1 off: no readout.
    Assert::IsFalse(overlays.Readout(session, 0.0, false).has_value());
  }

  TEST_METHOD(FlashesARowThatChangedAndNoOther)
  {
    const Protocol::StateUpdate state = LabSections();
    Client::Session session;
    Tell(session, state);
    Client::Overlays overlays;
    Client::HudAlerts alerts;
    overlays.Ask(session, {}, true);
    AnswerIntegrity(session, state, {Sound(2), Sound(5)});
    overlays.Update(session, 10.0, alerts);
    for (const Client::HudReadoutRow& row : overlays.Readout(session, 10.0, true).value_or(Client::HudReadout{}).rows)
    {
      Assert::IsFalse(row.flashing); // the first reading flashes nothing
    }
    // The next shot widens the hole: the view asks again, and HOLE alone flashes for FLASH_SECONDS.
    Protocol::IntegrityReading hit = Sound(2);
    hit.holeWidthVoxels = 3;
    overlays.Ask(session, {}, false);
    overlays.Ask(session, {}, true);
    AnswerIntegrity(session, state, {hit, Sound(5)});
    overlays.Update(session, 12.0, alerts);
    const Client::HudReadout flashing = overlays.Readout(session, 13.0, true).value_or(Client::HudReadout{});
    Assert::AreEqual(std::size_t{4}, flashing.rows.size());
    Assert::IsTrue(flashing.rows.at(0).flashing);
    Assert::IsFalse(flashing.rows.at(1).flashing);
    Assert::IsFalse(flashing.rows.at(2).flashing);
    Assert::IsFalse(flashing.rows.at(3).flashing);
    overlays.Update(session, 14.5, alerts);
    Assert::IsFalse(
      overlays.Readout(session, 12.0 + Client::Overlays::FLASH_SECONDS, true).value_or(Client::HudReadout{}).rows.at(0).flashing);
  }

  TEST_METHOD(ReadsGroundDimOnEarthWithNoGapAndBadWithOne)
  {
    Protocol::IntegrityReading earth = Sound(2);
    earth.ground = Protocol::GroundKind::Earth;
    earth.canBeUndermined = true;
    Assert::AreEqual(Client::DIM_SRGB, Client::Overlays::IntegrityRows(earth).at(3).srgb);
    earth.gapRunVoxels = 2;
    Assert::AreEqual(Client::BAD_SRGB, Client::Overlays::IntegrityRows(earth).at(3).srgb);
    // On rock a gap undermines nothing.
    Protocol::IntegrityReading rock = Sound(2);
    rock.gapRunVoxels = 2;
    Assert::AreEqual(Client::DIM_SRGB, Client::Overlays::IntegrityRows(rock).at(3).srgb);
  }

  TEST_METHOD(DocksTheReadoutClearOfW2AtTheDefaultFraming)
  {
    // The longest reading the paper test words: a stone floor up, its widest verdict, a gap on earth, and falling.
    const Protocol::StateUpdate state = LabSections();
    Client::Session session;
    Tell(session, state);
    Client::Overlays overlays;
    overlays.Ask(session, {}, true);
    Protocol::IntegrityReading widest = Sound(2);
    widest.holeWidthVoxels = 4;
    widest.hasOpening = true;
    widest.openingFloor = Protocol::OpeningFloor::Stone;
    widest.openingFloorVoxels = 1;
    widest.openingClearVoxels = 4;
    widest.gapRunVoxels = 4;
    widest.ground = Protocol::GroundKind::Earth;
    widest.canBeUndermined = true;
    widest.falling = true;
    AnswerIntegrity(session, state, {widest, Sound(5)});
    const Client::HudModel model{.resources = {},
                                 .objective = {.title = "ITEM 16 OF 16", .detail = "", .detailOff = false},
                                 .mode = {},
                                 .alerts = {},
                                 .selectedEngine = Protocol::NO_ENTITY,
                                 .integrityView = true,
                                 .passabilityView = false,
                                 .readout = overlays.Readout(session, 0.0, true)};
    Assert::AreEqual(std::size_t{5}, model.readout.value_or(Client::HudReadout{}).rows.size());
    for (const float scale : {1.0F, 1.5F, 2.0F})
    {
      for (const auto& [width, height] : {std::pair{1280U, 720U}, std::pair{1920U, 1080U}})
      {
        Client::Hud hud;
        std::vector<Client::ScreenQuad> quads;
        hud.Build(model, width, height, scale, quads);
        Assert::IsTrue(hud.Readout().has_value());
        const Client::PixelRect readout = hud.Readout().value_or(Client::PixelRect{});
        const Client::PixelRect screen{0, 0, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height)};
        Assert::IsTrue(readout.Inside(screen));
        const auto widthPixels = static_cast<float>(width);
        const auto heightPixels = static_cast<float>(height);
        const Client::PixelRect w2 =
          OnScreen(state.structures.front(), DefaultFraming(widthPixels, heightPixels, Client::TOP_BAR_LOGICAL_PIXELS * scale), widthPixels,
                   heightPixels);
        Assert::IsFalse(readout.Overlaps(w2));
        // Every string inside the readout, nothing over anything.
        const std::span<const Client::HudText> texts = hud.Texts();
        for (std::size_t index = 0; index < texts.size(); ++index)
        {
          Assert::IsTrue(texts[index].cell.Inside(texts[index].element));
          for (std::size_t other = index + 1; other < texts.size(); ++other)
          {
            Assert::IsFalse(texts[index].cell.Overlaps(texts[other].cell));
          }
        }
      }
    }
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
    const std::array<Protocol::ChunkSnapshot, 1> distant = {snapshot(2, 2)};
    overlays.Ask(session, distant, true);
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
    for (const Client::Tile& offScreen : {Client::Tile{0, 0}, Client::Tile{20, 20}, Client::Tile{11, 12}})
    {
      Assert::IsTrue(std::ranges::find(visible, offScreen) == visible.end());
    }
    Assert::AreEqual(std::size_t{35}, visible.size());
    // Zoomed out the farthest, at most 36, around the middle.
    const Engine::OrthographicCamera widest({.elevationRadians = DirectX::XMConvertToRadians(Client::ELEVATION_DEGREES),
                                             .azimuthRadians = DirectX::XMConvertToRadians(Client::AZIMUTH_DEGREES),
                                             .viewHeightMeters = Client::MAX_VIEW_HEIGHT_METERS,
                                             .minViewHeightMeters = Client::MIN_VIEW_HEIGHT_METERS,
                                             .maxViewHeightMeters = Client::MAX_VIEW_HEIGHT_METERS,
                                             .rotationSeconds = 0.0F,
                                             .targetMeters = {380.5F, 9.0F, 180.0F}});
    DirectX::XMFLOAT4X4 farProjection{};
    DirectX::XMStoreFloat4x4(&farProjection, widest.ViewProjection(1280.0F / 720.0F));
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

  TEST_METHOD(LaysThirtyStructuresLinesOnTheScreenWholeOrAsNamesAndNoneOverAnother)
  {
    // Input 23, as the owner amended it for M4-I6: a line for each structure, laid nearest the screen's middle first, as
    // its name alone where it would cover another; the selected always whole; all between the top bar and the deck.
    const Protocol::StateUpdate state = ThirtySections();
    Client::Session session;
    Tell(session, state);
    Client::Overlays overlays;
    overlays.Ask(session, {}, true);
    std::vector<Protocol::IntegrityReading> readings;
    for (const Protocol::StructureView& structure : state.structures)
    {
      Protocol::IntegrityReading reading = Sound(structure.id);
      reading.holeWidthVoxels = 3;
      readings.push_back(reading);
    }
    AnswerIntegrity(session, state, readings);
    constexpr Protocol::EntityId SELECTED = 115; // in the grid's middle
    for (const float scale : {1.0F, 1.5F})
    {
      for (const auto& [width, height] : {std::pair{1280U, 720U}, std::pair{1920U, 1080U}})
      {
        Client::HudModel model{};
        model.deck = true;
        model.worldLabels = Client::Overlays::WorldLabels(session, Overhead(), width, height, 60.0F, SELECTED, true);
        Assert::AreEqual(std::size_t{30}, model.worldLabels.size());
        Assert::IsTrue(model.worldLabels.front().selected);
        Client::Hud hud;
        std::vector<Client::ScreenQuad> quads;
        hud.Build(model, width, height, scale, quads);
        const std::span<const Client::HudLaidLabel> laid = hud.Labels();
        Assert::IsTrue(laid.size() >= 8);
        const Client::HudDeck deck = hud.Deck().value_or(Client::HudDeck{});
        const auto top = static_cast<std::int32_t>(std::lround(Client::TOP_BAR_LOGICAL_PIXELS * scale));
        const Client::PixelRect band{0, top, static_cast<std::int32_t>(width), deck.deck.yPixels - top};
        std::size_t whole = 0;
        for (std::size_t index = 0; index < laid.size(); ++index)
        {
          Assert::IsTrue(laid[index].rect.Inside(band));
          for (std::size_t other = index + 1; other < laid.size(); ++other)
          {
            Assert::IsFalse(laid[index].rect.Overlaps(laid[other].rect));
          }
          const auto label = std::ranges::find_if(model.worldLabels, [&laid, index](const Client::HudWorldLabel& _label)
                                                  { return _label.line == laid[index].text || _label.name == laid[index].text; });
          Assert::IsTrue(label != model.worldLabels.end());
          whole += label->line == laid[index].text ? 1 : 0;
        }
        Assert::IsTrue(whole >= 1);
        Assert::AreEqual(std::string("W15 · WEAK HOLE 3/5 M · Locked · 10 s"), laid.front().text);
      }
    }
  }

  TEST_METHOD(NamesStructuresAloneAboveOneHundredFiftyMetresButTheSelected)
  {
    const Protocol::StateUpdate state = ThirtySections();
    Client::Session session;
    Tell(session, state);
    const std::vector<Client::HudWorldLabel> labels = Client::Overlays::WorldLabels(session, Overhead(), 1280, 720, 160.0F, 115, true);
    for (const Client::HudWorldLabel& label : labels)
    {
      Assert::AreEqual(label.selected, label.line != label.name);
    }
    // Off with the view.
    Assert::IsTrue(Client::Overlays::WorldLabels(session, Overhead(), 1280, 720, 60.0F, 115, false).empty());
  }

  TEST_METHOD(LeavesTheLockOffTheReadoutOutsideLayoutA)
  {
    // The lab's readout keeps the paper test's rows: nobody repairs there, so the lock would only be noise.
    const Protocol::StateUpdate state = ThirtySections();
    Client::Session session;
    Tell(session, state);
    Client::Overlays overlays;
    overlays.Ask(session, {}, true);
    AnswerIntegrity(session, state, {Sound(115)});
    overlays.Select(115);
    const Client::HudReadout readout = overlays.Readout(session, 1.0, true).value_or(Client::HudReadout{});
    Assert::IsFalse(readout.rows.empty());
    Assert::IsTrue(std::ranges::none_of(readout.rows, [](const Client::HudReadoutRow& _row) { return _row.key == "REPAIR"; }));
  }

  TEST_METHOD(ReadsTheLockOnTheReadoutWithoutFlashingAndFocusesTheSelectedFirstInLayoutA)
  {
    const Protocol::StateUpdate state = ThirtySections();
    Client::Session session;
    Tell(session, state);
    Client::Overlays overlays;
    overlays.SelectedFirst(true);
    overlays.Ask(session, {}, true);
    AnswerIntegrity(session, state, {Sound(115), Sound(101)});
    // Nothing by default in layout A, where W2's lab is not.
    Assert::AreEqual(Protocol::NO_ENTITY, overlays.Focused(session));
    overlays.Hover(101);
    overlays.Select(115);
    Assert::AreEqual(Protocol::EntityId{115}, overlays.Focused(session));
    const Client::HudReadout readout = overlays.Readout(session, 1.0, true).value_or(Client::HudReadout{});
    Assert::AreEqual(std::string("REPAIR"), readout.rows.back().key);
    Assert::AreEqual(std::string("Locked · 10 s"), readout.rows.back().value);
    Assert::IsFalse(readout.rows.back().flashing);
    Assert::AreEqual(std::string("Ready"), Client::Overlays::RepairValue(0));
    Assert::AreEqual(std::string("Locked · 1 s"), Client::Overlays::RepairValue(1));
    // The weak reading, nearer its fall; none while neither falls within the section.
    Protocol::IntegrityReading reading = Sound(115);
    reading.holeWidthVoxels = 2;
    reading.gapRunVoxels = 3;
    Assert::AreEqual(std::string("GAP 3/5 M"), Client::Overlays::WeakValue(reading).value_or(""));
    reading.gapFallRunVoxels = 0;
    Assert::AreEqual(std::string("HOLE 2/5 M"), Client::Overlays::WeakValue(reading).value_or(""));
    reading.holeFallWidthVoxels = 0;
    Assert::IsFalse(Client::Overlays::WeakValue(reading).has_value());
  }
};

} // namespace ClientTests
