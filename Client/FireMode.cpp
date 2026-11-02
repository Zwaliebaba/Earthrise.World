#include "pch.h"

#include "FireMode.h"

#include "Look.h"
#include "Strings.h"

namespace Client
{

namespace
{

constexpr std::string_view CLICK_SOURCE = "click";
constexpr std::string_view FIRE_COMMAND = "FIRE";

} // namespace

bool FireMode::Update(const Input& _input, const Protocol::EngineView* _engine, Session& _session)
{
  if (_engine == nullptr || _engine->id != m_engine)
  {
    // A new selection, or none: the mode is the old engine's.
    m_active = false;
    m_asked.reset();
  }
  if (_engine == nullptr)
  {
    m_engine = Protocol::NO_ENTITY;
    m_pointed.reset();
    return false;
  }
  m_engine = _engine->id;
  bool usedEscape = false;
  if (m_active && _input.escapePressed)
  {
    m_active = false;
    usedEscape = true;
  }
  else if (!m_active && _input.fPressed && Ready(*_engine))
  {
    m_active = true;
  }

  // The aim check, asked again when the pointer moves to another voxel or the world changes.
  m_pointed = m_active ? _input.pointedVoxels : std::nullopt;
  if (!m_pointed.has_value())
  {
    m_asked.reset();
  }
  else if (m_pointed != m_asked || _input.worldChanged)
  {
    _session.Ask(Protocol::AimCheck{.engine = m_engine, .targetVoxels = *m_pointed});
    m_asked = m_pointed;
  }

  // A right click fires at the voxel under the pointer, in the mode or with none on.
  if (_input.rightClicked && _input.pointedVoxels.has_value() && Ready(*_engine))
  {
    _session.Submit(Protocol::FireEngine{.engine = m_engine, .targetVoxels = *_input.pointedVoxels}, CLICK_SOURCE);
    if (!_input.shiftHeld)
    {
      m_active = false;
      m_pointed.reset();
      m_asked.reset();
    }
  }
  return usedEscape;
}

bool FireMode::Active() const noexcept
{
  return m_active;
}

void FireMode::Cancel() noexcept
{
  m_active = false;
  m_pointed.reset();
  m_asked.reset();
}

HudMode FireMode::Mode() const
{
  if (!m_active)
  {
    return {};
  }
  return {.command = std::string(FIRE_COMMAND),
          .banner = std::string(FIRE_BANNER),
          .buttons = {{.control = HudControl::Cancel, .label = std::string(CANCEL_ESC)}}};
}

bool FireMode::Ready(const Protocol::EngineView& _engine) noexcept
{
  return _engine.reloadTicks == 0;
}

const Protocol::AimReading* FireMode::Reading(const Session& _session) const noexcept
{
  if (!m_active || !m_pointed.has_value())
  {
    return nullptr;
  }
  return _session.Aim(m_engine, *m_pointed);
}

std::optional<AttackCursor> FireMode::Cursor(const Session& _session) const
{
  if (!m_active || !m_pointed.has_value())
  {
    return std::nullopt;
  }
  const Protocol::AimReading* reading = Reading(_session);
  if (reading == nullptr)
  {
    return AttackCursor{.label = std::string(AIM_WAITING), .srgb = DIM_SRGB};
  }
  const std::int32_t meters = reading->distanceMeters;
  switch (reading->verdict)
  {
  case Protocol::AimVerdict::InRange:
    return AttackCursor{.label = std::format(AIM_IN_RANGE, meters), .srgb = GOOD_SRGB};
  case Protocol::AimVerdict::Blocked:
    return AttackCursor{.label = std::format(AIM_BLOCKED, meters), .srgb = BAD_SRGB};
  case Protocol::AimVerdict::TooClose:
    return AttackCursor{.label = std::format(AIM_TOO_CLOSE, meters), .srgb = BAD_SRGB};
  case Protocol::AimVerdict::OutOfRange:
    return AttackCursor{.label = std::format(AIM_OUT_OF_RANGE, meters), .srgb = BAD_SRGB};
  }
  return std::nullopt;
}

std::vector<OverlayBox> FireMode::StrikeMark(const Session& _session) const
{
  const Protocol::AimReading* reading = Reading(_session);
  if (reading == nullptr || !reading->strikes)
  {
    return {};
  }
  if (reading->verdict == Protocol::AimVerdict::InRange)
  {
    return {{.cellVoxels = reading->strikeVoxels, .paletteIndex = OVERLAY_ACCENT_INDEX}};
  }
  if (reading->verdict == Protocol::AimVerdict::Blocked)
  {
    return {{.cellVoxels = reading->strikeVoxels, .paletteIndex = OVERLAY_BAD_INDEX}};
  }
  return {};
}

} // namespace Client
