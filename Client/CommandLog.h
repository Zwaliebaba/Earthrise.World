#pragma once

#include "Command.h"
#include "Session.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Client
{

/// A command the player issued, as the trial's log writes it (Hud.md §11; MvpPlan-M4.md, *The trial's log*, the
/// command rows).
struct CommandRecord
{
  std::uint32_t tick; // the tick it was handed to the simulation for
  double seconds;     // on the client's clock
  std::string command;
  std::string source; // "key" or "click"
};

/// Every command the player issued in a run, with where it came from and when (ADR-007, decision 4; M4-I7), for J2's
/// log. A command's source is the one it was submitted under: its hotkey, a card cell's click, or a right click.
class CommandLog
{
public:
  /// Takes the commands the session queued, records each for _tick at _seconds with its source, and gives their bytes,
  /// in the same order, for the simulation.
  [[nodiscard]] std::vector<std::vector<std::byte>> Take(Session& _session, std::uint32_t _tick, double _seconds);

  [[nodiscard]] std::span<const CommandRecord> Records() const noexcept;

  /// A command as the log writes it: its kind, then its fields, such as MoveSquad 7 120 9 64.
  [[nodiscard]] static std::string Describe(const Protocol::Command& _command);

private:
  std::vector<CommandRecord> m_records;
};

} // namespace Client
