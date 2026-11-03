#include "pch.h"

#include "CommandCard.h"
#include "Hud.h"
#include "Orders.h"
#include "SiegeModel.h"
#include "TickClock.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The Siege trial's flow (M4-J1; Hud.md §10), over the model Earthrise's loop drives: the objective and the resources,
// the end banner, and the pause, which stops the clock and nothing else.

namespace ClientTests
{

namespace
{

constexpr std::uint8_t ME = 1;
constexpr std::uint8_t FOE = 2;
constexpr std::uint32_t TICKS_PER_MINUTE = 60 * Client::TickClock::TICKS_PER_SECOND;

/// Forty minutes less 5 s left, 12.5 m³ of stone, and three of the player's figures with one of the foe's.
Protocol::StateUpdate Trial()
{
  Protocol::StateUpdate state{};
  state.tick = 100;
  state.player = ME;
  state.players = {{.player = ME, .side = Protocol::Side::Attacker, .stoneHundredthsCubicMeters = 1250, .timberHundredthsCubicMeters = 0},
                   {.player = FOE, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = 0, .timberHundredthsCubicMeters = 0}};
  for (const auto& [id, owner] : {std::pair{1U, ME}, std::pair{2U, ME}, std::pair{3U, ME}, std::pair{4U, FOE}})
  {
    state.units.push_back({.id = id, .squad = 1, .owner = owner, .kind = Protocol::UnitKind::Spearman, .position = {}, .cellVoxels = {}});
  }
  state.trial.ticksLeft = (40 * TICKS_PER_MINUTE) - (5 * Client::TickClock::TICKS_PER_SECOND);
  state.trial.holdTicksNeeded = 2 * TICKS_PER_MINUTE;
  return state;
}

} // namespace

TEST_CLASS(SiegeFlowTests)
{
public:
  TEST_METHOD(TheObjectiveAndTheResourcesReadAsHudMdSays)
  {
    Protocol::StateUpdate state = Trial();
    Client::HudObjective objective = Client::SiegeModel::Objective(state);
    Assert::AreEqual(std::string("TAKE THE KEEP"), objective.title);
    Assert::IsFalse(objective.progress.has_value());
    // The storming clock at 42 s of its 2 min.
    state.trial.holdTicks = 42 * Client::TickClock::TICKS_PER_SECOND;
    objective = Client::SiegeModel::Objective(state);
    Assert::AreEqual(std::string("HOLD THE KEEP'S FOOT"), objective.title);
    Assert::AreEqual(std::string("0:42 / 2:00"), objective.detail);
    Assert::AreEqual(0.35F, objective.progress.value_or(0.0F), 0.0001F);
    Assert::IsFalse(objective.detailOff);
    state.trial.holdContested = true;
    Assert::IsTrue(Client::SiegeModel::Objective(state).detailOff);
    // STONE, ARMY and TIME.
    std::vector<Client::HudResource> resources = Client::SiegeModel::Resources(state);
    Assert::AreEqual(std::size_t{3}, resources.size());
    Assert::AreEqual(std::string("STONE"), resources.at(0).key);
    Assert::AreEqual(std::string("12.5 m³"), resources.at(0).value);
    Assert::IsFalse(resources.at(0).warn);
    Assert::AreEqual(std::string("ARMY"), resources.at(1).key);
    Assert::AreEqual(std::string("3"), resources.at(1).value);
    Assert::AreEqual(std::string("TIME"), resources.at(2).key);
    Assert::AreEqual(std::string("39:55"), resources.at(2).value);
    // Below one shot's stone, STONE is in bad.
    state.players.front().stoneHundredthsCubicMeters = Client::SHOT_STONE_HUNDREDTHS - 1;
    resources = Client::SiegeModel::Resources(state);
    Assert::AreEqual(std::string("1.4 m³"), resources.at(0).value);
    Assert::IsTrue(resources.at(0).warn);
  }

  TEST_METHOD(TheObjectiveAndItsProgressFitTheTopBarAtEveryScale)
  {
    Protocol::StateUpdate state = Trial();
    state.trial.holdTicks = 42 * Client::TickClock::TICKS_PER_SECOND;
    Client::HudModel model{};
    model.deck = true;
    model.resources = Client::SiegeModel::Resources(state);
    model.objective = Client::SiegeModel::Objective(state);
    for (const auto& [width, height, scale] :
         {std::tuple{1280U, 720U, 1.0F}, std::tuple{1920U, 1080U, 1.0F}, std::tuple{1920U, 1080U, 1.5F}})
    {
      Client::Hud hud;
      std::vector<Client::ScreenQuad> quads;
      hud.Build(model, width, height, scale, quads);
      const Client::PixelRect bar = hud.Panels().front();
      for (const Client::HudText& text : hud.Texts())
      {
        Assert::IsTrue(text.cell.Inside(text.element));
      }
      // Every quad the top bar's band holds stays inside it, the progress bar's among them.
      for (const Client::ScreenQuad& quad : quads)
      {
        if (quad.yPixels < static_cast<float>(bar.heightPixels))
        {
          Assert::IsTrue(quad.yPixels + quad.heightPixels <= static_cast<float>(bar.heightPixels));
        }
      }
    }
  }

  TEST_METHOD(TheEndBannerFollowsTheOutcomeWithTheTime)
  {
    Protocol::StateUpdate state = Trial();
    Assert::IsFalse(Client::SiegeModel::EndBanner(state).has_value());
    state.tick = (23 * TICKS_PER_MINUTE) + (41 * Client::TickClock::TICKS_PER_SECOND);
    for (const auto& [outcome, banner] : {std::pair{Protocol::TrialOutcome::Won, "THE KEEP HAS FALLEN · 23:41"},
                                          std::pair{Protocol::TrialOutcome::LostToClock, "TIME HAS RUN OUT · 23:41"},
                                          std::pair{Protocol::TrialOutcome::LostArmy, "YOUR ARMY IS GONE · 23:41"}})
    {
      state.trial.outcome = outcome;
      const std::optional<Client::HudMode> end = Client::SiegeModel::EndBanner(state);
      Assert::IsTrue(end.has_value());
      Assert::AreEqual(std::string(banner), end.value_or(Client::HudMode{}).banner);
    }
  }

  TEST_METHOD(PauseStopsTheClockAndNothingElse)
  {
    Client::SiegeModel model;
    Client::TickClock clock;
    Assert::IsTrue(clock.Advance(1.0, 1.0) > 0);
    model.TogglePause();
    Assert::IsTrue(model.Paused());
    Assert::IsTrue(model.PauseBanner().has_value());
    clock.SetPaused(model.Paused());
    Assert::AreEqual(std::uint32_t{0}, clock.Advance(1.0, 1.0));
    // The top bar still reads, and the card still gives its orders, which wait for the next tick.
    const Protocol::StateUpdate state = Trial();
    Assert::AreEqual(std::string("39:55"), Client::SiegeModel::Resources(state).at(2).value);
    Protocol::StateUpdate field = state;
    field.squads.push_back({.id = 1,
                            .owner = ME,
                            .units = {1, 2, 3},
                            .hasGoal = false,
                            .goalVoxels = {},
                            .shortOfGoal = false,
                            .garrisonedIn = Protocol::NO_ENTITY,
                            .kind = Protocol::UnitKind::Spearman,
                            .order = Protocol::SquadOrder::Idle,
                            .fullFigures = 6});
    Client::Selection selection;
    const std::array<Protocol::EntityId, 1> squad = {1};
    Client::Selector::Select(selection, field, squad, false);
    const Client::HudCommandCard card = Client::BuildCard(selection, field, Client::CardCommand::None);
    Client::Session session;
    Client::Orders orders;
    static_cast<void>(orders.Update({.hotkey = 'X'}, selection, field, card, session));
    Assert::AreEqual(std::size_t{1}, session.TakeCommands().size());
    // Again, and the clock goes on.
    model.TogglePause();
    clock.SetPaused(model.Paused());
    Assert::IsFalse(model.PauseBanner().has_value());
    Assert::IsTrue(clock.Advance(1.0, 1.0) > 0);
  }
};

} // namespace ClientTests
