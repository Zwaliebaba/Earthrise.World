#pragma once

#include "Commander.h"

namespace Simulation
{

/// The Siege trial's defender (ADR-027; MvpPlan.md, M4, input 18): every half second it aims each garrison at sappers
/// first, then rams, then the trebuchet, which it would otherwise leave for the nearest figure; sends one laborer to each
/// of the most damaged structures not locked, the nearest laborer first; and sends archers who are out to the nearest
/// working building with room. It reads only its player's update.
class SiegeDefender final : public Commander
{
public:
  static constexpr std::string_view KIND = "siegeDefender";
  static constexpr std::uint8_t RECORD_VERSION = 1;

  /// New, or read from _saved when it is not null.
  explicit SiegeDefender(Core::ByteReader* _saved);

  void Decide(CommanderView& _view, std::vector<Protocol::Command>& _commands) override;
  void Write(Core::ByteWriter& _writer) const override;
  [[nodiscard]] std::string_view Kind() const noexcept override;
  [[nodiscard]] std::unique_ptr<Commander> Clone() const override;

private:
  static void AimGarrisons(const CommanderView& _view, std::vector<Protocol::Command>& _commands);
  static void SendLaborers(const CommanderView& _view, std::vector<Protocol::Command>& _commands);
  static void RegarrisonArchers(const CommanderView& _view, std::vector<Protocol::Command>& _commands);
};

} // namespace Simulation
