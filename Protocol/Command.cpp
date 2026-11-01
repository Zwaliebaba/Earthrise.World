#include "pch.h"

#include "Command.h"

namespace Protocol
{

namespace
{

constexpr std::uint16_t MOVE_SQUAD_TAG = 1;
constexpr std::uint16_t FIRE_ENGINE_TAG = 2;

void WritePosition(VoxelPosition _position, Core::ByteWriter& _writer)
{
  _writer.WriteI32(_position.xVoxels);
  _writer.WriteI32(_position.yVoxels);
  _writer.WriteI32(_position.zVoxels);
}

VoxelPosition ReadPosition(Core::ByteReader& _reader)
{
  // Read in order: braced initialisation evaluates its elements left to right.
  return VoxelPosition{.xVoxels = _reader.ReadI32(), .yVoxels = _reader.ReadI32(), .zVoxels = _reader.ReadI32()};
}

} // namespace

void Encode(const Command& _command, Core::ByteWriter& _writer)
{
  if (const auto* move = std::get_if<MoveSquad>(&_command))
  {
    _writer.WriteU16(MOVE_SQUAD_TAG);
    _writer.WriteU16(COMMAND_RECORD_VERSION);
    _writer.WriteU64(move->squad);
    WritePosition(move->goalVoxels, _writer);
  }
  else if (const auto* fire = std::get_if<FireEngine>(&_command))
  {
    _writer.WriteU16(FIRE_ENGINE_TAG);
    _writer.WriteU16(COMMAND_RECORD_VERSION);
    _writer.WriteU64(fire->engine);
    WritePosition(fire->targetVoxels, _writer);
  }
}

Command DecodeCommand(Core::ByteReader& _reader)
{
  const std::uint16_t tag = _reader.ReadU16();
  const std::uint16_t version = _reader.ReadU16();
  if (tag != MOVE_SQUAD_TAG && tag != FIRE_ENGINE_TAG)
  {
    throw std::invalid_argument("a command with a tag this build does not know");
  }
  if (version != COMMAND_RECORD_VERSION)
  {
    throw std::invalid_argument("a command at a record version this build does not read");
  }
  const EntityId entity = _reader.ReadU64();
  const VoxelPosition position = ReadPosition(_reader);
  if (tag == MOVE_SQUAD_TAG)
  {
    return MoveSquad{.squad = entity, .goalVoxels = position};
  }
  return FireEngine{.engine = entity, .targetVoxels = position};
}

} // namespace Protocol
