#include "pch.h"

#include "CommandCard.h"
#include "Minimap.h"
#include "Overlays.h"
#include "SelectionPanel.h"
#include "Session.h"
#include "SiegeModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The control's client (M4-J3; ADR-028, decision 6; Hud.md §1): the HUD model built from one state in both versions
// differs in the health bars over the wall sections and in HEALTH in place of the critical part's row on towers, the
// gatehouse and the keep, and in nothing else, compared field by field.

namespace ClientTests
{

namespace
{

constexpr std::uint8_t ME = 1;
constexpr std::uint8_t FOE = 2;

/// The castle's four kinds of structure, the foe's, and one squad of the player's; with hit points in the control.
Protocol::StateUpdate Castle(bool _control)
{
  Protocol::StateUpdate state{};
  state.tick = 400;
  state.player = ME;
  state.widthCells = 128;
  state.depthCells = 128;
  state.trial.ticksLeft = 40000;
  state.trial.holdTicksNeeded = 2400;
  state.players = {{.player = ME, .side = Protocol::Side::Attacker, .stoneHundredthsCubicMeters = 900, .timberHundredthsCubicMeters = 0},
                   {.player = FOE, .side = Protocol::Side::Defender, .stoneHundredthsCubicMeters = 0, .timberHundredthsCubicMeters = 0}};
  const auto add = [&state, _control](Protocol::EntityId _id, std::string_view _template, std::string_view _name, std::int32_t _x)
  {
    Protocol::StructureView structure{.id = _id,
                                      .owner = FOE,
                                      .templateId = std::string(_template),
                                      .originVoxels = {_x, 9, 60},
                                      .quarterTurns = 0,
                                      .extentVoxels = {4, 10, 4},
                                      .name = std::string(_name)};
    structure.criticalPercent = 80;
    structure.garrisonCapacity = 6;
    structure.repairLockTicks = 60;
    if (_control)
    {
      structure.hitPoints = 450;
      structure.maxHitPoints = 900;
    }
    state.structures.push_back(structure);
  };
  add(10, "T3", "W1", 20);
  add(11, "T4", "NE", 40);
  add(12, "T5", "GH", 60);
  add(13, "T2", "KEEP", 80);
  state.squads.push_back({.id = 1,
                          .owner = ME,
                          .units = {100},
                          .hasGoal = false,
                          .goalVoxels = {},
                          .shortOfGoal = false,
                          .garrisonedIn = Protocol::NO_ENTITY,
                          .kind = Protocol::UnitKind::Spearman,
                          .order = Protocol::SquadOrder::Idle,
                          .fullFigures = 6});
  state.units.push_back({.id = 100,
                         .squad = 1,
                         .owner = ME,
                         .kind = Protocol::UnitKind::Spearman,
                         .position = {},
                         .cellVoxels = {.xVoxels = 30, .yVoxels = 9, .zVoxels = 30}});
  return state;
}

/// Seen from straight above, 128 m square onto the target.
DirectX::XMFLOAT4X4 Overhead()
{
  DirectX::XMFLOAT4X4 matrix{};
  matrix.m[0][0] = 2.0F / 128.0F;
  matrix.m[2][1] = 2.0F / 128.0F;
  matrix.m[3][0] = -1.0F;
  matrix.m[3][1] = -1.0F;
  matrix.m[3][3] = 1.0F;
  return matrix;
}

/// The parts of the HUD model the state gives, as Earthrise builds them each frame, with _structure selected.
Client::HudModel ModelOf(const Client::Session& _session, Protocol::EntityId _structure)
{
  const Protocol::StateUpdate& state = _session.State();
  const Client::Selection selection{.squads = {}, .structure = _structure, .activeKind = std::nullopt};
  Client::HudModel model{};
  model.deck = true;
  model.resources = Client::SiegeModel::Resources(state);
  model.objective = Client::SiegeModel::Objective(state);
  model.panel = Client::BuildPanel(selection, state, _session.Integrity(_structure));
  model.commandCard = Client::BuildCard(selection, state, Client::CardCommand::None);
  model.minimap.dots = Client::Minimap::Dots(state, selection, 188);
  model.worldLabels = Client::Overlays::WorldLabels(_session, Overhead(), 1920, 1080, 84.0F, _structure, true);
  model.healthBars = Client::Overlays::HealthBars(_session, Overhead(), 1920, 1080);
  return model;
}

} // namespace

TEST_CLASS(ControlParityTests)
{
public:
  TEST_METHOD(TheControlDiffersInTheHealthBarsAndTheHealthRowsAlone)
  {
    Client::Session voxelSession;
    voxelSession.Receive(Castle(false), {}, {});
    Client::Session controlSession;
    controlSession.Receive(Castle(true), {}, {});
    for (const auto& [structure, part] : {std::pair{Protocol::EntityId{10}, ""}, std::pair{Protocol::EntityId{11}, "PLATFORM"},
                                          std::pair{Protocol::EntityId{12}, "GATE"}, std::pair{Protocol::EntityId{13}, "TOWER"}})
    {
      const Client::HudModel voxel = ModelOf(voxelSession, structure);
      const Client::HudModel control = ModelOf(controlSession, structure);
      // The top bar, the card, the minimap and the lines in the world are the same.
      Assert::AreEqual(voxel.resources.size(), control.resources.size());
      for (std::size_t index = 0; index < voxel.resources.size(); ++index)
      {
        Assert::AreEqual(voxel.resources.at(index).value, control.resources.at(index).value);
        Assert::AreEqual(voxel.resources.at(index).warn, control.resources.at(index).warn);
      }
      Assert::AreEqual(voxel.objective.title, control.objective.title);
      Assert::AreEqual(voxel.commandCard.kind, control.commandCard.kind);
      for (std::size_t slot = 0; slot < voxel.commandCard.slots.size(); ++slot)
      {
        Assert::AreEqual(voxel.commandCard.slots.at(slot).label, control.commandCard.slots.at(slot).label);
        Assert::AreEqual(voxel.commandCard.slots.at(slot).enabled, control.commandCard.slots.at(slot).enabled);
      }
      Assert::AreEqual(voxel.minimap.dots.size(), control.minimap.dots.size());
      Assert::AreEqual(voxel.worldLabels.size(), control.worldLabels.size());
      for (std::size_t index = 0; index < voxel.worldLabels.size(); ++index)
      {
        Assert::AreEqual(voxel.worldLabels.at(index).line, control.worldLabels.at(index).line);
      }
      // The panel, but for the critical part's row, which the control reads as HEALTH.
      Assert::AreEqual(voxel.panel.name, control.panel.name);
      Assert::AreEqual(voxel.panel.enemy, control.panel.enemy);
      Assert::AreEqual(voxel.panel.portrait.icon, control.panel.portrait.icon);
      Assert::IsTrue(voxel.panel.hit == control.panel.hit);
      Assert::AreEqual(voxel.panel.hitText, control.panel.hitText);
      Assert::AreEqual(voxel.panel.rows.size(), control.panel.rows.size());
      for (std::size_t row = 0; row < voxel.panel.rows.size(); ++row)
      {
        const Client::HudStatRow& voxelRow = voxel.panel.rows.at(row);
        const Client::HudStatRow& controlRow = control.panel.rows.at(row);
        if (voxelRow.key == part)
        {
          Assert::AreEqual(std::string("80%"), voxelRow.value);
          Assert::AreEqual(std::string("HEALTH"), controlRow.key);
          Assert::AreEqual(std::string("450 / 900"), controlRow.value);
          continue;
        }
        Assert::AreEqual(voxelRow.key, controlRow.key);
        Assert::AreEqual(voxelRow.value, controlRow.value);
      }
      // The health bars: none in the voxel version, one over the wall section, at half, in the control.
      Assert::IsTrue(voxel.healthBars.empty());
      Assert::AreEqual(std::size_t{1}, control.healthBars.size());
      Assert::AreEqual(0.5F, control.healthBars.front().fraction);
    }
  }

  TEST_METHOD(APoolBelowNothingDrawsAnEmptyBar)
  {
    Protocol::StateUpdate state = Castle(true);
    state.structures.front().hitPoints = -300; // a hit past 0 takes the pool on down (the owner, 2026-11-02)
    Client::Session session;
    session.Receive(state, {}, {});
    const std::vector<Client::HudHealthBar> bars = Client::Overlays::HealthBars(session, Overhead(), 1920, 1080);
    Assert::AreEqual(std::size_t{1}, bars.size());
    Assert::AreEqual(0.0F, bars.front().fraction);
  }
};

} // namespace ClientTests
