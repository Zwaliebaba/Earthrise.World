#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Earthrise
{

/// The measured run (M4-J4; GameDesign.md, *Starting the MVP*, item 3): `Earthrise --siege --watch --measure` plays the
/// trial live with the scripted attacker for its first ten minutes of game time, the camera flying M1's path about the
/// castle, and keeps each frame's voxel pass and line pass on the GPU and each tick's time on the CPU. It writes them to
/// Measurements\SiegeMeasure-<yyyymmdd-hhmmss>.csv beside the executable, with the 95th percentiles and the slowest tick,
/// against M2's budgets of 8 ms for the voxel pass and 25 ms for a tick.
class SiegeMeasure
{
public:
  static constexpr std::uint32_t TICKS = 10 * 60 * 20; // ten minutes of game time
  static constexpr double VOXEL_PASS_BUDGET_MILLISECONDS = 8.0;
  static constexpr double TICK_BUDGET_MILLISECONDS = 25.0;

  /// What the CSV's comment lines record.
  struct Setup
  {
    std::wstring adapter;
    std::wstring driverVersion;
    std::uint32_t widthPixels;
    std::uint32_t heightPixels;
    std::wstring path; // the camera's, in words
  };

  /// A frame's passes, as the GPU's timestamps last gave them.
  void AddFrame(double _voxelPassMilliseconds, double _linePassMilliseconds);
  /// A tick's time on the CPU.
  void AddTick(double _milliseconds);
  /// Whether the ten minutes have run.
  [[nodiscard]] bool Done() const noexcept;

  /// Writes the CSV into _folder, and gives the results in words, for the message the run ends with. Throws
  /// std::runtime_error when the file cannot be written.
  [[nodiscard]] std::wstring Write(const std::filesystem::path& _folder, const std::wstring& _stem, const Setup& _setup) const;

  /// The value _share of the way up _values, by nearest rank: 0.95 the 95th percentile. 0 for none.
  [[nodiscard]] static double Percentile(std::vector<double> _values, double _share);

private:
  std::vector<double> m_voxelPass;
  std::vector<double> m_linePass;
  std::vector<double> m_ticks;
};

} // namespace Earthrise
