#include "pch.h"

#include "WorldModels.h"

#include "Catalog.h"
#include "Interpolation.h"
#include "Look.h"

#include <limits>
#include <set>

namespace Client
{

namespace
{

constexpr std::uint32_t QUARTER_TURNS = 4;
/// A horizontal move shorter than this in a tick is no step: a figure walks about 7 cm a tick, and a machine 3.5 cm.
constexpr float STEP_METERS = 0.001F;
/// Along x and z within this of each other, a step is diagonal.
constexpr float DIAGONAL_METERS = 0.0001F;

const Content::ModelView& Model(std::string_view _id)
{
  const Content::ModelView* model = Content::FindModel(_id);
  if (model == nullptr)
  {
    throw std::runtime_error(std::format("Content has no model {}", _id));
  }
  return *model;
}

/// A model's size in its own frame, before its turn, in meters.
struct Extent
{
  float widthMeters; // along x
  float heightMeters;
  float depthMeters; // along z
};

Extent ExtentOf(const Content::ModelView& _model) noexcept
{
  const float edge = static_cast<float>(_model.grid.voxelEdgeMillimeters) / 1000.0F;
  return {.widthMeters = edge * static_cast<float>(_model.grid.widthVoxels),
          .heightMeters = edge * static_cast<float>(_model.grid.heightVoxels),
          .depthMeters = edge * static_cast<float>(_model.grid.depthVoxels)};
}

/// The minimum corner of a box from (_xMeters, _zMeters), _widthMeters along x and _depthMeters along z, turned _turns
/// quarter turns clockwise seen from above about the origin of its frame: a turn takes +z towards +x.
std::pair<float, float> TurnedMinimum(float _xMeters, float _zMeters, float _widthMeters, float _depthMeters, std::uint32_t _turns) noexcept
{
  const std::array<std::pair<float, float>, 4> corners = {{{_xMeters, _zMeters},
                                                           {_xMeters + _widthMeters, _zMeters},
                                                           {_xMeters, _zMeters + _depthMeters},
                                                           {_xMeters + _widthMeters, _zMeters + _depthMeters}}};
  std::pair<float, float> minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
  for (const auto& [x, z] : corners)
  {
    // x' = x cos + z sin, z' = -x sin + z cos, at a whole quarter turn.
    const std::array<std::pair<float, float>, QUARTER_TURNS> turned = {{{x, z}, {z, -x}, {-x, -z}, {-z, x}}};
    const auto [turnedX, turnedZ] = turned.at(_turns % QUARTER_TURNS);
    minimum = {std::min(minimum.first, turnedX), std::min(minimum.second, turnedZ)};
  }
  return minimum;
}

/// _id standing on its footprint's middle at _middle, its feet there, turned _turns quarter turns.
ModelPlacement CenteredAt(std::string_view _id, const DirectX::XMFLOAT3& _middle, std::uint32_t _turns, std::uint8_t _side)
{
  const Content::ModelView& model = Model(_id);
  const Extent extent = ExtentOf(model);
  const bool across = _turns % 2 == 1;
  return {.modelId = model.id,
          .xMeters = _middle.x - (0.5F * (across ? extent.depthMeters : extent.widthMeters)),
          .yMeters = _middle.y,
          .zMeters = _middle.z - (0.5F * (across ? extent.widthMeters : extent.depthMeters)),
          .quarterTurns = _turns % QUARTER_TURNS,
          .sideColorIndex = _side};
}

/// One of the trebuchet's poses, _poseId, at S3's placement _released, moved _offsetXMeters along the model's own x
/// before its turn (Look.h).
ModelPlacement PoseOf(const ModelPlacement& _released, std::string_view _poseId, float _offsetXMeters)
{
  const Extent released = ExtentOf(Model(_released.modelId));
  const Content::ModelView& pose = Model(_poseId);
  const Extent posed = ExtentOf(pose);
  const std::pair<float, float> releasedMinimum =
    TurnedMinimum(0.0F, 0.0F, released.widthMeters, released.depthMeters, _released.quarterTurns);
  const std::pair<float, float> posedMinimum =
    TurnedMinimum(_offsetXMeters, 0.0F, posed.widthMeters, posed.depthMeters, _released.quarterTurns);
  return {.modelId = pose.id,
          .xMeters = _released.xMeters + (posedMinimum.first - releasedMinimum.first),
          .yMeters = _released.yMeters,
          .zMeters = _released.zMeters + (posedMinimum.second - releasedMinimum.second),
          .quarterTurns = _released.quarterTurns,
          .sideColorIndex = _released.sideColorIndex};
}

/// The model's turn that faces a front of _front towards _heading, both as the quarter turns that take +z there.
std::uint32_t TurnsFacing(std::uint32_t _heading, std::uint32_t _front) noexcept
{
  return (_heading + QUARTER_TURNS - (_front % QUARTER_TURNS)) % QUARTER_TURNS;
}

template <typename View> const View* FindView(const std::vector<View>& _views, Protocol::EntityId _id)
{
  const auto found = std::ranges::find(_views, _id, &View::id);
  return found == _views.end() ? nullptr : &*found;
}

/// The open model of _structure's gate, when it has one.
const OpenGateLook* OpenGateOf(const Protocol::StructureView& _structure) noexcept
{
  const auto found = std::ranges::find(OPEN_GATES, std::string_view(_structure.templateId), &OpenGateLook::templateId);
  return found == OPEN_GATES.end() ? nullptr : &*found;
}

/// _gate's model where it stands in _structure: at its offset from the template's footprint before the turn, turned with
/// the structure, its footprint's minimum corner kept at the template's.
ModelPlacement OpenGatePlacement(const Protocol::StructureView& _structure, const OpenGateLook& _gate, std::uint8_t _side)
{
  const Content::ModelView& model = Model(_gate.modelId);
  const Extent extent = ExtentOf(model);
  const std::uint32_t turns = _structure.quarterTurns % QUARTER_TURNS;
  const bool across = turns % 2 == 1;
  // The template's footprint before its turn.
  const auto widthMeters = static_cast<float>(across ? _structure.extentVoxels.zVoxels : _structure.extentVoxels.xVoxels);
  const auto depthMeters = static_cast<float>(across ? _structure.extentVoxels.xVoxels : _structure.extentVoxels.zVoxels);
  const std::pair<float, float> footprint = TurnedMinimum(0.0F, 0.0F, widthMeters, depthMeters, turns);
  const std::pair<float, float> gate = TurnedMinimum(_gate.xMeters, _gate.zMeters, extent.widthMeters, extent.depthMeters, turns);
  return {.modelId = model.id,
          .xMeters = static_cast<float>(_structure.originVoxels.xVoxels) + (gate.first - footprint.first),
          .yMeters = static_cast<float>(_structure.originVoxels.yVoxels) + _gate.yMeters,
          .zMeters = static_cast<float>(_structure.originVoxels.zVoxels) + (gate.second - footprint.second),
          .quarterTurns = turns,
          .sideColorIndex = _side};
}

} // namespace

std::uint8_t WorldModels::SideColor(const Protocol::StateUpdate& _state, std::uint8_t _owner)
{
  const auto player = std::ranges::find(_state.players, _owner, &Protocol::PlayerView::player);
  if (player != _state.players.end())
  {
    return player->side == Protocol::Side::Attacker ? ATTACKER_COLOR_INDEX : DEFENDER_COLOR_INDEX;
  }
  return _owner == _state.player ? ATTACKER_COLOR_INDEX : DEFENDER_COLOR_INDEX;
}

std::string_view WorldModels::UnitModel(Protocol::UnitKind _kind) noexcept
{
  switch (_kind)
  {
  case Protocol::UnitKind::Laborer:
    return "U1";
  case Protocol::UnitKind::Spearman:
    return "U2";
  case Protocol::UnitKind::Archer:
    return "U3";
  case Protocol::UnitKind::Outrider:
    return "U4";
  case Protocol::UnitKind::ManAtArms:
    return "U5";
  case Protocol::UnitKind::Sapper:
    return "S2";
  case Protocol::UnitKind::Ram:
    return "S1";
  case Protocol::UnitKind::Trebuchet:
    return "S3p";
  }
  return "U2";
}

std::uint32_t WorldModels::Heading(Protocol::EntityId _id, const Protocol::FixedPosition* _previous, const Protocol::FixedPosition& _last,
                                   std::uint32_t _otherwise)
{
  const auto known = m_headings.find(_id);
  std::uint32_t heading = known != m_headings.end() ? known->second : _otherwise % QUARTER_TURNS;
  if (_previous != nullptr)
  {
    const DirectX::XMFLOAT3 from = Meters(*_previous);
    const DirectX::XMFLOAT3 to = Meters(_last);
    const float dx = to.x - from.x;
    const float dz = to.z - from.z;
    const float alongX = std::abs(dx);
    const float alongZ = std::abs(dz);
    if (std::max(alongX, alongZ) > STEP_METERS)
    {
      const std::uint32_t xHeading = dx > 0.0F ? 1U : 3U;
      const std::uint32_t zHeading = dz > 0.0F ? 0U : 2U;
      const bool mostlyX = alongX > alongZ + DIAGONAL_METERS;
      const bool mostlyZ = alongZ > alongX + DIAGONAL_METERS;
      // A diagonal keeps a facing along either of its axes, so that a zigzag does not flicker; else it faces along x.
      const bool keeps = !mostlyX && !mostlyZ && (heading == xHeading || heading == zHeading);
      if (!keeps)
      {
        heading = mostlyZ ? zHeading : xHeading;
      }
    }
  }
  m_headings.insert_or_assign(_id, heading);
  return heading;
}

void WorldModels::Place(const Protocol::StateUpdate& _previous, const Protocol::StateUpdate& _last, const FireTally& _tally,
                        double _tickFraction, std::vector<ModelPlacement>& _placements, std::vector<Protocol::VoxelPosition>& _hidden)
{
  _placements.clear();
  _hidden.clear();
  m_engineBoxes.clear();
  std::set<Protocol::EntityId> present;

  for (const Protocol::EngineView& engine : _last.engines)
  {
    present.insert(engine.id);
    const Protocol::EngineView* before = FindView(_previous.engines, engine.id);
    const DirectX::XMFLOAT3 body =
      DrawnPosition(before != nullptr ? &before->position : nullptr, engine.position, _tickFraction, UNIT_SNAP_METERS);
    // The body's position is the middle of its first cell; the model stands on its footprint's.
    const float toMiddle = 0.5F * static_cast<float>(std::max(1, engine.footprintVoxels) - 1);
    const DirectX::XMFLOAT3 middle{body.x + toMiddle, body.y, body.z + toMiddle};
    const std::uint8_t side = SideColor(_last, engine.owner);
    const Protocol::FixedPosition* previousPosition = before != nullptr ? &before->position : nullptr;
    ModelPlacement placement{};
    if (engine.kind == Protocol::EngineKind::Ram)
    {
      const std::uint32_t heading = Heading(engine.id, previousPosition, engine.position, engine.quarterTurns + FRONT_PLUS_Z);
      placement = CenteredAt("S1", middle, TurnsFacing(heading, FRONT_PLUS_Z), side);
    }
    else if (engine.state == Protocol::EngineState::Packed || engine.state == Protocol::EngineState::Deploying)
    {
      // The wagon, facing where it last stepped, or as it would pack where it stands.
      const std::uint32_t heading = Heading(engine.id, previousPosition, engine.position, engine.quarterTurns + PACKED_TREBUCHET_FRONT);
      const ModelPlacement released = CenteredAt("S3", middle, TurnsFacing(heading, PACKED_TREBUCHET_FRONT), side);
      placement = PoseOf(released, "S3p", PACKED_POSE_OFFSET_X_METERS);
    }
    else
    {
      // Deployed or packing: at its deploy's turn, released for a moment after each throw, else cocked. It packs into
      // the wagon at that turn, so that is the way the wagon then faces.
      m_headings.insert_or_assign(engine.id, (engine.quarterTurns + PACKED_TREBUCHET_FRONT) % QUARTER_TURNS);
      const ModelPlacement released = CenteredAt("S3", middle, engine.quarterTurns, side);
      const std::optional<std::uint32_t> fired = _tally.LastFiredTick(engine.id);
      const bool justFired = fired.has_value() && _last.tick - *fired < RELEASED_POSE_TICKS;
      placement = justFired ? released : PoseOf(released, "S3c", COCKED_POSE_OFFSET_X_METERS);
    }
    placement.silhouette = true;
    _placements.push_back(placement);
    const Extent extent = ExtentOf(Model(placement.modelId));
    const bool across = placement.quarterTurns % 2 == 1;
    m_engineBoxes.push_back({.engine = engine.id,
                             .xMeters = placement.xMeters,
                             .zMeters = placement.zMeters,
                             .widthMeters = across ? extent.depthMeters : extent.widthMeters,
                             .depthMeters = across ? extent.widthMeters : extent.depthMeters,
                             .groundMeters = placement.yMeters,
                             .heightMeters = extent.heightMeters});
  }

  for (const Protocol::UnitView& unit : _last.units)
  {
    if (unit.kind == Protocol::UnitKind::Ram || unit.kind == Protocol::UnitKind::Trebuchet)
    {
      continue; // an engine's body, which its engine draws (ADR-023)
    }
    const Protocol::SquadView* squad = FindView(_last.squads, unit.squad);
    if (squad != nullptr && squad->garrisonedIn != Protocol::NO_ENTITY)
    {
      continue; // in a building, and so in no cell (ADR-024, decision 5)
    }
    present.insert(unit.id);
    const Protocol::UnitView* before = FindView(_previous.units, unit.id);
    const Protocol::FixedPosition* previousPosition = before != nullptr ? &before->position : nullptr;
    // Facing north until its first step, as M3 drew every figure.
    const std::uint32_t heading = Heading(unit.id, previousPosition, unit.position, FRONT_PLUS_Z);
    ModelPlacement placement =
      CenteredAt(UnitModel(unit.kind), DrawnPosition(previousPosition, unit.position, _tickFraction, UNIT_SNAP_METERS),
                 TurnsFacing(heading, FRONT_PLUS_Z), SideColor(_last, unit.owner));
    placement.silhouette = true;
    _placements.push_back(placement);
  }
  std::erase_if(m_headings, [&present](const auto& _heading) { return !present.contains(_heading.first); });

  for (const Protocol::ShotView& shot : _last.shots)
  {
    if (shot.kind != Protocol::ShotKind::Stone)
    {
      continue; // an arrow is a line model, which the line pass draws (ADR-018; M4-H2)
    }
    const Protocol::ShotView* before = FindView(_previous.shots, shot.id);
    const DirectX::XMFLOAT3 at =
      DrawnPosition(before != nullptr ? &before->position : nullptr, shot.position, _tickFraction, NEVER_SNAP_METERS);
    // The stone's position is its middle.
    const Extent stone = ExtentOf(Model("X1"));
    _placements.push_back(CenteredAt("X1", {at.x, at.y - (0.5F * stone.heightMeters), at.z}, 0, 0));
  }

  for (const Protocol::StructureView& structure : _last.structures)
  {
    const OpenGateLook* gate = OpenGateOf(structure);
    if (!structure.gateOpen || gate == nullptr)
    {
      continue;
    }
    _placements.push_back(OpenGatePlacement(structure, *gate, SideColor(_last, structure.owner)));
    _hidden.insert(_hidden.end(), structure.gateVoxels.begin(), structure.gateVoxels.end());
  }
}

const std::vector<EngineBox>& WorldModels::EngineBoxes() const noexcept
{
  return m_engineBoxes;
}

} // namespace Client
