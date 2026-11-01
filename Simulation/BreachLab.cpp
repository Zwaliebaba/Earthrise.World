#include "pch.h"

#include "BreachLab.h"

#include "BreachRun.h"
#include "ByteWriter.h"
#include "Hash.h"
#include "Save.h"

namespace Simulation
{

namespace
{

constexpr std::uint8_t EARTH = 2;
constexpr std::int32_t SECTION_LENGTH_VOXELS = 8;
constexpr std::int32_t TREBUCHET_STANDOFF_VOXELS = 40;
constexpr std::int32_t SQUAD_STANDOFF_VOXELS = 10;
constexpr std::int32_t GOAL_INSIDE_VOXELS = 6; // nearer the south wall than the north, so the squad waits south

/// The first empty voxel of a column, from the top down.
std::int32_t FirstEmptyAbove(const World& _world, std::int32_t _x, std::int32_t _z)
{
  const std::int32_t height = _world.Description().heightChunks * Protocol::CHUNK_EDGE_VOXELS;
  for (std::int32_t y = height - 1; y >= 0; --y)
  {
    if (_world.Voxels().At({.xVoxels = _x, .yVoxels = y, .zVoxels = _z}) != Protocol::Chunk::EMPTY)
    {
      return y + 1;
    }
  }
  return 0;
}

/// Brings every column of the footprint to BreachRun::GROUND_VOXELS: earth below, nothing above.
void Level(World& _world)
{
  const std::int32_t height = _world.Description().heightChunks * Protocol::CHUNK_EDGE_VOXELS;
  for (std::int32_t z = BreachRun::LEVEL_SOUTH_VOXELS; z <= BreachRun::LEVEL_NORTH_VOXELS; ++z)
  {
    for (std::int32_t x = BreachRun::LEVEL_WEST_VOXELS; x <= BreachRun::LEVEL_EAST_VOXELS; ++x)
    {
      for (std::int32_t y = 0; y < height; ++y)
      {
        const Protocol::VoxelPosition voxel{.xVoxels = x, .yVoxels = y, .zVoxels = z};
        if (y < BreachRun::GROUND_VOXELS && _world.Voxels().At(voxel) == Protocol::Chunk::EMPTY)
        {
          _world.Voxels().Edit(voxel, EARTH);
        }
        else if (y >= BreachRun::GROUND_VOXELS)
        {
          _world.Voxels().Edit(voxel, Protocol::Chunk::EMPTY);
        }
      }
    }
  }
}

/// The enclosure, as structures W1 to W10, numbered anticlockwise seen from above from the south-west
/// corner, south side first: three sections a side north and south and two a side east and west
/// between them, each facing out. Turns are clockwise seen from above, as the voxel pass counts them:
/// the south side's fronts face south after two, the east side's east after one, and the west side's
/// west after three. Returns W2.
Protocol::EntityId RaiseEnclosure(World& _world)
{
  const ServerTemplate* t3 = _world.Data().FindTemplate("T3");
  if (t3 == nullptr)
  {
    throw std::runtime_error("the breach lab needs T3 in the server data");
  }
  const auto thickness = static_cast<std::int32_t>(t3->depthVoxels);
  const auto at = [](std::int32_t _x, std::int32_t _z)
  { return Protocol::VoxelPosition{.xVoxels = _x, .yVoxels = BreachRun::GROUND_VOXELS, .zVoxels = _z}; };
  std::int32_t number = 0;
  std::vector<Protocol::EntityId> raised;
  const auto raise = [&](std::int32_t _x, std::int32_t _z, std::uint32_t _quarterTurns)
  { raised.push_back(_world.AddStructure(BreachRun::DEFENDER, "T3", at(_x, _z), _quarterTurns, "W" + std::to_string(++number))); };
  for (std::int32_t section = 0; section < 3; ++section)
  {
    raise(BreachRun::WALL_WEST_VOXELS + section * SECTION_LENGTH_VOXELS, BreachRun::WALL_SOUTH_VOXELS, 2);
  }
  for (std::int32_t section = 0; section < 2; ++section)
  {
    raise(BreachRun::WALL_EAST_VOXELS - thickness + 1, BreachRun::WALL_SOUTH_VOXELS + thickness + section * SECTION_LENGTH_VOXELS, 1);
  }
  for (std::int32_t section = 2; section >= 0; --section)
  {
    raise(BreachRun::WALL_WEST_VOXELS + section * SECTION_LENGTH_VOXELS, BreachRun::WALL_NORTH_VOXELS - thickness + 1, 0);
  }
  for (std::int32_t section = 1; section >= 0; --section)
  {
    raise(BreachRun::WALL_WEST_VOXELS, BreachRun::WALL_SOUTH_VOXELS + thickness + section * SECTION_LENGTH_VOXELS, 3);
  }
  return raised.at(1);
}

/// An aim on W2's south face, which spans x 377 to 384, at height _y. A shot descends on the face at
/// 45 degrees, so it strikes the face one voxel above where it is aimed: aimed at y 8, the earth at the
/// wall's foot, it strikes the wall's ground row, y 9; aimed at y 9, the row above, and so on up to
/// the crenellation, y 14 and 15.
constexpr Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y) noexcept
{
  return {.xVoxels = _x, .yVoxels = _y, .zVoxels = BreachRun::WALL_SOUTH_VOXELS};
}

/// _count aims in a row at height _y, from _x eastwards.
std::vector<Protocol::VoxelPosition> Row(std::int32_t _x, std::int32_t _y, std::int32_t _count)
{
  std::vector<Protocol::VoxelPosition> aims;
  for (std::int32_t x = _x; x < _x + _count; ++x)
  {
    aims.push_back(At(x, _y));
  }
  return aims;
}

std::vector<LabItem> MakeItems()
{
  // The set (GameDesign.md, M3's values, item 2): eight open and eight shut by the simulation's key,
  // chosen from a sweep of about 1,900 candidates run headless on this site, the implementer's. Each
  // was kept only if its prior shots leave W2 shut, it never opens and closes again while its asked
  // shots land (a third shot through a 1 m doorway craters the ground behind it, and the squad can be
  // inside before it does), and the squad ends in the courtyard exactly when the key is open. Most
  // items have a twin on the other side of the key, alike but for one thing, so that no reading of
  // the set alone is enough: neither where the prior damage is, nor the mark's row, nor N. The keys
  // are BreachLabTests' table, never typed here.
  return {
    // The practice item: an intact wall and two shots at its ground row, which stays shut.
    {.id = "P0", .priorAimVoxels = {}, .markVoxels = At(381, 8), .shotsAsked = 2},
    // Two at one mark a row up open a doorway 1 m wide, 2 m high, through both layers.
    {.id = "L01", .priorAimVoxels = {}, .markVoxels = At(380, 9), .shotsAsked = 2},
    // A hole 4 wide at the ground row, and one shot far from it: shut.
    {.id = "L02", .priorAimVoxels = Row(380, 9, 4), .markVoxels = At(377, 9), .shotsAsked = 1},
    // The same hole further west, and one shot into its floor: open.
    {.id = "L03", .priorAimVoxels = Row(378, 9, 4), .markVoxels = At(379, 8), .shotsAsked = 1},
    // Three single holes with pillars between, and one shot beside the last: shut.
    {.id = "L04", .priorAimVoxels = {At(377, 9), At(379, 9), At(381, 9)}, .markVoxels = At(382, 9), .shotsAsked = 1},
    // A hole 4 wide high in the wall, whose top falls, and one shot low beneath it: open.
    {.id = "L05", .priorAimVoxels = Row(377, 12, 4), .markVoxels = At(380, 9), .shotsAsked = 1},
    // The same high hole at the section's east end, and one shot low: shut.
    {.id = "L06", .priorAimVoxels = Row(381, 12, 4), .markVoxels = At(383, 9), .shotsAsked = 1},
    // A hole 4 wide at mid-height, and one shot low beside it, which drops a piece: shut.
    {.id = "L07", .priorAimVoxels = Row(379, 11, 4), .markVoxels = At(378, 9), .shotsAsked = 1},
    // Two merlons shot away, and one shot at the second row: open.
    {.id = "L08", .priorAimVoxels = {At(378, 14), At(379, 14)}, .markVoxels = At(379, 10), .shotsAsked = 1},
    // A high hole 3 wide, and two shots at its east end, a row up from the ground: open.
    {.id = "L09", .priorAimVoxels = Row(380, 12, 3), .markVoxels = At(382, 10), .shotsAsked = 2},
    // A high hole 4 wide, and two shots inside it at the same row: shut.
    {.id = "L10", .priorAimVoxels = Row(379, 12, 4), .markVoxels = At(380, 10), .shotsAsked = 2},
    // A ground hole 4 wide at the west end, and four shots at the east end: shut.
    {.id = "L11", .priorAimVoxels = Row(377, 9, 4), .markVoxels = At(384, 10), .shotsAsked = 4},
    // A ground hole 4 wide at the east, and three shots beside it a row up: open.
    {.id = "L12", .priorAimVoxels = Row(380, 9, 4), .markVoxels = At(384, 10), .shotsAsked = 3},
    // A ground hole 4 wide at the west, and four shots high above its west end: open.
    {.id = "L13", .priorAimVoxels = Row(377, 9, 4), .markVoxels = At(377, 12), .shotsAsked = 4},
    // A ground hole 3 wide, and four shots at the crenellation above it: shut.
    {.id = "L14", .priorAimVoxels = Row(380, 9, 3), .markVoxels = At(380, 14), .shotsAsked = 4},
    // A hole 4 wide at the second row, and five shots inside it a row up: shut.
    {.id = "L15", .priorAimVoxels = Row(380, 10, 4), .markVoxels = At(382, 11), .shotsAsked = 5},
    // A high hole 4 wide, and four shots at the crenellation above its west end: open.
    {.id = "L16", .priorAimVoxels = Row(379, 12, 4), .markVoxels = At(379, 14), .shotsAsked = 4},
  };
}

void WritePosition(Core::ByteWriter& _writer, Protocol::VoxelPosition _position)
{
  _writer.WriteI32(_position.xVoxels);
  _writer.WriteI32(_position.yVoxels);
  _writer.WriteI32(_position.zVoxels);
}

} // namespace

