#include "pch.h"

#include "CommandCard.h"
#include "CommandLog.h"
#include "Orders.h"
#include "Session.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Where each command came from (M4-I7; Hud.md §11; ADR-007, decision 4): a command by its hotkey records key, and one
// by the card's cell or by a right click records click, each with its tick and its time, for the trial's log.

namespace ClientTests
{

namespace
{

constexpr std::uint8_t ME = 1;
constexpr Protocol::EntityId SPEARMEN = 1;

Protocol::StateUpdate Field()
{
  Protocol::StateUpdate state{};
  state.player = ME;
  state.squads.push_back({.id = SPEARMEN,
                          .owner = ME,
                          .units = {10},
                          .hasGoal = false,
                          .goalVoxels = {},
                          .shortOfGoal = false,
                          .garrisonedIn = Protocol::NO_ENTITY,
                          .kind = Protocol::UnitKind::Spearman,
                          .order = Protocol::SquadOrder::Idle,
                          .fullFigures = 6});
  state.units.push_back({.id = 10,
                         .squad = SPEARMEN,
                         .owner = ME,
                         .kind = Protocol::UnitKind::Spearman,
                         .position = {},
                         .cellVoxels = {.xVoxels = 10, .yVoxels = 9, .zVoxels = 20}});
  return state;
}

} // namespace

TEST_CLASS(CommandSourceTests)
{
public:
  TEST_METHOD(AHotkeyRecordsKeyAndACellOrARightClickRecordsClick)
  {
    const Protocol::StateUpdate state = Field();
    Client::Selection selection;
    const std::array<Protocol::EntityId, 1> spearmen = {SPEARMEN};
    Client::Selector::Select(selection, state, spearmen, false);
    const Client::HudCommandCard card = Client::BuildCard(selection, state, Client::CardCommand::None);
    const auto stop = std::ranges::find(card.slots, Client::CardCommand::Stop, &Client::CommandOf);
    Assert::IsTrue(stop != card.slots.end());
    Client::Session session;
    Client::Orders orders;
    Client::CommandLog log;
    // STOP by its hotkey at tick 40.
    static_cast<void>(orders.Update({.hotkey = stop->hotkey.front()}, selection, state, card, session));
    static_cast<void>(log.Take(session, 40, 2.0));
    // STOP by its cell at tick 41.
    static_cast<void>(orders.Update({.slotClicked = static_cast<std::size_t>(stop - card.slots.begin())}, selection, state, card, session));
    static_cast<void>(log.Take(session, 41, 2.05));
    // MOVE by a right click on the ground at tick 42.
    static_cast<void>(
      orders.Update({.rightPressed = true, .pointedVoxels = Protocol::VoxelPosition{40, 8, 30}}, selection, state, card, session));
    const std::vector<std::vector<std::byte>> given = log.Take(session, 42, 2.1);
    Assert::AreEqual(std::size_t{1}, given.size()); // the bytes go on to the simulation
    const std::span<const Client::CommandRecord> records = log.Records();
    Assert::AreEqual(std::size_t{3}, records.size());
    Assert::AreEqual(std::string("key"), records[0].source);
    Assert::AreEqual(std::string("StopSquad 1"), records[0].command);
    Assert::AreEqual(std::uint32_t{40}, records[0].tick);
    Assert::AreEqual(2.0, records[0].seconds);
    Assert::AreEqual(std::string("click"), records[1].source);
    Assert::AreEqual(std::string("StopSquad 1"), records[1].command);
    Assert::AreEqual(std::string("click"), records[2].source);
    Assert::AreEqual(std::string("MoveSquad 1 40 9 30"), records[2].command);
    Assert::AreEqual(std::uint32_t{42}, records[2].tick);
  }

  TEST_METHOD(DescribesEveryKindOfCommand)
  {
    Assert::AreEqual(std::string("FireEngine 60 1 2 3"),
                     Client::CommandLog::Describe(Protocol::FireEngine{.engine = 60, .targetVoxels = {1, 2, 3}}));
    Assert::AreEqual(std::string("MergeSquads 1 2 3"), Client::CommandLog::Describe(Protocol::MergeSquads{.squads = {1, 2, 3}}));
    Assert::AreEqual(std::string("DigTrench 4 1 2 3 5 6 7"),
                     Client::CommandLog::Describe(Protocol::DigTrench{.squad = 4, .minVoxels = {1, 2, 3}, .maxVoxels = {5, 6, 7}}));
    Assert::AreEqual(std::string("SetGate 9 1"), Client::CommandLog::Describe(Protocol::SetGate{.structure = 9, .open = true}));
    Assert::AreEqual(std::string("DeployEngine 60 2"),
                     Client::CommandLog::Describe(Protocol::DeployEngine{.engine = 60, .quarterTurns = 2}));
  }
};

} // namespace ClientTests
