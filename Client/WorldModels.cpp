#include "pch.h"

#include "WorldModels.h"

#include "Catalog.h"
#include "Interpolation.h"
#include "Look.h"
#include "ModelTurn.h"
#include "TickClock.h"

#include <limits>
#include <set>

namespace Client
{

namespace
{

constexpr std::uint32_t QUARTER_TURNS = 4;
/// A horizontal move shorter than this in a tick is no step: a figure walks about 7 cm a tick, and a machine 3.5 cm.
constexpr float STEP_METERS = 0.001F;
/// A model faces its travel once it is this far from where it last stood still, a tick or two of walking, and then the
/// bearing of a chord this long behind it, so that a path's straight and diagonal steps between cells (ADR-015) do not
/// make it wobble (ADR-030, decision 5).
constexpr float HEADING_START_METERS = 0.03F;
constexpr float HEADING_CHORD_METERS = 1.0F;
/// A stone this far from the middle of its engine's cell across the ground has a bearing to take as the engine's aim.
constexpr float AIM_START_METERS = 0.01F;

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

/// _id standing on its footprint's middle at _middle, its feet there, turned _yawRadians about that middle.
ModelPlacement CenteredAt(std::string_view _id, const DirectX::XMFLOAT3& _middle, float _yawRadians, std::uint8_t _side)
{
  const Content::ModelView& model = Model(_id);
  const Extent extent = ExtentOf(model);
  const float yaw = NormalizedYaw(_yawRadians);
  const auto [width, depth] = TurnedExtent(extent.widthMeters, extent.depthMeters, yaw);
  return {.modelId = model.id,
          .xMeters = _middle.x - (0.5F * width),
          .yMeters = _middle.y,
          .zMeters = _middle.z - (0.5F * depth),
          .yawRadians = yaw,
          .sideColorIndex = _side};
}

/// One of the trebuchet's poses, _poseId, where S3 would stand with its footprint's middle at _middle, moved
/// _offsetXMeters along the model's own x before its turn (Look.h), turned with it.
ModelPlacement PoseOf(const DirectX::XMFLOAT3& _middle, float _yawRadians, std::string_view _poseId, float _offsetXMeters,
                      std::uint8_t _side)
{
  const Extent released = ExtentOf(Model("S3"));
  const Extent posed = ExtentOf(Model(_poseId));
  // From S3's footprint's middle to the pose's, both from the same minimum corner, in the model's own frame.
  const auto [x, z] = TurnedOffset(_offsetXMeters + (0.5F * (posed.widthMeters - released.widthMeters)),
                                   0.5F * (posed.depthMeters - released.depthMeters), _yawRadians);
  return CenteredAt(_poseId, {_middle.x + x, _middle.y, _middle.z + z}, _yawRadians, _side);
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
          .yawRadians = static_cast<float>(turns) * QUARTER_TURN_RADIANS,
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

float WorldModels::Turn(Protocol::EntityId _id, const DirectX::XMFLOAT3& _drawn, bool _moving, float _defaultYawRadians,
                        std::uint32_t _front, float _degreesPerSecond, float _seconds)
{
  const float defaultYaw = NormalizedYaw(_defaultYawRadians);
  Facing& facing =
    m_facings
      .try_emplace(_id, Facing{.yawRadians = defaultYaw, .targetRadians = defaultYaw, .anchorXMeters = _drawn.x, .anchorZMeters = _drawn.z})
      .first->second;
  const float dx = _drawn.x - facing.anchorXMeters;
  const float dz = _drawn.z - facing.anchorZMeters;
  const float travelled = std::hypot(dx, dz);
  if (!_moving || travelled > HEADING_CHORD_METERS + UNIT_SNAP_METERS)
  {
    // Standing, its next move starts afresh from here; snapped, it keeps its facing from its new place.
    facing.anchorXMeters = _drawn.x;
    facing.anchorZMeters = _drawn.z;
  }
  else if (travelled >= HEADING_START_METERS)
  {
    // The bearing clockwise from +z, less the model's front.
    facing.targetRadians = NormalizedYaw(std::atan2(dx, dz) - (static_cast<float>(_front % QUARTER_TURNS) * QUARTER_TURN_RADIANS));
    if (travelled > HEADING_CHORD_METERS)
    {
      facing.anchorXMeters = _drawn.x - (dx * HEADING_CHORD_METERS / travelled);
      facing.anchorZMeters = _drawn.z - (dz * HEADING_CHORD_METERS / travelled);
    }
  }
  const float most = _degreesPerSecond * (std::numbers::pi_v<float> / 180.0F) * _seconds;
  const float turn = ShorterTurn(facing.yawRadians, facing.targetRadians);
  facing.yawRadians = std::abs(turn) <= most ? facing.targetRadians : NormalizedYaw(facing.yawRadians + std::copysign(most, turn));
  return facing.yawRadians;
}

void WorldModels::NoteAims(const Protocol::StateUpdate& _last)
{
  std::erase_if(m_stonesSeen, [&_last](Protocol::EntityId _shot) { return FindView(_last.shots, _shot) == nullptr; });
  for (const Protocol::ShotView& shot : _last.shots)
  {
    const Protocol::EngineView* engine = FindView(_last.engines, shot.engine);
    if (shot.kind != Protocol::ShotKind::Stone || engine == nullptr || m_stonesSeen.contains(shot.id))
    {
      continue;
    }
    // The simulation throws a stone in the vertical plane through the middle of its engine's cell and its target.
    const DirectX::XMFLOAT3 at = Meters(shot.position);
    const float dx = at.x - (static_cast<float>(engine->cellVoxels.xVoxels) + 0.5F);
    const float dz = at.z - (static_cast<float>(engine->cellVoxels.zVoxels) + 0.5F);
    if (std::hypot(dx, dz) >= AIM_START_METERS)
    {
      m_stonesSeen.insert(shot.id);
      m_aims.insert_or_assign(engine->id, std::atan2(dx, dz));
    }
  }
}

void WorldModels::Place(const Protocol::StateUpdate& _previous, const Protocol::StateUpdate& _last, const FireTally& _tally,
                        double _tickFraction, std::vector<ModelPlacement>& _placements, std::vector<Protocol::VoxelPosition>& _hidden)
{
  _placements.clear();
  _hidden.clear();
  m_engineBoxes.clear();
  std::set<Protocol::EntityId> present;
  // The game time drawn since the last call, which the models turn by.
  const double drawnTicks = static_cast<double>(_last.tick) + _tickFraction;
  const float seconds =
    m_drawnTicks.has_value() ? static_cast<float>(std::max(0.0, drawnTicks - *m_drawnTicks) / TickClock::TICKS_PER_SECOND) : 0.0F;
  m_drawnTicks = drawnTicks;
  NoteAims(_last);
  const auto moved = [](const Protocol::FixedPosition* _before, const Protocol::FixedPosition& _after)
  {
    if (_before == nullptr)
    {
      return false;
    }
    const DirectX::XMFLOAT3 from = Meters(*_before);
    const DirectX::XMFLOAT3 to = Meters(_after);
    return std::hypot(to.x - from.x, to.z - from.z) > STEP_METERS;
  };

  for (const Protocol::EngineView& engine : _last.engines)
  {
    present.insert(engine.id);
    const Protocol::EngineView* before = FindView(_previous.engines, engine.id);
    const Protocol::FixedPosition* previousPosition = before != nullptr ? &before->position : nullptr;
    const DirectX::XMFLOAT3 body = DrawnPosition(previousPosition, engine.position, _tickFraction, UNIT_SNAP_METERS);
    // The body's position is the middle of its first cell; the model stands on its footprint's.
    const float toMiddle = 0.5F * static_cast<float>(std::max(1, engine.footprintVoxels) - 1);
    const DirectX::XMFLOAT3 middle{body.x + toMiddle, body.y, body.z + toMiddle};
    const std::uint8_t side = SideColor(_last, engine.owner);
    const bool moving = moved(previousPosition, engine.position);
    // Until it moves, an engine stands at its turn in the simulation, as the model would deploy or pack there.
    const float deployYaw = static_cast<float>(engine.quarterTurns % QUARTER_TURNS) * QUARTER_TURN_RADIANS;
    ModelPlacement placement{};
    if (engine.kind == Protocol::EngineKind::Ram)
    {
      const float yaw = Turn(engine.id, middle, moving, deployYaw, FRONT_PLUS_Z, MACHINE_TURN_DEGREES_PER_SECOND, seconds);
      placement = CenteredAt("S1", middle, yaw, side);
    }
    else if (engine.state == Protocol::EngineState::Packed || engine.state == Protocol::EngineState::Deploying)
    {
      // The wagon, facing its travel; packed again, it deploys afresh along its new deploy's turn.
      m_aims.erase(engine.id);
      const float yaw = Turn(engine.id, middle, moving, deployYaw, PACKED_TREBUCHET_FRONT, MACHINE_TURN_DEGREES_PER_SECOND, seconds);
      placement = PoseOf(middle, yaw, "S3p", PACKED_POSE_OFFSET_X_METERS, side);
    }
    else
    {
      // Deployed or packing: facing its latest stone, or its deploy's turn before its first, turned to at once, since the
      // stone has left by then (ADR-030, decision 5). It packs into the wagon at that turn, so that is the way the wagon
      // then faces. Released for a moment after each throw, else cocked.
      const auto aim = m_aims.find(engine.id);
      const float yaw = NormalizedYaw(
        aim != m_aims.end() ? aim->second - (static_cast<float>(DEPLOYED_TREBUCHET_FRONT) * QUARTER_TURN_RADIANS) : deployYaw);
      m_facings.insert_or_assign(engine.id,
                                 Facing{.yawRadians = yaw, .targetRadians = yaw, .anchorXMeters = middle.x, .anchorZMeters = middle.z});
      const std::optional<std::uint32_t> fired = _tally.LastFiredTick(engine.id);
      const bool justFired = fired.has_value() && _last.tick - *fired < RELEASED_POSE_TICKS;
      placement = justFired ? CenteredAt("S3", middle, yaw, side) : PoseOf(middle, yaw, "S3c", COCKED_POSE_OFFSET_X_METERS, side);
    }
    placement.silhouette = true;
    _placements.push_back(placement);
    // Its box, where a click selects it, bounds the model as turned (ADR-030, decision 7).
    const Extent extent = ExtentOf(Model(placement.modelId));
    const auto [width, depth] = TurnedExtent(extent.widthMeters, extent.depthMeters, placement.yawRadians);
    m_engineBoxes.push_back({.engine = engine.id,
                             .xMeters = placement.xMeters,
                             .zMeters = placement.zMeters,
                             .widthMeters = width,
                             .depthMeters = depth,
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
    const DirectX::XMFLOAT3 feet = DrawnPosition(previousPosition, unit.position, _tickFraction, UNIT_SNAP_METERS);
    // Facing north until it first moves, as M3 drew every figure.
    const float yaw =
      Turn(unit.id, feet, moved(previousPosition, unit.position), 0.0F, FRONT_PLUS_Z, FIGURE_TURN_DEGREES_PER_SECOND, seconds);
    ModelPlacement placement = CenteredAt(UnitModel(unit.kind), feet, yaw, SideColor(_last, unit.owner));
    placement.silhouette = true;
    _placements.push_back(placement);
  }
  std::erase_if(m_facings, [&present](const auto& _facing) { return !present.contains(_facing.first); });
  std::erase_if(m_aims, [&present](const auto& _aim) { return !present.contains(_aim.first); });

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
    _placements.push_back(CenteredAt("X1", {at.x, at.y - (0.5F * stone.heightMeters), at.z}, 0.0F, 0));
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
