#pragma once

#include "Commander.h"

namespace Simulation
{

/// The Siege trial's defender (ADR-027; MvpPlan.md, M4, inputs 13 and 18): every half second it aims each garrison at
/// sappers first, then rams, then the trebuchet, which it would otherwise leave for the nearest figure; sends one laborer
/// to each of the most damaged structures not locked, the nearest laborer first; sends archers who are out to the nearest
/// working building with room; sallies its men-at-arms and outriders against an engine it sees within the leash of the
/// curtain, opening its gate for them and shutting it behind them or when an enemy soldier comes near; and sends them to
/// the inner mouth of a section that opens. It reads only its player's update.
class SiegeDefender final : public Commander
{
public:
  static constexpr std::string_view KIND = "siegeDefender";
  static constexpr std::uint8_t RECORD_VERSION = 2; // M4-F3: the sally's state

  /// New, or read from _saved when it is not null.
  explicit SiegeDefender(Core::ByteReader* _saved);

  void Decide(CommanderView& _view, std::vector<Protocol::Command>& _commands) override;
  void Write(Core::ByteWriter& _writer) const override;
  [[nodiscard]] std::string_view Kind() const noexcept override;
  [[nodiscard]] std::unique_ptr<Commander> Clone() const override;

private:
  /// Where the sally force is: at home, out after an engine, coming back, or holding a breach for good.
  enum class Sally : std::uint8_t
  {
    Home,
    Out,
    Returning,
    Breach
  };

  /// The sally and the gate (input 18 and input 13).
  void SallyAndGate(const CommanderView& _view, std::vector<Protocol::Command>& _commands);
  /// The force to a newly opened section's inner mouth, once each.
  void AnswerBreaches(const CommanderView& _view, std::vector<Protocol::Command>& _commands);

  Sally m_sally = Sally::Home;
  Protocol::EntityId m_sallyTarget = Protocol::NO_ENTITY;
  std::map<Protocol::EntityId, Protocol::VoxelPosition> m_homes; // each force squad's lead cell when first seen
  std::set<Protocol::EntityId> m_answered;                       // the sections whose mouth it was sent to

  static void AimGarrisons(const CommanderView& _view, std::vector<Protocol::Command>& _commands);
  static void SendLaborers(const CommanderView& _view, std::vector<Protocol::Command>& _commands);
  static void RegarrisonArchers(const CommanderView& _view, std::vector<Protocol::Command>& _commands);
};

} // namespace Simulation
