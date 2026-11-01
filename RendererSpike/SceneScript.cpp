#include "pch.h"

#include "SceneScript.h"

#include "Catalog.h"
#include "SplitMix64.h"

namespace RendererSpike
{

namespace
{

// Palette index of water (GameData/Server/Palette.json): nobody stands in the river.
constexpr std::uint8_t WATER = 9;

// Walkers: a circle 2 m across at walking pace (the owner's choice of 2026-11-01).
constexpr float WALK_RADIUS_METERS = 1.0F;
constexpr float WALK_METERS_PER_SECOND = 1.4F;
// How far above where it stands a walker may climb, so that a loop past a wall stays on the ground.
constexpr std::int32_t WALK_CLIMB_VOXELS = 3;

/// A hash of two integers and a salt, as a float from 0 to 1.
float Hash01(std::int64_t _x, std::int64_t _z, std::uint64_t _salt, std::uint64_t _seed) noexcept
{
  const std::uint64_t key = static_cast<std::uint64_t>(_x) ^ (static_cast<std::uint64_t>(_z) << 32);
  return static_cast<float>(Core::KeyedDraw(_seed, _salt, key) & 0xffffU) / 65536.0F;
}

const Content::ModelView& Model(std::string_view _id)
{
  const Content::ModelView* found = Content::FindModel(_id);
  if (found == nullptr)
  {
    throw std::runtime_error("the test scene needs a model Content does not have");
  }
  return *found;
}

/// A model's footprint in meters after a turn.
std::pair<float, float> Footprint(const Content::ModelView& _model, std::uint32_t _quarterTurns) noexcept
{
  const float edge = static_cast<float>(_model.grid.voxelEdgeMillimeters) / 1000.0F;
  const float width = static_cast<float>(_model.grid.widthVoxels) * edge;
  const float depth = static_cast<float>(_model.grid.depthVoxels) * edge;
  return _quarterTurns % 2 == 0 ? std::pair{width, depth} : std::pair{depth, width};
}

/// The top of the ground under a footprint: the highest of its columns' tops, in meters.
float GroundUnder(const Simulation::ChunkStore& _store, float _xMeters, float _zMeters, float _widthMeters, float _depthMeters)
{
  std::int32_t top = 0;
  for (auto z = static_cast<std::int32_t>(std::floor(_zMeters)); static_cast<float>(z) < _zMeters + _depthMeters; ++z)
  {
    for (auto x = static_cast<std::int32_t>(std::floor(_xMeters)); static_cast<float>(x) < _xMeters + _widthMeters; ++x)
    {
      top = std::max(top, Simulation::TopSolidAtOrBelow(_store, x, z, Simulation::TestWorld::HEIGHT_VOXELS - 1) + 1);
    }
  }
  return static_cast<float>(top);
}

/// A group of units scattered about a point, as the v5 scene scatters them, at least 1.1 m apart.
/// Its point and radii are the scene's, in the prototype's frame (ADR-017).
struct Group
{
  float xMeters;
  float zMeters;
  int count;
  float xRadiusMeters;
  float zRadiusMeters;
  std::uint8_t sideColorIndex;
  std::uint64_t salt;
  float spearShare;           // spearmen among them
  std::string_view otherUnit; // the rest
};

void Scatter(const Simulation::ChunkStore& _store, const Simulation::TestWorld& _world, const Group& _group, std::uint64_t _seed,
             std::vector<std::pair<float, float>>& _taken, std::vector<Walker>& _walkers)
{
  int remaining = _group.count;
  // Up to six tries a unit, where the v5 scene allows three, because it places on fewer
  // collisions: its count comes close to the plan's 95 soldiers.
  for (int attempt = 0; attempt < _group.count * 6 && remaining > 0; ++attempt)
  {
    const float sceneX =
      _group.xMeters + (Hash01(attempt, static_cast<std::int64_t>(_group.salt), 71, _seed) - 0.5F) * 2.0F * _group.xRadiusMeters;
    const float sceneZ =
      _group.zMeters + (Hash01(attempt, static_cast<std::int64_t>(_group.salt), 72, _seed) - 0.5F) * 2.0F * _group.zRadiusMeters;
    const float x = _world.WorldXMeters(sceneX);
    const float z = _world.WorldZMeters(sceneZ);
    if (std::ranges::any_of(_taken, [x, z](const std::pair<float, float>& _other)
                            { return std::hypot(_other.first - x, _other.second - z) < 1.1F; }))
    {
      continue;
    }
    _taken.emplace_back(x, z);
    const auto columnX = static_cast<std::int32_t>(std::floor(x));
    const auto columnZ = static_cast<std::int32_t>(std::floor(z));
    const std::int32_t top = Simulation::TopSolidAtOrBelow(_store, columnX, columnZ, Simulation::TestWorld::HEIGHT_VOXELS - 1);
    --remaining;
    if (top < 0 || _store.At({.xVoxels = columnX, .yVoxels = top, .zVoxels = columnZ}) == WATER)
    {
      continue; // the v5 scene stands nobody in the river
    }
    const bool spear = Hash01(attempt, static_cast<std::int64_t>(_group.salt), 73, _seed) < _group.spearShare;
    _walkers.push_back({.modelId = spear ? std::string_view{"U2"} : _group.otherUnit,
                        .centerXMeters = x,
                        .centerZMeters = z,
                        .phaseRadians = 6.2831853F * Hash01(attempt, static_cast<std::int64_t>(_group.salt), 74, _seed),
                        .homeGroundVoxels = top + 1,
                        .sideColorIndex = _group.sideColorIndex});
  }
}

std::vector<Walker> BuildUnits(const Simulation::ChunkStore& _store, const Simulation::TestWorld& _world, std::uint64_t _seed)
{
  // The v5 scene's groups: the attackers at the breach, behind the mantlets, in reserve and with
  // the trebuchets; the defenders inside the breach; and the labourers in the quarry and fields.
  // Spearmen are U2, the v5 scene's figures without spears men-at-arms (U5) or archers (U3).
  constexpr std::uint8_t RED = ATTACKER_COLOR_INDEX;
  constexpr std::uint8_t BLUE = DEFENDER_COLOR_INDEX;
  const std::array<Group, 12> groups = {
    Group{54.0F, 61.0F, 18, 3.5F, 3.0F, RED, 1, 0.5F, "U5"}, Group{59.0F, 61.0F, 5, 0.6F, 1.2F, RED, 11, 0.3F, "U5"},
    Group{42.0F, 55.5F, 7, 1.2F, 2.0F, RED, 2, 0.6F, "U5"},  Group{42.0F, 66.5F, 7, 1.2F, 2.0F, RED, 3, 0.6F, "U5"},
    Group{38.0F, 61.0F, 18, 3.0F, 4.0F, RED, 4, 0.8F, "U5"}, Group{30.0F, 54.0F, 5, 2.0F, 1.5F, RED, 9, 0.0F, "U3"},
    Group{32.0F, 74.5F, 5, 2.0F, 1.2F, RED, 10, 0.0F, "U3"}, Group{64.0F, 61.0F, 18, 2.5F, 4.5F, BLUE, 5, 0.7F, "U5"},
    Group{66.0F, 52.0F, 6, 2.0F, 1.5F, BLUE, 6, 0.5F, "U3"}, Group{71.0F, 61.0F, 6, 1.0F, 3.0F, BLUE, 12, 0.9F, "U5"},
    Group{75.0F, 94.0F, 14, 11.0F, 7.0F, 0, 7, 0.0F, "U1"},  Group{114.0F, 106.0F, 7, 8.0F, 12.0F, 0, 8, 0.0F, "U1"}};
  std::vector<std::pair<float, float>> taken;
  std::vector<Walker> walkers;
  for (const Group& group : groups)
  {
    Scatter(_store, _world, group, _seed, taken, walkers);
  }
  return walkers;
}

/// A fixed model stood on the ground under its footprint, which is given as the scene states it: its
/// minimum corner and its turn in the prototype's frame, taken to the world's (ADR-017).
ModelPlacement Stand(const Simulation::ChunkStore& _store, const Simulation::TestWorld& _world, std::string_view _id, float _xMeters,
                     float _zMeters, std::uint32_t _quarterTurns, std::uint8_t _sideColorIndex)
{
  const auto [width, depth] = Footprint(Model(_id), _quarterTurns);
  const float x = _world.WorldXMeters(_xMeters);
  const float z = _world.WorldZMeters(_zMeters + depth); // the mirror makes its far edge the near one
  return {.modelId = _id,
          .xMeters = x,
          .yMeters = GroundUnder(_store, x, z, width, depth),
          .zMeters = z,
          .quarterTurns = Simulation::TestWorld::WorldQuarterTurns(_quarterTurns),
          .sideColorIndex = _sideColorIndex};
}

} // namespace

Simulation::TemplateLookup ContentTemplates()
{
  return [](std::string_view _id) -> std::optional<Simulation::TemplateShape>
  {
    const Content::TemplateView* found = Content::FindTemplate(_id);
    if (found == nullptr)
    {
      return std::nullopt;
    }
    return Simulation::TemplateShape{.widthVoxels = found->grid.widthVoxels,
                                     .heightVoxels = found->grid.heightVoxels,
                                     .depthVoxels = found->grid.depthVoxels,
                                     .indices = found->grid.indices};
  };
}

TestScene BuildTestScene(Simulation::ChunkStore& _store, std::uint64_t _seed)
{
  // The world is Simulation's stand-in for the generator; the scene adds what only the client
  // draws: the units and the models (ADR-006).
  TestScene scene{.world = Simulation::BuildTestWorld(_store, Simulation::TestWorld::SCENE_EDGE_VOXELS, _seed, ContentTemplates()),
                  .fixedModels = {},
                  .walkers = {},
                  .centerXMeters = 0.0F,
                  .centerYMeters = static_cast<float>(Simulation::TestWorld::CASTLE_GROUND_VOXELS + 3),
                  .centerZMeters = 0.0F};
  scene.walkers = BuildUnits(_store, scene.world, _seed);
  scene.centerXMeters = scene.world.CastleXMeters();
  scene.centerZMeters = scene.world.CastleZMeters();
  // The scene's places below are the prototype's, in its frame; Stand takes them to the world's.
  // The two trebuchets west of the ditch, their counterweights to the east as in the v5 scene.
  scene.fixedModels.push_back(Stand(_store, scene.world, "S3", 28.0F, 49.0F, 0, 0));
  scene.fixedModels.push_back(Stand(_store, scene.world, "S3", 30.0F, 69.5F, 0, 0));
  // The mantlets before the ditch, turned to face the castle.
  for (const auto& [x, z] : {std::pair{45.0F, 53.5F}, std::pair{44.0F, 57.5F}, std::pair{45.0F, 64.5F}, std::pair{44.0F, 68.5F}})
  {
    scene.fixedModels.push_back(Stand(_store, scene.world, "P1", x - 1.0F, z, 1, 0));
  }
  // The defenders' banner on top of the keep's great tower.
  ModelPlacement banner = Stand(_store, scene.world, "P2", 74.0F, 61.0F, 0, DEFENDER_COLOR_INDEX);
  banner.yMeters = static_cast<float>(Simulation::TopSolidAtOrBelow(_store, scene.world.WorldXVoxels(74), scene.world.WorldZVoxels(61),
                                                                    Simulation::TestWorld::HEIGHT_VOXELS - 1) +
                                      1);
  scene.fixedModels.push_back(banner);
  return scene;
}

void PlaceWalkers(const Simulation::ChunkStore& _store, const TestScene& _scene, std::uint32_t _frame,
                  std::vector<ModelPlacement>& _placements)
{
  const float seconds = static_cast<float>(_frame) / static_cast<float>(Simulation::TestWorldEdits::FRAMES_PER_SECOND);
  const float turned = seconds * WALK_METERS_PER_SECOND / WALK_RADIUS_METERS;
  for (const Walker& walker : _scene.walkers)
  {
    const float angle = walker.phaseRadians + turned;
    const float x = walker.centerXMeters + WALK_RADIUS_METERS * std::cos(angle);
    const float z = walker.centerZMeters + WALK_RADIUS_METERS * std::sin(angle);
    // It walks anticlockwise seen from above, so it heads along (-sin, cos). Its front, +z as
    // authored, takes the nearest quarter turn: 0 faces +z, 1 faces +x, 2 faces -z and 3 faces -x.
    const float headingX = -std::sin(angle);
    const float headingZ = std::cos(angle);
    std::uint32_t quarterTurns = 0;
    if (std::abs(headingZ) >= std::abs(headingX))
    {
      quarterTurns = headingZ >= 0.0F ? 0 : 2;
    }
    else
    {
      quarterTurns = headingX >= 0.0F ? 1 : 3;
    }
    const auto [width, depth] = Footprint(Model(walker.modelId), quarterTurns);
    const std::int32_t top =
      Simulation::TopSolidAtOrBelow(_store, static_cast<std::int32_t>(std::floor(x)), static_cast<std::int32_t>(std::floor(z)),
                                    walker.homeGroundVoxels + WALK_CLIMB_VOXELS - 1);
    _placements.push_back({.modelId = walker.modelId,
                           .xMeters = x - 0.5F * width,
                           .yMeters = static_cast<float>(top + 1),
                           .zMeters = z - 0.5F * depth,
                           .quarterTurns = quarterTurns,
                           .sideColorIndex = walker.sideColorIndex});
  }
}

} // namespace RendererSpike
