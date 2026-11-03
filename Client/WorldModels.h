#pragma once

#include "FireTally.h"
#include "ModelPlacement.h"
#include "StateUpdate.h"
#include "WorldPicker.h"

#include <cstdint>
#include <map>
#include <string_view>
#include <vector>

namespace Client
{

/// What the client draws of the war as voxel models (M4-H1; MvpAssets.md, the models' table and *Poses, projectiles,
/// gates and debris*), from its last two state updates:
/// - each unit as its kind's figure in its side's faction entry, facing its last step, and none while garrisoned;
/// - each engine on the middle of its footprint, the ram as S1 facing its last step, and the trebuchet as S3p, the
///   wagon, while packed or deploying, facing its last step, and as S3c, cocked, while deployed or packing, at its
///   deploy's turn, or S3, released, for RELEASED_POSE_TICKS after it fires;
/// - each stone in flight;
/// - and each open gate as its open model, G1 or G2, with the gate's own voxels named for the world view to leave out.
/// Units, engines and stones are drawn between their last two positions a tick behind; a unit or an engine that moved
/// more than UNIT_SNAP_METERS snaps, and a stone never does (MvpPlan.md, M4, input 20). Arrows are line models (M4-H2).
class WorldModels
{
public:
  /// Replaces _placements with this frame's models and _hidden with the world voxels the world view leaves out: every
  /// open gate's that has an open model. _previous is the update before _last, empty before the second. What each unit
  /// and engine last stepped towards is kept from call to call, so that one standing still keeps its facing. Throws
  /// std::runtime_error when Content lacks a model it draws.
  void Place(const Protocol::StateUpdate& _previous, const Protocol::StateUpdate& _last, const FireTally& _tally, double _tickFraction,
             std::vector<ModelPlacement>& _placements, std::vector<Protocol::VoxelPosition>& _hidden);

  /// Each engine's model's box as Place last placed it, where a click selects it (M3-E3; M3-H3).
  [[nodiscard]] const std::vector<EngineBox>& EngineBoxes() const noexcept;

  /// The model a unit of _kind is drawn as: U1 to U5, S2 for the sapper, and the engines' bodies' engines' S1 and S3p.
  [[nodiscard]] static std::string_view UnitModel(Protocol::UnitKind _kind) noexcept;

  /// The faction entry _owner's models and arrows are drawn in: their side's in _state, or, in a world without players,
  /// the attacker's for the player whose update it is, as M3's runs have it (Look.h).
  [[nodiscard]] static std::uint8_t SideColor(const Protocol::StateUpdate& _state, std::uint8_t _owner);

private:
  /// The direction _id last stepped in, as the quarter turns that take +z there, updated from _previous to _last; or
  /// _otherwise when it has not stepped since it was first seen.
  [[nodiscard]] std::uint32_t Heading(Protocol::EntityId _id, const Protocol::FixedPosition* _previous,
                                      const Protocol::FixedPosition& _last, std::uint32_t _otherwise);

  std::map<Protocol::EntityId, std::uint32_t> m_headings; // units' and engines', as Heading gives them
  std::vector<EngineBox> m_engineBoxes;
};

} // namespace Client
