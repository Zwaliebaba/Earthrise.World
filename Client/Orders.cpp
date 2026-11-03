#include "pch.h"

#include "Orders.h"

#include "FireMode.h"
#include "Strings.h"

namespace Client
{

namespace
{

constexpr std::string_view KEY_SOURCE = "key";
constexpr std::string_view CLICK_SOURCE = "click";

const Protocol::SquadView* FindSquad(const Protocol::StateUpdate& _state, Protocol::EntityId _squad)
{
  const auto found = std::ranges::find(_state.squads, _squad, &Protocol::SquadView::id);
  return found == _state.squads.end() ? nullptr : &*found;
}

const Protocol::EngineView* EngineOf(const Protocol::StateUpdate& _state, Protocol::EntityId _squad)
{
  const auto found = std::ranges::find(_state.engines, _squad, &Protocol::EngineView::squad);
  return found == _state.engines.end() ? nullptr : &*found;
}

/// The selected squads that _test passes, with their views.
template <typename Test>
std::vector<const Protocol::SquadView*> Squads(const Selection& _selection, const Protocol::StateUpdate& _state, const Test& _test)
{
  std::vector<const Protocol::SquadView*> squads;
  for (const Protocol::EntityId id : _selection.squads)
  {
    const Protocol::SquadView* squad = FindSquad(_state, id);
    if (squad != nullptr && _test(*squad))
    {
      squads.push_back(squad);
    }
  }
  return squads;
}

/// A squad that walks: not a deployed or deploying trebuchet, which cannot move.
bool Walks(const Protocol::SquadView& _squad, const Protocol::StateUpdate& _state)
{
  const Protocol::EngineView* engine = EngineOf(_state, _squad.id);
  return engine == nullptr || engine->kind != Protocol::EngineKind::Trebuchet || engine->state == Protocol::EngineState::Packed;
}

bool Enemy(const Protocol::StateUpdate& _state, std::uint8_t _owner)
{
  return _owner != _state.player;
}

} // namespace

Protocol::VoxelPosition Orders::GoalAbove(const Protocol::VoxelPosition& _voxels) noexcept
{
  return {.xVoxels = _voxels.xVoxels, .yVoxels = _voxels.yVoxels + 1, .zVoxels = _voxels.zVoxels};
}

std::uint32_t Orders::TurnsAlong(const DirectX::XMFLOAT3& _forward) noexcept
{
  // One turn takes +x to -z, two to -x and three to +z.
  if (std::abs(_forward.x) >= std::abs(_forward.z))
  {
    return _forward.x >= 0.0F ? 0U : 2U;
  }
  return _forward.z >= 0.0F ? 3U : 1U;
}

Orders::Result Orders::Update(const Input& _input, const Selection& _selection, const Protocol::StateUpdate& _state,
                              const HudCommandCard& _card, Session& _session)
{
  Result result;
  // A mode whose slot the card no longer holds, or holds disabled, ends: the selection changed under it.
  if (m_mode != CardCommand::None)
  {
    const bool held =
      std::ranges::any_of(_card.slots, [this](const HudCommandSlot& _slot) { return _slot.enabled && CommandOf(_slot) == m_mode; });
    if (!held)
    {
      Cancel();
    }
  }
  if (_input.escapePressed && m_mode != CardCommand::None)
  {
    Cancel();
    result.usedEscape = true;
  }
  // A slot by its hotkey or its cell: W, A, S and D pan, and no card holds them (Hud.md §7).
  if (_input.hotkey.has_value())
  {
    const auto slot = std::ranges::find(_card.slots, std::string(1, *_input.hotkey), &HudCommandSlot::hotkey);
    if (slot != _card.slots.end() && slot->enabled)
    {
      Give(CommandOf(*slot), KEY_SOURCE, _selection, _state, _input, _session, result);
    }
  }
  if (_input.slotClicked.has_value() && *_input.slotClicked < _card.slots.size())
  {
    const HudCommandSlot& slot = _card.slots.at(*_input.slotClicked);
    if (slot.enabled)
    {
      Give(CommandOf(slot), CLICK_SOURCE, _selection, _state, _input, _session, result);
    }
  }
  if (_input.rightPressed || _input.rightReleased)
  {
    RightClick(_input, _selection, _state, _session);
  }
  // The archers' attack cursor asks again whenever the pointer's cell changes.
  m_aimSquad = Protocol::NO_ENTITY;
  if (m_mode == CardCommand::Attack && _selection.activeKind == Protocol::UnitKind::Archer && _input.pointedVoxels.has_value())
  {
    const std::vector<const Protocol::SquadView*> archers =
      Squads(_selection, _state, [](const Protocol::SquadView& _squad) { return _squad.kind == Protocol::UnitKind::Archer; });
    if (!archers.empty())
    {
      m_aimSquad = archers.front()->id;
      const Protocol::VoxelPosition target = GoalAbove(*_input.pointedVoxels);
      if (m_aimed != target)
      {
        _session.Ask(Protocol::SquadAim{.squad = m_aimSquad, .targetVoxels = target});
        m_aimed = target;
      }
    }
  }
  else
  {
    m_aimed.reset();
  }
  return result;
}

void Orders::Give(CardCommand _command, std::string_view _source, const Selection& _selection, const Protocol::StateUpdate& _state,
                  const Input& _input, Session& _session, Result& _result)
{
  if (_command == CardCommand::Fire)
  {
    _result.fire = true; // FireMode's
    return;
  }
  if (LookOf(_command).targeted)
  {
    m_mode = _command;
    m_trenchFrom.reset();
    return;
  }
  const auto active = [&_selection](const Protocol::SquadView& _squad) { return _selection.activeKind == _squad.kind; };
  const auto all = [](const Protocol::SquadView&) { return true; };
  switch (_command)
  {
  case CardCommand::Stop:
    for (const Protocol::SquadView* squad : Squads(_selection, _state, all))
    {
      _session.Submit(Protocol::StopSquad{.squad = squad->id}, _source);
    }
    break;
  case CardCommand::Hold:
    for (const Protocol::SquadView* squad : Squads(_selection, _state, all))
    {
      _session.Submit(Protocol::HoldSquad{.squad = squad->id}, _source);
    }
    break;
  case CardCommand::Split:
    for (const Protocol::SquadView* squad : Squads(_selection, _state, active))
    {
      if (std::ranges::count(_state.units, squad->id, &Protocol::UnitView::squad) > 1)
      {
        _session.Submit(Protocol::SplitSquad{.squad = squad->id}, _source);
      }
    }
    break;
  case CardCommand::Merge:
  {
    Protocol::MergeSquads merge;
    for (const Protocol::SquadView* squad : Squads(_selection, _state, active))
    {
      merge.squads.push_back(squad->id);
    }
    _session.Submit(merge, _source);
    break;
  }
  case CardCommand::Pack:
  case CardCommand::Deploy:
    for (const Protocol::SquadView* squad : Squads(_selection, _state, active))
    {
      const Protocol::EngineView* engine = EngineOf(_state, squad->id);
      if (engine == nullptr || engine->kind != Protocol::EngineKind::Trebuchet)
      {
        continue;
      }
      if (_command == CardCommand::Pack && engine->state == Protocol::EngineState::Deployed)
      {
        _session.Submit(Protocol::PackEngine{.engine = engine->id}, _source);
      }
      else if (_command == CardCommand::Deploy && engine->state == Protocol::EngineState::Packed)
      {
        // Facing where the camera looks, which is where the player means to throw.
        _session.Submit(Protocol::DeployEngine{.engine = engine->id, .quarterTurns = TurnsAlong(_input.cameraForward)}, _source);
      }
    }
    break;
  default:
    break;
  }
}

void Orders::RightClick(const Input& _input, const Selection& _selection, const Protocol::StateUpdate& _state, Session& _session)
{
  // TRENCH is a right-drag: the press sets a corner and the release the other.
  if (m_mode == CardCommand::Trench)
  {
    if (_input.rightPressed && _input.pointedVoxels.has_value())
    {
      m_trenchFrom = _input.pointedVoxels;
    }
    if (_input.rightReleased && m_trenchFrom.has_value() && _input.pointedVoxels.has_value())
    {
      const Protocol::VoxelPosition& from = *m_trenchFrom;
      const Protocol::VoxelPosition& to = *_input.pointedVoxels;
      const std::int32_t top = std::max(from.yVoxels, to.yVoxels);
      const Protocol::DigTrench box{
        .squad = Protocol::NO_ENTITY,
        .minVoxels = {.xVoxels = std::min(from.xVoxels, to.xVoxels),
                      .yVoxels = std::max(0, top - TRENCH_DEPTH_VOXELS + 1),
                      .zVoxels = std::min(from.zVoxels, to.zVoxels)},
        .maxVoxels = {.xVoxels = std::max(from.xVoxels, to.xVoxels), .yVoxels = top, .zVoxels = std::max(from.zVoxels, to.zVoxels)}};
      for (const Protocol::SquadView* squad :
           Squads(_selection, _state, [](const Protocol::SquadView& _squad) { return _squad.kind == Protocol::UnitKind::Sapper; }))
      {
        Protocol::DigTrench order = box;
        order.squad = squad->id;
        _session.Submit(order, CLICK_SOURCE);
      }
      m_trenchFrom.reset();
      if (!_input.shiftHeld)
      {
        Cancel();
      }
    }
    return;
  }
  if (!_input.rightPressed)
  {
    return;
  }
  // A deployed trebuchet's right click is FIRE's, which FireMode gives.
  const bool firing = m_mode == CardCommand::None && _selection.activeKind == Protocol::UnitKind::Trebuchet &&
                      !Squads(_selection, _state, [&_state](const Protocol::SquadView& _squad) { return !Walks(_squad, _state); }).empty();
  if (firing)
  {
    return;
  }
  // What lies under the pointer: an enemy squad, an enemy engine's body's, or an enemy structure.
  Protocol::EntityId enemy = Protocol::NO_ENTITY;
  if (_input.pointedEngine != Protocol::NO_ENTITY)
  {
    const auto engine = std::ranges::find(_state.engines, _input.pointedEngine, &Protocol::EngineView::id);
    if (engine != _state.engines.end() && Enemy(_state, engine->owner))
    {
      enemy = engine->id;
    }
  }
  else if (_input.pointedSquad.has_value())
  {
    const Protocol::SquadView* squad = FindSquad(_state, *_input.pointedSquad);
    if (squad != nullptr && Enemy(_state, squad->owner))
    {
      enemy = squad->id;
    }
  }
  const auto structure = std::ranges::find(_state.structures, _input.pointedStructure, &Protocol::StructureView::id);
  const bool enemyStructure = structure != _state.structures.end() && Enemy(_state, structure->owner);
  const std::optional<Protocol::VoxelPosition> goal =
    _input.pointedVoxels.has_value() ? std::optional(GoalAbove(*_input.pointedVoxels)) : std::nullopt;
  const auto walkers = Squads(_selection, _state, [&_state](const Protocol::SquadView& _squad) { return Walks(_squad, _state); });
  bool given = false;
  switch (m_mode)
  {
  case CardCommand::Move:
    if (goal.has_value())
    {
      for (const Protocol::SquadView* squad : walkers)
      {
        _session.Submit(Protocol::MoveSquad{.squad = squad->id, .goalVoxels = *goal}, CLICK_SOURCE);
      }
      given = true;
    }
    break;
  case CardCommand::Storm:
    if (goal.has_value())
    {
      for (const Protocol::SquadView* squad : walkers)
      {
        _session.Submit(Protocol::StormTo{.squad = squad->id, .goalVoxels = *goal}, CLICK_SOURCE);
      }
      given = true;
    }
    break;
  default:
  {
    // ATTACK, or no mode: what lies under the pointer. A ram attacks a structure; with no mode, ground is MOVE.
    for (const Protocol::SquadView* squad : walkers)
    {
      const bool ram = squad->kind == Protocol::UnitKind::Ram;
      if (enemy != Protocol::NO_ENTITY)
      {
        _session.Submit(Protocol::AttackTarget{.squad = squad->id, .target = enemy}, CLICK_SOURCE);
        given = true;
      }
      else if (ram && enemyStructure)
      {
        _session.Submit(Protocol::AttackTarget{.squad = squad->id, .target = structure->id}, CLICK_SOURCE);
        given = true;
      }
      else if (m_mode == CardCommand::None && goal.has_value())
      {
        _session.Submit(Protocol::MoveSquad{.squad = squad->id, .goalVoxels = *goal}, CLICK_SOURCE);
        given = true;
      }
    }
    break;
  }
  }
  if (given && !_input.shiftHeld)
  {
    Cancel();
  }
}

CardCommand Orders::Mode() const noexcept
{
  return m_mode;
}

void Orders::Cancel() noexcept
{
  m_mode = CardCommand::None;
  m_trenchFrom.reset();
}

HudMode Orders::Banner() const
{
  if (m_mode == CardCommand::None)
  {
    return {};
  }
  const CommandLook& look = LookOf(m_mode);
  return {.command = std::string(look.label),
          .banner = std::format(MODE_BANNER, look.label, look.rule),
          .buttons = {{.control = HudControl::Cancel, .label = std::string(CANCEL_ESC)}}};
}

std::optional<AttackCursor> Orders::Cursor(const Session& _session) const
{
  if (m_aimSquad == Protocol::NO_ENTITY || !m_aimed.has_value())
  {
    return std::nullopt;
  }
  return CursorOf(_session.Aim(m_aimSquad, *m_aimed));
}

} // namespace Client
