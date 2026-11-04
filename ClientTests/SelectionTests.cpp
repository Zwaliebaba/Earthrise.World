#include "pch.h"

#include "Selection.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Selecting (M4-I2; Hud.md §5 and §8), with no GPU: a click by a squad's footprint on the ground, a box, a
// double-click, Shift and Ctrl, and Tab through the kinds.

namespace ClientTests
{

namespace
{

constexpr std::uint8_t ME = 1;
constexpr std::uint8_t FOE = 2;
constexpr std::int32_t GROUND = 9;
constexpr std::uint32_t SCREEN_PIXELS = 100;

Protocol::FixedPosition Feet(std::int32_t _x, std::int32_t _z)
{
  const auto middle = [](std::int32_t _cell) { return Core::Fixed::FromRaw((_cell * Core::Fixed::ONE_RAW) + (Core::Fixed::ONE_RAW / 2)); };
  return {.xMeters = middle(_x), .yMeters = Core::Fixed::FromRaw(GROUND * Core::Fixed::ONE_RAW), .zMeters = middle(_z)};
}

/// _squad of _kind and _owner with a figure at each of _cells.
void AddSquad(Protocol::StateUpdate& _state, Protocol::EntityId _squad, Protocol::UnitKind _kind, std::uint8_t _owner,
              std::span<const std::pair<std::int32_t, std::int32_t>> _cells)
{
  Protocol::SquadView squad{.id = _squad,
                            .owner = _owner,
                            .units = {},
                            .hasGoal = false,
                            .goalVoxels = {},
                            .shortOfGoal = false,
                            .garrisonedIn = Protocol::NO_ENTITY,
                            .kind = _kind};
  for (const auto& [x, z] : _cells)
  {
    const Protocol::EntityId unit = (_squad * 10) + squad.units.size();
    squad.units.push_back(unit);
    _state.units.push_back({.id = unit,
                            .squad = _squad,
                            .owner = _owner,
                            .kind = _kind,
                            .position = Feet(x, z),
                            .cellVoxels = {.xVoxels = x, .yVoxels = GROUND, .zVoxels = z}});
  }
  _state.squads.push_back(squad);
}

Protocol::StateUpdate Field()
{
  Protocol::StateUpdate state{};
  state.player = ME;
  const std::array<std::pair<std::int32_t, std::int32_t>, 2> spearmen = {{{10, 20}, {11, 20}}};
  const std::array<std::pair<std::int32_t, std::int32_t>, 1> more = {{{40, 30}}};
  const std::array<std::pair<std::int32_t, std::int32_t>, 1> archers = {{{20, 30}}};
  const std::array<std::pair<std::int32_t, std::int32_t>, 1> foes = {{{30, 30}}};
  AddSquad(state, 1, Protocol::UnitKind::Spearman, ME, spearmen);
  AddSquad(state, 2, Protocol::UnitKind::Spearman, ME, more);
  AddSquad(state, 3, Protocol::UnitKind::Archer, ME, archers);
  AddSquad(state, 4, Protocol::UnitKind::Spearman, FOE, foes);
  return state;
}

/// Straight down onto (_x, _z).
Engine::PickRay Down(float _x, float _z)
{
  return {.originMeters = {_x, 60.0F, _z}, .direction = {0.0F, -1.0F, 0.0F}};
}

/// Seen from straight above, 100 m square onto 100 pixels: pixel x is x, and pixel y is 100 - z.
DirectX::XMFLOAT4X4 TopDown()
{
  DirectX::XMFLOAT4X4 matrix{};
  matrix.m[0][0] = 1.0F / 50.0F;
  matrix.m[2][1] = 1.0F / 50.0F;
  matrix.m[3][0] = -1.0F;
  matrix.m[3][1] = -1.0F;
  matrix.m[3][3] = 1.0F;
  return matrix;
}

} // namespace

TEST_CLASS(SelectionTests)
{
public:
  TEST_METHOD(AClickInsideASquadsFootprintSelectsItAndOutsideDoesNot)
  {
    const Protocol::StateUpdate state = Field();
    // The spearmen stand in cells (10, 20) and (11, 20); their footprint reaches a cell out, to x 9 to 12.
    Assert::AreEqual(Protocol::EntityId{1}, Client::Selector::SquadAt(Down(12.5F, 21.5F), state).value_or(0));
    Assert::AreEqual(Protocol::EntityId{1}, Client::Selector::SquadAt(Down(9.1F, 19.2F), state).value_or(0));
    Assert::IsFalse(Client::Selector::SquadAt(Down(13.5F, 20.5F), state).has_value());
    Assert::IsFalse(Client::Selector::SquadAt(Down(10.5F, 22.5F), state).has_value());
    // A slanting ray is tested on the plane of the figures' feet, not where it meets the ground further on.
    const Engine::PickRay slant{.originMeters = {10.5F - 30.0F, 9.0F + 30.0F, 20.5F}, .direction = {0.7071068F, -0.7071068F, 0.0F}};
    Assert::AreEqual(Protocol::EntityId{1}, Client::Selector::SquadAt(slant, state).value_or(0));
    // A garrisoned squad is in no cell.
    Protocol::StateUpdate garrisoned = state;
    garrisoned.squads.front().garrisonedIn = 77;
    Assert::IsFalse(Client::Selector::SquadAt(Down(10.5F, 20.5F), garrisoned).has_value());
  }

  TEST_METHOD(ABoxOverTwoSquadsSelectsBothAndNoEnemy)
  {
    const Protocol::StateUpdate state = Field();
    // The archers at (20, 30) are at pixel (20.5, 69.5), the enemy at (30.5, 69.5) and the second spearmen at (40.5, 69.5);
    // the first spearmen, at (10.5, 79.5) and (11.5, 79.5), are outside the box.
    const std::vector<Protocol::EntityId> boxed =
      Client::Selector::SquadsInBox(state, ME, TopDown(), SCREEN_PIXELS, SCREEN_PIXELS, 50, 75, 15, 60);
    Assert::AreEqual(std::size_t{2}, boxed.size());
    Assert::IsTrue(std::ranges::find(boxed, Protocol::EntityId{3}) != boxed.end());
    Assert::IsTrue(std::ranges::find(boxed, Protocol::EntityId{2}) != boxed.end());
    Client::Selection selection;
    Client::Selector::Select(selection, state, boxed, false);
    Assert::AreEqual(std::size_t{2}, selection.squads.size());
    // Spearmen come before archers in Hud.md's order, so they drive the card.
    Assert::IsTrue(selection.activeKind == Protocol::UnitKind::Spearman);
  }

  TEST_METHOD(ADoubleClickPicksOnlyTheType)
  {
    const Protocol::StateUpdate state = Field();
    Client::Selection selection;
    Client::Selector::SelectKind(selection, state, 2, TopDown(), SCREEN_PIXELS, SCREEN_PIXELS);
    // Both of my spearmen squads, not my archers, not the enemy's spearmen.
    Assert::AreEqual(std::size_t{2}, selection.squads.size());
    Assert::IsTrue(std::ranges::find(selection.squads, Protocol::EntityId{1}) != selection.squads.end());
    Assert::IsTrue(std::ranges::find(selection.squads, Protocol::EntityId{2}) != selection.squads.end());
    Assert::IsTrue(selection.activeKind == Protocol::UnitKind::Spearman);
  }

  TEST_METHOD(TabMovesTheActiveKindInHudMdsOrder)
  {
    Protocol::StateUpdate state = Field();
    const std::array<std::pair<std::int32_t, std::int32_t>, 1> wagon = {{{60, 60}}};
    const std::array<std::pair<std::int32_t, std::int32_t>, 1> laborer = {{{70, 70}}};
    AddSquad(state, 5, Protocol::UnitKind::Trebuchet, ME, wagon);
    AddSquad(state, 6, Protocol::UnitKind::Laborer, ME, laborer);
    Client::Selection selection;
    const std::array<Protocol::EntityId, 4> picked = {6, 3, 1, 5};
    Client::Selector::Select(selection, state, picked, false);
    Assert::IsTrue(selection.activeKind == Protocol::UnitKind::Trebuchet);
    for (const Protocol::UnitKind next :
         {Protocol::UnitKind::Spearman, Protocol::UnitKind::Archer, Protocol::UnitKind::Laborer, Protocol::UnitKind::Trebuchet})
    {
      Client::Selector::CycleKind(selection, state);
      Assert::IsTrue(selection.activeKind == next);
    }
  }

  TEST_METHOD(ShiftAddsAndRemovesAStructureIsAloneAndNothingDeselects)
  {
    Protocol::StateUpdate state = Field();
    state.structures.push_back({.id = 70, .owner = FOE, .templateId = "T3", .originVoxels = {}, .quarterTurns = 0, .name = "W1"});
    Client::Selection selection;
    Client::Selector::Click(selection, state, Protocol::EntityId{1}, Protocol::NO_ENTITY, false);
    Client::Selector::Click(selection, state, Protocol::EntityId{3}, Protocol::NO_ENTITY, true);
    Assert::AreEqual(std::size_t{2}, selection.squads.size());
    Client::Selector::Click(selection, state, Protocol::EntityId{1}, Protocol::NO_ENTITY, true);
    Assert::AreEqual(std::size_t{1}, selection.squads.size());
    Assert::IsTrue(selection.activeKind == Protocol::UnitKind::Archer);
    Client::Selector::Click(selection, state, std::nullopt, 70, false);
    Assert::IsTrue(selection.squads.empty());
    Assert::AreEqual(Protocol::EntityId{70}, selection.structure);
    Client::Selector::Click(selection, state, std::nullopt, Protocol::NO_ENTITY, false);
    Assert::IsTrue(selection.Empty());
  }

  TEST_METHOD(ASquadThatDiesLeavesTheSelection)
  {
    Protocol::StateUpdate state = Field();
    Client::Selection selection;
    const std::array<Protocol::EntityId, 2> picked = {1, 3};
    Client::Selector::Select(selection, state, picked, false);
    std::erase_if(state.units, [](const Protocol::UnitView& _unit) { return _unit.squad == 1; });
    std::erase_if(state.squads, [](const Protocol::SquadView& _squad) { return _squad.id == 1; });
    Client::Selector::Prune(selection, state);
    Assert::AreEqual(std::size_t{1}, selection.squads.size());
    Assert::IsTrue(selection.activeKind == Protocol::UnitKind::Archer);
  }

  TEST_METHOD(ASelectedSquadIsMarkedByTheGroundUnderEachFigure)
  {
    constexpr std::uint8_t MARK_INDEX = 245;
    Protocol::StateUpdate state = Field();
    const std::array<std::pair<std::int32_t, std::int32_t>, 6> ranks = {{{60, 60}, {61, 60}, {62, 60}, {60, 61}, {61, 61}, {62, 61}}};
    AddSquad(state, 5, Protocol::UnitKind::ManAtArms, ME, ranks);
    Client::Selection selection;
    const std::array<Protocol::EntityId, 1> picked = {5};
    Client::Selector::Select(selection, state, picked, false);
    const std::vector<Client::OverlayBox> marks = Client::Selector::Marks(selection, state, MARK_INDEX);
    Assert::AreEqual(ranks.size(), marks.size());
    for (const auto& [x, z] : ranks)
    {
      const Protocol::VoxelPosition ground{.xVoxels = x, .yVoxels = GROUND - 1, .zVoxels = z};
      Assert::IsTrue(std::ranges::find(marks, ground, &Client::OverlayBox::cellVoxels) != marks.end());
    }
    Assert::IsTrue(std::ranges::all_of(marks, [](const Client::OverlayBox& _mark) { return _mark.paletteIndex == MARK_INDEX; }));
    // Squads 1 to 4 are not selected, so nothing marks their figures.
    Assert::IsTrue(std::ranges::none_of(marks, [](const Client::OverlayBox& _mark) { return _mark.cellVoxels.xVoxels < 60; }));
  }

  TEST_METHOD(AnEnginesBodyAndAGarrisonedSquadAreNotMarked)
  {
    Protocol::StateUpdate state = Field();
    const std::array<std::pair<std::int32_t, std::int32_t>, 1> ram = {{{50, 50}}};
    const std::array<std::pair<std::int32_t, std::int32_t>, 1> trebuchet = {{{55, 50}}};
    AddSquad(state, 5, Protocol::UnitKind::Ram, ME, ram);
    AddSquad(state, 6, Protocol::UnitKind::Trebuchet, ME, trebuchet);
    // The archers in a tower, their figures left in the update, so that the test reads the garrison and not the figures.
    Client::Selection selection;
    selection.squads = {3, 5, 6};
    state.squads.at(2).garrisonedIn = 70;
    Assert::IsTrue(Client::Selector::Marks(selection, state, 245).empty());
  }

  TEST_METHOD(EverySelectedEngineIsOutlinedWhicheverKindIsActive)
  {
    Protocol::StateUpdate state = Field();
    const std::array<std::pair<std::int32_t, std::int32_t>, 1> ram = {{{50, 50}}};
    const std::array<std::pair<std::int32_t, std::int32_t>, 1> trebuchet = {{{55, 50}}};
    const std::array<std::pair<std::int32_t, std::int32_t>, 1> other = {{{70, 50}}};
    AddSquad(state, 5, Protocol::UnitKind::Ram, ME, ram);
    AddSquad(state, 6, Protocol::UnitKind::Trebuchet, ME, trebuchet);
    AddSquad(state, 7, Protocol::UnitKind::Ram, ME, other);
    const auto engine = [](Protocol::EntityId _id, Protocol::EngineKind _kind, Protocol::EntityId _squad)
    {
      return Protocol::EngineView{
        .id = _id, .owner = ME, .kind = _kind, .cellVoxels = {}, .quarterTurns = 0, .reloadTicks = 0, .state = {}, .squad = _squad};
    };
    state.engines.push_back(engine(50, Protocol::EngineKind::Ram, 5));
    state.engines.push_back(engine(60, Protocol::EngineKind::Trebuchet, 6));
    state.engines.push_back(engine(70, Protocol::EngineKind::Ram, 7));
    Client::Selection selection;
    const std::array<Protocol::EntityId, 3> picked = {1, 5, 6};
    Client::Selector::Select(selection, state, picked, false);
    for (const Protocol::UnitKind active : {Protocol::UnitKind::Trebuchet, Protocol::UnitKind::Ram, Protocol::UnitKind::Spearman})
    {
      selection.activeKind = active;
      const std::vector<Protocol::EntityId> outlined = Client::Selector::Engines(selection, state);
      Assert::AreEqual(std::size_t{2}, outlined.size());
      Assert::IsTrue(std::ranges::find(outlined, Protocol::EntityId{50}) != outlined.end());
      Assert::IsTrue(std::ranges::find(outlined, Protocol::EntityId{60}) != outlined.end());
    }
  }
};

} // namespace ClientTests
