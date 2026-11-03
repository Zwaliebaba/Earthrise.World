#include "pch.h"

#include "Camera.h"
#include "Hud.h"
#include "HudModel.h"
#include "Look.h"
#include "Strings.h"
#include "UiScale.h"

#include <limits>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Layout A (M4-I1; Hud.md §2 and §3): the deck along the bottom, the control groups' strip above it, the scale's cap,
// and the camera framing the area between the top bar and the deck.

namespace ClientTests
{

namespace
{

constexpr std::array<float, 3> STEPS = {1.0F, 1.5F, 2.0F};
// The owner's runs: 1280 x 720 and 1920 x 1080 (MvpPlan-M4.md, M4-I1).
constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 2> SCREENS = {{{1280, 720}, {1920, 1080}}};

Client::HudAlert Alert(std::string _text, std::string _sub)
{
  return {.text = std::move(_text),
          .sub = std::move(_sub),
          .location = Protocol::VoxelPosition{1, 2, 3},
          .urgent = false,
          .expiresSeconds = std::numeric_limits<double>::infinity()};
}

/// The Siege's HUD at its fullest M4-I1 has: the top bar, the FIRE banner, three alerts with JUMP, the integrity
/// readout of a section with its every row, and the deck.
Client::HudModel FullestModel()
{
  constexpr std::uint32_t LONGEST_TICK = ((59 * 60) + 59) * 20; // 59:59
  Client::HudReadout readout{.title = "W12", .rows = {}};
  for (const std::string_view key : {"GROUND", "HOLE", "GAP", "OPENING", "FALLS", "LOCKED"})
  {
    readout.rows.push_back({.key = std::string(key),
                            .value = "ON EARTH · CAN BE UNDERMINED",
                            .srgb = Client::TEXT_SRGB,
                            .flashing = false,
                            .fill = key == "HOLE" ? std::optional<float>(0.5F) : std::nullopt});
  }
  return {
    .resources = Client::BreachResources(LONGEST_TICK, 99),
    .objective = {.title = std::string(Client::SIEGE_OBJECTIVE_TITLE), .detail = {}},
    .mode = {.command = "FIRE",
             .banner = std::string(Client::FIRE_BANNER),
             .buttons = {{.control = Client::HudControl::Cancel, .label = std::string(Client::CANCEL_ESC)}}},
    .alerts = {Alert("W12 WILL FALL IN 10 S", ""), Alert("W12 HAS FALLEN", "THE BREACH IS OPEN"), Alert("HIT W12", "REPAIR LOCKED 10 S")},
    .selectedEngine = Protocol::NO_ENTITY,
    .integrityView = true,
    .passabilityView = false,
    .readout = readout,
    .selection = std::nullopt,
    .deck = true};
}

Client::Hud Built(const Client::HudModel& _model, std::uint32_t _width, std::uint32_t _height, float _scale)
{
  Client::Hud hud;
  std::vector<Client::ScreenQuad> quads;
  hud.Build(_model, _width, _height, _scale, quads);
  return hud;
}

} // namespace

TEST_CLASS(HudLayoutTests)
{
public:
  TEST_METHOD(AtFullHdTheDecksColumnsAreTheMinimapTheRestAndTheCard)
  {
    const Client::Hud hud = Built(FullestModel(), 1920, 1080, 1.0F);
    Assert::IsTrue(hud.Deck().has_value());
    const Client::HudDeck deck = hud.Deck().value_or(Client::HudDeck{});
    Assert::AreEqual(1080 - 204, deck.deck.yPixels);
    Assert::AreEqual(204, deck.deck.heightPixels);
    Assert::AreEqual(8, deck.minimap.xPixels);
    Assert::AreEqual(188, deck.minimap.widthPixels);
    Assert::AreEqual(188, deck.minimap.heightPixels);
    Assert::AreEqual((4 * 112) + (3 * 4), deck.card.widthPixels);
    Assert::AreEqual(1920 - 8, deck.card.xPixels + deck.card.widthPixels);
    Assert::AreEqual(1920 - 8 - 188 - 8 - 8 - ((4 * 112) + (3 * 4)) - 8, deck.selection.widthPixels);
    for (const Client::PixelRect& cell : deck.cells)
    {
      Assert::AreEqual(112, cell.widthPixels);
      Assert::AreEqual(60, cell.heightPixels);
      Assert::IsTrue(cell.Inside(deck.card));
    }
    // The strip: groups 1 to 5 at 120 x 44 and SET at 64 x 44, from left 212, its foot 212 above the bottom.
    Assert::AreEqual(212, deck.groups.front().xPixels);
    Assert::AreEqual(1080 - 212, deck.groups.front().yPixels + deck.groups.front().heightPixels);
    for (std::size_t slot = 0; slot < deck.groups.size(); ++slot)
    {
      Assert::AreEqual(slot + 1 == deck.groups.size() ? 64 : 120, deck.groups.at(slot).widthPixels);
      Assert::AreEqual(44, deck.groups.at(slot).heightPixels);
    }
    Assert::AreEqual(876, deck.groups.back().xPixels + deck.groups.back().widthPixels);
  }

  TEST_METHOD(AtItsNarrowestTheDeckKeepsTheSelectionsWidth)
  {
    // 972 logical px: the card's cells shrink to 100 and the selection keeps 340.
    const Client::Hud hud = Built(FullestModel(), 972, 720, 1.0F);
    const Client::HudDeck deck = hud.Deck().value_or(Client::HudDeck{});
    Assert::AreEqual(100, deck.cells.front().widthPixels);
    Assert::AreEqual(340, deck.selection.widthPixels);
  }

  TEST_METHOD(TheScaleCapsAtOneAt1280AndAtOneAndAHalfAt1920WhateverTheDpi)
  {
    for (const float step : STEPS)
    {
      Assert::AreEqual(1.0F, Client::CappedUiScale(step, 1280));
      Assert::AreEqual(std::min(step, 1.5F), Client::CappedUiScale(step, 1920));
    }
    for (const std::uint32_t dpi : {96U, 144U, 192U})
    {
      Assert::AreEqual(1.0F, Client::CappedUiScale(Client::UiScaleForDpi(dpi), 1280));
      Assert::IsTrue(Client::CappedUiScale(Client::UiScaleForDpi(dpi), 1920) <= 1.5F);
    }
    Assert::AreEqual(2.0F, Client::CappedUiScale(2.0F, 2560));
  }

  TEST_METHOD(TheCamerasLookAtPointSitsInTheMiddleOfTheUncoveredArea)
  {
    for (const auto& [width, height] : SCREENS)
    {
      const float scale = Client::CappedUiScale(2.0F, width);
      const float topBar = Client::TOP_BAR_LOGICAL_PIXELS * scale;
      const float deck = Client::Hud::DECK_LOGICAL_PIXELS * scale;
      const DirectX::XMFLOAT3 target{120.5F, 9.0F, 240.5F};
      const Client::Camera camera(target);
      const DirectX::XMMATRIX viewProjection =
        camera.ViewProjection(static_cast<float>(width) / static_cast<float>(height), height, topBar, deck);
      DirectX::XMFLOAT3 clip{};
      DirectX::XMStoreFloat3(&clip, DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&target), viewProjection));
      const float yPixels = (1.0F - clip.y) * 0.5F * static_cast<float>(height);
      Assert::AreEqual(0.5F * (topBar + (static_cast<float>(height) - deck)), yPixels, 0.01F);
      Assert::AreEqual(0.0F, clip.x, 0.0001F);
    }
  }

  TEST_METHOD(NothingOverlapsAtEveryScaleTheWidthAllows)
  {
    const Client::HudModel model = FullestModel();
    for (const auto& [width, height] : SCREENS)
    {
      const auto screen = Client::PixelRect{0, 0, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height)};
      for (const float step : STEPS)
      {
        if (Client::CappedUiScale(step, width) != step)
        {
          continue; // the width does not allow it
        }
        const Client::Hud hud = Built(model, width, height, step);
        const std::span<const Client::PixelRect> panels = hud.Panels();
        for (std::size_t first = 0; first < panels.size(); ++first)
        {
          Assert::IsTrue(panels[first].Inside(screen));
          for (std::size_t second = first + 1; second < panels.size(); ++second)
          {
            Assert::IsFalse(panels[first].Overlaps(panels[second]), std::format(L"panels {} and {} at {}", first, second, width).c_str());
          }
        }
        for (const Client::HudText& text : hud.Texts())
        {
          Assert::IsTrue(text.cell.Inside(text.element));
        }
        // The readout stands above the deck, and the newest alert shows: older ones that would meet the readout are left
        // out, as M3's are.
        Assert::IsTrue(hud.Readout().has_value());
        Assert::IsTrue(hud.Readout().value_or(Client::PixelRect{}).yPixels + hud.Readout().value_or(Client::PixelRect{}).heightPixels <=
                       hud.Deck().value_or(Client::HudDeck{}).deck.yPixels);
        const auto jumps = std::ranges::count_if(hud.Targets(), [](const Client::HudTarget& _target)
                                                 { return _target.control == Client::HudControl::Jump; });
        Assert::IsTrue(jumps >= 1);
      }
    }
  }

  TEST_METHOD(WithoutTheDeckTheReadoutKeepsM3sCorner)
  {
    Client::HudModel model = FullestModel();
    model.deck = false;
    const Client::Hud hud = Built(model, 1280, 720, 1.0F);
    Assert::IsFalse(hud.Deck().has_value());
    const Client::PixelRect readout = hud.Readout().value_or(Client::PixelRect{});
    Assert::AreEqual(8, readout.xPixels);
    Assert::AreEqual(720 - 8, readout.yPixels + readout.heightPixels);
  }
};

} // namespace ClientTests
