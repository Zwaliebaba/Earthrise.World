#include "pch.h"

#include "Hud.h"
#include "ReasonPrompt.h"
#include "SiegeLog.h"
#include "Strings.h"
#include "UiScale.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The Siege playtest's reason prompt (M5-C3; GameDesign.md, M5's values, item 8): the first stone, blow and dig of a game
// each stop the clock and ask why there, once each; Space takes the prompt down and gives its row; a bare --siege,
// --watch or --replay never prompts; and the banner fits the screen at every size the owner runs.

namespace ClientTests
{

namespace
{

constexpr Protocol::VoxelPosition E3_FACE{.xVoxels = 371, .yVoxels = 11, .zVoxels = 330};
constexpr Protocol::VoxelPosition GH_GATE{.xVoxels = 309, .yVoxels = 10, .zVoxels = 272};

std::string BannerOf(const Client::ReasonPrompt& _prompt)
{
  const std::optional<Client::HudMode> banner = _prompt.Banner();
  return banner.has_value() ? banner->banner : std::string();
}

} // namespace

TEST_CLASS(PromptTests)
{
public:
  TEST_METHOD(AGamesFirstStoneOnE3PromptsAndItsSecondDoesNot)
  {
    Client::ReasonPrompt prompt(true);
    Assert::IsFalse(prompt.Up());
    prompt.Raise(600, Client::SiegeWork::Stone, "E3", E3_FACE, 30.0);
    Assert::IsTrue(prompt.Up());
    Assert::AreEqual(std::string("WHY E3? SAY IT ALOUD · SPACE GOES ON"), BannerOf(prompt));
    Assert::IsTrue(prompt.Mark().value_or(Protocol::VoxelPosition{}) == E3_FACE);
    const std::optional<Client::ReasonAnswered> answered = prompt.GoOn(42.5);
    Assert::IsTrue(answered.has_value());
    Assert::AreEqual(600U, answered.value_or(Client::ReasonAnswered{}).ask.tick);
    Assert::AreEqual(12.5, answered.value_or(Client::ReasonAnswered{}).secondsUp, 1e-9);
    Assert::IsFalse(prompt.Up());
    prompt.Raise(1000, Client::SiegeWork::Stone, "S3", E3_FACE, 50.0);
    Assert::IsFalse(prompt.Up());
    Assert::IsFalse(prompt.GoOn(51.0).has_value());
  }

  TEST_METHOD(ABlowAndADigEachPromptOnceAndTwoInOneTickWaitInTurn)
  {
    Client::ReasonPrompt prompt(true);
    prompt.Raise(900, Client::SiegeWork::Blow, "GH", GH_GATE, 45.0);
    prompt.Raise(900, Client::SiegeWork::Trench, "E3", E3_FACE, 45.0);
    prompt.Raise(1300, Client::SiegeWork::Blow, "GH", GH_GATE, 65.0);
    Assert::AreEqual(std::string("WHY GH? SAY IT ALOUD · SPACE GOES ON"), BannerOf(prompt));
    const std::optional<Client::ReasonAnswered> blow = prompt.GoOn(50.0);
    Assert::IsTrue(blow.has_value() && blow->ask.work == Client::SiegeWork::Blow);
    // The dig waits behind the blow, and is up from when the blow went.
    Assert::AreEqual(std::string("WHY E3? SAY IT ALOUD · SPACE GOES ON"), BannerOf(prompt));
    const std::optional<Client::ReasonAnswered> dig = prompt.GoOn(53.0);
    Assert::IsTrue(dig.has_value() && dig->ask.work == Client::SiegeWork::Trench);
    Assert::AreEqual(3.0, dig.value_or(Client::ReasonAnswered{}).secondsUp, 1e-9);
    Assert::IsFalse(prompt.Up());
  }

  TEST_METHOD(APromptNotEnabledNeverPrompts)
  {
    // A bare --siege, --watch and --replay: the run builds its prompt not enabled.
    Client::ReasonPrompt prompt(false);
    prompt.Raise(600, Client::SiegeWork::Stone, "E3", E3_FACE, 30.0);
    prompt.Raise(900, Client::SiegeWork::Blow, "GH", GH_GATE, 45.0);
    prompt.Raise(900, Client::SiegeWork::Trench, "E3", E3_FACE, 45.0);
    Assert::IsFalse(prompt.Up());
    Assert::IsFalse(prompt.Banner().has_value());
    Assert::IsFalse(prompt.Mark().has_value());
  }

  TEST_METHOD(AnAskedRowHasEveryColumn)
  {
    Client::SiegeLog log;
    log.Asked(600, "E3", Client::SiegeWork::Stone, 12.5);
    const std::vector<std::string> rows = log.TakeRows();
    Assert::AreEqual(std::size_t{1}, rows.size());
    Assert::AreEqual(std::string("asked,600,E3,stone,12.50,,"), rows.front());
  }

  TEST_METHOD(TheBannerFitsTheScreenAtEverySizeTheOwnerRuns)
  {
    // The widest name the castle gives a structure, at 1280 x 720 and 1920 x 1080, at each UI scale the width allows
    // (MvpPlan.md, M4, input 21): 1 at 1280 px, and 1 and 1.5 at 1920 px.
    Client::ReasonPrompt prompt(true);
    prompt.Raise(600, Client::SiegeWork::Stone, "KEEP", E3_FACE, 30.0);
    Client::HudModel model;
    model.mode = prompt.Banner().value_or(Client::HudMode{});
    for (const auto& [width, height, scales] :
         {std::tuple{1280U, 720U, std::vector<float>{1.0F}}, std::tuple{1920U, 1080U, std::vector<float>{1.0F, 1.5F}}})
    {
      for (const float scale : scales)
      {
        Client::Hud hud;
        std::vector<Client::ScreenQuad> quads;
        hud.Build(model, width, height, scale, quads);
        // The banner is the one text below the top bar, as HudTests finds it.
        const std::int32_t barHeight = Client::ScaledPixels(56.0F, scale);
        std::vector<Client::HudText> banner;
        std::ranges::copy_if(hud.Texts(), std::back_inserter(banner),
                             [barHeight](const Client::HudText& _text) { return _text.cell.yPixels >= barHeight; });
        Assert::IsFalse(banner.empty());
        const Client::PixelRect screen{0, 0, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height)};
        for (const Client::HudText& text : banner)
        {
          Assert::IsTrue(text.cell.Inside(text.element));
          Assert::IsTrue(text.element.Inside(screen));
        }
      }
    }
  }
};

} // namespace ClientTests
