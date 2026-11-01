#include "pch.h"

#include "Script.h"

namespace Simulation
{

void RunScript(World& _world, std::span<const ScriptedCommand> _script, std::uint32_t _ticks)
{
  for (std::uint32_t ran = 0; ran < _ticks; ++ran)
  {
    const std::uint32_t tick = _world.TickCount();
    for (const ScriptedCommand& scripted : _script)
    {
      if (scripted.tick == tick)
      {
        Core::ByteWriter writer;
        Protocol::Encode(scripted.command, writer);
        _world.Submit(scripted.player, writer.Bytes());
      }
    }
    _world.Tick();
  }
}

} // namespace Simulation
