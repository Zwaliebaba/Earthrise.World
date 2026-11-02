#include "pch.h"

#include "Save.h"

#include "ByteReader.h"
#include "ByteWriter.h"
#include "ChunkEdits.h"
#include "Hash.h"

namespace Simulation
{

namespace
{

constexpr std::array<std::byte, 4> SAVE_MAGIC = {std::byte{'E'}, std::byte{'R'}, std::byte{'S'}, std::byte{'V'}};

[[noreturn]] void Refuse(const std::string& _check)
{
  throw std::runtime_error("a save refused: " + _check);
}

/// The chunk the generator makes at _coordinate, or an empty one outside the generated world.
std::unique_ptr<Protocol::Chunk> BaseChunk(const World::Desc& _desc, Protocol::ChunkCoordinate _coordinate)
{
  auto chunk = std::make_unique<Protocol::Chunk>();
  if (_coordinate.xChunks >= 0 && _coordinate.xChunks < _desc.widthChunks && _coordinate.yChunks >= 0 &&
      _coordinate.yChunks < _desc.heightChunks && _coordinate.zChunks >= 0 && _coordinate.zChunks < _desc.depthChunks)
  {
    _desc.generate(_coordinate, *chunk);
  }
  return chunk;
}

void WritePosition(Core::ByteWriter& _writer, Protocol::VoxelPosition _position)
{
  _writer.WriteI32(_position.xVoxels);
  _writer.WriteI32(_position.yVoxels);
  _writer.WriteI32(_position.zVoxels);
}

Protocol::VoxelPosition ReadPosition(Core::ByteReader& _reader)
{
  return {.xVoxels = _reader.ReadI32(), .yVoxels = _reader.ReadI32(), .zVoxels = _reader.ReadI32()};
}

void WriteFixed(Core::ByteWriter& _writer, Core::Fixed _value)
{
  _writer.WriteI32(_value.Raw());
}

Core::Fixed ReadFixed(Core::ByteReader& _reader)
{
  return Core::Fixed::FromRaw(_reader.ReadI32());
}

void WriteFixedPosition(Core::ByteWriter& _writer, const FixedPosition& _position)
{
  _writer.WriteI32(_position.xMeters.Raw());
  _writer.WriteI32(_position.yMeters.Raw());
  _writer.WriteI32(_position.zMeters.Raw());
}

FixedPosition ReadFixedPosition(Core::ByteReader& _reader)
{
  return {.xMeters = Core::Fixed::FromRaw(_reader.ReadI32()),
          .yMeters = Core::Fixed::FromRaw(_reader.ReadI32()),
          .zMeters = Core::Fixed::FromRaw(_reader.ReadI32())};
}

template <typename Enum> Enum ReadEnum(Core::ByteReader& _reader, Enum _last, const char* _what)
{
  const std::uint8_t value = _reader.ReadU8();
  if (value > static_cast<std::uint8_t>(_last))
  {
    Refuse(std::string("an entity holds a ") + _what + " this build does not know");
  }
  return static_cast<Enum>(value);
}

std::uint32_t ReadQuarterTurns(Core::ByteReader& _reader)
{
  const std::uint8_t value = _reader.ReadU8();
  if (value > 3)
  {
    Refuse("an entity is turned " + std::to_string(value) + " quarter turns, more than 3");
  }
  return value;
}

void WriteText(Core::ByteWriter& _writer, std::string_view _text)
{
  _writer.WriteU32(static_cast<std::uint32_t>(_text.size()));
  _writer.WriteBytes(std::as_bytes(std::span(_text.data(), _text.size())));
}

std::string ReadText(Core::ByteReader& _reader)
{
  const std::span<const std::byte> bytes = _reader.ReadBytes(_reader.ReadU32());
  std::string text(bytes.size(), '\0');
  std::ranges::transform(bytes, text.begin(), [](std::byte _byte) { return static_cast<char>(_byte); });
  return text;
}

bool ReadBool(Core::ByteReader& _reader)
{
  const std::uint8_t value = _reader.ReadU8();
  if (value > 1)
  {
    Refuse("an entity holds a flag that is neither 0 nor 1");
  }
  return value == 1;
}

// ── Each kind's record ──────────────────────────────────────────────────────────────────────────

void WriteRecord(Core::ByteWriter& _writer, const Squad& _squad)
{
  _writer.WriteU64(_squad.id);
  _writer.WriteU8(_squad.owner);
  _writer.WriteU8(static_cast<std::uint8_t>(_squad.navigationClass));
  _writer.WriteU8(_squad.hasGoal ? std::uint8_t{1} : std::uint8_t{0});
  WritePosition(_writer, _squad.goalVoxels);
  _writer.WriteU32(static_cast<std::uint32_t>(_squad.units.size()));
  for (const Protocol::EntityId unit : _squad.units)
  {
    _writer.WriteU64(unit);
  }
  _writer.WriteU32(static_cast<std::uint32_t>(_squad.path.size()));
  for (const PathSearch::CellIndex cell : _squad.path)
  {
    _writer.WriteI64(cell);
  }
  _writer.WriteU8(_squad.shortOfGoal ? std::uint8_t{1} : std::uint8_t{0});
  _writer.WriteU32(static_cast<std::uint32_t>(_squad.trail.size()));
  for (const PathSearch::CellIndex cell : _squad.trail)
  {
    _writer.WriteI64(cell);
  }
  _writer.WriteU64(_squad.garrisonedIn);
  _writer.WriteU8(static_cast<std::uint8_t>(_squad.kind));
  _writer.WriteU8(_squad.facing);
  _writer.WriteU8(static_cast<std::uint8_t>(_squad.order));
  _writer.WriteU64(_squad.target);
  WritePosition(_writer, _squad.trenchMinVoxels);
  WritePosition(_writer, _squad.trenchMaxVoxels);
  _writer.WriteU8(_squad.trenchNearSide);
  WriteFixed(_writer, _squad.digProgress);
}

Squad ReadSquad(Core::ByteReader& _reader)
{
  Squad squad{.id = _reader.ReadU64(),
              .owner = _reader.ReadU8(),
              .navigationClass = ReadEnum(_reader, NavigationClass::Machines, "navigation class"),
              .units = {},
              .hasGoal = ReadBool(_reader),
              .goalVoxels = ReadPosition(_reader),
              .path = {},
              .shortOfGoal = false,
              .trail = {},
              .garrisonedIn = Protocol::NO_ENTITY,
              .kind = UnitKind::Spearman,
              .facing = 0,
              .order = Protocol::SquadOrder::Idle,
              .target = Protocol::NO_ENTITY,
              .trenchMinVoxels = {},
              .trenchMaxVoxels = {},
              .trenchNearSide = 0,
              .digProgress = {}};
  const std::uint32_t units = _reader.ReadU32();
  for (std::uint32_t number = 0; number < units; ++number)
  {
    squad.units.push_back(_reader.ReadU64());
  }
  const std::uint32_t path = _reader.ReadU32();
  for (std::uint32_t step = 0; step < path; ++step)
  {
    squad.path.push_back(_reader.ReadI64());
  }
  squad.shortOfGoal = ReadBool(_reader);
  const std::uint32_t trail = _reader.ReadU32();
  for (std::uint32_t step = 0; step < trail; ++step)
  {
    squad.trail.push_back(_reader.ReadI64());
  }
  squad.garrisonedIn = _reader.ReadU64();
  squad.kind = ReadEnum(_reader, UnitKind::Trebuchet, "kind of unit");
  squad.facing = _reader.ReadU8();
  if (squad.facing > 7)
  {
    Refuse("a squad faces a direction this build does not know");
  }
  squad.order = ReadEnum(_reader, Protocol::SquadOrder::Garrisoned, "squad's order");
  squad.target = _reader.ReadU64();
  squad.trenchMinVoxels = ReadPosition(_reader);
  squad.trenchMaxVoxels = ReadPosition(_reader);
  squad.trenchNearSide = _reader.ReadU8();
  if (squad.trenchNearSide > 3)
  {
    Refuse("a squad's trench has its ramp at an end this build does not know");
  }
  squad.digProgress = ReadFixed(_reader);
  return squad;
}

void WriteRecord(Core::ByteWriter& _writer, const Unit& _unit)
{
  _writer.WriteU64(_unit.id);
  _writer.WriteU8(_unit.owner);
  _writer.WriteU8(static_cast<std::uint8_t>(_unit.navigationClass));
  _writer.WriteU64(_unit.squad);
  WriteFixedPosition(_writer, _unit.position);
  WritePosition(_writer, _unit.cellVoxels);
  _writer.WriteU32(_unit.trailStep);
  _writer.WriteI64(_unit.stepEffort);
  _writer.WriteU8(static_cast<std::uint8_t>(_unit.kind));
  _writer.WriteI32(_unit.hitPoints);
  _writer.WriteI64(_unit.reservedCell);
  _writer.WriteI64(_unit.stepFromCell);
  _writer.WriteU32(_unit.waitTicks);
  _writer.WriteU32(_unit.reloadTicks);
  _writer.WriteU64(_unit.target);
}

Unit ReadUnit(Core::ByteReader& _reader)
{
  return {.id = _reader.ReadU64(),
          .owner = _reader.ReadU8(),
          .navigationClass = ReadEnum(_reader, NavigationClass::Machines, "navigation class"),
          .squad = _reader.ReadU64(),
          .position = ReadFixedPosition(_reader),
          .cellVoxels = ReadPosition(_reader),
          .trailStep = _reader.ReadU32(),
          .stepEffort = _reader.ReadI64(),
          .kind = ReadEnum(_reader, UnitKind::Trebuchet, "kind of unit"),
          .hitPoints = _reader.ReadI32(),
          .reservedCell = _reader.ReadI64(),
          .stepFromCell = _reader.ReadI64(),
          .waitTicks = _reader.ReadU32(),
          .reloadTicks = _reader.ReadU32(),
          .target = _reader.ReadU64()};
}

void WriteRecord(Core::ByteWriter& _writer, const SiegeEngine& _engine)
{
  _writer.WriteU64(_engine.id);
  _writer.WriteU8(_engine.owner);
  _writer.WriteU8(static_cast<std::uint8_t>(_engine.kind));
  WriteFixedPosition(_writer, _engine.position);
  WritePosition(_writer, _engine.cellVoxels);
  _writer.WriteU32(_engine.reloadTicks);
  _writer.WriteU8(static_cast<std::uint8_t>(_engine.quarterTurns));
  _writer.WriteU8(static_cast<std::uint8_t>(_engine.state));
  _writer.WriteU64(_engine.squad);
  _writer.WriteU32(_engine.stateTicksLeft);
}

SiegeEngine ReadSiegeEngine(Core::ByteReader& _reader)
{
  return {.id = _reader.ReadU64(),
          .owner = _reader.ReadU8(),
          .kind = ReadEnum(_reader, EngineKind::Ram, "kind of engine"),
          .position = ReadFixedPosition(_reader),
          .cellVoxels = ReadPosition(_reader),
          .reloadTicks = _reader.ReadU32(),
          .quarterTurns = ReadQuarterTurns(_reader),
          .state = ReadEnum(_reader, EngineState::Packing, "engine state"),
          .squad = _reader.ReadU64(),
          .stateTicksLeft = _reader.ReadU32()};
}

void WriteRecord(Core::ByteWriter& _writer, const Shot& _shot)
{
  // Version 2 (M4-C4): the kind, then a stone's arc or an arrow's line, then an arrow's shooter, target and damage.
  _writer.WriteU64(_shot.id);
  _writer.WriteU64(_shot.engine);
  _writer.WriteU8(_shot.owner);
  _writer.WriteU8(static_cast<std::uint8_t>(_shot.kind));
  if (const auto* line = std::get_if<ArrowLine>(&_shot.flight))
  {
    WriteFixedPosition(_writer, line->start);
    WriteFixedPosition(_writer, line->end);
    WriteFixed(_writer, line->flightTicks);
  }
  else
  {
    const ShotArc& arc = std::get<ShotArc>(_shot.flight);
    WriteFixedPosition(_writer, arc.start);
    WriteFixed(_writer, arc.acrossXMeters);
    WriteFixed(_writer, arc.acrossZMeters);
    WriteFixed(_writer, arc.flightTicks);
    WriteFixed(_writer, arc.riseMetersPerTick);
    WriteFixed(_writer, arc.gravityMetersPerTickSquared);
  }
  _writer.WriteU32(_shot.ticksFlown);
  _writer.WriteU64(_shot.shooter);
  _writer.WriteU64(_shot.target);
  _writer.WriteI32(_shot.damage);
}

Shot ReadShot(Core::ByteReader& _reader)
{
  Shot shot{.id = _reader.ReadU64(),
            .engine = _reader.ReadU64(),
            .owner = _reader.ReadU8(),
            .flight = ShotArc{},
            .ticksFlown = 0,
            .kind = ReadEnum(_reader, Protocol::ShotKind::Arrow, "kind of shot"),
            .shooter = Protocol::NO_ENTITY,
            .target = Protocol::NO_ENTITY,
            .damage = 0};
  if (shot.kind == Protocol::ShotKind::Arrow)
  {
    ArrowLine line{.start = ReadFixedPosition(_reader), .end = {}, .flightTicks = {}};
    line.end = ReadFixedPosition(_reader);
    line.flightTicks = ReadFixed(_reader);
    shot.flight = line;
  }
  else
  {
    shot.flight = ShotArc{.start = ReadFixedPosition(_reader),
                          .acrossXMeters = ReadFixed(_reader),
                          .acrossZMeters = ReadFixed(_reader),
                          .flightTicks = ReadFixed(_reader),
                          .riseMetersPerTick = ReadFixed(_reader),
                          .gravityMetersPerTickSquared = ReadFixed(_reader)};
  }
  shot.ticksFlown = _reader.ReadU32();
  shot.shooter = _reader.ReadU64();
  shot.target = _reader.ReadU64();
  shot.damage = _reader.ReadI32();
  return shot;
}

void WritePositions(Core::ByteWriter& _writer, std::span<const Protocol::VoxelPosition> _positions)
{
  _writer.WriteU32(static_cast<std::uint32_t>(_positions.size()));
  for (const Protocol::VoxelPosition& position : _positions)
  {
    WritePosition(_writer, position);
  }
}

std::vector<Protocol::VoxelPosition> ReadPositions(Core::ByteReader& _reader)
{
  const std::uint32_t count = _reader.ReadU32();
  std::vector<Protocol::VoxelPosition> positions;
  positions.reserve(std::min<std::size_t>(count, _reader.Remaining() / 12)); // 12 bytes a position, at most what is left
  for (std::uint32_t number = 0; number < count; ++number)
  {
    positions.push_back(ReadPosition(_reader));
  }
  return positions;
}

void WriteRecord(Core::ByteWriter& _writer, const Collapse& _collapse)
{
  _writer.WriteU64(_collapse.id);
  WritePositions(_writer, _collapse.piece);
  const CollapsePlan& plan = _collapse.plan;
  _writer.WriteU8(static_cast<std::uint8_t>(plan.direction));
  _writer.WriteU32(plan.warningTicks);
  _writer.WriteU32(plan.fallTicks);
  _writer.WriteU32(plan.fallenVoxels);
  _writer.WriteU32(static_cast<std::uint32_t>(plan.landing.size()));
  for (const PlacedVoxel& placed : plan.landing)
  {
    WritePosition(_writer, placed.position);
    _writer.WriteU8(placed.paletteIndex);
  }
  WritePositions(_writer, plan.strip);
  _writer.WriteU32(_collapse.ticksElapsed);
  _writer.WriteU32(static_cast<std::uint32_t>(_collapse.struck.size()));
  for (const Protocol::EntityId unit : _collapse.struck)
  {
    _writer.WriteU64(unit);
  }
  _writer.WriteU64(_collapse.structure);
  _writer.WriteU32(static_cast<std::uint32_t>(_collapse.fallen.size()));
  for (const PlacedVoxel& voxel : _collapse.fallen)
  {
    WritePosition(_writer, voxel.position);
    _writer.WriteU8(voxel.paletteIndex);
  }
}

Collapse ReadCollapse(Core::ByteReader& _reader)
{
  Collapse collapse{.id = _reader.ReadU64(),
                    .piece = ReadPositions(_reader),
                    .plan = {},
                    .ticksElapsed = 0,
                    .struck = {},
                    .structure = Protocol::NO_ENTITY,
                    .fallen = {}};
  CollapsePlan& plan = collapse.plan;
  plan.direction = ReadEnum(_reader, FallDirection::West, "fall direction");
  plan.warningTicks = _reader.ReadU32();
  plan.fallTicks = _reader.ReadU32();
  plan.fallenVoxels = _reader.ReadU32();
  const std::uint32_t landing = _reader.ReadU32();
  for (std::uint32_t number = 0; number < landing; ++number)
  {
    const Protocol::VoxelPosition position = ReadPosition(_reader);
    plan.landing.push_back({.position = position, .paletteIndex = _reader.ReadU8()});
  }
  plan.strip = ReadPositions(_reader);
  collapse.ticksElapsed = _reader.ReadU32();
  const std::uint32_t struck = _reader.ReadU32();
  for (std::uint32_t number = 0; number < struck; ++number)
  {
    collapse.struck.push_back(_reader.ReadU64());
  }
  collapse.structure = _reader.ReadU64();
  const std::uint32_t fallen = _reader.ReadU32();
  for (std::uint32_t number = 0; number < fallen; ++number)
  {
    const Protocol::VoxelPosition position = ReadPosition(_reader);
    collapse.fallen.push_back({.position = position, .paletteIndex = _reader.ReadU8()});
  }
  return collapse;
}

void WriteRecord(Core::ByteWriter& _writer, const Structure& _structure)
{
  _writer.WriteU64(_structure.id);
  _writer.WriteU8(_structure.owner);
  WriteText(_writer, _structure.templateId);
  WritePosition(_writer, _structure.originVoxels);
  _writer.WriteU8(static_cast<std::uint8_t>(_structure.quarterTurns));
  WriteText(_writer, _structure.name);
  // Version 2 (M4-D1; ADR-024): the built mask, then what it is at work.
  _writer.WriteU32(static_cast<std::uint32_t>(_structure.builtMask.size()));
  for (const std::uint8_t byte : _structure.builtMask)
  {
    _writer.WriteU8(byte);
  }
  _writer.WriteU8(_structure.criticalPercent);
  _writer.WriteU8(_structure.working ? std::uint8_t{1} : std::uint8_t{0});
  _writer.WriteU32(_structure.repairLockTicks);
  WriteFixed(_writer, _structure.repairProgress);
  _writer.WriteU8(_structure.gateOpen ? std::uint8_t{1} : std::uint8_t{0});
  _writer.WriteI32(_structure.hitPoints);
}

Structure ReadStructure(Core::ByteReader& _reader)
{
  Structure structure{.id = _reader.ReadU64(),
                      .owner = _reader.ReadU8(),
                      .templateId = {},
                      .originVoxels = {},
                      .quarterTurns = 0,
                      .name = {},
                      .builtMask = {},
                      .criticalPercent = 100,
                      .working = true,
                      .repairLockTicks = 0,
                      .repairProgress = {},
                      .gateOpen = false,
                      .hitPoints = 0};
  structure.templateId = ReadText(_reader);
  structure.originVoxels = ReadPosition(_reader);
  structure.quarterTurns = ReadQuarterTurns(_reader);
  structure.name = ReadText(_reader);
  const std::uint32_t maskBytes = _reader.ReadU32();
  for (std::uint32_t byte = 0; byte < maskBytes; ++byte)
  {
    structure.builtMask.push_back(_reader.ReadU8());
  }
  structure.criticalPercent = _reader.ReadU8();
  const std::uint8_t working = _reader.ReadU8();
  structure.repairLockTicks = _reader.ReadU32();
  structure.repairProgress = ReadFixed(_reader);
  const std::uint8_t gateOpen = _reader.ReadU8();
  structure.hitPoints = _reader.ReadI32();
  if (structure.criticalPercent > 100 || working > 1 || gateOpen > 1)
  {
    Refuse("a structure's share, working or gate holds a value it cannot");
  }
  structure.working = working == 1;
  structure.gateOpen = gateOpen == 1;
  return structure;
}

/// One entity: its tag, its record's version and length, and the record.
template <typename Kind> void WriteEntity(Core::ByteWriter& _writer, SaveEntityTag _tag, const Kind& _entity)
{
  Core::ByteWriter record;
  WriteRecord(record, _entity);
  _writer.WriteU16(static_cast<std::uint16_t>(_tag));
  _writer.WriteU16(RecordVersion(_tag));
  _writer.WriteU32(static_cast<std::uint32_t>(record.Bytes().size()));
  _writer.WriteBytes(record.Bytes());
}

// The sections a save and the state hash share.

void WriteMatch(Core::ByteWriter& _writer, const World& _world)
{
  _writer.WriteU32(static_cast<std::uint32_t>(_world.Players().size()));
  for (const auto& [id, player] : _world.Players())
  {
    _writer.WriteU8(player.id);
    _writer.WriteU8(static_cast<std::uint8_t>(player.side));
    _writer.WriteI32(player.stoneHundredthsCubicMeters);
    _writer.WriteI32(player.timberHundredthsCubicMeters);
    _writer.WriteU8(player.revealed ? std::uint8_t{1} : std::uint8_t{0});
  }
  const std::optional<TrialRules>& trial = _world.Trial();
  _writer.WriteU8(trial.has_value() ? std::uint8_t{1} : std::uint8_t{0});
  if (trial.has_value())
  {
    _writer.WriteU8(static_cast<std::uint8_t>(trial->version));
    _writer.WriteU8(trial->attacker);
    _writer.WriteU8(trial->defender);
    _writer.WriteU64(trial->keep);
    _writer.WriteU32(trial->timeLimitTicks);
    _writer.WriteU32(trial->holdTicks);
    _writer.WriteI32(trial->ringCells);
    WritePosition(_writer, trial->campMinVoxels);
    WritePosition(_writer, trial->campMaxVoxels);
  }
}

void ReadMatch(Core::ByteReader& _reader, World& _world)
{
  const std::uint32_t players = _reader.ReadU32();
  for (std::uint32_t number = 0; number < players; ++number)
  {
    const std::uint8_t id = _reader.ReadU8();
    const Protocol::Side side = ReadEnum(_reader, Protocol::Side::Defender, "side");
    const std::int32_t stone = _reader.ReadI32();
    const std::int32_t timber = _reader.ReadI32();
    _world.AddPlayer(
      {.id = id, .side = side, .stoneHundredthsCubicMeters = stone, .timberHundredthsCubicMeters = timber, .revealed = ReadBool(_reader)});
  }
  if (ReadBool(_reader))
  {
    TrialRules trial{.version = ReadEnum(_reader, Protocol::TrialVersion::Control, "trial version"),
                     .attacker = _reader.ReadU8(),
                     .defender = _reader.ReadU8(),
                     .keep = _reader.ReadU64(),
                     .timeLimitTicks = _reader.ReadU32(),
                     .holdTicks = _reader.ReadU32(),
                     .ringCells = _reader.ReadI32(),
                     .campMinVoxels = {},
                     .campMaxVoxels = {}};
    trial.campMinVoxels = ReadPosition(_reader);
    trial.campMaxVoxels = ReadPosition(_reader);
    _world.SetTrial(trial);
  }
}

void WriteEntities(Core::ByteWriter& _writer, const World& _world)
{
  const std::map<Protocol::EntityId, Squad>& squads = _world.Squads();
  const std::map<Protocol::EntityId, Unit>& units = _world.Units();
  const std::map<Protocol::EntityId, SiegeEngine>& engines = _world.SiegeEngines();
  const std::map<Protocol::EntityId, Shot>& shots = _world.Shots();
  const std::map<Protocol::EntityId, Collapse>& collapses = _world.Collapses();
  const std::map<Protocol::EntityId, Structure>& structures = _world.Structures();
  // Every entity in id order, across the kinds (ADR-014, decision 4).
  std::vector<std::pair<Protocol::EntityId, SaveEntityTag>> order;
  order.reserve(squads.size() + units.size() + engines.size() + shots.size() + collapses.size() + structures.size());
  for (const auto& [id, squad] : squads)
  {
    order.emplace_back(id, SaveEntityTag::Squad);
  }
  for (const auto& [id, unit] : units)
  {
    order.emplace_back(id, SaveEntityTag::Unit);
  }
  for (const auto& [id, engine] : engines)
  {
    order.emplace_back(id, SaveEntityTag::SiegeEngine);
  }
  for (const auto& [id, shot] : shots)
  {
    order.emplace_back(id, SaveEntityTag::Shot);
  }
  for (const auto& [id, collapse] : collapses)
  {
    order.emplace_back(id, SaveEntityTag::Collapse);
  }
  for (const auto& [id, structure] : structures)
  {
    order.emplace_back(id, SaveEntityTag::Structure);
  }
  std::ranges::sort(order);
  _writer.WriteU32(static_cast<std::uint32_t>(order.size()));
  for (const auto& [id, tag] : order)
  {
    switch (tag)
    {
    case SaveEntityTag::Squad:
      WriteEntity(_writer, tag, squads.at(id));
      break;
    case SaveEntityTag::Unit:
      WriteEntity(_writer, tag, units.at(id));
      break;
    case SaveEntityTag::SiegeEngine:
      WriteEntity(_writer, tag, engines.at(id));
      break;
    case SaveEntityTag::Shot:
      WriteEntity(_writer, tag, shots.at(id));
      break;
    case SaveEntityTag::Collapse:
      WriteEntity(_writer, tag, collapses.at(id));
      break;
    case SaveEntityTag::Structure:
      WriteEntity(_writer, tag, structures.at(id));
      break;
    }
  }
}

void WritePendingWork(Core::ByteWriter& _writer, std::span<const SubmittedCommand> _commands,
                      std::span<const Protocol::VoxelPosition> _unsearched, std::span<const Protocol::VoxelPosition> _betweenTicks,
                      const std::set<std::int64_t>& _slumpQueue, const std::set<std::int64_t>& _openedColumns,
                      const std::map<std::pair<std::uint32_t, Protocol::EntityId>, PathSearch>& _searches)
{
  _writer.WriteU32(static_cast<std::uint32_t>(_commands.size()));
  for (const SubmittedCommand& command : _commands)
  {
    _writer.WriteU8(command.player);
    _writer.WriteU32(static_cast<std::uint32_t>(command.bytes.size()));
    _writer.WriteBytes(command.bytes);
  }
  WritePositions(_writer, _unsearched);
  WritePositions(_writer, _betweenTicks);
  _writer.WriteU32(static_cast<std::uint32_t>(_slumpQueue.size()));
  for (const std::int64_t column : _slumpQueue)
  {
    _writer.WriteI64(column);
  }
  _writer.WriteU32(static_cast<std::uint32_t>(_openedColumns.size()));
  for (const std::int64_t column : _openedColumns)
  {
    _writer.WriteI64(column);
  }
  _writer.WriteU32(static_cast<std::uint32_t>(_searches.size()));
  for (const auto& [key, search] : _searches)
  {
    _writer.WriteU32(key.first);
    _writer.WriteU64(key.second);
    search.Write(_writer);
  }
}

/// How the trial stands (M4-F1): its outcome, the tick of it and the storming clock, the sections opened in id order,
/// and each figure in a section's footprint, in figure order, with its section and the side it came in by.
void WriteTrialProgress(Core::ByteWriter& _writer, const TrialProgress& _progress)
{
  _writer.WriteU8(static_cast<std::uint8_t>(_progress.outcome));
  _writer.WriteU32(_progress.endTick);
  _writer.WriteU32(_progress.holdTicks);
  _writer.WriteU32(static_cast<std::uint32_t>(_progress.openedSections.size()));
  for (const Protocol::EntityId section : _progress.openedSections)
  {
    _writer.WriteU64(section);
  }
  _writer.WriteU32(static_cast<std::uint32_t>(_progress.passing.size()));
  for (const auto& [unit, passing] : _progress.passing)
  {
    _writer.WriteU64(unit);
    _writer.WriteU64(passing.first);
    _writer.WriteU8(passing.second ? std::uint8_t{1} : std::uint8_t{0});
  }
}

TrialProgress ReadTrialProgress(Core::ByteReader& _reader)
{
  TrialProgress progress;
  progress.outcome = ReadEnum(_reader, Protocol::TrialOutcome::LostArmy, "trial's outcome");
  progress.endTick = _reader.ReadU32();
  progress.holdTicks = _reader.ReadU32();
  const std::uint32_t opened = _reader.ReadU32();
  for (std::uint32_t number = 0; number < opened; ++number)
  {
    progress.openedSections.insert(_reader.ReadU64());
  }
  const std::uint32_t passing = _reader.ReadU32();
  for (std::uint32_t number = 0; number < passing; ++number)
  {
    const Protocol::EntityId unit = _reader.ReadU64();
    const Protocol::EntityId section = _reader.ReadU64();
    progress.passing.emplace(unit, std::pair{section, ReadBool(_reader)});
  }
  return progress;
}

void WriteStructureView(Core::ByteWriter& _writer, const Protocol::StructureView& _view)
{
  _writer.WriteU64(_view.id);
  _writer.WriteU8(_view.owner);
  WriteText(_writer, _view.templateId);
  WritePosition(_writer, _view.originVoxels);
  _writer.WriteU32(_view.quarterTurns);
  WritePosition(_writer, _view.extentVoxels);
  WriteText(_writer, _view.name);
  _writer.WriteU8(_view.working ? std::uint8_t{1} : std::uint8_t{0});
  _writer.WriteU8(_view.criticalPercent);
  _writer.WriteU32(_view.repairLockTicks);
  _writer.WriteU8(_view.gateOpen ? std::uint8_t{1} : std::uint8_t{0});
  _writer.WriteU8(_view.garrisonCount);
  _writer.WriteU8(_view.garrisonCapacity);
  _writer.WriteI32(_view.hitPoints);
  _writer.WriteI32(_view.maxHitPoints);
  _writer.WriteU8(_view.opened ? std::uint8_t{1} : std::uint8_t{0});
  _writer.WriteU8(_view.lastSeen ? std::uint8_t{1} : std::uint8_t{0});
}

Protocol::StructureView ReadStructureView(Core::ByteReader& _reader)
{
  Protocol::StructureView view{.id = _reader.ReadU64(),
                               .owner = _reader.ReadU8(),
                               .templateId = ReadText(_reader),
                               .originVoxels = ReadPosition(_reader),
                               .quarterTurns = _reader.ReadU32(),
                               .extentVoxels = ReadPosition(_reader),
                               .name = ReadText(_reader)};
  view.working = ReadBool(_reader);
  view.criticalPercent = _reader.ReadU8();
  view.repairLockTicks = _reader.ReadU32();
  view.gateOpen = ReadBool(_reader);
  view.garrisonCount = _reader.ReadU8();
  view.garrisonCapacity = _reader.ReadU8();
  view.hitPoints = _reader.ReadI32();
  view.maxHitPoints = _reader.ReadI32();
  view.opened = ReadBool(_reader);
  view.lastSeen = ReadBool(_reader);
  return view;
}

/// Each player's structures as they last saw them, in player then id order (ADR-025).
void WriteLastSeen(Core::ByteWriter& _writer,
                   const std::map<std::uint8_t, std::map<Protocol::EntityId, Protocol::StructureView>>& _lastSeen)
{
  _writer.WriteU32(static_cast<std::uint32_t>(_lastSeen.size()));
  for (const auto& [player, views] : _lastSeen)
  {
    _writer.WriteU8(player);
    _writer.WriteU32(static_cast<std::uint32_t>(views.size()));
    for (const auto& [id, view] : views)
    {
      WriteStructureView(_writer, view);
    }
  }
}

} // namespace

std::uint16_t RecordVersion(SaveEntityTag _tag) noexcept
{
  switch (_tag)
  {
  case SaveEntityTag::Squad: // M4-B4: the building it is garrisoned in; M4-C1: its kind; M4-C2: its facing; M4-C3: its order;
                             // M4-D6: its trench
    return 6;
  case SaveEntityTag::Unit:        // M4-C1: its hit points; M4-C2: its reservation, its step and its wait; M4-C3: its reload and target
  case SaveEntityTag::SiegeEngine: // M4-B4: its state; M4-C1: its body's squad; M4-C5: its deploy's or pack's ticks left
    return 4;
  case SaveEntityTag::Shot:      // M4-C4: its kind, an arrow's line, shooter, target and damage
  case SaveEntityTag::Structure: // M4-D1: its built mask, share, working, lock, repair's progress, gate and hit points
    return 2;
  default:
    return 1;
  }
}

std::vector<std::byte> WriteSave(const World& _world)
{
  const World::Desc& desc = _world.m_desc;
  Core::ByteWriter writer;
  writer.WriteBytes(SAVE_MAGIC);
  writer.WriteU32(SAVE_FORMAT_VERSION);
  writer.WriteU64(desc.buildId);
  writer.WriteU64(desc.serverData->Hash());
  writer.WriteU32(desc.generatorVersion);
  writer.WriteU64(desc.worldSeed);
  writer.WriteU64(desc.resourceSeed);
  writer.WriteU64(desc.matchSeed);
  writer.WriteU32(_world.m_tick);
  writer.WriteU64(_world.m_nextEntityId);
  WriteMatch(writer, _world);
  WriteTrialProgress(writer, _world.m_trialProgress);

  // The edited chunks that still differ from their base, in rising key order.
  std::vector<std::pair<std::uint64_t, std::vector<std::byte>>> chunks;
  for (const std::uint64_t key : _world.m_voxels.EditedChunks())
  {
    const Protocol::Chunk* edited = _world.m_voxels.Store().Find(key);
    const std::unique_ptr<Protocol::Chunk> base = BaseChunk(desc, Protocol::UnpackChunkKey(key));
    std::optional<std::vector<std::byte>> edits = EncodeChunkEdits(*base, edited != nullptr ? *edited : Protocol::Chunk{});
    if (edits.has_value())
    {
      chunks.emplace_back(key, std::move(*edits));
    }
  }
  writer.WriteU32(static_cast<std::uint32_t>(chunks.size()));
  for (const auto& [key, edits] : chunks)
  {
    writer.WriteU64(key);
    writer.WriteBytes(edits);
  }

  WriteEntities(writer, _world);
  WritePendingWork(writer, _world.m_pendingCommands, _world.m_unsearchedEdits, _world.m_voxels.TickEdits(), _world.m_slumpQueue,
                   _world.m_openedColumns, _world.m_searches);
  WriteLastSeen(writer, _world.m_lastSeen);
  writer.WriteU32(static_cast<std::uint32_t>(_world.m_chunksSent.size()));
  for (const auto& [player, sent] : _world.m_chunksSent)
  {
    writer.WriteU8(player);
    writer.WriteU32(static_cast<std::uint32_t>(sent.size()));
    for (const auto& [key, version] : sent)
    {
      writer.WriteU64(key);
      writer.WriteU64(version);
    }
  }
  return writer.TakeBytes();
}

World LoadSave(std::span<const std::byte> _save, const World::Desc& _desc)
{
  if (_desc.serverData == nullptr)
  {
    throw std::invalid_argument("a save loads against a Desc with server data");
  }
  Core::ByteReader reader(_save);
  try
  {
    if (_save.size() < SAVE_MAGIC.size() || !std::ranges::equal(reader.ReadBytes(SAVE_MAGIC.size()), SAVE_MAGIC))
    {
      Refuse("it does not begin with the magic number ERSV");
    }
    const std::uint32_t version = reader.ReadU32();
    if (version != SAVE_FORMAT_VERSION)
    {
      Refuse("its format version is " + std::to_string(version) + ", and this build reads " + std::to_string(SAVE_FORMAT_VERSION));
    }
    if (reader.ReadU64() != _desc.buildId)
    {
      Refuse("it was made by another build (ADR-014, decision 6)");
    }
    if (reader.ReadU64() != _desc.serverData->Hash())
    {
      Refuse("it was made with other server data, whose hash differs (ADR-014, decision 6)");
    }
    const std::uint32_t generatorVersion = reader.ReadU32();
    if (generatorVersion != _desc.generatorVersion)
    {
      Refuse("it was made by generator version " + std::to_string(generatorVersion) + ", and the world is version " +
             std::to_string(_desc.generatorVersion));
    }
    World::Desc desc = _desc;
    desc.worldSeed = reader.ReadU64();
    desc.resourceSeed = reader.ReadU64();
    desc.matchSeed = reader.ReadU64();
    World world(desc);
    world.m_tick = reader.ReadU32();
    world.m_nextEntityId = reader.ReadU64();
    ReadMatch(reader, world);
    world.m_trialProgress = ReadTrialProgress(reader);

    const std::uint32_t chunks = reader.ReadU32();
    for (std::uint32_t number = 0; number < chunks; ++number)
    {
      const Protocol::ChunkCoordinate coordinate = Protocol::UnpackChunkKey(reader.ReadU64());
      const std::unique_ptr<Protocol::Chunk> chunk = BaseChunk(desc, coordinate);
      ApplyChunkEdits(reader, *chunk);
      world.m_voxels.RestoreChunk(coordinate, *chunk);
    }
    // Navigation and its reach are computed from the voxels, so they are rebuilt rather than saved
    // (ADR-014, decision 4).
    world.m_navigation.RebuildAll(world.m_voxels.Store());
    world.m_navigation.UpdateReach();

    const std::uint32_t entities = reader.ReadU32();
    for (std::uint32_t number = 0; number < entities; ++number)
    {
      const std::uint16_t tag = reader.ReadU16();
      const std::uint16_t recordVersion = reader.ReadU16();
      const std::uint32_t length = reader.ReadU32();
      if (tag >= static_cast<std::uint16_t>(SaveEntityTag::Squad) && tag <= static_cast<std::uint16_t>(SaveEntityTag::Structure) &&
          recordVersion != RecordVersion(static_cast<SaveEntityTag>(tag)))
      {
        Refuse("an entity's record is at version " + std::to_string(recordVersion) + ", and this build reads " +
               std::to_string(RecordVersion(static_cast<SaveEntityTag>(tag))));
      }
      Core::ByteReader record(reader.ReadBytes(length));
      switch (static_cast<SaveEntityTag>(tag))
      {
      case SaveEntityTag::Squad:
      {
        Squad squad = ReadSquad(record);
        world.m_squads.emplace(squad.id, std::move(squad));
        break;
      }
      case SaveEntityTag::Unit:
      {
        const Unit unit = ReadUnit(record);
        world.m_units.emplace(unit.id, unit);
        break;
      }
      case SaveEntityTag::SiegeEngine:
      {
        const SiegeEngine engine = ReadSiegeEngine(record);
        world.m_siegeEngines.emplace(engine.id, engine);
        break;
      }
      case SaveEntityTag::Shot:
      {
        const Shot shot = ReadShot(record);
        world.m_shots.emplace(shot.id, shot);
        break;
      }
      case SaveEntityTag::Collapse:
      {
        Collapse collapse = ReadCollapse(record);
        world.m_collapses.emplace(collapse.id, std::move(collapse));
        break;
      }
      case SaveEntityTag::Structure:
      {
        Structure structure = ReadStructure(record);
        world.m_structures.emplace(structure.id, std::move(structure));
        break;
      }
      default:
        Refuse("it holds an entity of type " + std::to_string(tag) + ", which this build does not know");
      }
      if (!record.AtEnd())
      {
        Refuse("an entity's record runs on past its fields");
      }
    }
    // An open gate's voxels are air to navigation, which the structures' records say (ADR-024, decision 6).
    world.RestoreGates();

    const std::uint32_t commands = reader.ReadU32();
    for (std::uint32_t number = 0; number < commands; ++number)
    {
      const std::uint8_t player = reader.ReadU8();
      const std::span<const std::byte> bytes = reader.ReadBytes(reader.ReadU32());
      world.m_pendingCommands.push_back({.player = player, .bytes = {bytes.begin(), bytes.end()}});
    }
    world.m_unsearchedEdits = ReadPositions(reader);
    for (const Protocol::VoxelPosition& edit : ReadPositions(reader))
    {
      world.m_voxels.RestoreTickEdit(edit);
    }
    const std::uint32_t slumping = reader.ReadU32();
    for (std::uint32_t number = 0; number < slumping; ++number)
    {
      world.m_slumpQueue.insert(reader.ReadI64());
    }
    const std::uint32_t opened = reader.ReadU32();
    for (std::uint32_t number = 0; number < opened; ++number)
    {
      world.m_openedColumns.insert(reader.ReadI64());
    }
    const std::uint32_t searches = reader.ReadU32();
    for (std::uint32_t number = 0; number < searches; ++number)
    {
      const std::uint32_t tick = reader.ReadU32();
      const Protocol::EntityId squad = reader.ReadU64();
      world.m_searches.emplace(std::pair{tick, squad}, PathSearch::Read(reader));
    }
    // Fog's (ADR-025): the structures each player last saw, then the chunk versions each was last sent; visibility
    // itself is rebuilt from the units and structures.
    const std::uint32_t seeing = reader.ReadU32();
    for (std::uint32_t number = 0; number < seeing; ++number)
    {
      std::map<Protocol::EntityId, Protocol::StructureView>& views = world.m_lastSeen[reader.ReadU8()];
      const std::uint32_t count = reader.ReadU32();
      for (std::uint32_t view = 0; view < count; ++view)
      {
        Protocol::StructureView read = ReadStructureView(reader);
        views.insert_or_assign(read.id, std::move(read));
      }
    }
    const std::uint32_t sending = reader.ReadU32();
    for (std::uint32_t number = 0; number < sending; ++number)
    {
      std::map<std::uint64_t, std::uint64_t>& sent = world.m_chunksSent[reader.ReadU8()];
      const std::uint32_t count = reader.ReadU32();
      for (std::uint32_t chunk = 0; chunk < count; ++chunk)
      {
        const std::uint64_t key = reader.ReadU64();
        sent.insert_or_assign(key, reader.ReadU64());
      }
    }
    world.RecomputeVisibility();
    if (!reader.AtEnd())
    {
      Refuse("it holds bytes after its last section");
    }
    return world;
  }
  catch (const std::out_of_range&)
  {
    Refuse("it ends before its sections do");
  }
  catch (const std::invalid_argument& error)
  {
    Refuse(error.what());
  }
}

std::uint64_t StateHash(const World& _world)
{
  Core::ByteWriter writer;
  writer.WriteU32(_world.m_tick);
  writer.WriteU64(_world.m_nextEntityId);
  WriteMatch(writer, _world);
  WriteTrialProgress(writer, _world.m_trialProgress);
  WriteEntities(writer, _world);
  WritePendingWork(writer, _world.m_pendingCommands, _world.m_unsearchedEdits, _world.m_voxels.TickEdits(), _world.m_slumpQueue,
                   _world.m_openedColumns, _world.m_searches);
  // The chunk versions sent are the client's calls' doing, not the simulation's, so the hash leaves them out.
  WriteLastSeen(writer, _world.m_lastSeen);
  return Core::Fnv1a64(writer.Bytes(), _world.m_voxels.VoxelHash());
}

} // namespace Simulation
