#include "pch.h"

#include "ByteReader.h"
#include "ByteWriter.h"
#include "FireMode.h"
#include "Look.h"
#include "Session.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// FIRE, the attack cursor and click-to-fire (M3-E4; Hud.md §7 and §8).

namespace ClientTests
{

namespace
{

constexpr Protocol::EntityId TREBUCHET = 4;
constexpr Protocol::VoxelPosition WALL_FOOT{.xVoxels = 380, .yVoxels = 9, .zVoxels = 180};

Protocol::EngineView Trebuchet(std::uint32_t _reloadTicks = 0)
{
  return {.id = TREBUCHET,
          .owner = 1,
          .kind = Protocol::EngineKind::Trebuchet,
          .cellVoxels = {380, 9, 140},
          .quarterTurns = 3,
          .reloadTicks = _reloadTicks};
}

Client::FireMode::Input Pressing(bool _f, bool _escape = false)
{
  return {
    .fPressed = _f, .escapePressed = _escape, .rightClicked = false, .shiftHeld = false, .pointedVoxels = WALL_FOOT, .worldChanged = false};
}

Client::FireMode::Input RightClick(bool _shift)
{
  return {.fPressed = false,
          .escapePressed = false,
          .rightClicked = true,
          .shiftHeld = _shift,
          .pointedVoxels = WALL_FOOT,
          .worldChanged = false};
}

/// The fires the session queued since the last call, decoded.
std::vector<Protocol::FireEngine> TakeFires(Client::Session& _session)
{
  std::vector<Protocol::FireEngine> fires;
  for (const std::vector<std::byte>& bytes : _session.TakeCommands())
  {
    Core::ByteReader reader(bytes);
    const Protocol::Command command = Protocol::DecodeCommand(reader);
    const auto* fire = std::get_if<Protocol::FireEngine>(&command);
    Assert::IsNotNull(fire);
    if (fire != nullptr)
    {
      fires.push_back(*fire);
    }
  }
  return fires;
}

/// Answers every aim check the session asked with _reading, as the simulation's answer would.
void Answer(Client::Session& _session, const Protocol::AimReading& _reading)
{
  std::vector<std::vector<std::byte>> answers;
  for (const std::vector<std::byte>& bytes : _session.TakeQueries())
  {
    Core::ByteReader reader(bytes);
    const Protocol::QueryRequest request = Protocol::DecodeQueryRequest(reader);
    Assert::IsTrue(std::holds_alternative<Protocol::AimCheck>(request.query));
    Core::ByteWriter writer;
    Protocol::Encode(Protocol::Answer{.queryId = request.id, .tick = 1, .reading = _reading}, writer);
    answers.push_back(writer.TakeBytes());
  }
  _session.Receive(Protocol::StateUpdate{}, {}, answers);
}

} // namespace

TEST_CLASS(FireModeTests)
{
public:
  TEST_METHOD(EntersOnFWithATrebuchetSelectedAndSetsTheBanner)
  {
    Client::Session session;
    Client::FireMode fire;
    const Protocol::EngineView trebuchet = Trebuchet();
    fire.Update(Pressing(false), &trebuchet, session);
    Assert::IsFalse(fire.Active());
    Assert::IsTrue(fire.Mode().command.empty());
    fire.Update(Pressing(true), &trebuchet, session);
    Assert::IsTrue(fire.Active());
    Assert::AreEqual(std::string("FIRE"), fire.Mode().command);
    Assert::AreEqual(std::string("FIRE · RIGHT-CLICK A TARGET"), fire.Mode().banner);
    // It asks for the voxel under the pointer once, and again only when it moves or the world changes.
    Assert::AreEqual(std::size_t{1}, session.TakeQueries().size());
    fire.Update(Pressing(false), &trebuchet, session);
    Assert::AreEqual(std::size_t{0}, session.TakeQueries().size());
    Client::FireMode::Input moved = Pressing(false);
    moved.pointedVoxels = Protocol::VoxelPosition{381, 9, 180};
    fire.Update(moved, &trebuchet, session);
    Assert::AreEqual(std::size_t{1}, session.TakeQueries().size());
    moved.worldChanged = true;
    fire.Update(moved, &trebuchet, session);
    Assert::AreEqual(std::size_t{1}, session.TakeQueries().size());
  }

  TEST_METHOD(FiresAtThePickedVoxelOnARightClickAndLeavesTheMode)
  {
    Client::Session session;
    Client::FireMode fire;
    const Protocol::EngineView trebuchet = Trebuchet();
    fire.Update(Pressing(true), &trebuchet, session);
    fire.Update(RightClick(false), &trebuchet, session);
    Assert::IsFalse(fire.Active());
    const std::vector<Protocol::FireEngine> fires = TakeFires(session);
    Assert::AreEqual(std::size_t{1}, fires.size());
    Assert::IsTrue(fires.front() == Protocol::FireEngine{.engine = TREBUCHET, .targetVoxels = WALL_FOOT});
    Assert::AreEqual(std::size_t{1}, session.TakenSources().size());
    Assert::AreEqual(std::string("click"), session.TakenSources().front());
  }

  TEST_METHOD(StaysInTheModeWithShift)
  {
    Client::Session session;
    Client::FireMode fire;
    const Protocol::EngineView trebuchet = Trebuchet();
    fire.Update(Pressing(true), &trebuchet, session);
    fire.Update(RightClick(true), &trebuchet, session);
    Assert::IsTrue(fire.Active());
    Assert::AreEqual(std::size_t{1}, TakeFires(session).size());
    // While it reloads, a right click fires nothing, and the mode stays.
    const Protocol::EngineView reloading = Trebuchet(120);
    fire.Update(RightClick(true), &reloading, session);
    Assert::IsTrue(fire.Active());
    Assert::AreEqual(std::size_t{0}, TakeFires(session).size());
  }

  TEST_METHOD(LeavesTheModeOnEscape)
  {
    Client::Session session;
    Client::FireMode fire;
    const Protocol::EngineView trebuchet = Trebuchet();
    fire.Update(Pressing(true), &trebuchet, session);
    Assert::IsTrue(fire.Update(Pressing(false, true), &trebuchet, session)); // it used Esc
    Assert::IsFalse(fire.Active());
    Assert::IsFalse(fire.Cursor(session).has_value());
    // With the mode off, Esc is not the mode's: it deselects.
    Assert::IsFalse(fire.Update(Pressing(false, true), &trebuchet, session));
  }

  TEST_METHOD(DoesNothingWithoutASelectionOrWhileItReloads)
  {
    Client::Session session;
    Client::FireMode fire;
    fire.Update(Pressing(true), nullptr, session);
    Assert::IsFalse(fire.Active());
    const Protocol::EngineView reloading = Trebuchet(120);
    fire.Update(Pressing(true), &reloading, session);
    Assert::IsFalse(fire.Active());
    fire.Update(RightClick(false), &reloading, session);
    Assert::AreEqual(std::size_t{0}, TakeFires(session).size());
    Assert::AreEqual(std::size_t{0}, session.TakeQueries().size());
    // Deselecting turns the mode off.
    const Protocol::EngineView trebuchet = Trebuchet();
    fire.Update(Pressing(true), &trebuchet, session);
    Assert::IsTrue(fire.Active());
    fire.Update(Pressing(false), nullptr, session);
    Assert::IsFalse(fire.Active());
    // A right click with the trebuchet selected and no mode on fires too (Hud.md §8, Order).
    fire.Update(RightClick(false), &trebuchet, session);
    Assert::AreEqual(std::size_t{1}, TakeFires(session).size());
  }

  TEST_METHOD(ReadsTheCursorFromTheVerdictAndTheDistance)
  {
    Client::Session session;
    Client::FireMode fire;
    const Protocol::EngineView trebuchet = Trebuchet();
    fire.Update(Pressing(true), &trebuchet, session);
    // Until the answer comes, an ellipsis in dim, and no strike mark.
    Assert::AreEqual(std::string("…"), fire.Cursor(session).value_or(Client::AttackCursor{}).label);
    Assert::AreEqual(Client::DIM_SRGB, fire.Cursor(session).value_or(Client::AttackCursor{}).srgb);
    Assert::IsTrue(fire.StrikeMark(session).empty());

    const Protocol::VoxelPosition strike{380, 10, 180};
    const struct
    {
      Protocol::AimVerdict verdict;
      std::int32_t meters;
      std::string label;
      std::uint32_t srgb;
      std::size_t marks;
      std::uint8_t markIndex;
    } cases[] = {{Protocol::AimVerdict::InRange, 40, "IN RANGE · 40 m", Client::GOOD_SRGB, 1, Client::OVERLAY_ACCENT_INDEX},
                 {Protocol::AimVerdict::Blocked, 41, "BLOCKED · 41 m", Client::BAD_SRGB, 1, Client::OVERLAY_BAD_INDEX},
                 {Protocol::AimVerdict::TooClose, 10, "TOO CLOSE · 10 m", Client::BAD_SRGB, 0, 0},
                 {Protocol::AimVerdict::OutOfRange, 60, "OUT OF RANGE · 60 m", Client::BAD_SRGB, 0, 0}};
    for (const auto& item : cases)
    {
      Client::FireMode::Input input = Pressing(false);
      input.worldChanged = true; // so it asks again
      fire.Update(input, &trebuchet, session);
      Answer(session, {.verdict = item.verdict, .distanceMeters = item.meters, .strikes = true, .strikeVoxels = strike});
      const Client::AttackCursor cursor = fire.Cursor(session).value_or(Client::AttackCursor{});
      Assert::AreEqual(item.label, cursor.label);
      Assert::AreEqual(item.srgb, cursor.srgb);
      const std::vector<Client::OverlayBox> marks = fire.StrikeMark(session);
      Assert::AreEqual(item.marks, marks.size());
      if (!marks.empty())
      {
        Assert::IsTrue(marks.front().cellVoxels == strike);
        Assert::AreEqual(item.markIndex, marks.front().paletteIndex);
      }
    }
  }
};

} // namespace ClientTests
