#include "pch.h"

#include "ArrowDraw.h"
#include "Look.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The arrows as the client hands them to the line pass (M4-H2; ADR-018), with no GPU: where each is between ticks and
// which way it points.

namespace ClientTests
{

namespace
{

constexpr std::uint8_t ATTACKER = 1;
constexpr std::uint8_t DEFENDER = 2;
constexpr float CLOSE = 0.0001F;

Protocol::FixedPosition AtMeters(float _x, float _y, float _z)
{
  const auto raw = [](float _meters)
  { return Core::Fixed::FromRaw(static_cast<std::int32_t>(std::lround(_meters * Core::Fixed::ONE_RAW))); };
  return {.xMeters = raw(_x), .yMeters = raw(_y), .zMeters = raw(_z)};
}

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

Protocol::ShotView Arrow(Protocol::EntityId _id, std::uint8_t _owner, float _x, float _y, float _z)
{
  return {.id = _id, .engine = 50, .position = AtMeters(_x, _y, _z), .kind = Protocol::ShotKind::Arrow, .owner = _owner};
}

} // namespace

TEST_CLASS(ArrowDrawTests)
{
public:
  TEST_METHOD(AnArrowTwoMetersAlongInATickIsDrawnOnItsLineAndPointsAlongItsFlight)
  {
    // A defender's arrow flying down and south, 2 m in the tick: 1.6 m along -z and 1.2 m down.
    Protocol::StateUpdate before = Update(10);
    before.shots.push_back(Arrow(9, DEFENDER, 100.0F, 20.0F, 300.0F));
    Protocol::StateUpdate after = Update(11);
    after.shots.push_back(Arrow(9, DEFENDER, 100.0F, 18.8F, 298.4F));
    std::vector<Client::ArrowDrawn> arrows;
    for (const double fraction : {0.25, 0.5, 0.75})
    {
      Client::PlaceArrows(before, after, fraction, arrows);
      Assert::AreEqual(std::size_t{1}, arrows.size());
      const Client::ArrowDrawn& arrow = arrows.front();
      // On its line, as far along as the clock is, never snapped to its last position.
      const auto along = static_cast<float>(fraction);
      Assert::AreEqual(100.0F, arrow.middleMeters.x, CLOSE);
      Assert::AreEqual(20.0F - (1.2F * along), arrow.middleMeters.y, CLOSE);
      Assert::AreEqual(300.0F - (1.6F * along), arrow.middleMeters.z, CLOSE);
      Assert::AreEqual(0.0F, arrow.direction.x, CLOSE);
      Assert::AreEqual(-0.6F, arrow.direction.y, CLOSE);
      Assert::AreEqual(-0.8F, arrow.direction.z, CLOSE);
      Assert::AreEqual(Client::DEFENDER_COLOR_INDEX, arrow.sideColorIndex);
    }
  }

  TEST_METHOD(ANewArrowWaitsForItsSecondPositionAndAStoneIsNoArrow)
  {
    Protocol::StateUpdate before = Update(10);
    before.shots.push_back(
      {.id = 8, .engine = 5, .position = AtMeters(0.0F, 20.0F, 0.0F), .kind = Protocol::ShotKind::Stone, .owner = ATTACKER});
    Protocol::StateUpdate after = Update(11);
    after.shots = before.shots;
    after.shots.front().position = AtMeters(12.0F, 20.0F, 0.0F);
    after.shots.push_back(Arrow(9, ATTACKER, 100.0F, 20.0F, 300.0F));
    std::vector<Client::ArrowDrawn> arrows;
    Client::PlaceArrows(before, after, 0.5, arrows);
    Assert::IsTrue(arrows.empty());
    // A tick later it has a flight, and is drawn in the attacker's colour.
    Protocol::StateUpdate later = Update(12);
    later.shots.push_back(Arrow(9, ATTACKER, 102.0F, 20.0F, 300.0F));
    Client::PlaceArrows(after, later, 0.5, arrows);
    Assert::AreEqual(std::size_t{1}, arrows.size());
    Assert::AreEqual(101.0F, arrows.front().middleMeters.x, CLOSE);
    Assert::AreEqual(1.0F, arrows.front().direction.x, CLOSE);
    Assert::AreEqual(Client::ATTACKER_COLOR_INDEX, arrows.front().sideColorIndex);
  }
};

} // namespace ClientTests