std::span<const LabItem> LabItems()
{
  static const std::vector<LabItem> ITEMS = MakeItems();
  return ITEMS;
}

std::uint64_t LabItemsHash()
{
  // Each item as its id's length and bytes, its prior aims' count and each aim, its mark and N.
  Core::ByteWriter writer;
  for (const LabItem& item : LabItems())
  {
    writer.WriteU32(static_cast<std::uint32_t>(item.id.size()));
    writer.WriteBytes(std::as_bytes(std::span(item.id)));
    writer.WriteU32(static_cast<std::uint32_t>(item.priorAimVoxels.size()));
    for (const Protocol::VoxelPosition aim : item.priorAimVoxels)
    {
      WritePosition(writer, aim);
    }
    WritePosition(writer, item.markVoxels);
    writer.WriteU32(item.shotsAsked);
  }
  return Core::Fnv1a64(writer.Bytes());
}

World::Desc LabDesc(const ServerData& _data, std::uint64_t _buildId)
{
  return BreachRunDesc(_data, _buildId);
}

LabSetup BuildLabSite(World& _world)
{
  if (_world.TickCount() != 0)
  {
    throw std::invalid_argument("the breach lab's site is built on a world that has not ticked");
  }
  Level(_world);
  const Protocol::EntityId target = RaiseEnclosure(_world);

  const std::int32_t middleX = (BreachRun::WALL_WEST_VOXELS + BreachRun::WALL_EAST_VOXELS + 1) / 2;
  const std::int32_t engineZ = BreachRun::WALL_SOUTH_VOXELS - TREBUCHET_STANDOFF_VOXELS;
  const Protocol::VoxelPosition engineCell{.xVoxels = middleX, .yVoxels = FirstEmptyAbove(_world, middleX, engineZ), .zVoxels = engineZ};
  const Protocol::EntityId engine =
    _world.AddSiegeEngine(BreachRun::PLAYER, EngineKind::Trebuchet, engineCell, BreachRun::TREBUCHET_QUARTER_TURNS);

  // Six men-at-arms in a row from east to west, the lead at the east end, 10 m south of the wall.
  std::vector<Protocol::VoxelPosition> cells;
  cells.reserve(BreachRun::SQUAD_FIGURES);
  for (std::size_t figure = 0; figure < BreachRun::SQUAD_FIGURES; ++figure)
  {
    cells.push_back({.xVoxels = middleX + 2 - static_cast<std::int32_t>(figure),
                     .yVoxels = BreachRun::GROUND_VOXELS,
                     .zVoxels = BreachRun::WALL_SOUTH_VOXELS - SQUAD_STANDOFF_VOXELS});
  }
  const Protocol::EntityId squad = _world.AddSquad(BreachRun::PLAYER, NavigationClass::Foot, cells, UnitKind::ManAtArms);
  return {
    .engine = engine,
    .squad = squad,
    .target = target,
    .squadStartVoxels = cells.front(),
    .goalVoxels = {.xVoxels = middleX, .yVoxels = BreachRun::GROUND_VOXELS, .zVoxels = BreachRun::WALL_SOUTH_VOXELS + GOAL_INSIDE_VOXELS}};
}

