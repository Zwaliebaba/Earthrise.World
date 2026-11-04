#pragma once

#include "ChunkCoordinate.h"
#include "HudModel.h"
#include "SiegeLog.h"

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>

namespace Client
{

/// What a reason prompt asked about: the work that first struck a structure, and when (M5-C3).
struct ReasonAsk
{
  std::uint32_t tick;
  SiegeWork work;
  std::string structure;
  Protocol::VoxelPosition voxel; // the voxel struck or dug, which the prompt marks
  double raisedSeconds;          // on the client's clock
};

/// A prompt answered: what it asked, and the seconds it was up, for the trial's asked row.
struct ReasonAnswered
{
  ReasonAsk ask;
  double secondsUp;
};

/// The Siege playtest's reason prompt (M5-C3; MvpPlan-M5.md, *The session*, step 4; GameDesign.md, M5's values, item 8).
/// The first time in a game that the player's stones strike a structure, that the ram strikes one, and that their sappers
/// dig beside one, the clock stops after that tick and the banner asks WHY <name>? SAY IT ALOUD · SPACE GOES ON, with
/// the voxel marked, so that the facilitator writes the reason down at the moment of choice. Space takes it down. At
/// most three a game, one of each kind; two raised in one tick wait one behind the other. It asks no prediction, and it
/// prompts alike in both versions, since siege work is raised alike in both (M5-B1). A prompt that is not enabled, as
/// under a bare --siege, --watch or --replay, never raises one.
class ReasonPrompt
{
public:
  explicit ReasonPrompt(bool _enabled) noexcept;

  /// _work on _structure at _tick: a prompt, the first time this kind of work is raised in the game.
  void Raise(std::uint32_t _tick, SiegeWork _work, std::string_view _structure, Protocol::VoxelPosition _voxel, double _seconds);

  /// Whether a prompt is up, which stops the clock.
  [[nodiscard]] bool Up() const noexcept;
  /// The prompt's banner while one is up.
  [[nodiscard]] std::optional<HudMode> Banner() const;
  /// The voxel the prompt up marks.
  [[nodiscard]] std::optional<Protocol::VoxelPosition> Mark() const noexcept;
  /// Space at _seconds: the prompt up goes, and what it asked, for its row. Nothing when none is up.
  [[nodiscard]] std::optional<ReasonAnswered> GoOn(double _seconds);

private:
  static constexpr std::size_t WORKS = 3;

  bool m_enabled;
  std::array<bool, WORKS> m_raised{};
  std::deque<ReasonAsk> m_waiting; // the one up first
};

} // namespace Client
