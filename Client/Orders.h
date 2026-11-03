#pragma once

#include <DirectXMath.h>

#include "CommandCard.h"
#include "Hud.h"
#include "HudModel.h"
#include "Selection.h"
#include "Session.h"
#include "StateUpdate.h"

#include <optional>
#include <string_view>

namespace Client
{

/// The command card's orders and targeting modes (Hud.md §7 and §8; M4-I3), FIRE's aside, which FireMode keeps. A
/// slot's hotkey, or a click on it, gives its order at once, or enters its mode: MOVE, ATTACK, STORM and TRENCH, each
/// with its rule as the banner. In a mode a right click on the world gives the order and ends the mode unless Shift
/// is held; TRENCH's is a right-drag, its box from the press to the release. With no mode a right click gives the
/// order what lies under the pointer asks for: ATTACK on an enemy squad or engine, the ram's ATTACK on an enemy
/// structure, and else MOVE there. Every order goes to the session with its source, `key` or `click` (Hud.md §11).
class Orders
{
public:
  /// One frame's input, as the executable reads it. The right button's and the pointer's are the world's: off the HUD.
  struct Input
  {
    std::optional<char> hotkey;             // a letter key pressed this frame, upper case
    std::optional<std::size_t> slotClicked; // a card cell clicked this frame
    bool escapePressed = false;
    bool rightPressed = false;  // the right button went down on the world
    bool rightReleased = false; // and came up
    bool shiftHeld = false;
    std::optional<Protocol::VoxelPosition> pointedVoxels; // the world voxel under the pointer
    std::optional<Protocol::EntityId> pointedSquad;       // the squad whose footprint is under it
    Protocol::EntityId pointedEngine = Protocol::NO_ENTITY;
    Protocol::EntityId pointedStructure = Protocol::NO_ENTITY;
    DirectX::XMFLOAT3 cameraForward{0.0F, 0.0F, 1.0F}; // which way a trebuchet deploys to throw
  };

  /// What a frame did besides the orders it gave.
  struct Result
  {
    bool usedEscape = false; // it ended a mode, so Esc does not also deselect
    bool fire = false;       // FIRE's slot or key, for FireMode
  };

  Result Update(const Input& _input, const Selection& _selection, const Protocol::StateUpdate& _state, const HudCommandCard& _card,
                Session& _session);

  [[nodiscard]] CardCommand Mode() const noexcept;
  void Cancel() noexcept;

  /// The mode banner while a mode is on, its rule and CANCEL · ESC, or no mode.
  [[nodiscard]] HudMode Banner() const;

  /// The attack cursor while ATTACK is on with archers active and the pointer is over the world: the reading of the
  /// SquadAim it asked for, as FIRE's is drawn (Hud.md §8).
  [[nodiscard]] std::optional<AttackCursor> Cursor(const Session& _session) const;

  /// Where a right click on _voxels sends a squad: the cell above the voxel.
  [[nodiscard]] static Protocol::VoxelPosition GoalAbove(const Protocol::VoxelPosition& _voxels) noexcept;

  /// The quarter turns that throw along _forward across the ground: S3 throws along its model's +x (BreachRun.h).
  [[nodiscard]] static std::uint32_t TurnsAlong(const DirectX::XMFLOAT3& _forward) noexcept;

  /// A trench dug from the ground's top this many voxels down, a box the player drags (ADR-029, decision 1; its E3 box
  /// was 8 deep); it stops at rock.
  static constexpr std::int32_t TRENCH_DEPTH_VOXELS = 8;

private:
  /// Gives _command at once, or enters its mode.
  void Give(CardCommand _command, std::string_view _source, const Selection& _selection, const Protocol::StateUpdate& _state,
            const Input& _input, Session& _session, Result& _result);
  /// A right click's order in the mode, or with none, from what lies under the pointer.
  void RightClick(const Input& _input, const Selection& _selection, const Protocol::StateUpdate& _state, Session& _session);

  CardCommand m_mode = CardCommand::None;
  std::optional<Protocol::VoxelPosition> m_trenchFrom; // TRENCH's first corner, from the right press
  Protocol::EntityId m_aimSquad = Protocol::NO_ENTITY; // the archers the attack cursor reads for
  std::optional<Protocol::VoxelPosition> m_aimed;      // the cell it asked about
};

} // namespace Client
