#pragma once

#include <DirectXMath.h>

#include "EntityId.h"
#include "Selection.h"
#include "StateUpdate.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace Client
{

/// The control groups 1 to 9 (Hud.md §3, §7 and §8; M4-I4), kept by the client: Ctrl + 1–9, or Ctrl-click or SET then
/// a click on a strip's slot, assigns the selection's squads to one; 1–9 or a click on its slot selects it, and doing so
/// twice in quick succession also centres the camera on it. A squad that dies leaves every group.
class ControlGroups
{
public:
  static constexpr std::size_t GROUPS = 9;
  /// A second recall of the same group within this is "twice" (Hud.md §7).
  static constexpr double TWICE_SECONDS = 0.5;

  /// Group _group, 0 to 8 for 1 to 9, becomes the selection's squads.
  void Assign(std::size_t _group, const Selection& _selection);

  /// Selects group _group, if it holds anything the update holds, and returns whether it was recalled twice: the same
  /// group within TWICE_SECONDS of _nowSeconds, when the camera centres on it.
  bool Recall(std::size_t _group, Selection& _selection, const Protocol::StateUpdate& _state, double _nowSeconds);

  /// Drops the squads the update no longer holds: dead, garrisoned or out of sight.
  void Prune(const Protocol::StateUpdate& _state);

  /// The middle of group _group's figures' feet, where the camera centres, or nothing for an empty group.
  [[nodiscard]] std::optional<DirectX::XMFLOAT3> Center(std::size_t _group, const Protocol::StateUpdate& _state) const;

  [[nodiscard]] const std::array<std::vector<Protocol::EntityId>, GROUPS>& Groups() const noexcept;

  /// What group _group's slot reads after its number: its first kind's short label and its figures, or nothing.
  [[nodiscard]] std::string Label(std::size_t _group, const Protocol::StateUpdate& _state) const;

private:
  std::array<std::vector<Protocol::EntityId>, GROUPS> m_groups;
  std::optional<std::size_t> m_lastGroup;
  double m_lastSeconds = 0.0;
};

} // namespace Client
