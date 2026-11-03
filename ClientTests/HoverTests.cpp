#include "pch.h"

#include "AlertWatch.h"
#include "ByteReader.h"
#include "ByteWriter.h"
#include "CommandCard.h"
#include "ControlGroups.h"
#include "Hud.h"
#include "Orders.h"
#include "Overlays.h"
#include "SelectionPanel.h"
#include "Session.h"
#include "Strings.h"

#include <set>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Hover and ADR-007, decision 4 (M4-I7; Hud.md §8): every reading in Strings.h's table is reached by a key or a click
// alone, driven through the HUD model with no pointer movement: no Hover call, and the pointer, where a mode reads it,
// resting where it was.

namespace ClientTests
{

namespace
{

constexpr std::uint8_t ME = 1;
constexpr std::uint8_t FOE = 2;
constexpr Protocol::EntityId SPEARMEN = 1;
constexpr Protocol::EntityId ARCHERS = 3;
constexpr Protocol::EntityId W5 = 50;

void AddSquad(Protocol::StateUpdate& _state, Protocol::EntityId _squad, Protocol::UnitKind _kind, std::uint8_t _owner)
{
  const Protocol::EntityId unit = _squad * 10;
  _state.squads.push_back({.id = _squad,
                           .owner = _owner,
                           .units = {unit},
                           .hasGoal = false,
                           .goalVoxels = {},
                           .shortOfGoal = false,
                           .garrisonedIn = Protocol::NO_ENTITY,
                           .kind = _kind,
                           .order = Protocol::SquadOrder::Idle,
                           .fullFigures = 6});
  _state.units.push_back({.id = unit,
                          .squad = _squad,
                          .owner = _owner,
                          .kind = _kind,
                          .position = {},
                          .cellVoxels = {.xVoxels = 10 * static_cast<std::int32_t>(_squad), .yVoxels = 9, .zVoxels = 20}});
}

/// The attacker's spearmen and archers, and the foe's W5.
Protocol::StateUpdate Field()
{
  Protocol::StateUpdate state{};
  state.tick = 5;
  state.player = ME;
  state.widthCells = 128;
  state.depthCells = 128;
  state.players = {{.player = ME, .side = Protocol::Side::Attacker, .stoneHundredthsCubicMeters = 1000, .timberHundredthsCubicMeters = 0},
                   {.player = FOE, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = 0, .timberHundredthsCubicMeters = 0}};
  AddSquad(state, SPEARMEN, Protocol::UnitKind::Spearman, ME);
  AddSquad(state, ARCHERS, Protocol::UnitKind::Archer, ME);
  Protocol::StructureView wall{
    .id = W5, .owner = FOE, .templateId = "T3", .originVoxels = {60, 9, 60}, .quarterTurns = 0, .extentVoxels = {4, 10, 4}, .name = "W5"};
  wall.repairLockTicks = 120;
  state.structures.push_back(wall);
  return state;
}

/// Answers each query the session asked: an integrity query with _reading, an aim with IN RANGE at 24 m.
void Answer(Client::Session& _session, const Protocol::StateUpdate& _state, const Protocol::IntegrityReading& _reading)
{
  std::vector<std::vector<std::byte>> answers;
  for (const std::vector<std::byte>& bytes : _session.TakeQueries())
  {
    Core::ByteReader reader(bytes);
    const Protocol::QueryRequest request = Protocol::DecodeQueryRequest(reader);
    Core::ByteWriter writer;
    if (std::holds_alternative<Protocol::SectionIntegrity>(request.query))
    {
      Protocol::Encode(Protocol::Answer{.queryId = request.id, .tick = _state.tick, .reading = _reading}, writer);
    }
    else
    {
      const Protocol::AimReading aim{.verdict = Protocol::AimVerdict::InRange, .distanceMeters = 24, .strikes = false, .strikeVoxels = {}};
      Protocol::Encode(Protocol::Answer{.queryId = request.id, .tick = _state.tick, .reading = aim}, writer);
    }
    answers.push_back(writer.TakeBytes());
  }
  _session.Receive(_state, {}, answers);
}

/// Whether the HUD, as last built, drew a string inside _inside.
bool Drew(const Client::Hud& _hud, const Client::PixelRect& _inside)
{
  return std::ranges::any_of(_hud.Texts(), [&_inside](const Client::HudText& _text) { return _text.cell.Inside(_inside); });
}

Client::HudModel Deck()
{
  Client::HudModel model{};
  model.deck = true;
  return model;
}

} // namespace

TEST_CLASS(HoverTests)
{
public:
  TEST_METHOD(TheTableNamesEveryReadingOnceWithAWayWithoutHover)
  {
    std::set<Client::HudReading> readings;
    for (const Client::HoverWay& way : Client::HOVER_TABLE)
    {
      Assert::IsTrue(readings.insert(way.reading).second);
      Assert::IsFalse(way.withoutHover.empty());
    }
    Assert::AreEqual(static_cast<std::size_t>(Client::HudReading::Minimap) + 1, readings.size());
  }

  TEST_METHOD(TheTopBarTheCardAndTheMinimapAreAlwaysDrawn)
  {
    const Protocol::StateUpdate state = Field();
    Client::Selection selection;
    const std::array<Protocol::EntityId, 1> spearmen = {SPEARMEN};
    Client::Selector::Select(selection, state, spearmen, false);
    Client::HudModel model = Deck();
    model.resources = {{.key = "STONE", .value = "10", .warn = false}};
    model.commandCard = Client::BuildCard(selection, state, Client::CardCommand::None);
    Client::Hud hud;
    std::vector<Client::ScreenQuad> quads;
    hud.Build(model, 1280, 720, 1.0F, quads);
    // The top bar's resource, each cell's hotkey and label, and the minimap's target, with the pointer nowhere.
    Assert::IsTrue(Drew(hud, hud.Panels().front()));
    const Client::HudDeck deck = hud.Deck().value_or(Client::HudDeck{});
    Assert::IsTrue(Drew(hud, deck.cells.at(0)));
    Assert::IsTrue(
      std::ranges::any_of(hud.Targets(), [](const Client::HudTarget& _target) { return _target.control == Client::HudControl::Minimap; }));
  }

  TEST_METHOD(ATargetedCommandsKeyShowsItsLabelAndRuleInTheBanner)
  {
    const Protocol::StateUpdate state = Field();
    Client::Selection selection;
    const std::array<Protocol::EntityId, 1> spearmen = {SPEARMEN};
    Client::Selector::Select(selection, state, spearmen, false);
    const Client::HudCommandCard card = Client::BuildCard(selection, state, Client::CardCommand::None);
    Client::Session session;
    Client::Orders orders;
    static_cast<void>(orders.Update({.hotkey = 'M'}, selection, state, card, session));
    Assert::AreEqual(std::format(Client::MODE_BANNER, Client::MOVE_LABEL, Client::MOVE_RULE), orders.Banner().banner);
    // The tooltip's hotkey and label are the cell's own, drawn without hover.
    const auto move = std::ranges::find(card.slots, std::string(Client::MOVE_LABEL), &Client::HudCommandSlot::label);
    Assert::IsTrue(move != card.slots.end());
    Assert::IsTrue(Client::Tooltip(*move).starts_with(move->hotkey + " · " + move->label));
  }

  TEST_METHOD(AClickedStructureReadsInTheReadoutWithoutHover)
  {
    const Protocol::StateUpdate state = Field();
    Client::Session session;
    session.Receive(state, {}, {});
    Client::Overlays overlays;
    overlays.SelectedFirst(true);
    overlays.Ask(session, {}, true);
    Protocol::IntegrityReading reading{};
    reading.structure = W5;
    Answer(session, state, reading);
    overlays.Select(W5); // the click
    const std::optional<Client::HudReadout> readout = overlays.Readout(session, 1.0, true);
    Assert::IsTrue(readout.has_value());
    Assert::AreEqual(std::string("W5"), readout.value_or(Client::HudReadout{}).title);
  }

  TEST_METHOD(F1DrawsEveryStructuresLineInTheWorld)
  {
    const Protocol::StateUpdate state = Field();
    Client::Session session;
    session.Receive(state, {}, {});
    // Seen from straight above, 128 m onto the whole target.
    DirectX::XMFLOAT4X4 overhead{};
    overhead.m[0][0] = 2.0F / 128.0F;
    overhead.m[2][1] = 2.0F / 128.0F;
    overhead.m[3][0] = -1.0F;
    overhead.m[3][1] = -1.0F;
    overhead.m[3][3] = 1.0F;
    Client::HudModel model = Deck();
    model.worldLabels = Client::Overlays::WorldLabels(session, overhead, 1280, 720, 60.0F, Protocol::NO_ENTITY, true);
    Client::Hud hud;
    std::vector<Client::ScreenQuad> quads;
    hud.Build(model, 1280, 720, 1.0F, quads);
    Assert::AreEqual(std::size_t{1}, hud.Labels().size());
    Assert::AreEqual(std::string("W5 · Locked · 6 s"), hud.Labels().front().text);
  }

  TEST_METHOD(AClickTabAndAGroupsKeyFillThePanel)
  {
    const Protocol::StateUpdate state = Field();
    Client::Selection selection;
    Client::Selector::Click(selection, state, SPEARMEN, Protocol::NO_ENTITY, false);
    Assert::AreEqual(std::string("SPEARMEN"), Client::BuildPanel(selection, state).name);
    // Both squads, then Tab to the archers.
    const std::array<Protocol::EntityId, 2> both = {SPEARMEN, ARCHERS};
    Client::Selector::Select(selection, state, both, false);
    const std::optional<Protocol::UnitKind> before = selection.activeKind;
    Client::Selector::CycleKind(selection, state);
    Assert::IsTrue(selection.activeKind != before);
    // A control group's key.
    Client::ControlGroups groups;
    groups.Assign(2, selection);
    selection = {};
    static_cast<void>(groups.Recall(2, selection, state, 1.0));
    Assert::IsTrue(Client::BuildPanel(selection, state).shows == Client::HudPanel::Shows::Several);
    Assert::IsFalse(groups.Label(2, state).empty());
  }

  TEST_METHOD(TheAttackCursorReadsWhereThePointerRestsOnceItsKeyIsPressed)
  {
    const Protocol::StateUpdate state = Field();
    Client::Selection selection;
    const std::array<Protocol::EntityId, 1> archers = {ARCHERS};
    Client::Selector::Select(selection, state, archers, false);
    const Client::HudCommandCard card = Client::BuildCard(selection, state, Client::CardCommand::None);
    Client::Session session;
    session.Receive(state, {}, {});
    Client::Orders orders;
    // The attack's key, with the pointer resting on the ground at (60, 8, 50): the reading is asked there at once.
    const Protocol::VoxelPosition resting{.xVoxels = 60, .yVoxels = 8, .zVoxels = 50};
    static_cast<void>(orders.Update({.hotkey = 'T', .pointedVoxels = resting}, selection, state, card, session));
    Assert::IsTrue(orders.Mode() == Client::CardCommand::Attack);
    Answer(session, state, Protocol::IntegrityReading{});
    const std::optional<Client::AttackCursor> cursor = orders.Cursor(session);
    Assert::IsTrue(cursor.has_value());
    Assert::AreEqual(std::format(Client::AIM_IN_RANGE, 24), cursor.value_or(Client::AttackCursor{}).label);
  }

  TEST_METHOD(JumpAndSpaceReachAnAlertsPlace)
  {
    Protocol::StateUpdate state = Field();
    state.structures.front().opened = true;
    Client::Session session;
    session.Receive(state, {}, {});
    Client::AlertWatch watch;
    Client::HudAlerts alerts;
    watch.Observe(session, 1.0, alerts);
    Client::HudModel model = Deck();
    model.alerts = alerts.Shown();
    Client::Hud hud;
    std::vector<Client::ScreenQuad> quads;
    hud.Build(model, 1920, 1080, 1.0F, quads);
    Assert::IsTrue(
      std::ranges::any_of(hud.Targets(), [](const Client::HudTarget& _target) { return _target.control == Client::HudControl::Jump; }));
    Assert::IsTrue(Client::AlertWatch::NewestPlace(model.alerts).has_value());
  }
};

} // namespace ClientTests
