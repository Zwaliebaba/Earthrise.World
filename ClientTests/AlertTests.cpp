#include "pch.h"

#include "AlertWatch.h"
#include "Hud.h"
#include "HudAlerts.h"
#include "Session.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// M4's alerts (M4-I6; Hud.md §10), with no GPU: each one's trigger, its text and its place, JUMP and Space, and a
// refusal that gives one alert however often it comes.

namespace ClientTests
{

namespace
{

constexpr std::uint8_t ME = 1;
constexpr std::uint8_t FOE = 2;
constexpr Protocol::EntityId E3 = 30;
constexpr Protocol::EntityId W5 = 50;

/// The foe's sections E3, at x 100 to 103, and W5, at x 40 to 43, both 4 x 10 x 4 from z 60 and y 9.
Protocol::StateUpdate Castle(std::uint32_t _tick)
{
  Protocol::StateUpdate state{};
  state.tick = _tick;
  state.player = ME;
  state.structures.push_back({.id = E3,
                              .owner = FOE,
                              .templateId = "T3",
                              .originVoxels = {100, 9, 60},
                              .quarterTurns = 0,
                              .extentVoxels = {4, 10, 4},
                              .name = "E3"});
  state.structures.push_back(
    {.id = W5, .owner = FOE, .templateId = "T3", .originVoxels = {40, 9, 60}, .quarterTurns = 0, .extentVoxels = {4, 10, 4}, .name = "W5"});
  return state;
}

void AddSquad(Protocol::StateUpdate& _state, Protocol::EntityId _squad, std::uint8_t _owner, Protocol::UnitKind _kind,
              const Protocol::VoxelPosition& _cell)
{
  const Protocol::EntityId unit = _squad * 10;
  _state.squads.push_back({.id = _squad,
                           .owner = _owner,
                           .units = {unit},
                           .hasGoal = false,
                           .goalVoxels = {},
                           .shortOfGoal = false,
                           .garrisonedIn = Protocol::NO_ENTITY,
                           .kind = _kind});
  _state.units.push_back({.id = unit, .squad = _squad, .owner = _owner, .kind = _kind, .position = {}, .cellVoxels = _cell});
}

/// Gives _session _state, as the executable does each tick, and lets _watch read it at _seconds.
void Tick(Client::Session& _session, Client::AlertWatch& _watch, Client::HudAlerts& _alerts, const Protocol::StateUpdate& _state,
          double _seconds)
{
  _session.Receive(_state, {}, {});
  _watch.Observe(_session, _seconds, _alerts);
}

} // namespace

TEST_CLASS(AlertTests)
{
public:
  TEST_METHOD(SappersAreAnnouncedOnceASquadAtTheStructureNearestThem)
  {
    Client::Session session;
    Client::AlertWatch watch;
    Client::HudAlerts alerts;
    Protocol::StateUpdate state = Castle(1);
    AddSquad(state, 7, FOE, Protocol::UnitKind::Sapper, {98, 9, 58}); // by E3
    AddSquad(state, 8, ME, Protocol::UnitKind::Sapper, {44, 9, 58});  // the player's own: none
    Tick(session, watch, alerts, state, 1.0);
    std::vector<Client::HudAlert> shown = alerts.Shown();
    Assert::AreEqual(std::size_t{1}, shown.size());
    Assert::AreEqual(std::string("SAPPERS AT E3"), shown.front().text);
    Assert::IsFalse(shown.front().urgent);
    Assert::IsTrue(shown.front().location == Protocol::VoxelPosition{98, 9, 58});
    // Seen again, nothing; a second squad, by W5, its own.
    state.tick = 2;
    AddSquad(state, 9, FOE, Protocol::UnitKind::Sapper, {39, 9, 59});
    Tick(session, watch, alerts, state, 2.0);
    shown = alerts.Shown();
    Assert::AreEqual(std::size_t{2}, shown.size());
    Assert::AreEqual(std::string("SAPPERS AT W5"), shown.front().text);
  }

  TEST_METHOD(ABreachIsAnnouncedTheFirstTimeASectionReadsOpened)
  {
    Client::Session session;
    Client::AlertWatch watch;
    Client::HudAlerts alerts;
    Protocol::StateUpdate state = Castle(1);
    Tick(session, watch, alerts, state, 1.0);
    Assert::IsTrue(alerts.Shown().empty());
    state.tick = 2;
    state.structures.front().opened = true;
    Tick(session, watch, alerts, state, 2.0);
    state.tick = 3;
    Tick(session, watch, alerts, state, 3.0);
    const std::vector<Client::HudAlert> shown = alerts.Shown();
    Assert::AreEqual(std::size_t{1}, shown.size());
    Assert::AreEqual(std::string("BREACH OPEN AT E3"), shown.front().text);
    Assert::IsTrue(shown.front().location == Protocol::VoxelPosition{102, 9, 62}); // its foot's middle
  }

  TEST_METHOD(APlayersShotOnASectionLocksItsRepairAndTheNextHitReplacesTheAlert)
  {
    Client::Session session;
    Client::AlertWatch watch;
    Client::HudAlerts alerts;
    const auto at = [](std::int32_t _whole) { return Core::Fixed::FromRaw(_whole * Core::Fixed::ONE_RAW); };
    // The player's stone, 5, and the foe's, 6, in flight.
    Protocol::StateUpdate state = Castle(1);
    state.shots.push_back({.id = 5, .engine = 90, .position = {at(60), at(20), at(40)}, .kind = Protocol::ShotKind::Stone, .owner = ME});
    state.shots.push_back({.id = 6, .engine = 91, .position = {at(60), at(20), at(80)}, .kind = Protocol::ShotKind::Stone, .owner = FOE});
    Tick(session, watch, alerts, state, 1.0);
    // Both strike W5, gone from the update they struck in; W5's repair is locked 10 s.
    state.tick = 2;
    state.shots.clear();
    state.hits = {{.shot = 5, .voxel = {41, 12, 60}}, {.shot = 6, .voxel = {42, 12, 63}}};
    state.structures.back().repairLockTicks = 200;
    Tick(session, watch, alerts, state, 2.0);
    std::vector<Client::HudAlert> shown = alerts.Shown();
    Assert::AreEqual(std::size_t{1}, shown.size());
    Assert::AreEqual(std::string("HIT W5 · REPAIR LOCKED 10 S"), shown.front().text);
    Assert::IsTrue(shown.front().location == Protocol::VoxelPosition{41, 12, 60});
    // The next of the player's shots on W5 replaces it where it stands.
    state.tick = 3;
    state.shots.push_back({.id = 7, .engine = 90, .position = {at(41), at(13), at(60)}, .kind = Protocol::ShotKind::Stone, .owner = ME});
    state.hits = {{.shot = 7, .voxel = {40, 13, 60}}};
    state.structures.back().repairLockTicks = 190;
    Tick(session, watch, alerts, state, 3.0);
    shown = alerts.Shown();
    Assert::AreEqual(std::size_t{1}, shown.size());
    Assert::IsTrue(shown.front().location == Protocol::VoxelPosition{40, 13, 60});
  }

  TEST_METHOD(ARefusalGivesOneAlertNotOneATick)
  {
    Client::Session session;
    Client::AlertWatch watch;
    Client::HudAlerts alerts;
    Protocol::StateUpdate state = Castle(1);
    state.refusals = {{.commandTag = 3, .reason = Protocol::Refusal::NotEnoughStone},
                      {.commandTag = 4, .reason = Protocol::Refusal::Reloading}};
    for (std::uint32_t tick = 1; tick <= 5; ++tick)
    {
      state.tick = tick;
      Tick(session, watch, alerts, state, static_cast<double>(tick));
      Tick(session, watch, alerts, state, static_cast<double>(tick) + 0.5); // the same tick again, as a paused answer brings it
    }
    std::vector<Client::HudAlert> shown = alerts.Shown();
    Assert::AreEqual(std::size_t{1}, shown.size());
    Assert::AreEqual(std::string("NOT ENOUGH STONE"), shown.front().text);
    Assert::IsFalse(shown.front().location.has_value());
    state.tick = 6;
    state.refusals = {{.commandTag = 5, .reason = Protocol::Refusal::NotEnoughTimber}};
    Tick(session, watch, alerts, state, 6.0);
    shown = alerts.Shown();
    Assert::AreEqual(std::size_t{2}, shown.size());
    Assert::AreEqual(std::string("NOT ENOUGH TIMBER"), shown.front().text);
  }

  TEST_METHOD(JumpAndSpaceGoToTheNewestAlertWithAPlace)
  {
    Client::Session session;
    Client::AlertWatch watch;
    Client::HudAlerts alerts;
    Protocol::StateUpdate state = Castle(1);
    state.structures.front().opened = true;
    Tick(session, watch, alerts, state, 1.0);
    state.tick = 2;
    state.refusals = {{.commandTag = 3, .reason = Protocol::Refusal::NotEnoughStone}};
    Tick(session, watch, alerts, state, 2.0);
    const std::vector<Client::HudAlert> shown = alerts.Shown();
    Assert::AreEqual(std::size_t{2}, shown.size());
    // Space: past the refusal, which has no place, to the breach's.
    Assert::IsTrue(Client::AlertWatch::NewestPlace(shown) == Protocol::VoxelPosition{102, 9, 62});
    // JUMP: on the breach's alert alone.
    Client::HudModel model{};
    model.alerts = shown;
    Client::Hud hud;
    std::vector<Client::ScreenQuad> quads;
    hud.Build(model, 1280, 720, 1.0F, quads);
    std::vector<Client::HudTarget> jumps;
    std::ranges::copy_if(hud.Targets(), std::back_inserter(jumps),
                         [](const Client::HudTarget& _target) { return _target.control == Client::HudControl::Jump; });
    Assert::AreEqual(std::size_t{1}, jumps.size());
    Assert::AreEqual(std::size_t{1}, jumps.front().alert);
    Assert::IsFalse(Client::AlertWatch::NewestPlace(std::span<const Client::HudAlert>{}).has_value());
  }
};

} // namespace ClientTests
