#include "pch.h"

#include "Measurement.h"

#include "Percentile.h"

namespace Spike
{

namespace
{

std::string Utf8(std::wstring_view _text)
{
  if (_text.empty())
  {
    return {};
  }
  const int length = WideCharToMultiByte(CP_UTF8, 0, _text.data(), static_cast<int>(_text.size()), nullptr, 0, nullptr, nullptr);
  std::string text(static_cast<std::size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, _text.data(), static_cast<int>(_text.size()), text.data(), length, nullptr, nullptr);
  return text;
}

std::ofstream OpenForWriting(const std::filesystem::path& _file)
{
  std::ofstream stream(_file, std::ios::binary | std::ios::trunc);
  if (!stream)
  {
    throw std::runtime_error("the measurement could not open " + Utf8(_file.wstring()) + " for writing");
  }
  return stream;
}

} // namespace

void Measurement::AddFrame(const FrameRow& _row)
{
  m_frames.push_back(_row);
}

void Measurement::AddEdit(const EditRow& _row)
{
  m_edits.push_back(_row);
}

std::wstring Measurement::Write(const std::filesystem::path& _folder, std::wstring_view _stem, const Setup& _setup) const
{
  // Rows arrive as each frame's timestamps come back, two frames late and in either slot's order.
  std::vector<FrameRow> frames = m_frames;
  std::ranges::sort(frames, {}, &FrameRow::frame);
  std::vector<double> measured;
  for (const FrameRow& row : frames)
  {
    if (row.frame >= _setup.firstMeasuredFrame)
    {
      measured.push_back(row.voxelPassMilliseconds);
    }
  }
  const double p95 = Core::NearestRankPercentile(measured, 95.0);

  std::vector<double> shownRefreshes;
  std::size_t neverShown = 0;
  for (const EditRow& edit : m_edits)
  {
    if (edit.milliseconds < 0.0)
    {
      ++neverShown;
    }
    else if (edit.displayRefreshes >= 0)
    {
      shownRefreshes.push_back(static_cast<double>(edit.displayRefreshes));
    }
  }

  const bool atPassMarkSize = _setup.displayWidthPixels == PASS_MARK_WIDTH_PIXELS &&
                              _setup.displayHeightPixels == PASS_MARK_HEIGHT_PIXELS && !_setup.resizedDuringRun;
  std::wstring results = std::format(L"Voxel pass, 95th percentile: {:.3f} ms over {} frames, against a pass mark of {:.0f} ms.\n", p95,
                                     measured.size(), PASS_MARK_MILLISECONDS);
  if (!atPassMarkSize)
  {
    results +=
      std::format(L"Not a pass-mark run: the display was {} x {}{}, not {} x {}.\n", _setup.displayWidthPixels, _setup.displayHeightPixels,
                  _setup.resizedDuringRun ? L" and changed during the run" : L"", PASS_MARK_WIDTH_PIXELS, PASS_MARK_HEIGHT_PIXELS);
  }
  if (shownRefreshes.empty())
  {
    results += std::format(L"Edits: {}, and DXGI said when none of them was shown.\n", m_edits.size());
  }
  else
  {
    results +=
      std::format(L"Edits: {}, shown after a median of {:.0f} display refreshes and at most {:.0f}; {} never shown.\n", m_edits.size(),
                  Core::NearestRankPercentile(shownRefreshes, 50.0), Core::NearestRankPercentile(shownRefreshes, 100.0), neverShown);
  }

  std::filesystem::create_directories(_folder);
  const std::filesystem::path framesFile = _folder / (std::wstring(_stem) + L"-frames.csv");
  const std::filesystem::path editsFile = _folder / (std::wstring(_stem) + L"-edits.csv");

  std::ofstream framesOut = OpenForWriting(framesFile);
  framesOut << "# Earthrise.World renderer spike, measured run (MvpPlan.md, M1 task 10)\n";
  framesOut << "# adapter: " << Utf8(_setup.adapter) << ", driver " << Utf8(_setup.driverVersion) << "\n";
  framesOut << std::format("# display {} x {}, render {} x {} at a scale of {:.2f}\n", _setup.displayWidthPixels,
                           _setup.displayHeightPixels, _setup.renderWidthPixels, _setup.renderHeightPixels, _setup.renderScale);
  framesOut << "# present: without waiting for the vertical blank, " << (_setup.tearing ? "tearing allowed" : "tearing not offered")
            << "\n";
  framesOut << "# path: " << Utf8(_setup.path) << "\n";
  framesOut << std::format("# frames before {} warm up and are left out of the percentile\n", _setup.firstMeasuredFrame);
  std::wstring_view remaining = results;
  while (!remaining.empty())
  {
    const std::size_t end = remaining.find(L'\n');
    framesOut << "# " << Utf8(remaining.substr(0, end)) << "\n";
    remaining.remove_prefix(end == std::wstring_view::npos ? remaining.size() : end + 1);
  }
  framesOut << "frame,azimuth_degrees,voxels_drawn,voxel_pass_ms,frame_ms\n";
  for (const FrameRow& row : frames)
  {
    framesOut << std::format("{},{:.1f},{},{:.4f},{:.4f}\n", row.frame, row.azimuthDegrees, row.voxelsDrawn, row.voxelPassMilliseconds,
                             row.frameMilliseconds);
  }

  std::ofstream editsOut = OpenForWriting(editsFile);
  editsOut << "# Earthrise.World renderer spike, edit-to-present latency (MvpPlan.md, M1 task 10)\n";
  editsOut << "# -1 means DXGI never reported the refresh, or the edit was never seen shown\n";
  editsOut << "frame,display_refreshes,ms\n";
  for (const EditRow& edit : m_edits)
  {
    editsOut << std::format("{},{},{:.3f}\n", edit.frame, edit.displayRefreshes, edit.milliseconds);
  }

  if (!framesOut.flush() || !editsOut.flush())
  {
    throw std::runtime_error("the measurement could not finish writing its files in " + Utf8(_folder.wstring()));
  }
  return results + std::format(L"Written to {}.", _folder.wstring());
}

} // namespace Spike
