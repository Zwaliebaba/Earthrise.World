#include "pch.h"

#include "Command.h"

#include <type_traits>

namespace Protocol
{

namespace
{

constexpr std::uint16_t LAST_TAG = static_cast<std::uint16_t>(std::variant_size_v<Command>);

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

/// Writes one command's fields in declaration order.
void WriteFields(const Command& _command, Core::ByteWriter& _writer)
{
  std::visit(
    [&_writer](const auto& _fields)
    {
      using Fields = std::decay_t<decltype(_fields)>;
      if constexpr (std::is_same_v<Fields, MoveSquad> || std::is_same_v<Fields, StormTo>)
      {
        _writer.WriteU64(_fields.squad);
        WritePosition(_fields.goalVoxels, _writer);
      }
      else if constexpr (std::is_same_v<Fields, FireEngine>)
      {
        _writer.WriteU64(_fields.engine);
        WritePosition(_fields.targetVoxels, _writer);
      }
      else if constexpr (std::is_same_v<Fields, AttackTarget>)
      {
        _writer.WriteU64(_fields.squad);
        _writer.WriteU64(_fields.target);
      }
      else if constexpr (std::is_same_v<Fields, StopSquad> || std::is_same_v<Fields, HoldSquad> || std::is_same_v<Fields, SplitSquad>)
      {
        _writer.WriteU64(_fields.squad);
      }
      else if constexpr (std::is_same_v<Fields, MergeSquads>)
      {
        if (_fields.squads.size() > UINT8_MAX)
        {
          throw std::invalid_argument("a MergeSquads of more squads than a u8 counts");
        }
        _writer.WriteU8(static_cast<std::uint8_t>(_fields.squads.size()));
        for (const EntityId squad : _fields.squads)
        {
          _writer.WriteU64(squad);
        }
      }
      else if constexpr (std::is_same_v<Fields, GarrisonSquad>)
      {
        _writer.WriteU64(_fields.squad);
        _writer.WriteU64(_fields.structure);
      }
      else if constexpr (std::is_same_v<Fields, EjectGarrison>)
      {
        _writer.WriteU64(_fields.structure);
      }
      else if constexpr (std::is_same_v<Fields, RepairStructure>)
      {
        _writer.WriteU64(_fields.laborer);
        _writer.WriteU64(_fields.structure);
      }
      else if constexpr (std::is_same_v<Fields, DigTrench>)
      {
        _writer.WriteU64(_fields.squad);
        WritePosition(_fields.minVoxels, _writer);
        WritePosition(_fields.maxVoxels, _writer);
      }
      else if constexpr (std::is_same_v<Fields, PackEngine>)
      {
        _writer.WriteU64(_fields.engine);
      }
      else if constexpr (std::is_same_v<Fields, DeployEngine>)
      {
        _writer.WriteU64(_fields.engine);
        _writer.WriteU32(_fields.quarterTurns);
      }
      else
      {
        static_assert(std::is_same_v<Fields, SetGate>, "every command is written");
        _writer.WriteU64(_fields.structure);
        _writer.WriteU8(_fields.open ? std::uint8_t{1} : std::uint8_t{0});
      }
    },
    _command);
}

/// Reads the fields of the command _tag names.
Command ReadFields(std::uint16_t _tag, Core::ByteReader& _reader)
{
  switch (_tag)
  {
  case 1:
  {
    MoveSquad move{.squad = _reader.ReadU64(), .goalVoxels = {}};
    move.goalVoxels = ReadPosition(_reader);
    return move;
  }
  case 2:
  {
    FireEngine fire{.engine = _reader.ReadU64(), .targetVoxels = {}};
    fire.targetVoxels = ReadPosition(_reader);
    return fire;
  }
  case 3:
    return AttackTarget{.squad = _reader.ReadU64(), .target = _reader.ReadU64()};
  case 4:
  {
    StormTo storm{.squad = _reader.ReadU64(), .goalVoxels = {}};
    storm.goalVoxels = ReadPosition(_reader);
    return storm;
  }
  case 5:
    return StopSquad{.squad = _reader.ReadU64()};
  case 6:
    return HoldSquad{.squad = _reader.ReadU64()};
  case 7:
    return SplitSquad{.squad = _reader.ReadU64()};
  case 8:
  {
    MergeSquads merge;
    const std::uint8_t count = _reader.ReadU8();
    for (std::uint8_t squad = 0; squad < count; ++squad)
    {
      merge.squads.push_back(_reader.ReadU64());
    }
    return merge;
  }
  case 9:
    return GarrisonSquad{.squad = _reader.ReadU64(), .structure = _reader.ReadU64()};
  case 10:
    return EjectGarrison{.structure = _reader.ReadU64()};
  case 11:
    return RepairStructure{.laborer = _reader.ReadU64(), .structure = _reader.ReadU64()};
  case 12:
  {
    DigTrench dig{.squad = _reader.ReadU64(), .minVoxels = {}, .maxVoxels = {}};
    dig.minVoxels = ReadPosition(_reader);
    dig.maxVoxels = ReadPosition(_reader);
    return dig;
  }
  case 13:
    return PackEngine{.engine = _reader.ReadU64()};
  case 14:
    return DeployEngine{.engine = _reader.ReadU64(), .quarterTurns = _reader.ReadU32()};
  default:
  {
    const EntityId structure = _reader.ReadU64();
    const std::uint8_t open = _reader.ReadU8();
    if (open > 1)
    {
      throw std::invalid_argument("a SetGate whose open is neither 0 nor 1");
    }
    return SetGate{.structure = structure, .open = open == 1};
  }
  }
}

} // namespace

std::uint16_t CommandTag(const Command& _command) noexcept
{
  return static_cast<std::uint16_t>(_command.index() + 1);
}

void Encode(const Command& _command, Core::ByteWriter& _writer)
{
  _writer.WriteU16(CommandTag(_command));
  _writer.WriteU16(COMMAND_RECORD_VERSION);
  WriteFields(_command, _writer);
}

Command DecodeCommand(Core::ByteReader& _reader)
{
  const std::uint16_t tag = _reader.ReadU16();
  const std::uint16_t version = _reader.ReadU16();
  if (tag == 0 || tag > LAST_TAG)
  {
    throw std::invalid_argument("a command with a tag this build does not know");
  }
  if (version != COMMAND_RECORD_VERSION)
  {
    throw std::invalid_argument("a command at a record version this build does not read");
  }
  return ReadFields(tag, _reader);
}

} // namespace Protocol