LabShots::LabShots(const LabSetup& _setup, std::span<const Protocol::VoxelPosition> _aimVoxels)
  : m_setup(_setup),
    m_aimVoxels(_aimVoxels.begin(), _aimVoxels.end())
{
}

bool LabShots::Advance(World& _world)
{
  if (m_done)
  {
    return true;
  }
  const auto encode = [&_world](const Protocol::Command& _command)
  {
    Core::ByteWriter writer;
    Protocol::Encode(_command, writer);
    _world.Submit(BreachRun::PLAYER, writer.Bytes());
  };
  if (_world.TickCount() == LAB_ORDER_TICK)
  {
    encode(Protocol::MoveSquad{.squad = m_setup.squad, .goalVoxels = m_setup.goalVoxels});
  }
  if (m_fired < m_aimVoxels.size() && _world.SiegeEngines().at(m_setup.engine).reloadTicks == 0)
  {
    encode(Protocol::FireEngine{.engine = m_setup.engine, .targetVoxels = m_aimVoxels[m_fired]});
    ++m_fired;
  }
  _world.Tick();
  ++m_ticks;
  if (m_fired == m_aimVoxels.size() && _world.SiegeEngines().at(m_setup.engine).reloadTicks == 0 && _world.Quiet())
  {
    m_done = true;
  }
  else if (m_ticks >= LAB_MAX_TICKS_PER_PHASE)
  {
    m_done = true;
    m_timedOut = true;
  }
  return m_done;
}

