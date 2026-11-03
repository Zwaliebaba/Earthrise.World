#include "pch.h"

#include "CommandLog.h"

#include "ByteReader.h"

namespace Client
{

namespace
{

/// Each command's name, in the variant's order.
constexpr std::array<std::string_view, std::variant_size_v<Protocol::Command>> NAMES = {
  "MoveSquad",     "FireEngine",    "AttackTarget",    "StormTo",   "StopSquad",  "HoldSquad",    "SplitSquad", "MergeSquads",
  "GarrisonSquad", "EjectGarrison", "RepairStructure", "DigTrench", "PackEngine", "DeployEngine", "SetGate"};

std::string At(const Protocol::VoxelPosition& _voxels)
{
  return std::format(" {} {} {}", _voxels.xVoxels, _voxels.yVoxels, _voxels.zVoxels);
}

// Each command's fields, after its name.
std::string Fields(const Protocol::MoveSquad& _command)
{
  return std::format(" {}", _command.squad) + At(_command.goalVoxels);
}
std::string Fields(const Protocol::FireEngine& _command)
{
  return std::format(" {}", _command.engine) + At(_command.targetVoxels);
}
std::string Fields(const Protocol::AttackTarget& _command)
{
  return std::format(" {} {}", _command.squad, _command.target);
}
std::string Fields(const Protocol::StormTo& _command)
{
  return std::format(" {}", _command.squad) + At(_command.goalVoxels);
}
std::string Fields(const Protocol::StopSquad& _command)
{
  return std::format(" {}", _command.squad);
}
std::string Fields(const Protocol::HoldSquad& _command)
{
  return std::format(" {}", _command.squad);
}
std::string Fields(const Protocol::SplitSquad& _command)
{
  return std::format(" {}", _command.squad);
}
std::string Fields(const Protocol::MergeSquads& _command)
{
  std::string fields;
  for (const Protocol::EntityId squad : _command.squads)
  {
    fields += std::format(" {}", squad);
  }
  return fields;
}
std::string Fields(const Protocol::GarrisonSquad& _command)
{
  return std::format(" {} {}", _command.squad, _command.structure);
}
std::string Fields(const Protocol::EjectGarrison& _command)
{
  return std::format(" {}", _command.structure);
}
std::string Fields(const Protocol::RepairStructure& _command)
{
  return std::format(" {} {}", _command.laborer, _command.structure);
}
std::string Fields(const Protocol::DigTrench& _command)
{
  return std::format(" {}", _command.squad) + At(_command.minVoxels) + At(_command.maxVoxels);
}
std::string Fields(const Protocol::PackEngine& _command)
{
  return std::format(" {}", _command.engine);
}
std::string Fields(const Protocol::DeployEngine& _command)
{
  return std::format(" {} {}", _command.engine, _command.quarterTurns);
}
std::string Fields(const Protocol::SetGate& _command)
{
  return std::format(" {} {}", _command.structure, _command.open ? 1 : 0);
}

} // namespace

std::vector<std::vector<std::byte>> CommandLog::Take(Session& _session, std::uint32_t _tick, double _seconds)
{
  std::vector<std::vector<std::byte>> commands = _session.TakeCommands();
  const std::vector<std::string>& sources = _session.TakenSources();
  for (std::size_t index = 0; index < commands.size(); ++index)
  {
    Core::ByteReader reader(commands.at(index));
    m_records.push_back({.tick = _tick,
                         .seconds = _seconds,
                         .command = Describe(Protocol::DecodeCommand(reader)),
                         .source = index < sources.size() ? sources.at(index) : std::string()});
  }
  return commands;
}

std::span<const CommandRecord> CommandLog::Records() const noexcept
{
  return m_records;
}

std::string CommandLog::Describe(const Protocol::Command& _command)
{
  return std::string(NAMES.at(_command.index())) + std::visit([](const auto& _alternative) { return Fields(_alternative); }, _command);
}

} // namespace Client
