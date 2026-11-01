#pragma once

#include "ChunkSnapshot.h"
#include "Command.h"
#include "Query.h"
#include "StateUpdate.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace Client
{

/// The client's copy of what the simulation told it, and what it asks (ADR-020, decision 2). It holds every state
/// update, chunk and answer by copy, and draws from nothing else; it hands the executable its commands and queries as
/// bytes.
class Session
{
public:
  /// Takes a tick's state update, the chunks published and the answers, by copy.
  void Receive(const Protocol::StateUpdate& _state, std::span<const Protocol::ChunkSnapshot> _snapshots,
               std::span<const std::vector<std::byte>> _answers);

  [[nodiscard]] const Protocol::StateUpdate& State() const noexcept;
  /// The update before the last, for the shot's interpolation (GameDesign.md, M3's values, item 17); empty before the
  /// second.
  [[nodiscard]] const Protocol::StateUpdate& PreviousState() const noexcept;

  /// The chunks received since the last call, oldest first, for the world view to take.
  [[nodiscard]] std::vector<Protocol::ChunkSnapshot> TakeSnapshots();

  /// The commands and queries the client has queued since the last call, as bytes, for the executable to hand the
  /// simulation.
  [[nodiscard]] std::vector<std::vector<std::byte>> TakeCommands();
  [[nodiscard]] std::vector<std::vector<std::byte>> TakeQueries();

  /// Queues one. _source is "key" or "click", for the lab's log (Hud.md §11).
  void Submit(const Protocol::Command& _command, std::string_view _source);
  /// Queues one, under an id of its own. A query is keyed by what it asks about, so that the newest answer replaces the
  /// last (ADR-009, decision 3).
  void Ask(const Protocol::Query& _query);

  /// The newest answer about each, or nullptr when none has come.
  [[nodiscard]] const Protocol::AimReading* Aim(Protocol::EntityId _engine, Protocol::VoxelPosition _target) const noexcept;
  [[nodiscard]] const Protocol::IntegrityReading* Integrity(Protocol::EntityId _structure) const noexcept;
  [[nodiscard]] const Protocol::TileReachReading* Reach(std::int32_t _tileX, std::int32_t _tileZ) const noexcept;

  /// The sources of the commands TakeCommands last gave out, in the same order.
  [[nodiscard]] const std::vector<std::string>& TakenSources() const noexcept;

private:
  using AimKey = std::tuple<Protocol::EntityId, std::int32_t, std::int32_t, std::int32_t>;

  Protocol::StateUpdate m_state{};
  Protocol::StateUpdate m_previous{};
  std::vector<Protocol::ChunkSnapshot> m_snapshots;
  std::vector<std::vector<std::byte>> m_commands;
  std::vector<std::string> m_sources;
  std::vector<std::string> m_takenSources;
  std::vector<std::vector<std::byte>> m_queries;
  std::uint32_t m_nextQueryId = 1;
  std::map<std::uint32_t, Protocol::Query> m_asked; // until answered
  std::map<AimKey, Protocol::AimReading> m_aims;
  std::map<Protocol::EntityId, Protocol::IntegrityReading> m_integrity;
  std::map<std::pair<std::int32_t, std::int32_t>, Protocol::TileReachReading> m_reach;
};

} // namespace Client
