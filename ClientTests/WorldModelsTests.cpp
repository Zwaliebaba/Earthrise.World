#include "pch.h"

#include "Catalog.h"
#include "FireTally.h"
#include "Look.h"
#include "WorldModels.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The war as the client draws it in voxel models (M4-H1), with no GPU: which model each unit and engine is, where it stands
// and which way it faces, and the open gates, whose own voxels the world view leaves out.

namespace ClientTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr float CLOSE_METERS = 0.0001F;
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

/// A model's footprint after its turn, in meters along x and z.
std::pair<float, float> TurnedFootprint(const Client::ModelPlacement& _placement)
{
  const Content::ModelView* model = Content::FindModel(_placement.modelId);
  Assert::IsNotNull(model);
  const float edge = static_cast<float>(model->grid.voxelEdgeMillimeters) / 1000.0F;
  const float width = edge * static_cast<float>(model->grid.widthVoxels);
  const float depth = edge * static_cast<float>(model->grid.depthVoxels);
  return _placement.quarterTurns % 2 == 1 ? std::pair{depth, width} : std::pair{width, depth};
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

  TEST_METHOD(AFigureFacesItsLastStepAndKeepsItStandingStill)
  {
    Client::WorldModels models;
    const auto facing = [&models](float _fromX, float _fromZ, float _toX, float _toZ)
    {
      Protocol::StateUpdate before = Update(10);
      before.units.push_back(Figure(1, Protocol::UnitKind::Spearman, ATTACKER, _fromX, _fromZ));
      Protocol::StateUpdate after = Update(11);
      after.units.push_back(Figure(1, Protocol::UnitKind::Spearman, ATTACKER, _toX, _toZ));
      return PlaceWith(models, before, after, Client::FireTally{}, 0.5).placements.front().quarterTurns;
    };
    Assert::AreEqual(0U, facing(10.5F, 20.5F, 10.5F, 20.5F));    // north until its first step
    Assert::AreEqual(1U, facing(10.5F, 20.5F, 10.57F, 20.5F));   // east: its front, +z, turned once
    Assert::AreEqual(1U, facing(10.57F, 20.5F, 10.57F, 20.5F));  // standing, it keeps it
    Assert::AreEqual(2U, facing(10.57F, 20.5F, 10.57F, 20.43F)); // south
    Assert::AreEqual(2U, facing(10.57F, 20.43F, 10.5F, 20.36F)); // a diagonal keeps a facing along one of its axes
    Assert::AreEqual(3U, facing(10.5F, 20.36F, 10.43F, 20.36F)); // west
  }

  TEST_METHOD(AGarrisonedSquadsFiguresAreNotDrawn)
  {
    Protocol::StateUpdate state = Update(10);
    state.units.push_back(Figure(1, Protocol::UnitKind::Archer, DEFENDER, 10.5F, 20.5F));
    state.squads.push_back(
      {.id = 101, .owner = DEFENDER, .units = {1}, .hasGoal = false, .goalVoxels = {}, .shortOfGoal = false, .garrisonedIn = 7});
    Assert::IsTrue(PlaceOnce(state).placements.empty());
  }

  TEST_METHOD(TheRamIsS1OnItsFootprintsMiddleFacingItsLastStep)
  {
    // Its body's position is the middle of its first cell, and machines stand on 3 x 3 cells from there.
    Protocol::StateUpdate before = Update(10);
    before.engines.push_back(EngineAt(5, Protocol::EngineKind::Ram, Protocol::EngineState::Packed, 10.5F, 20.5F, 0));
    Client::WorldModels models;
    Placed placed = PlaceWith(models, Protocol::StateUpdate{}, before, Client::FireTally{}, 0.0);
    Assert::AreEqual(std::size_t{1}, placed.placements.size());
    Assert::AreEqual(std::string("S1"), std::string(placed.placements.front().modelId));
    Assert::AreEqual(0U, placed.placements.front().quarterTurns);
    const auto [x, z] = MiddleOf(placed.placements.front());
    Assert::AreEqual(11.5F, x, CLOSE_METERS);
    Assert::AreEqual(21.5F, z, CLOSE_METERS);
    // Its box, where a click selects it, is the model's as placed.
    Assert::AreEqual(std::size_t{1}, models.EngineBoxes().size());
    const Client::EngineBox& box = models.EngineBoxes().front();
    Assert::AreEqual(placed.placements.front().xMeters, box.xMeters);
    Assert::AreEqual(TurnedFootprint(placed.placements.front()).second, box.depthMeters);
    // Stepping east, it turns to face east, its front +z turned once.
    Protocol::StateUpdate after = Update(11);
    after.engines.push_back(EngineAt(5, Protocol::EngineKind::Ram, Protocol::EngineState::Packed, 10.535F, 20.5F, 0));
    placed = PlaceWith(models, before, after, Client::FireTally{}, 1.0);
    Assert::AreEqual(1U, placed.placements.front().quarterTurns);
    Assert::AreEqual(11.535F, MiddleOf(placed.placements.front()).first, CLOSE_METERS);
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

  TEST_METHOD(TheWagonSitsAtTheFramesPlacementAndFacesItsStep)
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
    Assert::AreEqual(0U, wagon.quarterTurns);
    // Moving north, its draw pole, the baked grid's -x, leads: one turn takes -x to +z.
    Protocol::StateUpdate after = Update(11);
    after.engines.push_back(EngineAt(5, Protocol::EngineKind::Trebuchet, Protocol::EngineState::Packed, 30.5F, 40.535F, 0));
    Client::WorldModels moving;
    Assert::AreEqual(1U, PlaceWith(moving, packed, after, Client::FireTally{}, 1.0).placements.front().quarterTurns);
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
      Assert::AreEqual(turns, raised.quarterTurns);
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
