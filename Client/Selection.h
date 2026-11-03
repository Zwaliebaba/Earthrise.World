#pragma once

#include <DirectXMath.h>

#include "EntityId.h"
#include "EntityKinds.h"
#include "StateUpdate.h"
#include "VoxelPick.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Client
{

/// What the player has selected (Hud.md §5; MvpPlan-M4.md, *Contracts*, `Selection`): squads, the engines' bodies'
/// among them and each laborer's own, or one structure alone; and the kind that drives the command card.
struct Selection
{
  std::vector<Protocol::EntityId> squads;             // in the order selected
  Protocol::EntityId structure = Protocol::NO_ENTITY; // selected alone
  std::optional<Protocol::UnitKind> activeKind;       // one of the squads' kinds; Tab moves it

  [[nodiscard]] bool Empty() const noexcept;
};

/// Hud.md §5's order of the kinds a selection holds, which Tab follows and the panel's tiles keep: trebuchet, ram, then
/// the structures, which a squad never is, then men-at-arms, spearmen, archers, outriders, sappers and laborers.
inline constexpr std::array<Protocol::UnitKind, 8> KIND_ORDER = {
  Protocol::UnitKind::Trebuchet, Protocol::UnitKind::Ram,      Protocol::UnitKind::ManAtArms, Protocol::UnitKind::Spearman,
  Protocol::UnitKind::Archer,    Protocol::UnitKind::Outrider, Protocol::UnitKind::Sapper,    Protocol::UnitKind::Laborer};

/// Picking and changing the selection from the session's last update (Hud.md §5 and §8; M4-I2). Every test is against
/// the update alone, so only what the player sees can be picked.
class Selector
{
public:
  /// The squad whose footprint on the ground _ray meets first: each figure's cell and the cells about it, one cell out,
  /// on the plane of the figure's feet, not the figure's voxels, which are a few pixels wide (Hud.md §5). Engines are
  /// picked by their models' boxes instead (WorldPicker), and garrisoned squads are in no cell.
  [[nodiscard]] static std::optional<Protocol::EntityId> SquadAt(const Engine::PickRay& _ray, const Protocol::StateUpdate& _state);

  /// _owner's squads with a figure's feet inside the screen rectangle from (_x0, _y0) to (_x1, _y1), in either order,
  /// as drawn with _viewProjection into a target _widthPixels by _heightPixels: the box select, engines included.
  [[nodiscard]] static std::vector<Protocol::EntityId> SquadsInBox(const Protocol::StateUpdate& _state, std::uint8_t _owner,
                                                                   const DirectX::XMFLOAT4X4& _viewProjection, std::uint32_t _widthPixels,
                                                                   std::uint32_t _heightPixels, std::int32_t _x0, std::int32_t _y0,
                                                                   std::int32_t _x1, std::int32_t _y1);

  /// A click's selection: _squad alone, or with _toggle, as Shift and Ctrl give it, _squad added to the selection or taken
  /// out of it; a structure's click is always alone. A click on nothing deselects, unless toggling.
  static void Click(Selection& _selection, const Protocol::StateUpdate& _state, std::optional<Protocol::EntityId> _squad,
                    Protocol::EntityId _structure, bool _toggle);

  /// _squads selected in place of the selection, or with _toggle added to it.
  static void Select(Selection& _selection, const Protocol::StateUpdate& _state, std::span<const Protocol::EntityId> _squads, bool _toggle);

  /// A double-click on _squad: every squad of its owner and its kind with a figure on screen.
  static void SelectKind(Selection& _selection, const Protocol::StateUpdate& _state, Protocol::EntityId _squad,
                         const DirectX::XMFLOAT4X4& _viewProjection, std::uint32_t _widthPixels, std::uint32_t _heightPixels);

  /// Tab: the active kind moves on to the next of the selection's kinds in KIND_ORDER, round to the first.
  static void CycleKind(Selection& _selection, const Protocol::StateUpdate& _state);

  /// Drops what the update no longer holds, a squad dead or out of sight and a structure gone, and keeps the active kind
  /// one the selection holds.
  static void Prune(Selection& _selection, const Protocol::StateUpdate& _state);

  /// The kinds the selection's squads are, in KIND_ORDER, each once.
  [[nodiscard]] static std::vector<Protocol::UnitKind> Kinds(const Selection& _selection, const Protocol::StateUpdate& _state);

  /// Where _point lands on a target _widthPixels by _heightPixels drawn with _viewProjection, for row vectors, or nothing
  /// behind the camera.
  [[nodiscard]] static std::optional<DirectX::XMFLOAT2> Project(const DirectX::XMFLOAT4X4& _viewProjection, const DirectX::XMFLOAT3& _point,
                                                                std::uint32_t _widthPixels, std::uint32_t _heightPixels) noexcept;
};

} // namespace Client
