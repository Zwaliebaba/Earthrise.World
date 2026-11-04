#include "pch.h"

#include "Catalog.h"
#include "FireTally.h"
#include "Look.h"
#include "ModelTurn.h"
#include "WorldModels.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The war as the client draws it in voxel models (M4-H1), with no GPU: which model each unit and engine is, where it stands
// and which way it faces, turned to any angle (ADR-030; M5-G3), and the open gates, whose own voxels the world view leaves
// out.

namespace ClientTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr float CLOSE_METERS = 0.0001F;
constexpr float CLOSE_RADIANS = 0.001F;
constexpr float DEGREE = std::numbers::pi_v<float> / 180.0F;
constexpr std::int32_t GROUND = 9;
constexpr std::int32_t MACHINES_SIDE = 3; // the server's machinesWidthMeters

Protocol::FixedPosition AtMeters(float _x, float _y, float _z)
{
  const auto raw = [](float _meters)
  { return Core::Fixed::FromRaw(static_cast<std::int32_t>(std::lround(_meters * Core::Fixed::ONE_RAW))); };
  return {.xMeters = raw(_x), .yMeters = raw(_y), .zMeters = raw(_z)};
}

/// An update of two players, the attacker and the defender, at _tick.
Protocol::StateUpdate Update(std::uint32_t _tick)
{
  Protocol::StateUpdate state{};
  state.tick = _tick;
  state.player = ATTACKER;
  state.players = {
    {.player = ATTACKER, .side = Protocol::Side::Attacker, .stoneHundredthsCubicMeters = 0, .timberHundredthsCubicMeters = 0},
    {.player = DEFENDER, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = 0, .timberHundredthsCubicMeters = 0}};
  return state;
}

Protocol::UnitView Figure(Protocol::EntityId _id, Protocol::UnitKind _kind, std::uint8_t _owner, float _x, float _z)
{
  return {.id = _id,
          .squad = _id + 100,
          .owner = _owner,
          .kind = _kind,
          .position = AtMeters(_x, static_cast<float>(GROUND), _z),
          .cellVoxels = {.xVoxels = static_cast<std::int32_t>(std::floor(_x)),
                         .yVoxels = GROUND,
                         .zVoxels = static_cast<std::int32_t>(std::floor(_z))},
          .hitPoints = 1,
          .maxHitPoints = 1};
}

Protocol::EngineView EngineAt(Protocol::EntityId _id, Protocol::EngineKind _kind, Protocol::EngineState _state, float _x, float _z,
                              std::uint32_t _turns)
{
  return {.id = _id,
          .owner = ATTACKER,
          .kind = _kind,
          .cellVoxels = {.xVoxels = static_cast<std::int32_t>(std::floor(_x)),
                         .yVoxels = GROUND,
                         .zVoxels = static_cast<std::int32_t>(std::floor(_z))},
          .quarterTurns = _turns,
          .reloadTicks = 0,
          .state = _state,
          .squad = _id + 100,
          .stateTicksLeft = 0,
          .position = AtMeters(_x, static_cast<float>(GROUND), _z),
          .hitPoints = 1,
          .maxHitPoints = 1,
          .footprintVoxels = MACHINES_SIDE};
}

struct Placed
{
  std::vector<Client::ModelPlacement> placements;
  std::vector<Protocol::VoxelPosition> hidden;
};

Placed PlaceWith(Client::WorldModels& _models, const Protocol::StateUpdate& _previous, const Protocol::StateUpdate& _last,
                 const Client::FireTally& _tally, double _tickFraction)
{
  Placed placed;
  _models.Place(_previous, _last, _tally, _tickFraction, placed.placements, placed.hidden);
  return placed;
}

Placed PlaceOnce(const Protocol::StateUpdate& _last)
{
  Client::WorldModels models;
  return PlaceWith(models, Protocol::StateUpdate{}, _last, Client::FireTally{}, 0.0);
}

