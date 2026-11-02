#pragma once

#include "ByteReader.h"
#include "ByteWriter.h"
#include "Command.h"
#include "Query.h"
#include "StateUpdate.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace Simulation
{

class ServerData;
class World;

/// What a commander may read (ADR-027, decision 3): its player's state update from step 11, and the readings its player
/// could ask for, called directly within its budget of work units (decision 4). It never reads past its player's filter.
class CommanderView
{
public:
  CommanderView(const World& _world, std::uint8_t _player, std::int32_t _workUnits) noexcept;

  /// Its player's state update, as step 11 published it.
  [[nodiscard]] const Protocol::StateUpdate& State() const;
  /// A section's integrity, costing what the query costs; empty past the budget or for a structure its player does not see.
  [[nodiscard]] std::optional<Protocol::IntegrityReading> Integrity(Protocol::EntityId _structure);
  /// Whether _class can walk from _fromVoxels to _toVoxels, costing a unit; false past the budget.
  [[nodiscard]] bool Reachable(Protocol::NavigationClass _class, Protocol::VoxelPosition _fromVoxels, Protocol::VoxelPosition _toVoxels);
  [[nodiscard]] std::int32_t WorkLeft() const noexcept;
  /// A draw keyed on the match's seed, the player, the tick and _salt (ADR-013): the same on every replay.
  [[nodiscard]] std::uint64_t Draw(std::uint64_t _salt) const noexcept;
  /// The rules and statistics, which every player knows.
  [[nodiscard]] const ServerData& Data() const noexcept;
  [[nodiscard]] std::uint8_t Player() const noexcept;
  /// The tick that has just run.
  [[nodiscard]] std::uint32_t Tick() const noexcept;

private:
  const World* m_world;
  std::uint8_t m_player;
  std::int32_t m_workLeft;
};

/// A source of commands for one player (ADR-027, decision 1): it decides at the end of a tick, on its cadence, and its
/// commands reach the next tick's step 1 as a player's do. Its state is saved and hashed.
class Commander
{
public:
  Commander() = default;
  Commander(const Commander&) = default;
  Commander& operator=(const Commander&) = default;
  Commander(Commander&&) = default;
  Commander& operator=(Commander&&) = default;
  virtual ~Commander() = default;

  /// Step 13, on its cadence: reads _view and adds the commands it gives to _commands.
  virtual void Decide(CommanderView& _view, std::vector<Protocol::Command>& _commands) = 0;
  /// Its state, for the save and the hash.
  virtual void Write(Core::ByteWriter& _writer) const = 0;
  /// The scenario's name for its kind, which MakeCommander takes.
  [[nodiscard]] virtual std::string_view Kind() const noexcept = 0;
  /// A copy, for a world that is copied.
  [[nodiscard]] virtual std::unique_ptr<Commander> Clone() const = 0;
};

/// A commander of _kind, the scenario's name for it, new or read from _saved. Throws std::invalid_argument for a kind it
/// does not know, and returns none for "human", whose seat has no commander.
[[nodiscard]] std::unique_ptr<Commander> MakeCommander(std::string_view _kind, Core::ByteReader* _saved);

/// A commander held by value, so that a world copies with it (ADR-027).
class CommanderSlot
{
public:
  explicit CommanderSlot(std::unique_ptr<Commander> _commander) noexcept;
  CommanderSlot(const CommanderSlot& _other);
  CommanderSlot& operator=(const CommanderSlot& _other);
  CommanderSlot(CommanderSlot&&) noexcept = default;
  CommanderSlot& operator=(CommanderSlot&&) noexcept = default;
  ~CommanderSlot() = default;

  [[nodiscard]] Commander& Get() const noexcept;

private:
  std::unique_ptr<Commander> m_commander;
};

} // namespace Simulation
