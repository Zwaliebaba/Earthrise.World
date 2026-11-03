#include "pch.h"

#include "ControlGroups.h"
#include "Hud.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The control groups (M4-I4; Hud.md §3, §7 and §8), with no GPU: assigning, selecting and centring, the dead leaving,
// and the strip's slots.

namespace ClientTests
{

namespace
{

constexpr std::uint8_t ME = 1;

void AddSquad(Protocol::StateUpdate& _state, Protocol::EntityId _squad, Protocol::UnitKind _kind, std::int32_t _x, std::int32_t _z)
{
  const Protocol::EntityId unit = _squad * 10;
  _state.squads.push_back({.id = _squad,
                           .owner = ME,
                           .units = {unit},
                           .hasGoal = false,
                           .goalVoxels = {},
                           .shortOfGoal = false,
                           .garrisonedIn = Protocol::NO_ENTITY,
                           .kind = _kind});
  const auto meters = [](std::int32_t _whole) { return Core::Fixed::FromRaw(_whole * Core::Fixed::ONE_RAW); };
  _state.units.push_back({.id = unit,
                          .squad = _squad,
                          .owner = ME,
                          .kind = _kind,
                          .position = {.xMeters = meters(_x), .yMeters = meters(9), .zMeters = meters(_z)},
                          .cellVoxels = {.xVoxels = _x, .yVoxels = 9, .zVoxels = _z}});
}

Protocol::StateUpdate Field()
{
  Protocol::StateUpdate state{};
  state.player = ME;
  AddSquad(state, 1, Protocol::UnitKind::Spearman, 10, 20);
  AddSquad(state, 2, Protocol::UnitKind::Archer, 30, 40);
  AddSquad(state, 3, Protocol::UnitKind::Sapper, 50, 60);
  return state;
}

} // namespace

TEST_CLASS(ControlGroupTests)
{
public:
  TEST_METHOD(AssignsSelectsAndCentersTwice)
  {
    const Protocol::StateUpdate state = Field();
    Client::ControlGroups groups;
    Client::Selection selection;
    const std::array<Protocol::EntityId, 2> two = {1, 2};
    Client::Selector::Select(selection, state, two, false);
    groups.Assign(0, selection);
    selection = {};
    Assert::IsFalse(groups.Recall(0, selection, state, 10.0));
    Assert::AreEqual(std::size_t{2}, selection.squads.size());
    // Again soon after: twice, and the camera centres on the middle of its figures' feet.
    Assert::IsTrue(groups.Recall(0, selection, state, 10.3));
    const DirectX::XMFLOAT3 center = groups.Center(0, state).value_or(DirectX::XMFLOAT3{});
    Assert::AreEqual(20.0F, center.x, 0.0001F);
    Assert::AreEqual(30.0F, center.z, 0.0001F);
    // Again, but later: once.
    Assert::IsFalse(groups.Recall(0, selection, state, 12.0));
    Assert::AreEqual(std::string("SPEAR 2"), groups.Label(0, state));
  }

  TEST_METHOD(ADeadSquadLeavesItsGroupAndAGroupOfNothingShowsEmpty)
  {
    Protocol::StateUpdate state = Field();
    Client::ControlGroups groups;
    Client::Selection selection;
    const std::array<Protocol::EntityId, 1> one = {3};
    Client::Selector::Select(selection, state, one, false);
    groups.Assign(4, selection);
    std::erase_if(state.units, [](const Protocol::UnitView& _unit) { return _unit.squad == 3; });
    std::erase_if(state.squads, [](const Protocol::SquadView& _squad) { return _squad.id == 3; });
    groups.Prune(state);
    Assert::IsTrue(groups.Groups().at(4).empty());
    Assert::IsTrue(groups.Label(4, state).empty());
    selection = {};
    Assert::IsFalse(groups.Recall(4, selection, state, 1.0));
    Assert::IsTrue(selection.Empty());
    Assert::IsFalse(groups.Center(4, state).has_value());
  }

  TEST_METHOD(TheStripsSlotsAre120By44HitTargets)
  {
    Client::HudModel model{};
    model.deck = true;
    model.groupLabels.at(0) = "SPEAR 12";
    Client::Hud hud;
    std::vector<Client::ScreenQuad> quads;
    hud.Build(model, 1920, 1080, 1.0F, quads);
    std::vector<Client::HudTarget> slots;
    std::ranges::copy_if(hud.Targets(), std::back_inserter(slots),
                         [](const Client::HudTarget& _target) { return _target.control == Client::HudControl::GroupSlot; });
    Assert::AreEqual(std::size_t{5}, slots.size());
    for (std::size_t index = 0; index < slots.size(); ++index)
    {
      Assert::AreEqual(index, slots.at(index).alert);
      Assert::AreEqual(120, slots.at(index).rect.widthPixels);
      Assert::AreEqual(44, slots.at(index).rect.heightPixels);
    }
    const auto set = std::ranges::find(hud.Targets(), Client::HudControl::SetGroup, &Client::HudTarget::control);
    Assert::IsTrue(set != hud.Targets().end());
    Assert::AreEqual(64, set->rect.widthPixels);
    for (const Client::HudText& text : hud.Texts())
    {
      Assert::IsTrue(text.cell.Inside(text.element));
    }
  }
};

} // namespace ClientTests
