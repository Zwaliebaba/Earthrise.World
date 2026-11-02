#include "pch.h"

#include "HudAlerts.h"

namespace Client
{

void HudAlerts::Push(HudAlert _alert)
{
  PushEntry({.key = std::nullopt, .alert = std::move(_alert)});
}

void HudAlerts::Raise(std::uint64_t _key, HudAlert _alert)
{
  const auto found = std::ranges::find(m_entries, std::optional<std::uint64_t>(_key), &Entry::key);
  if (found != m_entries.end())
  {
    found->alert = std::move(_alert);
    return;
  }
  PushEntry({.key = _key, .alert = std::move(_alert)});
}

void HudAlerts::PushEntry(Entry _entry)
{
  m_entries.insert(m_entries.begin(), std::move(_entry));
  if (m_entries.size() > SHOWN)
  {
    m_entries.resize(SHOWN);
  }
}

void HudAlerts::Expire(double _nowSeconds)
{
  std::erase_if(m_entries, [_nowSeconds](const Entry& _entry) { return _entry.alert.expiresSeconds <= _nowSeconds; });
}

std::vector<HudAlert> HudAlerts::Shown() const
{
  std::vector<HudAlert> shown;
  shown.reserve(m_entries.size());
  for (const Entry& entry : m_entries)
  {
    shown.push_back(entry.alert);
  }
  return shown;
}

} // namespace Client
