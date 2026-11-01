#include "pch.h"

#include "HudAlerts.h"

namespace Client
{

void HudAlerts::Push(HudAlert _alert)
{
  m_alerts.insert(m_alerts.begin(), std::move(_alert));
  if (m_alerts.size() > SHOWN)
  {
    m_alerts.resize(SHOWN);
  }
}

void HudAlerts::Expire(double _nowSeconds)
{
  std::erase_if(m_alerts, [_nowSeconds](const HudAlert& _alert) { return _alert.expiresSeconds <= _nowSeconds; });
}

const std::vector<HudAlert>& HudAlerts::Shown() const noexcept
{
  return m_alerts;
}

} // namespace Client
