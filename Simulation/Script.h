#pragma once

#include "Command.h"
#include "World.h"

#include <cstdint>
#include <span>

namespace Simulation
{

/// A command a script submits before tick _tick, which therefore takes effect in that tick's step 1.
struct ScriptedCommand
{
  std::uint32_t tick;
  std::uint8_t player;
  Protocol::Command command;
};

/// Runs _ticks ticks of _world from where it stands, submitting before each tick, as bytes, the
/// commands _script gives it, in the script's order. The breach run, the tests and the bench drive
/// the world with it.
void RunScript(World& _world, std::span<const ScriptedCommand> _script, std::uint32_t _ticks);

} // namespace Simulation
