#include "pch.h"

#include "Replay.h"

#include "ByteReader.h"
#include "ByteWriter.h"
#include "Save.h"

namespace Simulation
{

namespace
{

constexpr std::array<std::byte, 4> REPLAY_MAGIC = {std::byte{'E'}, std::byte{'R'}, std::byte{'R'}, std::byte{'P'}};

[[noreturn]] void Refuse(const std::string& _check)
{
  throw std::runtime_error("a replay refused: " + _check);
}

} // namespace

ReplayRecorder::ReplayRecorder(const World& _world)
{
  const std::vector<std::byte> start = WriteSave(_world);
  Core::ByteWriter writer;
  writer.WriteBytes(REPLAY_MAGIC);
  writer.WriteU32(REPLAY_FORMAT_VERSION);
  writer.WriteU32(static_cast<std::uint32_t>(start.size()));
  writer.WriteBytes(start);
  m_bytes = writer.TakeBytes();
}

void ReplayRecorder::Record(const World& _world)
{
  Core::ByteWriter writer;
  const std::span<const SubmittedCommand> commands = _world.LastTickCommands();
  writer.WriteU32(static_cast<std::uint32_t>(commands.size()));
  for (const SubmittedCommand& command : commands)
  {
    writer.WriteU8(command.player);
    writer.WriteU32(static_cast<std::uint32_t>(command.bytes.size()));
    writer.WriteBytes(command.bytes);
  }
  if (_world.TickCount() % REPLAY_HASH_TICKS == 0)
  {
    writer.WriteU8(1);
    writer.WriteU64(StateHash(_world));
  }
  else
  {
    writer.WriteU8(0);
  }
  const std::span<const std::byte> tick = writer.Bytes();
  m_bytes.insert(m_bytes.end(), tick.begin(), tick.end());
}

std::span<const std::byte> ReplayRecorder::Bytes() const noexcept
{
  return m_bytes;
}

ReplayResult Replay(std::span<const std::byte> _replay, const World::Desc& _desc)
{
  Core::ByteReader reader(_replay);
  std::span<const std::byte> start;
  try
  {
    if (!std::ranges::equal(reader.ReadBytes(REPLAY_MAGIC.size()), REPLAY_MAGIC))
    {
      Refuse("it does not begin with the magic number ERRP");
    }
    const std::uint32_t version = reader.ReadU32();
    if (version != REPLAY_FORMAT_VERSION)
    {
      Refuse("its format version is " + std::to_string(version) + ", and this build reads " + std::to_string(REPLAY_FORMAT_VERSION));
    }
    start = reader.ReadBytes(reader.ReadU32());
  }
  catch (const std::out_of_range&)
  {
    Refuse("it ends before its start does");
  }
  World world = LoadSave(start, _desc);

  ReplayResult result{.matched = true, .firstDifferingTick = std::nullopt, .ticksReplayed = 0};
  try
  {
    while (!reader.AtEnd())
    {
      const std::uint32_t commands = reader.ReadU32();
      for (std::uint32_t number = 0; number < commands; ++number)
      {
        const std::uint8_t player = reader.ReadU8();
        world.Submit(player, reader.ReadBytes(reader.ReadU32()));
      }
      const bool hashed = reader.ReadU8() != 0;
      const std::uint64_t expected = hashed ? reader.ReadU64() : 0;
      try
      {
        world.Tick();
      }
      catch (const std::invalid_argument&)
      {
        // A command the recorded run applied and this one cannot: the replay has gone wrong here.
        return {.matched = false, .firstDifferingTick = world.TickCount() + 1, .ticksReplayed = result.ticksReplayed};
      }
      catch (const std::out_of_range&)
      {
        return {.matched = false, .firstDifferingTick = world.TickCount() + 1, .ticksReplayed = result.ticksReplayed};
      }
      ++result.ticksReplayed;
      if (hashed && expected != StateHash(world))
      {
        return {.matched = false, .firstDifferingTick = world.TickCount(), .ticksReplayed = result.ticksReplayed};
      }
    }
  }
  catch (const std::out_of_range&)
  {
    Refuse("its last tick is cut short");
  }
  return result;
}

} // namespace Simulation
