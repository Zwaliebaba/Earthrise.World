#include "pch.h"

#include "Commander.h"

#include "Integrity.h"
#include "SiegeAttacker.h"
#include "SiegeDefender.h"
#include "SplitMix64.h"
#include "World.h"

namespace Simulation
{

CommanderView::CommanderView(const World& _world, std::uint8_t _player, std::int32_t _workUnits) noexcept
  : m_world(&_world),
    m_player(_player),
    m_workLeft(_workUnits)
{
}

const Protocol::StateUpdate& CommanderView::State() const
{
  return m_world->State(m_player);
}

std::optional<Protocol::IntegrityReading> CommanderView::Integrity(Protocol::EntityId _structure)
{
  const Protocol::Query query = Protocol::SectionIntegrity{.structure = _structure};
  if (m_workLeft < World::SECTION_INTEGRITY_WORK || !m_world->Structures().contains(_structure) || !m_world->AnswerVisible(m_player, query))
  {
    return std::nullopt;
  }
  m_workLeft -= World::SECTION_INTEGRITY_WORK;
  return ReadIntegrity(*m_world, _structure);
}

bool CommanderView::Reachable(Protocol::NavigationClass _class, Protocol::VoxelPosition _fromVoxels, Protocol::VoxelPosition _toVoxels)
{
  if (m_workLeft < World::TILE_REACH_WORK)
  {
    return false;
  }
  m_workLeft -= World::TILE_REACH_WORK;
  const NavigationGrid& navigation = m_world->Navigation();
  const std::int64_t width = navigation.WidthCells();
  const std::int64_t from = (std::int64_t{_fromVoxels.zVoxels} * width) + _fromVoxels.xVoxels;
  const std::int64_t to = (std::int64_t{_toVoxels.zVoxels} * width) + _toVoxels.xVoxels;
  return navigation.NearestReachable(from, to, _class) == to;
}

std::int32_t CommanderView::WorkLeft() const noexcept
{
  return m_workLeft;
}

std::uint64_t CommanderView::Draw(std::uint64_t _salt) const noexcept
{
  return Core::KeyedDraw(m_world->m_desc.matchSeed, _salt, (std::uint64_t{m_world->m_tick} << 8U) | m_player);
}

const ServerData& CommanderView::Data() const noexcept
{
  return m_world->Data();
}

std::uint8_t CommanderView::Player() const noexcept
{
  return m_player;
}

std::uint32_t CommanderView::Tick() const noexcept
{
  return m_world->m_tick;
}

std::unique_ptr<Commander> MakeCommander(std::string_view _kind, Core::ByteReader* _saved)
{
  if (_kind == "human")
  {
    return nullptr;
  }
  if (_kind == SiegeDefender::KIND)
  {
    return std::make_unique<SiegeDefender>(_saved);
  }
  if (_kind == SiegeAttacker::KIND)
  {
    return std::make_unique<SiegeAttacker>(_saved);
  }
  throw std::invalid_argument("no commander of kind " + std::string(_kind));
}

CommanderSlot::CommanderSlot(std::unique_ptr<Commander> _commander) noexcept
  : m_commander(std::move(_commander))
{
}

CommanderSlot::CommanderSlot(const CommanderSlot& _other)
  : m_commander(_other.m_commander->Clone())
{
}

CommanderSlot& CommanderSlot::operator=(const CommanderSlot& _other)
{
  if (this != &_other)
  {
    m_commander = _other.m_commander->Clone();
  }
  return *this;
}

Commander& CommanderSlot::Get() const noexcept
{
  return *m_commander;
}

} // namespace Simulation
