#include "pch.h"

#include "Hud.h"
#include "SelectionPanel.h"
#include "Strings.h"
#include "TextLayout.h"
#include "UiScale.h"

#include <limits>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The selection panel (M4-I2; Hud.md §5), with no GPU: what it reads for nothing, one thing and several, and that it
// fits the deck.

namespace ClientTests
{

namespace
{

constexpr std::uint8_t ME = 1;
constexpr std::uint8_t FOE = 2;
constexpr std::array<float, 3> STEPS = {1.0F, 1.5F, 2.0F};
constexpr float ROW_LOGICAL_PIXELS = 220.0F; // Hud.md §5: a stat row is at most about 220 px at 20 px

void AddSquad(Protocol::StateUpdate& _state, Protocol::EntityId _squad, Protocol::UnitKind _kind, std::uint8_t _owner, std::size_t _figures,
              std::uint8_t _full)
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
                            .fullFigures = _full};
  for (std::size_t figure = 0; figure < _figures; ++figure)
  {
    const Protocol::EntityId unit = (_squad * 10) + figure;
    squad.units.push_back(unit);
    _state.units.push_back(
      {.id = unit,
       .squad = _squad,
       .owner = _owner,
       .kind = _kind,
       .position = {},
       .cellVoxels = {.xVoxels = static_cast<std::int32_t>(_squad), .yVoxels = 9, .zVoxels = static_cast<std::int32_t>(figure)},
       .hitPoints = 30,
       .maxHitPoints = 45});
  }
  _state.squads.push_back(squad);
}

Protocol::StructureView Structure(Protocol::EntityId _id, std::string_view _template, std::uint8_t _owner)
{
  Protocol::StructureView view{
    .id = _id, .owner = _owner, .templateId = std::string(_template), .originVoxels = {}, .quarterTurns = 0, .name = "W12"};
  view.garrisonCount = 4;
  view.garrisonCapacity = 6;
  view.repairLockTicks = 200;
  view.criticalPercent = 100;
  return view;
}

/// Every kind and structure the panel shows alone, one at a time.
std::vector<Client::HudPanel> EveryOne()
{
  Protocol::StateUpdate state{};
  state.player = ME;
  state.trial.holdTicksNeeded = 2400;
  Protocol::EntityId id = 1;
  for (const Protocol::UnitKind kind : Client::KIND_ORDER)
  {
    AddSquad(state, id, kind, id % 2 == 0 ? ME : FOE, 6, 6);
    ++id;
  }
  state.engines.push_back({.id = 90,
                           .owner = ME,
                           .kind = Protocol::EngineKind::Trebuchet,
                           .cellVoxels = {},
                           .quarterTurns = 0,
                           .reloadTicks = 0,
                           .state = Protocol::EngineState::Deploying,
                           .squad = 1});
  for (const std::string_view templateId : {"T1", "T2", "T3", "T4", "T5", "T6", "T7", "T8", "T9"})
  {
    state.structures.push_back(Structure(100 + state.structures.size(), templateId, FOE));
  }
  std::vector<Client::HudPanel> panels;
  for (const Protocol::SquadView& squad : state.squads)
  {
    panels.push_back(Client::BuildPanel({.squads = {squad.id}, .structure = Protocol::NO_ENTITY, .activeKind = squad.kind}, state));
  }
  for (const Protocol::StructureView& structure : state.structures)
  {
    panels.push_back(Client::BuildPanel({.squads = {}, .structure = structure.id, .activeKind = std::nullopt}, state));
  }
  return panels;
}

Client::Hud Built(const Client::HudPanel& _panel, std::uint32_t _width, float _scale)
{
  Client::HudModel model{};
  model.deck = true;
  model.panel = _panel;
  Client::Hud hud;
  std::vector<Client::ScreenQuad> quads;
  hud.Build(model, _width, 720, _scale, quads);
  return hud;
}

} // namespace