bool LabShots::Done() const noexcept
{
  return m_done;
}

bool LabShots::TimedOut() const noexcept
{
  return m_timedOut;
}

bool CourtyardOpen(const World& _world, const LabSetup& _setup)
{
  const NavigationGrid& navigation = _world.Navigation();
  const std::int32_t width = navigation.WidthCells();
  const std::int64_t start = (std::int64_t{_setup.squadStartVoxels.zVoxels} * width) + _setup.squadStartVoxels.xVoxels;
  const std::int64_t goal = (std::int64_t{_setup.goalVoxels.zVoxels} * width) + _setup.goalVoxels.xVoxels;
  return navigation.NearestReachable(start, goal, NavigationClass::Foot) == goal;
}

LabOutcome ResolveLabItem(World& _world, const LabSetup& _setup, const LabItem& _item)
{
  LabShots prior(_setup, _item.priorAimVoxels);
  while (!prior.Advance(_world))
  {
  }
  const std::vector<Protocol::VoxelPosition> asked(_item.shotsAsked, _item.markVoxels);
  LabShots shots(_setup, asked);
  while (!shots.Advance(_world))
  {
  }
  return {.open = CourtyardOpen(_world, _setup),
          .ticks = _world.TickCount(),
          .stateHash = StateHash(_world),
          .timedOut = prior.TimedOut() || shots.TimedOut()};
}

} // namespace Simulation