/// The box that bounds a model's footprint after its turn, in meters along x and z.
std::pair<float, float> TurnedFootprint(const Client::ModelPlacement& _placement)
{
  const Content::ModelView* model = Content::FindModel(_placement.modelId);
  Assert::IsNotNull(model);
  const float edge = static_cast<float>(model->grid.voxelEdgeMillimeters) / 1000.0F;
  return Client::TurnedExtent(edge * static_cast<float>(model->grid.widthVoxels), edge * static_cast<float>(model->grid.depthVoxels),
                              _placement.yawRadians);
}

/// A run of updates a tick apart, each drawn at its own tick, so that a model turns by a tick's time from one to the next.
class Run
{
public:
  explicit Run(std::uint32_t _tick)
    : m_last(Update(_tick))
  {
  }

  /// The update to come, a tick after the last, with what _fill puts in it; then its frame's placements.
  template <typename Fill> Placed Next(Fill&& _fill)
  {
    Protocol::StateUpdate next = Update(m_last.tick + 1);
    std::forward<Fill>(_fill)(next);
    const Protocol::StateUpdate previous = std::exchange(m_last, std::move(next));
    m_tally.Observe(m_last);
    return PlaceWith(m_models, previous, m_last, m_tally, 1.0);
  }

  [[nodiscard]] Client::WorldModels& Models() noexcept
  {
    return m_models;
  }

private:
  Client::WorldModels m_models;
  Client::FireTally m_tally;
  Protocol::StateUpdate m_last;
};

/// How far _yaw is from _expected the shorter way, in radians.
float Off(float _expected, float _yaw)
{
  return std::abs(Client::ShorterTurn(_expected, _yaw));
}

/// The middle of a placement's footprint, along x and z.
std::pair<float, float> MiddleOf(const Client::ModelPlacement& _placement)
{
  const auto [width, depth] = TurnedFootprint(_placement);
  return {_placement.xMeters + (0.5F * width), _placement.zMeters + (0.5F * depth)};
}

/// A template cell (_x, _z) turned as Simulation::World stamps a template: one turn takes +z to +x, the footprint's
/// minimum corner kept at its origin.
std::pair<std::int32_t, std::int32_t> Turned(std::int32_t _x, std::int32_t _z, std::int32_t _width, std::int32_t _depth,
                                             std::uint32_t _turns)
{
  switch (_turns % 4)
  {
  case 1:
    return {_z, _width - 1 - _x};
  case 2:
    return {_width - 1 - _x, _depth - 1 - _z};
  case 3:
    return {_depth - 1 - _z, _x};
  default:
    return {_x, _z};
  }
}

/// _templateId at _origin turned _turns, its gate open: the gate's template cells x _gateFromX to _gateToX, the first
/// _gateHeight layers, at z _gateZ, turned into the world as the server sends them.
Protocol::StructureView OpenGate(std::string_view _templateId, Protocol::VoxelPosition _origin, std::uint32_t _turns,
                                 std::int32_t _gateFromX, std::int32_t _gateToX, std::int32_t _gateHeight, std::int32_t _gateZ)
{
  const Content::TemplateView* shape = Content::FindTemplate(_templateId);
  Assert::IsNotNull(shape);
  const auto width = static_cast<std::int32_t>(shape->grid.widthVoxels);
  const auto depth = static_cast<std::int32_t>(shape->grid.depthVoxels);
  const bool across = _turns % 2 == 1;
  Protocol::StructureView view{.id = 7,
                               .owner = DEFENDER,
                               .templateId = std::string(_templateId),
                               .originVoxels = _origin,
                               .quarterTurns = _turns,
                               .extentVoxels = {.xVoxels = across ? depth : width,
                                                .yVoxels = static_cast<std::int32_t>(shape->grid.heightVoxels),
                                                .zVoxels = across ? width : depth},
                               .name = "GH"};
  view.gateOpen = true;
  for (std::int32_t y = 0; y < _gateHeight; ++y)
  {
    for (std::int32_t x = _gateFromX; x <= _gateToX; ++x)
    {
      const auto [turnedX, turnedZ] = Turned(x, _gateZ, width, depth, _turns);
      view.gateVoxels.push_back(
        {.xVoxels = _origin.xVoxels + turnedX, .yVoxels = _origin.yVoxels + y, .zVoxels = _origin.zVoxels + turnedZ});
    }
  }
  return view;
}

