#pragma once

#include "Commander.h"

#include <limits>
#include <map>
#include <optional>

namespace Simulation
{

/// The Siege trial's scripted attacker (ADR-027, decision 1; MvpPlan-M4.md, M4-F5), for the tests, the bench and the
/// owner's checks, never offered as a way to play. Its plan is written for the Siege castle, on the flank of the south
/// curtain's middle tower, ST, from its gatehouse, east on Siege and west on its mirror, so that it mirrors itself:
/// - its trebuchets go round the flank, clear of the south towers' reach, to a stand outside every garrison's reach and
///   stop the north tower on the flank, by its template's fixed pattern (FirePattern.h);
/// - they then go to a stand that only that tower covers, off the keep's flank face, where they stop the keep and open the
///   flank curtain's section next to that tower, stopping that tower again first whenever it works again: one tower to
///   keep down, which one trebuchet can do against repair, where two it cannot (measured, M4-F5);
/// - once the keep has stopped and the section is open, its men-at-arms storm through the breach to the keep's far face,
///   which only the keep's and the two north towers' garrisons cover, and hold it, falling on any enemy figure that comes
///   near;
/// - throughout, the squads not storming stand behind the trebuchets and fall on any enemy that comes near them.
/// It reads only its player's update and asks nothing within its budget.
class SiegeAttacker final : public Commander
{
public:
  static constexpr std::string_view KIND = "siegeAttacker";
  static constexpr std::uint8_t RECORD_VERSION = 1;

  /// New, or read from _saved when it is not null.
  explicit SiegeAttacker(Core::ByteReader* _saved);

  void Decide(CommanderView& _view, std::vector<Protocol::Command>& _commands) override;
  void Write(Core::ByteWriter& _writer) const override;
  [[nodiscard]] std::string_view Kind() const noexcept override;
  [[nodiscard]] std::unique_ptr<Commander> Clone() const override;

private:
  /// Where its plan stands.
  enum class Phase : std::uint8_t
  {
    ToWaypoint,
    ToFirstStand,
    FirstStand,
    ToSecondStand,
    SecondStand,
    Storm
  };

  /// The castle as the plan reads it from the update.
  struct Castle;

  static constexpr std::uint32_t NOT_STARTED = std::numeric_limits<std::uint32_t>::max();

  [[nodiscard]] static std::optional<Castle> ReadCastle(const CommanderView& _view);
  /// The _shot-th aim at _target from _stand.
  [[nodiscard]] static Protocol::VoxelPosition Aim(const CommanderView& _view, const Castle& _castle,
                                                   const Protocol::StructureView& _target, Protocol::VoxelPosition _stand,
                                                   std::uint32_t _shot);
  /// The structure the trebuchets fire at this decision, or none once the stand's plan is done.
  [[nodiscard]] const Protocol::StructureView* Target(const CommanderView& _view, const Castle& _castle);

  Phase m_phase = Phase::ToFirstStand;
  Protocol::EntityId m_target = Protocol::NO_ENTITY;          // the structure it fires at until it is done with it
  std::uint32_t m_overshoot = 0;                              // the shots flown at it since it stopped
  std::uint32_t m_sweep = 0;                                  // the corner of the keep's ring the sweepers make for, counted round
  bool m_stormIn = false;                                     // the storm has gathered and gone in
  std::uint32_t m_gatherStartTick = NOT_STARTED;              // when the storm began to gather
  std::map<Protocol::EntityId, std::uint32_t> m_shots;        // the pattern's shots flown at each structure
  std::map<Protocol::EntityId, Protocol::EntityId> m_pending; // each engine's last ordered shot's target, until seen to fly
};

} // namespace Simulation
