#include "pch.h"

#include "ByteReader.h"
#include "CommandCard.h"
#include "Hud.h"
#include "Orders.h"
#include "Strings.h"
#include "TextLayout.h"
#include "UiScale.h"

#include <tuple>
#include <variant>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The command card and its modes (M4-I3; Hud.md §7 and §8), with no GPU: each kind's slots, their labels, keys and
// disabled states, and what a key and a right click give.

namespace ClientTests
{

namespace
{

constexpr std::uint8_t ME = 1;
constexpr std::uint8_t FOE = 2;
constexpr std::array<float, 3> STEPS = {1.0F, 1.5F, 2.0F};
constexpr float LABEL_LOGICAL_PIXELS = 90.0F; // Hud.md §7: a label is at most 90 logical px at 20 px

void AddSquad(Protocol::StateUpdate& _state, Protocol::EntityId _squad, Protocol::UnitKind _kind, std::uint8_t _owner, std::size_t _figures)
{
  Protocol::SquadView squad{.id = _squad,
                            .owner = _owner,
                            .units = {},
                            .hasGoal = false,
                            .goalVoxels = {},
                            .shortOfGoal = false,
                            .garrisonedIn = Protocol::NO_ENTITY,
                            .kind = _kind,
                            .order = Protocol::SquadOrder::Idle,
                            .fullFigures = 6};
  for (std::size_t figure = 0; figure < _figures; ++figure)
  {
    const Protocol::EntityId unit = (_squad * 10) + figure;
    squad.units.push_back(unit);
    _state.units.push_back({.id = unit,
                            .squad = _squad,
                            .owner = _owner,
                            .kind = _kind,
                            .position = {},
                            .cellVoxels = {.xVoxels = static_cast<std::int32_t>(_squad), .yVoxels = 9, .zVoxels = 0}});
  }
  _state.squads.push_back(squad);
}

/// The attacker with stone for _shots shots, its spearmen (1, 2), archers (3), sappers (4), ram (5) and trebuchet (6),
/// and an enemy squad (7).
Protocol::StateUpdate Army(std::int32_t _stoneHundredths, Protocol::EngineState _trebuchet, std::uint32_t _reloadTicks)
{
  Protocol::StateUpdate state{};
  state.player = ME;
  state.players = {
    {.player = ME, .side = Protocol::Side::Attacker, .stoneHundredthsCubicMeters = _stoneHundredths, .timberHundredthsCubicMeters = 0},
    {.player = FOE, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = 0, .timberHundredthsCubicMeters = 0}};
  AddSquad(state, 1, Protocol::UnitKind::Spearman, ME, 6);
  AddSquad(state, 2, Protocol::UnitKind::Spearman, ME, 1);
  AddSquad(state, 3, Protocol::UnitKind::Archer, ME, 6);
  AddSquad(state, 4, Protocol::UnitKind::Sapper, ME, 6);
  AddSquad(state, 5, Protocol::UnitKind::Ram, ME, 1);
  AddSquad(state, 6, Protocol::UnitKind::Trebuchet, ME, 1);
  AddSquad(state, 7, Protocol::UnitKind::Spearman, FOE, 6);
  state.engines.push_back({.id = 50,
                           .owner = ME,
                           .kind = Protocol::EngineKind::Ram,
                           .cellVoxels = {},
                           .quarterTurns = 0,
                           .reloadTicks = 0,
                           .state = Protocol::EngineState::Packed,
                           .squad = 5});
  state.engines.push_back({.id = 60,
                           .owner = ME,
                           .kind = Protocol::EngineKind::Trebuchet,
                           .cellVoxels = {},
                           .quarterTurns = 0,
                           .reloadTicks = _reloadTicks,
                           .state = _trebuchet,
                           .squad = 6});
  return state;
}

Client::Selection Selected(const Protocol::StateUpdate& _state, std::span<const Protocol::EntityId> _squads)
{
  Client::Selection selection;
  Client::Selector::Select(selection, _state, _squads, false);
  return selection;
}

std::vector<std::string> Labels(const Client::HudCommandCard& _card)
{
  std::vector<std::string> labels;
  labels.reserve(_card.slots.size());
  for (const Client::HudCommandSlot& slot : _card.slots)
  {
    labels.push_back(slot.label);
  }
  return labels;
}

const Client::HudCommandSlot& SlotOf(const Client::HudCommandCard& _card, std::string_view _label)
{
  const auto found = std::ranges::find(_card.slots, std::string(_label), &Client::HudCommandSlot::label);
  Assert::IsTrue(found != _card.slots.end());
  return *found;
}

std::vector<Protocol::Command> Commands(Client::Session& _session)
{
  std::vector<Protocol::Command> commands;
  for (const std::vector<std::byte>& bytes : _session.TakeCommands())
  {
    Core::ByteReader reader(bytes);
    commands.push_back(Protocol::DecodeCommand(reader));
  }
  return commands;
}

} // namespace

TEST_CLASS(CommandCardTests)
{
public:
  TEST_METHOD(EachKindsSlotsAreHudMdsInOrder)
  {
    const Protocol::StateUpdate state = Army(1000, Protocol::EngineState::Deployed, 0);
    const auto card = [&state](Protocol::EntityId _squad)
    {
      const std::array<Protocol::EntityId, 1> one = {_squad};
      return Labels(Client::BuildCard(Selected(state, one), state, Client::CardCommand::None));
    };
    using Row = std::vector<std::string>;
    Assert::IsTrue(card(1) == Row{"MOVE", "STOP", "HOLD", "ATTACK", "STORM", "SPLIT", "MERGE", "", "", "", "", ""});
    Assert::IsTrue(card(4) == Row{"MOVE", "STOP", "HOLD", "", "", "SPLIT", "MERGE", "TRENCH", "", "", "", ""});
    Assert::IsTrue(card(5) == Row{"MOVE", "STOP", "ATTACK", "", "", "", "", "", "", "", "", ""});
    Assert::IsTrue(card(6) == Row{"FIRE", "STOP", "", "", "PACK", "", "", "", "", "", "", ""});
    // Packed, the trebuchet's fifth slot is DEPLOY; and the defender has no STORM.
    const Protocol::StateUpdate packed = Army(1000, Protocol::EngineState::Packed, 0);
    const std::array<Protocol::EntityId, 1> trebuchet = {6};
    Assert::AreEqual(std::string("DEPLOY"),
                     Client::BuildCard(Selected(packed, trebuchet), packed, Client::CardCommand::None).slots.at(4).label);
    Protocol::StateUpdate defending = state;
    defending.players.front().side = Protocol::Side::Defender;
    const std::array<Protocol::EntityId, 1> spearmen = {1};
    Assert::IsTrue(Client::BuildCard(Selected(defending, spearmen), defending, Client::CardCommand::None).slots.at(4).label.empty());
  }

  TEST_METHOD(EveryLabelFits90PixelsAndEveryHotkeyIsUniqueAndNotWasd)
  {
    const Protocol::StateUpdate state = Army(1000, Protocol::EngineState::Deployed, 0);
    for (const Protocol::EntityId squad : {Protocol::EntityId{1}, Protocol::EntityId{4}, Protocol::EntityId{5}, Protocol::EntityId{6}})
    {
      const std::array<Protocol::EntityId, 1> one = {squad};
      const Client::HudCommandCard card = Client::BuildCard(Selected(state, one), state, Client::CardCommand::None);
      std::vector<std::string> keys;
      for (const Client::HudCommandSlot& slot : card.slots)
      {
        if (slot.command.empty())
        {
          continue;
        }
        for (const float step : STEPS)
        {
          const std::uint32_t scale = Client::TextLayout::PixelsPerFontPixel(Client::TextSize::Label, step);
          Assert::IsTrue(static_cast<std::int32_t>(Client::TextLayout::Measure(slot.label, scale)) <=
                         Client::ScaledPixels(LABEL_LOGICAL_PIXELS, step));
        }
        Assert::IsTrue(std::ranges::find(keys, slot.hotkey) == keys.end());
        Assert::IsTrue(slot.hotkey != "W" && slot.hotkey != "A" && slot.hotkey != "S" && slot.hotkey != "D");
        keys.push_back(slot.hotkey);
      }
    }
  }

  TEST_METHOD(DisablesAsHudMdSays)
  {
    const std::array<Protocol::EntityId, 1> trebuchet = {6};
    const auto slot = [&trebuchet](const Protocol::StateUpdate& _state, std::string_view _label)
    { return SlotOf(Client::BuildCard(Selected(_state, trebuchet), _state, Client::CardCommand::None), _label); };
    // FIRE: deployed and loaded with a shot's stone; not reloading, packed or short of stone.
    Assert::IsTrue(slot(Army(150, Protocol::EngineState::Deployed, 0), "FIRE").enabled);
    Assert::IsFalse(slot(Army(150, Protocol::EngineState::Deployed, 40), "FIRE").enabled);
    Assert::IsFalse(slot(Army(1000, Protocol::EngineState::Packed, 0), "FIRE").enabled);
    Assert::IsFalse(slot(Army(149, Protocol::EngineState::Deployed, 0), "FIRE").enabled);
    // PACK while it packs.
    Assert::IsTrue(slot(Army(1000, Protocol::EngineState::Deployed, 0), "PACK").enabled);
    Assert::IsFalse(slot(Army(1000, Protocol::EngineState::Packing, 0), "PACK").enabled);
    // SPLIT with one figure; MERGE below two squads.
    const Protocol::StateUpdate state = Army(1000, Protocol::EngineState::Deployed, 0);
    const std::array<Protocol::EntityId, 1> lone = {2};
    const std::array<Protocol::EntityId, 1> full = {1};
    const std::array<Protocol::EntityId, 2> both = {1, 2};
    Assert::IsFalse(SlotOf(Client::BuildCard(Selected(state, lone), state, Client::CardCommand::None), "SPLIT").enabled);
    Assert::IsTrue(SlotOf(Client::BuildCard(Selected(state, full), state, Client::CardCommand::None), "SPLIT").enabled);
    Assert::IsFalse(SlotOf(Client::BuildCard(Selected(state, full), state, Client::CardCommand::None), "MERGE").enabled);
    Assert::IsTrue(SlotOf(Client::BuildCard(Selected(state, both), state, Client::CardCommand::None), "MERGE").enabled);
  }

  TEST_METHOD(EveryTooltipFitsItsPanelAtEveryScale)
  {
    const Protocol::StateUpdate state = Army(1000, Protocol::EngineState::Deployed, 0);
    for (const Protocol::EntityId squad : {Protocol::EntityId{1}, Protocol::EntityId{4}, Protocol::EntityId{5}, Protocol::EntityId{6}})
    {
      const std::array<Protocol::EntityId, 1> one = {squad};
      Client::HudModel model{};
      model.deck = true;
      model.commandCard = Client::BuildCard(Selected(state, one), state, Client::CardCommand::None);
      for (const Client::HudCommandSlot& slot : model.commandCard.slots)
      {
        if (slot.command.empty())
        {
          continue;
        }
        model.tooltip = Client::Tooltip(slot);
        for (const float step : STEPS)
        {
          const auto width = static_cast<std::uint32_t>(Client::ScaledPixels(Client::DECK_MIN_LOGICAL_PIXELS, step));
          Client::Hud hud;
          std::vector<Client::ScreenQuad> quads;
          hud.Build(model, width, 720, step, quads);
          for (const Client::HudText& text : hud.Texts())
          {
            Assert::IsTrue(text.cell.Inside(text.element));
          }
        }
      }
    }
  }
};

TEST_CLASS(ModeTests)
{
public:
  TEST_METHOD(EachModesKeyEntersItAndSetsItsBanner)
  {
    const Protocol::StateUpdate state = Army(1000, Protocol::EngineState::Deployed, 0);
    for (const auto& [squad, key, label] :
         {std::tuple{Protocol::EntityId{1}, 'M', "MOVE"}, std::tuple{Protocol::EntityId{1}, 'T', "ATTACK"},
          std::tuple{Protocol::EntityId{1}, 'R', "STORM"}, std::tuple{Protocol::EntityId{4}, 'T', "TRENCH"}})
    {
      const std::array<Protocol::EntityId, 1> one = {squad};
      const Client::Selection selection = Selected(state, one);
      const Client::HudCommandCard card = Client::BuildCard(selection, state, Client::CardCommand::None);
      Client::Session session;
      Client::Orders orders;
      static_cast<void>(orders.Update({.hotkey = key}, selection, state, card, session));
      Assert::AreEqual(std::string(label), orders.Banner().command);
      Assert::IsTrue(orders.Banner().banner.starts_with(std::string(label) + " · "));
      Assert::IsTrue(session.TakeCommands().empty()); // a mode gives nothing until its click
    }
  }

  TEST_METHOD(ARightClickSubmitsItsCommandAndLeavesTheModeUnlessShiftIsHeld)
  {
    const Protocol::StateUpdate state = Army(1000, Protocol::EngineState::Deployed, 0);
    const std::array<Protocol::EntityId, 1> spearmen = {1};
    const Client::Selection selection = Selected(state, spearmen);
    const Client::HudCommandCard card = Client::BuildCard(selection, state, Client::CardCommand::None);
    Client::Session session;
    Client::Orders orders;
    static_cast<void>(orders.Update({.hotkey = 'M'}, selection, state, card, session));
    const Protocol::VoxelPosition ground{.xVoxels = 40, .yVoxels = 8, .zVoxels = 30};
    static_cast<void>(orders.Update({.rightPressed = true, .shiftHeld = true, .pointedVoxels = ground}, selection, state, card, session));
    std::vector<Protocol::Command> given = Commands(session);
    Assert::AreEqual(std::size_t{1}, given.size());
    const auto* move = std::get_if<Protocol::MoveSquad>(&given.front());
    Assert::IsNotNull(move);
    Assert::AreEqual(Protocol::EntityId{1}, move->squad);
    Assert::AreEqual(9, move->goalVoxels.yVoxels); // the cell above the ground clicked
    Assert::IsTrue(orders.Mode() == Client::CardCommand::Move);
    Assert::AreEqual(std::string("click"), session.TakenSources().front());
    static_cast<void>(orders.Update({.rightPressed = true, .pointedVoxels = ground}, selection, state, card, session));
    Assert::AreEqual(std::size_t{1}, Commands(session).size());
    Assert::IsTrue(orders.Mode() == Client::CardCommand::None);
  }

  TEST_METHOD(ARightClickWithNoModeAttacksAnEnemyAndElseMoves)
  {
    const Protocol::StateUpdate state = Army(1000, Protocol::EngineState::Deployed, 0);
    const std::array<Protocol::EntityId, 1> spearmen = {1};
    const Client::Selection selection = Selected(state, spearmen);
    const Client::HudCommandCard card = Client::BuildCard(selection, state, Client::CardCommand::None);
    Client::Session session;
    Client::Orders orders;
    const Protocol::VoxelPosition ground{.xVoxels = 40, .yVoxels = 8, .zVoxels = 30};
    static_cast<void>(orders.Update({.rightPressed = true, .pointedVoxels = ground, .pointedSquad = Protocol::EntityId{7}}, selection,
                                    state, card, session));
    std::vector<Protocol::Command> given = Commands(session);
    Assert::IsTrue(std::holds_alternative<Protocol::AttackTarget>(given.front()));
    Assert::AreEqual(Protocol::EntityId{7}, std::get<Protocol::AttackTarget>(given.front()).target);
    static_cast<void>(orders.Update({.rightPressed = true, .pointedVoxels = ground}, selection, state, card, session));
    given = Commands(session);
    Assert::IsTrue(std::holds_alternative<Protocol::MoveSquad>(given.front()));
  }

  TEST_METHOD(AHotkeyGivesAnOrderAtOnceAsAKeyAndTrenchIsADrag)
  {
    const Protocol::StateUpdate state = Army(1000, Protocol::EngineState::Deployed, 0);
    const std::array<Protocol::EntityId, 1> sappers = {4};
    const Client::Selection selection = Selected(state, sappers);
    const Client::HudCommandCard card = Client::BuildCard(selection, state, Client::CardCommand::None);
    Client::Session session;
    Client::Orders orders;
    static_cast<void>(orders.Update({.hotkey = 'X'}, selection, state, card, session));
    Assert::IsTrue(std::holds_alternative<Protocol::StopSquad>(Commands(session).front()));
    Assert::AreEqual(std::string("key"), session.TakenSources().front());
    static_cast<void>(orders.Update({.hotkey = 'T'}, selection, state, card, session));
    static_cast<void>(
      orders.Update({.rightPressed = true, .pointedVoxels = Protocol::VoxelPosition{30, 8, 20}}, selection, state, card, session));
    Assert::IsTrue(session.TakeCommands().empty());
    static_cast<void>(
      orders.Update({.rightReleased = true, .pointedVoxels = Protocol::VoxelPosition{25, 8, 27}}, selection, state, card, session));
    const std::vector<Protocol::Command> given = Commands(session);
    Assert::AreEqual(std::size_t{1}, given.size());
    const Protocol::DigTrench& trench = std::get<Protocol::DigTrench>(given.front());
    Assert::AreEqual(25, trench.minVoxels.xVoxels);
    Assert::AreEqual(30, trench.maxVoxels.xVoxels);
    Assert::AreEqual(20, trench.minVoxels.zVoxels);
    Assert::AreEqual(27, trench.maxVoxels.zVoxels);
    Assert::AreEqual(8, trench.maxVoxels.yVoxels);
    Assert::AreEqual(8 - Client::Orders::TRENCH_DEPTH_VOXELS + 1, trench.minVoxels.yVoxels);
    Assert::IsTrue(orders.Mode() == Client::CardCommand::None);
  }
};

} // namespace ClientTests