bool SameVoxels(std::vector<Protocol::VoxelPosition> _a, std::vector<Protocol::VoxelPosition> _b)
{
  const auto order = [](const Protocol::VoxelPosition& _left, const Protocol::VoxelPosition& _right)
  { return std::array{_left.xVoxels, _left.yVoxels, _left.zVoxels} < std::array{_right.xVoxels, _right.yVoxels, _right.zVoxels}; };
  std::ranges::sort(_a, order);
  std::ranges::sort(_b, order);
  return _a == _b;
}

} // namespace

TEST_CLASS(WorldModelsTests)
{
public:
  TEST_METHOD(EachKindGivesItsModel)
  {
    const std::array<std::pair<Protocol::UnitKind, std::string_view>, 8> kinds = {{{Protocol::UnitKind::Laborer, "U1"},
                                                                                   {Protocol::UnitKind::Spearman, "U2"},
                                                                                   {Protocol::UnitKind::Archer, "U3"},
                                                                                   {Protocol::UnitKind::Outrider, "U4"},
                                                                                   {Protocol::UnitKind::ManAtArms, "U5"},
                                                                                   {Protocol::UnitKind::Sapper, "S2"},
                                                                                   {Protocol::UnitKind::Ram, "S1"},
                                                                                   {Protocol::UnitKind::Trebuchet, "S3p"}}};
    Protocol::StateUpdate state = Update(10);
    Protocol::EntityId id = 0;
    for (const auto& [kind, modelId] : kinds)
    {
      Assert::AreEqual(std::string(modelId), std::string(Client::WorldModels::UnitModel(kind)));
      Assert::IsNotNull(Content::FindModel(modelId));
      ++id;
      state.units.push_back(Figure(id, kind, ATTACKER, 10.0F + static_cast<float>(id), 20.0F));
    }
    // The six figures are drawn as their models, and the two engines' bodies not at all: their engines draw them.
    const Placed placed = PlaceOnce(state);
    Assert::AreEqual(std::size_t{6}, placed.placements.size());
    for (std::size_t index = 0; index < placed.placements.size(); ++index)
    {
      Assert::AreEqual(std::string(kinds.at(index).second), std::string(placed.placements.at(index).modelId));
      Assert::AreEqual(Client::ATTACKER_COLOR_INDEX, placed.placements.at(index).sideColorIndex);
    }
  }

  TEST_METHOD(AFigureIsDrawnBetweenTicksOnItsFeetInItsSidesColor)
  {
    Protocol::StateUpdate before = Update(10);
    before.units.push_back(Figure(1, Protocol::UnitKind::Spearman, DEFENDER, 10.5F, 20.5F));
    Protocol::StateUpdate after = Update(11);
    after.units.push_back(Figure(1, Protocol::UnitKind::Spearman, DEFENDER, 10.5F, 20.6F));
    Client::WorldModels models;
    const Placed placed = PlaceWith(models, before, after, Client::FireTally{}, 0.5);
    Assert::AreEqual(std::size_t{1}, placed.placements.size());
    const Client::ModelPlacement& spearman = placed.placements.front();
    const auto [x, z] = MiddleOf(spearman);
    Assert::AreEqual(10.5F, x, CLOSE_METERS);
    Assert::AreEqual(20.55F, z, CLOSE_METERS);
    Assert::AreEqual(static_cast<float>(GROUND), spearman.yMeters, CLOSE_METERS);
    Assert::AreEqual(Client::DEFENDER_COLOR_INDEX, spearman.sideColorIndex);
  }

  TEST_METHOD(AFigureTurnsToFaceItsTravelAtItsRateAndKeepsItStandingStill)
  {
    // A figure turns at 360° a second, 18° a tick (Look.h).
    Run run(10);
    float x = 10.5F;
    float z = 20.5F;
    const auto step = [&run, &x, &z](float _dx, float _dz)
    {
      x += _dx;
      z += _dz;
      return run
        .Next([x, z](Protocol::StateUpdate& _state) { _state.units.push_back(Figure(1, Protocol::UnitKind::Spearman, ATTACKER, x, z)); })
        .placements.front()
        .yawRadians;
    };
    Assert::AreEqual(0.0F, step(0.0F, 0.0F), CLOSE_RADIANS);            // north until it first moves
    Assert::AreEqual(18.0F * DEGREE, step(0.07F, 0.0F), CLOSE_RADIANS); // east: a tick turns it 18°
    for (int tick = 0; tick < 3; ++tick)
    {
      step(0.07F, 0.0F);
    }
    Assert::AreEqual(90.0F * DEGREE, step(0.07F, 0.0F), CLOSE_RADIANS); // and four more, the whole way
    for (int tick = 0; tick < 3; ++tick)
    {
      Assert::AreEqual(90.0F * DEGREE, step(0.0F, 0.0F), CLOSE_RADIANS); // standing, it keeps it
    }
    // Off at 350°, it turns back the shorter way, through 0.
    const float northByWest = -10.0F * DEGREE;
    float yaw = 0.0F;
    for (int tick = 0; tick < 10; ++tick)
    {
      yaw = step(0.07F * std::sin(northByWest), 0.07F * std::cos(northByWest));
    }
    Assert::AreEqual(350.0F * DEGREE, yaw, CLOSE_RADIANS);
    step(0.0F, 0.0F);
    const float northByEast = 10.0F * DEGREE;
    Assert::AreEqual(8.0F * DEGREE, step(0.07F * std::sin(northByEast), 0.07F * std::cos(northByEast)), CLOSE_RADIANS);
  }

  TEST_METHOD(AFiguresPathOfStraightAndDiagonalStepsKeepsANearlySteadyHeading)
  {
    // The worst path for a heading: a step north, then one north-east, cell after cell (ADR-015), walked at 7 cm a tick.
    // Its bearing over the whole is atan(1/2), 26.6°. A figure facing each tick's step would swing 45° at every cell;
    // the bearing of its last metre keeps it within 13° (ADR-030, decision 5).
    std::vector<std::pair<float, float>> corners{{10.5F, 20.5F}};
    for (int cell = 0; cell < 12; ++cell)
    {
      const auto [x, z] = corners.back();
      corners.emplace_back(cell % 2 == 0 ? x : x + 1.0F, z + 1.0F);
    }
    Run run(10);
    std::pair<float, float> at = corners.front();
    std::size_t next = 1;
    std::vector<float> yaws;
    while (next < corners.size())
    {
      float left = 0.07F;
      while (next < corners.size() && left > 0.0F)
      {
        const float dx = corners.at(next).first - at.first;
        const float dz = corners.at(next).second - at.second;
        const float distance = std::hypot(dx, dz);
        const float taken = std::min(left, distance);
        at = {at.first + (dx * taken / distance), at.second + (dz * taken / distance)};
        left -= taken;
        next += taken == distance ? 1 : 0;
      }
      yaws.push_back(run
                       .Next([at](Protocol::StateUpdate& _state)
                             { _state.units.push_back(Figure(1, Protocol::UnitKind::Spearman, ATTACKER, at.first, at.second)); })
                       .placements.front()
                       .yawRadians);
    }
    const float bearing = std::atan2(1.0F, 2.0F);
    for (std::size_t tick = yaws.size() / 3; tick < yaws.size(); ++tick)
    {
      Assert::IsTrue(Off(bearing, yaws.at(tick)) <= 13.0F * DEGREE, std::format(L"tick {}: {}°", tick, yaws.at(tick) / DEGREE).c_str());
    }
  }

  TEST_METHOD(AGarrisonedSquadsFiguresAreNotDrawn)
  {
    Protocol::StateUpdate state = Update(10);
    state.units.push_back(Figure(1, Protocol::UnitKind::Archer, DEFENDER, 10.5F, 20.5F));
    state.squads.push_back(
      {.id = 101, .owner = DEFENDER, .units = {1}, .hasGoal = false, .goalVoxels = {}, .shortOfGoal = false, .garrisonedIn = 7});
    Assert::IsTrue(PlaceOnce(state).placements.empty());
  }

  TEST_METHOD(TheRamIsS1OnItsFootprintsMiddleTurningTowardsItsTravel)
  {
    // Its body's position is the middle of its first cell, and machines stand on 3 x 3 cells from there.
    Protocol::StateUpdate before = Update(10);
    before.engines.push_back(EngineAt(5, Protocol::EngineKind::Ram, Protocol::EngineState::Packed, 10.5F, 20.5F, 0));
    Client::WorldModels models;
    Placed placed = PlaceWith(models, Protocol::StateUpdate{}, before, Client::FireTally{}, 0.0);
    Assert::AreEqual(std::size_t{1}, placed.placements.size());
    Assert::AreEqual(std::string("S1"), std::string(placed.placements.front().modelId));
    Assert::AreEqual(0.0F, placed.placements.front().yawRadians);
    const auto [x, z] = MiddleOf(placed.placements.front());
    Assert::AreEqual(11.5F, x, CLOSE_METERS);
    Assert::AreEqual(21.5F, z, CLOSE_METERS);
    // Its box, where a click selects it, is the model's as placed.
    Assert::AreEqual(std::size_t{1}, models.EngineBoxes().size());
    const Client::EngineBox& box = models.EngineBoxes().front();
    Assert::AreEqual(placed.placements.front().xMeters, box.xMeters);
    Assert::AreEqual(TurnedFootprint(placed.placements.front()).second, box.depthMeters);
    // Stepping east, it turns towards east, its front +z, at 90° a second: 9° over the two ticks since it was drawn.
    Protocol::StateUpdate after = Update(11);
    after.engines.push_back(EngineAt(5, Protocol::EngineKind::Ram, Protocol::EngineState::Packed, 10.535F, 20.5F, 0));
    placed = PlaceWith(models, before, after, Client::FireTally{}, 1.0);
    Assert::AreEqual(9.0F * DEGREE, placed.placements.front().yawRadians, CLOSE_RADIANS);
    Assert::AreEqual(11.535F, MiddleOf(placed.placements.front()).first, CLOSE_METERS);
  }

  TEST_METHOD(TheRamDrivesTheDiagonalFacingAlongItAndItsBoxBoundsItTurned)
  {
    // Ordered up the screen from the default camera, it drives north-east (GameDesign.md, the first run of M5's games).
    Run run(10);
    float x = 10.5F;
    float z = 20.5F;
    Placed placed;
    for (int tick = 0; tick < 40; ++tick)
    {
      placed = run.Next([x, z](Protocol::StateUpdate& _state)
                        { _state.engines.push_back(EngineAt(5, Protocol::EngineKind::Ram, Protocol::EngineState::Packed, x, z, 0)); });
      x += 0.025F;
      z += 0.025F;
    }
    const Client::ModelPlacement& ram = placed.placements.front();
    Assert::AreEqual(45.0F * DEGREE, ram.yawRadians, CLOSE_RADIANS);
    // Its box, where a click selects it, bounds the model turned 45°, about the footprint's middle (ADR-030, decision 7).
    const Client::EngineBox& box = run.Models().EngineBoxes().front();
    const auto [width, depth] = TurnedFootprint(ram);
    Assert::AreEqual(width, box.widthMeters, CLOSE_METERS);
    Assert::AreEqual(depth, box.depthMeters, CLOSE_METERS);
    Assert::AreEqual(ram.xMeters, box.xMeters);
    Assert::AreEqual(ram.zMeters, box.zMeters);
    Assert::AreEqual(x - 0.025F + 1.0F, MiddleOf(ram).first, 0.001F);
  }

  TEST_METHOD(TheTrebuchetIsTheWagonPackedCockedDeployedAndReleasedAfterAThrow)
  {
    Client::WorldModels models;
    Client::FireTally tally;
    const auto pose = [&models, &tally](Protocol::EngineState _state, std::uint32_t _tick, std::uint32_t _reloadTicks)
    {
      Protocol::StateUpdate state = Update(_tick);
      state.engines.push_back(EngineAt(5, Protocol::EngineKind::Trebuchet, _state, 30.5F, 40.5F, 3));
      state.engines.front().reloadTicks = _reloadTicks;
      tally.Observe(state);
      return std::string(PlaceWith(models, Protocol::StateUpdate{}, state, tally, 0.0).placements.front().modelId);
    };
    Assert::AreEqual(std::string("S3p"), pose(Protocol::EngineState::Packed, 10, 0));
    Assert::AreEqual(std::string("S3p"), pose(Protocol::EngineState::Deploying, 11, 0));
    Assert::AreEqual(std::string("S3c"), pose(Protocol::EngineState::Deployed, 12, 0));
    // It fires in the tick its reload rises, and is drawn released for RELEASED_POSE_TICKS.
    Assert::AreEqual(std::string("S3"), pose(Protocol::EngineState::Deployed, 13, 200));
    Assert::AreEqual(std::string("S3"), pose(Protocol::EngineState::Deployed, 13 + Client::RELEASED_POSE_TICKS - 1, 180));
    Assert::AreEqual(std::string("S3c"), pose(Protocol::EngineState::Deployed, 13 + Client::RELEASED_POSE_TICKS, 179));
    Assert::AreEqual(std::string("S3c"), pose(Protocol::EngineState::Packing, 40, 150));
  }

  TEST_METHOD(TheWagonSitsAtTheFramesPlacementAndTurnsToItsTravel)
  {
    // Not stepped, the wagon stands as the frame would at the engine's turn; S3 and S3p share their footprint's
    // minimum corner at turn 0 (Look.h).
    Protocol::StateUpdate packed = Update(10);
    packed.engines.push_back(EngineAt(5, Protocol::EngineKind::Trebuchet, Protocol::EngineState::Packed, 30.5F, 40.5F, 0));
    Protocol::StateUpdate deployed = Update(10);
    deployed.engines.push_back(EngineAt(5, Protocol::EngineKind::Trebuchet, Protocol::EngineState::Deployed, 30.5F, 40.5F, 0));
    Client::FireTally tally;
    tally.Observe(deployed);
    deployed.tick = 11;
    deployed.engines.front().reloadTicks = 200;
    tally.Observe(deployed);
    const Client::ModelPlacement wagon = PlaceOnce(packed).placements.front();
    Client::WorldModels models;
    const Client::ModelPlacement frame = PlaceWith(models, Protocol::StateUpdate{}, deployed, tally, 0.0).placements.front();
    Assert::AreEqual(std::string("S3"), std::string(frame.modelId));
    Assert::AreEqual(frame.xMeters, wagon.xMeters, CLOSE_METERS);
    Assert::AreEqual(frame.zMeters, wagon.zMeters, CLOSE_METERS);
    Assert::AreEqual(0.0F, wagon.yawRadians);
    // Moving north, its draw pole, the baked grid's -x, comes to lead: a quarter turn takes -x to +z, at 90° a second.
    Run run(10);
    float z = 40.5F;
    float yaw = 0.0F;
    for (int tick = 0; tick < 25; ++tick)
    {
      yaw = run
              .Next([z](Protocol::StateUpdate& _state)
                    { _state.engines.push_back(EngineAt(5, Protocol::EngineKind::Trebuchet, Protocol::EngineState::Packed, 30.5F, z, 0)); })
              .placements.front()
              .yawRadians;
      z += 0.035F;
    }
    Assert::AreEqual(90.0F * DEGREE, yaw, CLOSE_RADIANS);
  }

  TEST_METHOD(ADeployedTrebuchetFacesItsDeployUntilItsFirstStoneAndThenItsLatestStone)
  {
    // S3 throws along its own +x (Orders::TurnsAlong), so facing a bearing it is turned that bearing less a quarter.
    Run run(10);
    std::vector<Protocol::ShotView> shots;
    const auto yaw = [&run, &shots]()
    {
      return run
        .Next(
          [&shots](Protocol::StateUpdate& _state)
          {
            _state.engines.push_back(EngineAt(5, Protocol::EngineKind::Trebuchet, Protocol::EngineState::Deployed, 30.5F, 40.5F, 3));
            _state.shots = shots;
          })
        .placements.front()
        .yawRadians;
    };
    Assert::AreEqual(270.0F * DEGREE, yaw(), CLOSE_RADIANS); // its deploy's turn
    // A stone first seen east of its cell: facing east, turned 0, at once.
    shots.push_back(
      {.id = 90, .engine = 5, .position = AtMeters(35.5F, 20.0F, 40.5F), .kind = Protocol::ShotKind::Stone, .owner = ATTACKER});
    Assert::AreEqual(0.0F, yaw(), CLOSE_RADIANS);
    // That stone flies on, and a second leaves north-east: the second is its aim.
    shots.front().position = AtMeters(38.5F, 25.0F, 40.5F);
    shots.push_back(
      {.id = 91, .engine = 5, .position = AtMeters(32.5F, 15.0F, 42.5F), .kind = Protocol::ShotKind::Stone, .owner = ATTACKER});
    Assert::AreEqual(315.0F * DEGREE, yaw(), CLOSE_RADIANS);
    // An arrow, or a stone of another engine, is no aim of its.
    shots = {{.id = 92, .engine = 5, .position = AtMeters(30.5F, 15.0F, 30.5F), .kind = Protocol::ShotKind::Arrow, .owner = ATTACKER},
             {.id = 93, .engine = 6, .position = AtMeters(30.5F, 15.0F, 30.5F), .kind = Protocol::ShotKind::Stone, .owner = ATTACKER}};
    Assert::AreEqual(315.0F * DEGREE, yaw(), CLOSE_RADIANS);
  }

  TEST_METHOD(AnOpenGatesInstanceHidesExactlyItsGateVoxels)
  {
    // T5's gate: template cells x 3 to 5, four high, at z 0 (Manifest.json's attach entry for G1).
    for (std::uint32_t turns = 0; turns < 4; ++turns)
    {
      Protocol::StateUpdate state = Update(10);
      const Protocol::VoxelPosition origin{.xVoxels = 100, .yVoxels = GROUND, .zVoxels = 200};
      state.structures.push_back(OpenGate("T5", origin, turns, 3, 5, 4, 0));
      const Placed placed = PlaceOnce(state);
      Assert::IsTrue(SameVoxels(state.structures.front().gateVoxels, placed.hidden));
      // G1, the raised gate's bottom edge, lies along the gate's whole width at its top, within its thickness.
      Assert::AreEqual(std::size_t{1}, placed.placements.size());
      const Client::ModelPlacement& raised = placed.placements.front();
      Assert::AreEqual(std::string("G1"), std::string(raised.modelId));
      Assert::AreEqual(turns, Client::WholeQuarterTurns(raised.yawRadians).value_or(9));
      Assert::AreEqual(static_cast<float>(GROUND) + 3.5F, raised.yMeters, CLOSE_METERS);
      std::int32_t minX = std::numeric_limits<std::int32_t>::max();
      std::int32_t maxX = std::numeric_limits<std::int32_t>::min();
      std::int32_t minZ = minX;
      std::int32_t maxZ = maxX;
      for (const Protocol::VoxelPosition& voxel : placed.hidden)
      {
        minX = std::min(minX, voxel.xVoxels);
        maxX = std::max(maxX, voxel.xVoxels + 1);
        minZ = std::min(minZ, voxel.zVoxels);
        maxZ = std::max(maxZ, voxel.zVoxels + 1);
      }
      const auto [width, depth] = TurnedFootprint(raised);
      const bool alongX = maxX - minX > maxZ - minZ;
      if (alongX)
      {
        Assert::AreEqual(static_cast<float>(minX), raised.xMeters, CLOSE_METERS);
        Assert::AreEqual(static_cast<float>(maxX), raised.xMeters + width, CLOSE_METERS);
        Assert::IsTrue(raised.zMeters >= static_cast<float>(minZ) - CLOSE_METERS &&
                       raised.zMeters + depth <= static_cast<float>(maxZ) + CLOSE_METERS);
      }
      else
      {
        Assert::AreEqual(static_cast<float>(minZ), raised.zMeters, CLOSE_METERS);
        Assert::AreEqual(static_cast<float>(maxZ), raised.zMeters + depth, CLOSE_METERS);
        Assert::IsTrue(raised.xMeters >= static_cast<float>(minX) - CLOSE_METERS &&
                       raised.xMeters + width <= static_cast<float>(maxX) + CLOSE_METERS);
      }
    }
  }

  TEST_METHOD(AShutGateHidesNothingAndThePalisadesOpensInward)
  {
    Protocol::StateUpdate state = Update(10);
    const Protocol::VoxelPosition origin{.xVoxels = 100, .yVoxels = GROUND, .zVoxels = 200};
    state.structures.push_back(OpenGate("T5", origin, 0, 3, 5, 4, 0));
    state.structures.front().gateOpen = false;
    state.structures.front().gateVoxels.clear();
    Placed placed = PlaceOnce(state);
    Assert::IsTrue(placed.placements.empty() && placed.hidden.empty());
    // T7's gate, x 2 to 5, three high, in the trimmed grid's one row: G2's leaves stand across it and swing 1 m behind it.
    state.structures = {OpenGate("T7", origin, 0, 2, 5, 3, 0)};
    placed = PlaceOnce(state);
    Assert::AreEqual(std::size_t{12}, placed.hidden.size());
    const Client::ModelPlacement& open = placed.placements.front();
    Assert::AreEqual(std::string("G2"), std::string(open.modelId));
    Assert::AreEqual(102.0F, open.xMeters, CLOSE_METERS);
    Assert::AreEqual(199.0F, open.zMeters, CLOSE_METERS);
    Assert::AreEqual(static_cast<float>(GROUND), open.yMeters, CLOSE_METERS);
  }

  TEST_METHOD(AStoneIsDrawnBetweenTicksAndNeverSnaps)
  {
    Protocol::StateUpdate before = Update(10);
    before.shots.push_back(
      {.id = 9, .engine = 5, .position = AtMeters(10.0F, 20.0F, 30.0F), .kind = Protocol::ShotKind::Stone, .owner = ATTACKER});
    before.shots.push_back(
      {.id = 10, .engine = 6, .position = AtMeters(0.0F, 20.0F, 0.0F), .kind = Protocol::ShotKind::Arrow, .owner = ATTACKER});
    Protocol::StateUpdate after = Update(11);
    after.shots = before.shots;
    after.shots.front().position = AtMeters(22.0F, 20.0F, 30.0F);
    Client::WorldModels models;
    const Placed placed = PlaceWith(models, before, after, Client::FireTally{}, 0.5);
    // The arrow is a line model, which the line pass draws (M4-H2).
    Assert::AreEqual(std::size_t{1}, placed.placements.size());
    Assert::AreEqual(std::string("X1"), std::string(placed.placements.front().modelId));
    Assert::AreEqual(16.0F, MiddleOf(placed.placements.front()).first, CLOSE_METERS);
  }
};

} // namespace ClientTests