TEST_CLASS(SelectionPanelTests)
{
public:
  TEST_METHOD(EveryRowFitsIts220PixelsAtEveryScale)
  {
    for (const Client::HudPanel& panel : EveryOne())
    {
      Assert::IsTrue(panel.shows == Client::HudPanel::Shows::One);
      Assert::IsTrue(panel.rows.size() <= 3);
      for (const Client::HudStatRow& row : panel.rows)
      {
        for (const float step : STEPS)
        {
          const std::uint32_t scale = Client::TextLayout::PixelsPerFontPixel(Client::TextSize::Label, step);
          const std::uint32_t width = Client::TextLayout::Measure(row.key + " " + row.value, scale);
          Assert::IsTrue(static_cast<std::int32_t>(width) <= Client::ScaledPixels(ROW_LOGICAL_PIXELS, step),
                         std::format(L"a row of {} px at {}", width, step).c_str());
        }
      }
    }
  }

  TEST_METHOD(EachPanelFitsTheNarrowestSelectionAtEveryScale)
  {
    // The selection is at its narrowest, 340 logical px, on a 972 px deck at 1, and every text lies in its element.
    std::vector<Client::HudPanel> panels = EveryOne();
    panels.emplace_back(); // nothing selected
    for (const Client::HudPanel& panel : panels)
    {
      for (const float step : STEPS)
      {
        const auto width = static_cast<std::uint32_t>(Client::ScaledPixels(Client::DECK_MIN_LOGICAL_PIXELS, step));
        const Client::Hud hud = Built(panel, width, step);
        Assert::AreEqual(Client::ScaledPixels(340.0F, step), hud.Deck().value_or(Client::HudDeck{}).selection.widthPixels);
        for (const Client::HudText& text : hud.Texts())
        {
          Assert::IsTrue(text.cell.Inside(text.element),
                         std::format(L"\"{}\" at {}", std::wstring(panel.name.begin(), panel.name.end()), step).c_str());
        }
      }
    }
  }

  TEST_METHOD(SeveralSquadsReadAsHudMdSays)
  {
    Protocol::StateUpdate state{};
    state.player = ME;
    AddSquad(state, 1, Protocol::UnitKind::Archer, ME, 6, 6);
    AddSquad(state, 2, Protocol::UnitKind::Spearman, ME, 6, 6);
    AddSquad(state, 3, Protocol::UnitKind::Spearman, ME, 4, 6);
    const Client::HudPanel panel =
      Client::BuildPanel({.squads = {1, 2, 3}, .structure = Protocol::NO_ENTITY, .activeKind = Protocol::UnitKind::Archer}, state);
    Assert::IsTrue(panel.shows == Client::HudPanel::Shows::Several);
    Assert::AreEqual(std::string("3 SQUADS · TAB CYCLES TYPE"), panel.title);
    Assert::AreEqual(std::size_t{2}, panel.tiles.size());
    // Spearmen before archers, in Hud.md's order; the archers' tile is the active type's.
    Assert::AreEqual(std::string("SPEAR"), panel.tiles.at(0).label);
    Assert::AreEqual(std::string("2"), panel.tiles.at(0).count);
    Assert::AreEqual(std::string("10"), panel.tiles.at(0).figures);
    Assert::IsFalse(panel.tiles.at(0).active);
    Assert::AreEqual(std::string("ARCHER"), panel.tiles.at(1).label);
    Assert::IsTrue(panel.tiles.at(1).active);
    Assert::AreEqual(30.0F / 45.0F, panel.tiles.at(1).hit.value_or(0.0F), 0.0001F);
  }

  TEST_METHOD(OneSquadAndAnEnemysTowerReadAsHudMdSays)
  {
    Protocol::StateUpdate state{};
    state.player = ME;
    AddSquad(state, 1, Protocol::UnitKind::Spearman, ME, 5, 6);
    state.structures.push_back(Structure(70, "T4", FOE));
    Client::HudPanel panel =
      Client::BuildPanel({.squads = {1}, .structure = Protocol::NO_ENTITY, .activeKind = Protocol::UnitKind::Spearman}, state);
    Assert::AreEqual(std::string("SPEARMEN"), panel.name);
    Assert::IsFalse(panel.enemy);
    Assert::AreEqual(std::string("5 / 6"), panel.figures);
    Assert::AreEqual(std::string("30 / 45"), panel.hitText);
    Assert::IsTrue(panel.portrait.cell.has_value());
    Assert::AreEqual(std::string("STRONG"), panel.rows.at(0).key);
    panel = Client::BuildPanel({.squads = {}, .structure = 70, .activeKind = std::nullopt}, state);
    Assert::AreEqual(std::string("TOWER"), panel.name);
    Assert::IsTrue(panel.enemy);
    Assert::IsFalse(panel.portrait.icon.empty());
    // Only its owner's update counts a garrison.
    Assert::AreEqual(std::string("? / 6"), panel.rows.at(0).value);
    Assert::AreEqual(std::string("Locked · 10 s"), panel.rows.at(2).value);
    Assert::IsTrue(Client::BuildPanel({}, state).shows == Client::HudPanel::Shows::Nothing);
  }
};

} // namespace ClientTests
