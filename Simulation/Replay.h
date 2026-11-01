#pragma once

#include "World.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Simulation
{

// A replay is a starting point and the commands (ADR-014, decision 5). Little-endian, written with
// Core's byte writer:
//   the four bytes ERRP; u32 REPLAY_FORMAT_VERSION; u32 the start's length, then the start, a save;
//   then each tick from the start's on, until the bytes end: u32 the commands it applied, each as its
//   u8 player, u32 length and bytes, in arrival order; then u8 1 and the u64 state hash after the
//   tick when the tick count is a multiple of REPLAY_HASH_TICKS, or u8 0.

inline constexpr std::uint32_t REPLAY_FORMAT_VERSION = 1;
/// A state hash every game second, so a replay that goes wrong shows where within a second.
inline constexpr std::uint32_t REPLAY_HASH_TICKS = 20;

/// Records a run: its start, then each tick's commands and, every REPLAY_HASH_TICKS, its state hash.
class ReplayRecorder
{
public:
  /// Starts a replay from _world as it stands, between ticks.
  explicit ReplayRecorder(const World& _world);

  /// Records the tick _world has just run: its LastTickCommands, and its StateHash when its tick
  /// count is a multiple of REPLAY_HASH_TICKS.
  void Record(const World& _world);

  [[nodiscard]] std::span<const std::byte> Bytes() const noexcept;

private:
  std::vector<std::byte> m_bytes;
};

/// Whether a replay ran as it was recorded, and where it first did not.
struct ReplayResult
{
  bool matched;
  /// The first tick count at which a recorded hash differed, or at which a tick refused its
  /// commands; none when it matched.
  std::optional<std::uint32_t> firstDifferingTick;
  std::uint32_t ticksReplayed;
};

/// Loads the replay's start against _desc and runs its ticks, comparing every recorded hash. Throws
/// std::runtime_error, naming the check, when the start is refused (LoadSave), as a replay from
/// another build or other server data is, or when the replay does not read.
[[nodiscard]] ReplayResult Replay(std::span<const std::byte> _replay, const World::Desc& _desc);

} // namespace Simulation
