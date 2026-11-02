#include "pch.h"

#include "FireTally.h"
#include "Hud.h"
#include "HudAlerts.h"
#include "HudModel.h"
#include "Strings.h"
#include "UiScale.h"

#include <limits>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The HUD's first pieces (M3-E2; Hud.md §2, §3 and §10).

namespace ClientTests
{

namespace
{

constexpr std::array<float, 3> SCALES = {1.0F, 1.5F, 2.0F};
// The owner's runs: 1280 x 720 and 1920 x 1080 (MvpPlan-M3.md, M3-E2).
constexpr std::array<std::uint32_t, 2> WIDTHS_PIXELS = {1280, 1920};
constexpr std::array<Client::HudControl, 4> CAMERA_BUTTONS = {Client::HudControl::TurnLeft, Client::HudControl::TurnRight,
                                                              Client::HudControl::ZoomOut, Client::HudControl::ZoomIn};

Client::HudAlert Alert(std::string _text, std::string _sub, bool _jump)
{
  return {.text = std::move(_text),
          .sub = std::move(_sub),
          .location = _jump ? std::optional<Protocol::VoxelPosition>(Protocol::VoxelPosition{1, 2, 3}) : std::nullopt,
          .urgent = false,
          .expiresSeconds = std::numeric_limits<double>::infinity()};
}

/// The breach lab's top bar at its widest figures, the FIRE banner, and three alerts.
Client::HudModel WidestModel()
{
  constexpr std::uint32_t LONGEST_TICK = ((59 * 60) + 59) * 20; // 59:59
  return {.resources = Client::BreachResources(LONGEST_TICK, 99),
          .objective = {.title = std::string(Client::BREACH_OBJECTIVE_TITLE), .detail = std::string(Client::BREACH_OBJECTIVE_DETAIL)},
          .mode = {.command = "FIRE",
                   .banner = std::string(Client::FIRE_BANNER),
                   .buttons = {{.control = Client::HudControl::Cancel, .label = std::string(Client::CANCEL_ESC)}}},
          .alerts = {Alert("W2 WILL FALL IN 10 S", "", true), Alert("W2 HAS FALLEN", "THE BREACH IS OPEN", true),
                     Alert("HIT W2", "REPAIR LOCKED 10 S", false)},
          .selectedEngine = Protocol::NO_ENTITY,
          .integrityView = true,
          .passabilityView = false};
}

std::vector<Client::HudTarget> CameraTargets(const Client::Hud& _hud)
{
  std::vector<Client::HudTarget> targets;
  for (const Client::HudTarget& target : _hud.Targets())
  {
    if (std::ranges::find(CAMERA_BUTTONS, target.control) != CAMERA_BUTTONS.end())
    {
      targets.push_back(target);
    }
  }
  return targets;
}

} // namespace

TEST_CLASS(HudTests)
{
public:
  TEST_METHOD(HitsEachCameraButtonOverASquareOf44LogicalPixels)
  {
    for (const auto& [scale, side] : {std::pair{1.0F, 44}, std::pair{1.5F, 66}, std::pair{2.0F, 88}})
    {
      Client::Hud hud;
      std::vector<Client::ScreenQuad> quads;
      hud.Build(WidestModel(), 1280, scale, quads);
      const std::vector<Client::HudTarget> targets = CameraTargets(hud);
      Assert::AreEqual(CAMERA_BUTTONS.size(), targets.size());
      for (std::size_t index = 0; index < targets.size(); ++index)
      {
        Assert::IsTrue(targets[index].control == CAMERA_BUTTONS.at(index)); // ↺ ↻ - +, from the left
        Assert::AreEqual(side, targets[index].rect.widthPixels);
        Assert::AreEqual(side, targets[index].rect.heightPixels);
        Assert::AreEqual(0, targets[index].rect.yPixels); // the top bar's height
        if (index != 0)
        {
          // They abut: a pixel is one button's or the next's, never both's.
          Assert::AreEqual(targets[index - 1].rect.xPixels + side, targets[index].rect.xPixels);
        }
      }
      Assert::IsTrue(targets.back().rect.xPixels + side <= 1280);
    }
  }

  TEST_METHOD(ConsumesAClickOnAButtonAndNamesIt)
  {
    Client::Hud hud;
    std::vector<Client::ScreenQuad> quads;
    hud.Build(WidestModel(), 1280, 1.0F, quads);
    for (const Client::HudTarget& target : CameraTargets(hud))
    {
      // Its corners, inside the 44 x 44 target though outside the 36 x 36 button.
      for (const auto& [x, y] :
           {std::pair{target.rect.xPixels, target.rect.yPixels}, std::pair{target.rect.xPixels + 43, target.rect.yPixels + 43}})
      {
        const std::optional<Client::HudTarget> hit = hud.HitTest(x, y);
        Assert::IsTrue(hit.has_value() && hit->control == target.control);
        Assert::IsTrue(hud.Covers(x, y));
      }
    }
    // The banner's CANCEL · ESC, and an alert's JUMP with its index.
    const std::span<const Client::HudTarget> targets = hud.Targets();
    const auto cancel = std::ranges::find(targets, Client::HudControl::Cancel, &Client::HudTarget::control);
    Assert::IsTrue(cancel != targets.end());
    const std::optional<Client::HudTarget> cancelHit = hud.HitTest(cancel->rect.xPixels + 1, cancel->rect.yPixels + 1);
    Assert::IsTrue(cancelHit.has_value() && cancelHit->control == Client::HudControl::Cancel);
    Assert::AreEqual(std::ptrdiff_t{2}, std::ranges::count(targets, Client::HudControl::Jump, &Client::HudTarget::control));
    const auto secondJump = std::ranges::find_if(targets, [](const Client::HudTarget& _target)
                                                 { return _target.control == Client::HudControl::Jump && _target.alert == 1; });
    Assert::IsTrue(secondJump != targets.end());
    // The top bar covers the world across its width, and a pixel below everything is the world's.
    Assert::IsTrue(hud.Covers(640, 43));
    Assert::IsFalse(hud.HitTest(640, 43).has_value());
    Assert::IsFalse(hud.Covers(640, 44));
    Assert::IsFalse(hud.Covers(1000, 400));
  }

  TEST_METHOD(ShowsThreeAlertsAndAFourthPushesTheOldestOut)
  {
    Client::HudAlerts alerts;
    for (const std::string_view text : {"FIRST", "SECOND", "THIRD"})
    {
      alerts.Push({.text = std::string(text), .sub = "", .location = std::nullopt, .urgent = false, .expiresSeconds = 10.0});
    }
    Assert::AreEqual(std::size_t{3}, alerts.Shown().size());
    alerts.Push({.text = "FOURTH", .sub = "", .location = std::nullopt, .urgent = true, .expiresSeconds = 20.0});
    Assert::AreEqual(std::size_t{3}, alerts.Shown().size());
    Assert::AreEqual(std::string("FOURTH"), alerts.Shown()[0].text); // newest first
    Assert::AreEqual(std::string("THIRD"), alerts.Shown()[1].text);
    Assert::AreEqual(std::string("SECOND"), alerts.Shown()[2].text);
    // Each goes when its time runs out.
    alerts.Expire(9.9);
    Assert::AreEqual(std::size_t{3}, alerts.Shown().size());
    alerts.Expire(10.0);
    Assert::AreEqual(std::size_t{1}, alerts.Shown().size());
    Assert::AreEqual(std::string("FOURTH"), alerts.Shown()[0].text);

    // The HUD draws at most three, with a hit target for each JUMP.
    Client::HudModel model = WidestModel();
    model.alerts.push_back(Alert("A FOURTH", "", true));
    Client::Hud hud;
    std::vector<Client::ScreenQuad> quads;
    hud.Build(model, 1280, 1.0F, quads);
    Assert::AreEqual(std::ptrdiff_t{2}, std::ranges::count(hud.Targets(), Client::HudControl::Jump, &Client::HudTarget::control));
  }

  TEST_METHOD(FitsEveryLabelInItsElementAtEveryScale)
  {
    // 2 resources of 2 strings, the objective's 2, 4 buttons, the banner's 2, and the alerts' 3 lines, 2 subs and 2
    // JUMPs. At 1280 px and a UI scale of 2 the top bar is 640 logical px wide, and the room between the resources and
    // the buttons, 202 logical px at the widest figures, takes the objective's title but not its detail, so the detail
    // alone is left out rather than drawn over anything.
    constexpr std::size_t ALL_TEXTS = 4 + 2 + 4 + 2 + 3 + 2 + 2;
    for (const float scale : SCALES)
    {
      for (const std::uint32_t width : WIDTHS_PIXELS)
      {
        Client::Hud hud;
        std::vector<Client::ScreenQuad> quads;
        hud.Build(WidestModel(), width, scale, quads);
        const std::span<const Client::HudText> texts = hud.Texts();
        const bool detailFits = !(scale == 2.0F && width == 1280);
        Assert::AreEqual(detailFits ? ALL_TEXTS : ALL_TEXTS - 1, texts.size());
        const Client::PixelRect screen{0, 0, static_cast<std::int32_t>(width), 1080};
        for (std::size_t index = 0; index < texts.size(); ++index)
        {
          const Client::HudText& text = texts[index];
          Assert::IsTrue(text.cell.Inside(text.element));
          Assert::IsTrue(text.element.Inside(screen));
          for (std::size_t other = index + 1; other < texts.size(); ++other)
          {
            Assert::IsFalse(text.cell.Overlaps(texts[other].cell));
          }
          // A string of the top bar's own never runs under a camera button's target.
          const bool inButton =
            std::ranges::any_of(CameraTargets(hud), [&text](const Client::HudTarget& _target) { return text.cell.Inside(_target.rect); });
          if (!inButton)
          {
            for (const Client::HudTarget& target : CameraTargets(hud))
            {
              Assert::IsFalse(text.cell.Overlaps(target.rect));
            }
          }
        }
      }
    }
  }

  TEST_METHOD(StacksTheQuestionsButtonsUnderItWhereOneRowWouldNotFit)
  {
    // The lab's question with its two answers (M3-G1): one row at 1920 px and a UI scale of 1, two at 1280 px and 2,
    // and at every size every string inside its element, nothing over anything, and the banner on screen.
    Client::HudModel model = WidestModel();
    model.alerts.clear();
    model.mode = {.command = "QUESTION",
                  .banner = std::format(Client::QUESTION_BANNER, 5),
                  .buttons = {{.control = Client::HudControl::AnswerOpen, .label = std::string(Client::OPEN_BUTTON)},
                              {.control = Client::HudControl::AnswerShut, .label = std::string(Client::SHUT_BUTTON)}}};
    for (const float scale : SCALES)
    {
      for (const std::uint32_t width : WIDTHS_PIXELS)
      {
        Client::Hud hud;
        std::vector<Client::ScreenQuad> quads;
        hud.Build(model, width, scale, quads);
        const std::span<const Client::HudTarget> targets = hud.Targets();
        const auto open = std::ranges::find(targets, Client::HudControl::AnswerOpen, &Client::HudTarget::control);
        const auto shut = std::ranges::find(targets, Client::HudControl::AnswerShut, &Client::HudTarget::control);
        Assert::IsTrue(open != targets.end() && shut != targets.end());
        Assert::IsTrue(open->rect.xPixels >= 0 && shut->rect.xPixels + shut->rect.widthPixels <= static_cast<std::int32_t>(width));
        Assert::IsFalse(open->rect.Overlaps(shut->rect));
        const std::span<const Client::HudText> texts = hud.Texts();
        const Client::PixelRect screen{0, 0, static_cast<std::int32_t>(width), 1080};
        for (std::size_t index = 0; index < texts.size(); ++index)
        {
          Assert::IsTrue(texts[index].cell.Inside(texts[index].element));
          Assert::IsTrue(texts[index].element.Inside(screen));
          for (std::size_t other = index + 1; other < texts.size(); ++other)
          {
            Assert::IsFalse(texts[index].cell.Overlaps(texts[other].cell));
          }
        }
        const bool stacked = open->rect.yPixels > Client::ScaledPixels(56.0F + 8.0F, scale);
        if (width == 1920 && scale == 1.0F)
        {
          Assert::IsFalse(stacked);
        }
        if (width == 1280 && scale == 2.0F)
        {
          Assert::IsTrue(stacked);
        }
      }
    }
  }

  TEST_METHOD(CentersAButtonlessBannersTextAndTheObjectiveOnTheScreen)
  {
    // M3-H2: with no button after it, the banner's text has as much room on its right as on its left, and the
    // objective's middle is the screen's, at the owner's two widths and every scale. The lab's two longest banners: the
    // practice item has no prior shots, and an item has at most 4.
    Client::HudModel model = WidestModel();
    model.alerts.clear();
    model.objective = {.title = std::format(Client::ITEM_OBJECTIVE, 16, 16), .detail = "", .detailOff = false};
    for (const std::string_view text : {"SETTING UP THE PRACTICE ITEM · 10×", "SETTING UP ITEM 16 · 4 SHOTS · 10×"})
    {
      model.mode = {.command = "WATCH", .banner = std::string(text), .buttons = {}};
      for (const float scale : SCALES)
      {
        for (const std::uint32_t width : WIDTHS_PIXELS)
        {
          Client::Hud hud;
          std::vector<Client::ScreenQuad> quads;
          hud.Build(model, width, scale, quads);
          const std::int32_t barHeight = Client::ScaledPixels(56.0F, scale);
          std::vector<Client::HudText> banner;
          std::ranges::copy_if(hud.Texts(), std::back_inserter(banner),
                               [barHeight](const Client::HudText& _text) { return _text.cell.yPixels >= barHeight; });
          Assert::AreEqual(std::size_t{1}, banner.size());
          const Client::PixelRect& cell = banner.front().cell;
          const Client::PixelRect& inside = banner.front().element;
          const std::int32_t left = cell.xPixels - inside.xPixels;
          const std::int32_t right = (inside.xPixels + inside.widthPixels) - (cell.xPixels + cell.widthPixels);
          Assert::AreEqual(left, right);
          Assert::IsTrue(cell.Inside(inside));
          Assert::IsTrue(inside.Inside({0, 0, static_cast<std::int32_t>(width), 1080}));
          Assert::IsTrue(std::abs((2 * inside.xPixels) + inside.widthPixels - static_cast<std::int32_t>(width)) <= 1);
          // The objective's title is laid after the camera buttons' four labels and the two resources' four strings.
          const Client::PixelRect& title = hud.Texts()[8].cell;
          Assert::IsTrue(std::abs((2 * title.xPixels) + title.widthPixels - static_cast<std::int32_t>(width)) <= 1);
        }
      }
    }
  }

  TEST_METHOD(CountsTheShotsFromTheReloadRising)
  {
    Client::FireTally tally;
    const auto update = [](std::uint32_t _tick, std::uint32_t _reloadTicks)
    {
      Protocol::StateUpdate state{};
      state.tick = _tick;
      state.engines.push_back({.id = 7, .owner = 1, .kind = {}, .cellVoxels = {}, .quarterTurns = 0, .reloadTicks = _reloadTicks});
      return state;
    };
    tally.Observe(update(1, 0));
    tally.Observe(update(2, 160)); // fired
    tally.Observe(update(3, 159));
    tally.Observe(update(4, 0));
    tally.Observe(update(5, 160)); // fired again
    Assert::AreEqual(2U, tally.Fired(1));
    Assert::AreEqual(0U, tally.Fired(2));
    Assert::IsTrue(tally.LastFiredTick(7) == std::optional<std::uint32_t>(5));
    Assert::IsFalse(tally.LastFiredTick(8).has_value());

    const std::vector<Client::HudResource> resources = Client::BreachResources((61 * 20) + 19, 2);
    Assert::AreEqual(std::string("TIME"), resources.at(0).key);
    Assert::AreEqual(std::string("1:01"), resources.at(0).value);
    Assert::AreEqual(std::string("SHOTS"), resources.at(1).key);
    Assert::AreEqual(std::string("2"), resources.at(1).value);
  }
};

} // namespace ClientTests
