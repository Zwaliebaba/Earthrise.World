#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Spike
{

/// The spike's measured run (MvpPlan.md, M1 task 10; ADR-011, decision 10): a row for each frame of
/// the camera path and one for each scripted edit, written as two CSV files and reduced to the 95th
/// percentile of the voxel pass's GPU time, its shadow passes included, which is how the pass mark
/// is read (GameDesign.md, *Starting the MVP*).
class Measurement
{
public:
  /// The pass mark: the voxel pass inside 8 ms at the default zoom, at 1920 x 1080 (GameDesign.md).
  /// The scene is drawn at that size on any display, and scaled to it (the owner's choice of
  /// 2026-11-01).
  static constexpr double PASS_MARK_MILLISECONDS = 8.0;
  static constexpr std::uint32_t PASS_MARK_WIDTH_PIXELS = 1920;
  static constexpr std::uint32_t PASS_MARK_HEIGHT_PIXELS = 1080;

  struct FrameRow
  {
    std::uint32_t frame; // along the camera path
    float azimuthDegrees;
    std::uint64_t voxelsDrawn;
    double voxelPassMilliseconds;  // GPU, from timestamps around the voxel pass
    double shadowPassMilliseconds; // GPU, the two shadow passes, which the pass mark counts with it
    double frameMilliseconds;      // CPU, from the previous frame's start to this one's
    double rebuildMilliseconds;    // CPU, the scripted edits and the chunk records rebuilt for them
  };

  struct EditRow
  {
    std::uint32_t frame;           // the frame whose publication carried the edit
    std::int64_t displayRefreshes; // from the vertical blank before it to the one that showed it; -1 if DXGI never said
    double milliseconds;           // from the publication until the spike saw it shown, to within a frame; -1 if never shown
  };

  struct Setup
  {
    std::wstring adapter;
    std::wstring driverVersion;
    std::uint32_t displayWidthPixels;
    std::uint32_t displayHeightPixels;
    std::uint32_t renderWidthPixels;
    std::uint32_t renderHeightPixels;
    float renderScale;
    float worldVoxelMeters;           // 1, or 0.5 for ADR-006's comparison
    bool tearing;                     // DXGI let the run present without waiting for the vertical blank
    bool resizedDuringRun;            // the window's size changed while the path ran, which stalls a frame
    std::wstring path;                // the camera path, in words
    std::wstring edits;               // the scripted edits, in words
    std::uint32_t firstMeasuredFrame; // the frames before it warm up and are left out
  };

  void AddFrame(const FrameRow& _row);
  void AddEdit(const EditRow& _row);

  /// Writes `<_stem>-frames.csv` and `<_stem>-edits.csv` in _folder, creating it, with the setup and
  /// the results in comment lines at the top of the frames file, and returns those results as text.
  /// Throws std::runtime_error when a file cannot be written, and std::invalid_argument when no
  /// frame was measured.
  std::wstring Write(const std::filesystem::path& _folder, std::wstring_view _stem, const Setup& _setup) const;

private:
  std::vector<FrameRow> m_frames;
  std::vector<EditRow> m_edits;
};

} // namespace Spike
