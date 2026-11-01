#include "pch.h"

#include "HudAlerts.h"
#include "Look.h"
#include "Overlays.h"
#include "Session.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The readability views and their alerts (M3-F1 to F3; ADR-021, decision 4; Hud.md §10).

namespace ClientTests
{

namespace
{

constexpr Protocol::EntityId W2 = 12;
constexpr Protocol::EntityId COLLAPSE = 40;

/// A state with W2 and one collapse of it: a piece of 9 voxels, 3 x 3 in a face, and a strip of 10 cells.
Protocol::StateUpdate Collapsing(std::uint32_t _warningTicksLeft, Protocol::EntityId _structure = W2)
{
  Protocol::StateUpdate state{};
  state.tick = 100;
  state.structures.push_back({.id = W2, .owner = 2, .templateId = "T3", .originVoxels = {375, 9, 180}, .quarterTurns = 0, .name = "W2"});
  Protocol::CollapseView collapse{};
  collapse.id = COLLAPSE;
  collapse.structure = _structure;
  collapse.warningTicksLeft = _warningTicksLeft;
  collapse.fallTicksLeft = 26;
  for (std::int32_t y = 11; y < 14; ++y)
  {
    for (std::int32_t x = 379; x < 382; ++x)
    {
      collapse.piece.push_back({x, y, 180});
    }
  }
  for (std::int32_t x = 376; x < 386; ++x)
  {
    collapse.strip.push_back({x, 9, 179});
  }
  state.collapses.push_back(collapse);
  return state;
}

/// Gives _session _state, as the executable does each tick. A session is built in place and never moved: with MSVC's
/// library a map's move may allocate, so Session's move may throw (bugprone-exception-escape).
void Tell(Client::Session& _session, const Protocol::StateUpdate& _state)
{
  _session.Receive(_state, {}, {});
}

} // namespace

TEST_CLASS(OverlaysTests)
{
public:
  TEST_METHOD(DrawsACollapsesPieceAndStripInBad)
  {
    Client::Session session;
    Tell(session, Collapsing(135));
    const Client::Overlays overlays;
    const std::vector<Client::OverlayLayer> layers = overlays.Layers(session, 0.25, true);
    std::size_t records = 0;
    for (const Client::OverlayLayer& layer : layers)
    {
      records += layer.boxes.size();
      for (const Client::OverlayBox& box : layer.boxes)
      {
        Assert::AreEqual(Client::OVERLAY_BAD_INDEX, box.paletteIndex);
      }
    }
    Assert::AreEqual(std::size_t{19}, records); // 9 voxels of the piece and 10 cells of the strip

    // The strip lies on the ground, the voxel under each cell where units are hit.
    const auto strip = std::ranges::find_if(layers, [](const Client::OverlayLayer& _layer) { return _layer.boxes.size() == 10; });
    Assert::IsTrue(strip != layers.end());
    for (const Client::OverlayBox& box : strip->boxes)
    {
      Assert::AreEqual(8, box.cellVoxels.yVoxels);
    }

    // The piece pulses over a second, and the preview draws only with the integrity view on.
    const auto pieceAlpha = [&overlays, &session](double _seconds)
    {
      for (const Client::OverlayLayer& layer : overlays.Layers(session, _seconds, true))
      {
        if (layer.boxes.size() == 9)
        {
          return layer.alpha;
        }
      }
      return -1.0F;
    };
    Assert::AreEqual(Client::PIECE_MIN_ALPHA, pieceAlpha(0.0), 1.0e-5F);
    Assert::AreEqual(Client::PIECE_MAX_ALPHA, pieceAlpha(0.5), 1.0e-5F);
    Assert::AreEqual(pieceAlpha(0.2), pieceAlpha(1.2), 1.0e-5F);
    for (const Client::OverlayLayer& layer : overlays.Layers(session, 0.25, false))
    {
      Assert::IsTrue(layer.boxes.empty());
    }
    // A collapse that is falling has no preview.
    Client::Session falling;
    Tell(falling, Collapsing(0));
    for (const Client::OverlayLayer& layer : Client::Overlays{}.Layers(falling, 0.25, true))
    {
      Assert::IsTrue(layer.boxes.empty());
    }
  }

  TEST_METHOD(CountsTheAlertDownInWholeSecondsRoundingUp)
  {
    const Protocol::StateUpdate state = Collapsing(135);
    for (const auto& [ticks, text] :
         {std::pair{135U, "W2 WILL FALL IN 7 S"}, std::pair{121U, "W2 WILL FALL IN 7 S"}, std::pair{120U, "W2 WILL FALL IN 6 S"},
          std::pair{21U, "W2 WILL FALL IN 2 S"}, std::pair{20U, "W2 WILL FALL IN 1 S"}, std::pair{1U, "W2 WILL FALL IN 1 S"}})
    {
      Protocol::CollapseView collapse = state.collapses.front();
      collapse.warningTicksLeft = ticks;
      Assert::AreEqual(std::string(text), Client::Overlays::WillFallText(state, collapse));
    }

    // Raised each frame in place, urgent and at the piece, and replaced by the fall's alert, which then expires.
    Client::Overlays overlays;
    Client::HudAlerts alerts;
    Client::Session told;
    Tell(told, Collapsing(135));
    overlays.Update(told, 1.0, alerts);
    Tell(told, Collapsing(119));
    overlays.Update(told, 1.8, alerts);
    Assert::AreEqual(std::size_t{1}, alerts.Shown().size());
    const Client::HudAlert countdown = alerts.Shown().front();
    Assert::AreEqual(std::string("W2 WILL FALL IN 6 S"), countdown.text);
    Assert::IsTrue(countdown.urgent);
    Assert::IsTrue(countdown.location == std::optional<Protocol::VoxelPosition>(Protocol::VoxelPosition{380, 11, 180}));
    Tell(told, Collapsing(0));
    overlays.Update(told, 8.0, alerts);
    Assert::AreEqual(std::size_t{1}, alerts.Shown().size());
    Assert::AreEqual(std::string("W2 HAS FALLEN"), alerts.Shown().front().text);
    alerts.Expire(8.0 + Client::HudAlerts::LIFETIME_SECONDS);
    Assert::AreEqual(std::size_t{0}, alerts.Shown().size());
  }

  TEST_METHOD(NamesACollapseWithNoStructureAWall)
  {
    const Protocol::StateUpdate state = Collapsing(40, Protocol::NO_ENTITY);
    Assert::AreEqual(std::string("A WALL WILL FALL IN 2 S"), Client::Overlays::WillFallText(state, state.collapses.front()));
    Assert::AreEqual(std::string("A WALL HAS FALLEN"), Client::Overlays::HasFallenText(state, state.collapses.front()));
  }
};

} // namespace ClientTests
