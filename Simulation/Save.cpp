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
constexpr std::uint16_t RECORD_VERSION = 1;

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
  _writer.WriteU8(_squad.hasGoal ? 1 : 0);
  WritePosition(_writer, _squad.goalVoxels);
  _writer.WriteU32(static_cast<std::uint32_t>(_squad.units.size()));
  for (const Protocol::EntityId unit : _squad.units)
  {
    _writer.WriteU64(unit);
  }
}

Squad ReadSquad(Core::ByteReader& _reader)
{
  Squad squad{.id = _reader.ReadU64(),
              .owner = _reader.ReadU8(),
              .navigationClass = ReadEnum(_reader, NavigationClass::Machines, "navigation class"),
              .units = {},
              .hasGoal = ReadBool(_reader),
              .goalVoxels = ReadPosition(_reader)};
  const std::uint32_t units = _reader.ReadU32();
  for (std::uint32_t number = 0; number < units; ++number)
  {
    squad.units.push_back(_reader.ReadU64());
  }
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
}

Unit ReadUnit(Core::ByteReader& _reader)
{
  return {.id = _reader.ReadU64(),
          .owner = _reader.ReadU8(),
          .navigationClass = ReadEnum(_reader, NavigationClass::Machines, "navigation class"),
          .squad = _reader.ReadU64(),
          .position = ReadFixedPosition(_reader),
          .cellVoxels = ReadPosition(_reader)};
}

void WriteRecord(Core::ByteWriter& _writer, const SiegeEngine& _engine)
{
  _writer.WriteU64(_engine.id);
  _writer.WriteU8(_engine.owner);
  _writer.WriteU8(static_cast<std::uint8_t>(_engine.kind));
  WriteFixedPosition(_writer, _engine.position);
  WritePosition(_writer, _engine.cellVoxels);
}

SiegeEngine ReadSiegeEngine(Core::ByteReader& _reader)
{
  return {.id = _reader.ReadU64(),
          .owner = _reader.ReadU8(),
          .kind = ReadEnum(_reader, EngineKind::Trebuchet, "kind of engine"),
          .position = ReadFixedPosition(_reader),
          .cellVoxels = ReadPosition(_reader)};
}

/// One entity: its tag, its record's version and length, and the record.
template <typename Kind> void WriteEntity(Core::ByteWriter& _writer, SaveEntityTag _tag, const Kind& _entity)
{
  Core::ByteWriter record;
  WriteRecord(record, _entity);
  _writer.WriteU16(static_cast<std::uint16_t>(_tag));
  _writer.WriteU16(RECORD_VERSION);
  _writer.WriteU32(static_cast<std::uint32_t>(record.Bytes().size()));
  _writer.WriteBytes(record.Bytes());
}

// The sections a save and the state hash share.

void WriteEntities(Core::ByteWriter& _writer, const std::map<Protocol::EntityId, Squad>& _squads,
                   const std::map<Protocol::EntityId, Unit>& _units, const std::map<Protocol::EntityId, SiegeEngine>& _engines)
{
  // Every entity in id order, across the kinds (ADR-014, decision 4).
  std::vector<std::pair<Protocol::EntityId, SaveEntityTag>> order;
  order.reserve(_squads.size() + _units.size() + _engines.size());
  for (const auto& [id, squad] : _squads)
  {
    order.emplace_back(id, SaveEntityTag::Squad);
  }
  for (const auto& [id, unit] : _units)
  {
    order.emplace_back(id, SaveEntityTag::Unit);
  }
  for (const auto& [id, engine] : _engines)
  {
    order.emplace_back(id, SaveEntityTag::SiegeEngine);
  }
  std::ranges::sort(order);
  _writer.WriteU32(static_cast<std::uint32_t>(order.size()));
  for (const auto& [id, tag] : order)
  {
    switch (tag)
    {
    case SaveEntityTag::Squad:
      WriteEntity(_writer, tag, _squads.at(id));
      break;
    case SaveEntityTag::Unit:
      WriteEntity(_writer, tag, _units.at(id));
      break;
    case SaveEntityTag::SiegeEngine:
      WriteEntity(_writer, tag, _engines.at(id));
      break;
    }
  }
}

void WritePendingCommands(Core::ByteWriter& _writer, std::span<const SubmittedCommand> _commands)
{
  _writer.WriteU32(static_cast<std::uint32_t>(_commands.size()));
  for (const SubmittedCommand& command : _commands)
  {
    _writer.WriteU8(command.player);
    _writer.WriteU32(static_cast<std::uint32_t>(command.bytes.size()));
    _writer.WriteBytes(command.bytes);
  }
}

} // namespace

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

  WriteEntities(writer, _world.m_squads, _world.m_units, _world.m_siegeEngines);
  WritePendingCommands(writer, _world.m_pendingCommands);
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

    const std::uint32_t chunks = reader.ReadU32();
    for (std::uint32_t number = 0; number < chunks; ++number)
    {
      const Protocol::ChunkCoordinate coordinate = Protocol::UnpackChunkKey(reader.ReadU64());
      const std::unique_ptr<Protocol::Chunk> chunk = BaseChunk(desc, coordinate);
      ApplyChunkEdits(reader, *chunk);
      world.m_voxels.RestoreChunk(coordinate, *chunk);
    }

    const std::uint32_t entities = reader.ReadU32();
    for (std::uint32_t number = 0; number < entities; ++number)
    {
      const std::uint16_t tag = reader.ReadU16();
      const std::uint16_t recordVersion = reader.ReadU16();
      const std::uint32_t length = reader.ReadU32();
      if (recordVersion != RECORD_VERSION)
      {
        Refuse("an entity's record is at version " + std::to_string(recordVersion) + ", and this build reads " +
               std::to_string(RECORD_VERSION));
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
      default:
        Refuse("it holds an entity of type " + std::to_string(tag) + ", which this build does not know");
      }
      if (!record.AtEnd())
      {
        Refuse("an entity's record runs on past its fields");
      }
    }

    const std::uint32_t commands = reader.ReadU32();
    for (std::uint32_t number = 0; number < commands; ++number)
    {
      const std::uint8_t player = reader.ReadU8();
      const std::span<const std::byte> bytes = reader.ReadBytes(reader.ReadU32());
      world.m_pendingCommands.push_back({.player = player, .bytes = {bytes.begin(), bytes.end()}});
    }
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
  WriteEntities(writer, _world.m_squads, _world.m_units, _world.m_siegeEngines);
  WritePendingCommands(writer, _world.m_pendingCommands);
  return Core::Fnv1a64(writer.Bytes(), _world.m_voxels.VoxelHash());
}

} // namespace Simulation
