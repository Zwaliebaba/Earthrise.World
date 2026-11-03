#include "pch.h"

#include "ControlGroups.h"

#include "Interpolation.h"
#include "Strings.h"

namespace Client
{

namespace
{

/// Hud.md §5's short labels, by UnitKind.
constexpr std::array<std::string_view, 8> SHORT_LABELS = {LABOR_LABEL, SPEAR_LABEL,  ARCHER_LABEL, RIDER_LABEL,
                                                          ARMS_LABEL,  SAPPER_LABEL, RAM_LABEL,    TREB_LABEL};

} // namespace

void ControlGroups::Assign(std::size_t _group, const Selection& _selection)
{
  if (_group < GROUPS)
  {
    m_groups.at(_group) = _selection.squads;
  }
}

bool ControlGroups::Recall(std::size_t _group, Selection& _selection, const Protocol::StateUpdate& _state, double _nowSeconds)
{
  if (_group >= GROUPS)
  {
    return false;
  }
  Prune(_state);
  const bool twice = m_lastGroup == _group && _nowSeconds - m_lastSeconds <= TWICE_SECONDS;
  m_lastGroup = _group;
  m_lastSeconds = _nowSeconds;
  if (m_groups.at(_group).empty())
  {
    return false;
  }
  Selector::Select(_selection, _state, m_groups.at(_group), false);
  return twice;
}

void ControlGroups::Prune(const Protocol::StateUpdate& _state)
{
  for (std::vector<Protocol::EntityId>& group : m_groups)
  {
    std::erase_if(group,
                  [&_state](Protocol::EntityId _squad)
                  {
                    const auto squad = std::ranges::find(_state.squads, _squad, &Protocol::SquadView::id);
                    return squad == _state.squads.end() || squad->garrisonedIn != Protocol::NO_ENTITY ||
                           std::ranges::none_of(_state.units, [_squad](const Protocol::UnitView& _unit) { return _unit.squad == _squad; });
                  });
  }
}

std::optional<DirectX::XMFLOAT3> ControlGroups::Center(std::size_t _group, const Protocol::StateUpdate& _state) const
{
  if (_group >= GROUPS)
  {
    return std::nullopt;
  }
  DirectX::XMFLOAT3 sum{0.0F, 0.0F, 0.0F};
  std::size_t figures = 0;
  for (const Protocol::UnitView& unit : _state.units)
  {
    if (std::ranges::find(m_groups.at(_group), unit.squad) != m_groups.at(_group).end())
    {
      const DirectX::XMFLOAT3 feet = Meters(unit.position);
      sum = {sum.x + feet.x, sum.y + feet.y, sum.z + feet.z};
      ++figures;
    }
  }
  if (figures == 0)
  {
    return std::nullopt;
  }
  const auto count = static_cast<float>(figures);
  return DirectX::XMFLOAT3{sum.x / count, sum.y / count, sum.z / count};
}

const std::array<std::vector<Protocol::EntityId>, ControlGroups::GROUPS>& ControlGroups::Groups() const noexcept
{
  return m_groups;
}

std::string ControlGroups::Label(std::size_t _group, const Protocol::StateUpdate& _state) const
{
  if (_group >= GROUPS || m_groups.at(_group).empty())
  {
    return {};
  }
  const auto first = std::ranges::find(_state.squads, m_groups.at(_group).front(), &Protocol::SquadView::id);
  if (first == _state.squads.end())
  {
    return {};
  }
  std::size_t figures = 0;
  for (const Protocol::UnitView& unit : _state.units)
  {
    figures += std::ranges::find(m_groups.at(_group), unit.squad) != m_groups.at(_group).end() ? 1 : 0;
  }
  return std::format("{} {}", SHORT_LABELS.at(static_cast<std::size_t>(first->kind)), figures);
}

} // namespace Client
