#pragma once

#include "HudModel.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Client
{

/// What the crater run shows of an item: its id and its key. The executable fills it from Simulation's crater items,
/// which the client never includes (ADR-020, decision 1).
struct CraterItemSpec
{
  std::string id; // C0 the practice item, then C1 to C8
  bool keyRam;    // the engine that made the damage: the ram, or the trebuchet
};

enum class CraterAnswer : std::uint8_t
{
  Ram,
  Trebuchet
};

/// An answered item, as the crater log's item row has it.
struct CraterItemRow
{
  std::string id;
  bool keyRam;
  CraterAnswer answer;
  double secondsToAnswer;

  [[nodiscard]] bool Correct() const noexcept;
};

/// What the crater log opens with.
struct CraterLogInfo
{
  std::string participant;
  std::uint64_t buildId;
  std::uint64_t serverDataHash;
  std::uint64_t itemsHash;
  std::string startTime;
};

/// The crater items' flow (M5-D2; MvpPlan-M5.md, *The session*, step 9; GameDesign.md, M5's values, item 12), as the
/// executable's loop drives it: the practice item, then C1 to C8 in the participant's order, each set up out of sight
/// and asked with no time limit, and never answered back: no result is shown. The executable builds the worlds and
/// tells the model when each is ready; the model decides what comes next, what the HUD shows and what the log says.
class CraterModel
{
public:
  /// _items is C0 first, then C1 to C8, as Simulation::CraterItems has them. The order is LabModel's for _seed.
  CraterModel(std::vector<CraterItemSpec> _items, std::uint64_t _seed);

  /// The item being set up or asked, or nullptr once finished.
  [[nodiscard]] const CraterItemSpec* Current() const noexcept;
  /// How many items have started: each a fresh world.
  [[nodiscard]] std::size_t Started() const noexcept;
  [[nodiscard]] bool Asking() const noexcept;
  [[nodiscard]] bool Finished() const noexcept;

  /// The item's world is quiet: the question goes up at _nowSeconds.
  void Ready(double _nowSeconds);
  /// The answer, by key or by the banner's buttons, at _nowSeconds. Ignored unless the question is up; otherwise the item
  /// closes, the next starts or the run finishes, and the item's row is given.
  [[nodiscard]] std::optional<CraterItemRow> Answer(CraterAnswer _answer, double _nowSeconds);

  /// SETTING UP while an item is built; the question with RAM · R and TREBUCHET · T; or the end.
  [[nodiscard]] HudMode Mode() const;
  /// PRACTICE, then ITEM n OF 8, then DONE.
  [[nodiscard]] HudObjective Objective() const;

  [[nodiscard]] static std::string LogHeader(const CraterLogInfo& _info);
  [[nodiscard]] static std::string ItemRowText(const CraterItemRow& _row);

private:
  std::vector<CraterItemSpec> m_items;
  std::vector<std::size_t> m_order;
  std::size_t m_started = 1; // the first item starts with the run
  bool m_asking = false;
  double m_askedAtSeconds = 0.0;
};

} // namespace Client
