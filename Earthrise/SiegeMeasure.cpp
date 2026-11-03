#include "pch.h"

#include "SiegeMeasure.h"

#include <fstream>

namespace Earthrise
{

namespace
{

/// _text in UTF-8, for the CSV.
std::string Utf8(const std::wstring& _text)
{
  if (_text.empty())
  {
    return {};
  }
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, _text.data(), static_cast<int>(_text.size()), nullptr, 0, nullptr, nullptr);
  std::string text(static_cast<std::size_t>(bytes), '\0');
  WideCharToMultiByte(CP_UTF8, 0, _text.data(), static_cast<int>(_text.size()), text.data(), bytes, nullptr, nullptr);
  return text;
}

} // namespace

void SiegeMeasure::AddFrame(double _voxelPassMilliseconds, double _linePassMilliseconds)
{
  m_voxelPass.push_back(_voxelPassMilliseconds);
  m_linePass.push_back(_linePassMilliseconds);
}

void SiegeMeasure::AddTick(double _milliseconds)
{
  m_ticks.push_back(_milliseconds);
}

bool SiegeMeasure::Done() const noexcept
{
  return m_ticks.size() >= TICKS;
}

double SiegeMeasure::Percentile(std::vector<double> _values, double _share)
{
  if (_values.empty())
  {
    return 0.0;
  }
  std::ranges::sort(_values);
  const auto rank = static_cast<std::size_t>(std::ceil(std::clamp(_share, 0.0, 1.0) * static_cast<double>(_values.size())));
  return _values.at(std::clamp<std::size_t>(rank, 1, _values.size()) - 1);
}

std::wstring SiegeMeasure::Write(const std::filesystem::path& _folder, const std::wstring& _stem, const Setup& _setup) const
{
  constexpr double SHARE = 0.95;
  const double voxelPass = Percentile(m_voxelPass, SHARE);
  const double linePass = Percentile(m_linePass, SHARE);
  const double tick = Percentile(m_ticks, SHARE);
  const auto slowest = std::ranges::max_element(m_ticks);
  const double slowestMilliseconds = slowest == m_ticks.end() ? 0.0 : *slowest;
  const auto slowestTick = static_cast<std::size_t>(slowest - m_ticks.begin());

  std::filesystem::create_directories(_folder);
  const std::filesystem::path path = _folder / (_stem + L".csv");
  std::ofstream file(path, std::ios::binary);
  if (!file)
  {
    throw std::runtime_error("the measured run could not write " + path.string());
  }
  file << "# adapter: " << Utf8(_setup.adapter) << '\n'
       << "# driver: " << Utf8(_setup.driverVersion) << '\n'
       << "# display: " << _setup.widthPixels << " x " << _setup.heightPixels << '\n'
       << "# path: " << Utf8(_setup.path) << '\n'
       << "# ticks: " << m_ticks.size() << '\n'
       << std::format("# voxel pass p95 ms: {:.3f} (budget {:.0f})\n", voxelPass, VOXEL_PASS_BUDGET_MILLISECONDS)
       << std::format("# line pass p95 ms: {:.3f}\n", linePass) << std::format("# tick p95 ms: {:.3f}\n", tick)
       << std::format("# slowest tick ms: {:.3f} at tick {} (budget {:.0f})\n", slowestMilliseconds, slowestTick, TICK_BUDGET_MILLISECONDS)
       << "kind,index,voxelPassMs,linePassMs,tickMs\n";
  for (std::size_t frame = 0; frame < m_voxelPass.size(); ++frame)
  {
    file << std::format("frame,{},{:.4f},{:.4f},\n", frame, m_voxelPass.at(frame), m_linePass.at(frame));
  }
  for (std::size_t index = 0; index < m_ticks.size(); ++index)
  {
    file << std::format("tick,{},,,{:.4f}\n", index, m_ticks.at(index));
  }
  return std::format(L"Wrote {}.\n\nVoxel pass, 95th percentile: {:.2f} ms against {:.0f} ms.\nLine pass, 95th percentile: {:.2f} ms.\n"
                     L"Tick, 95th percentile: {:.2f} ms; slowest {:.2f} ms at tick {}, against {:.0f} ms.",
                     path.wstring(), voxelPass, VOXEL_PASS_BUDGET_MILLISECONDS, linePass, tick, slowestMilliseconds, slowestTick,
                     TICK_BUDGET_MILLISECONDS);
}

} // namespace Earthrise
