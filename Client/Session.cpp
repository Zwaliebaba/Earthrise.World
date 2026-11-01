#include "pch.h"

#include "Session.h"

#include "ByteReader.h"
#include "ByteWriter.h"

namespace Client
{

void Session::Receive(const Protocol::StateUpdate& _state, std::span<const Protocol::ChunkSnapshot> _snapshots,
                      std::span<const std::vector<std::byte>> _answers)
{
  m_previous = std::exchange(m_state, _state);
  m_snapshots.insert(m_snapshots.end(), _snapshots.begin(), _snapshots.end());
  for (const std::vector<std::byte>& bytes : _answers)
  {
    Core::ByteReader reader(bytes);
    const Protocol::Answer answer = Protocol::DecodeAnswer(reader);
    const auto asked = m_asked.find(answer.queryId);
    if (asked == m_asked.end())
    {
      continue; // not one of this session's, or answered before
    }
    const Protocol::Query query = asked->second;
    m_asked.erase(asked);
    if (const auto* aim = std::get_if<Protocol::AimCheck>(&query))
    {
      if (const auto* reading = std::get_if<Protocol::AimReading>(&answer.reading))
      {
        m_aims.insert_or_assign(AimKey{aim->engine, aim->targetVoxels.xVoxels, aim->targetVoxels.yVoxels, aim->targetVoxels.zVoxels},
                                *reading);
      }
    }
    else if (const auto* integrity = std::get_if<Protocol::SectionIntegrity>(&query))
    {
      if (const auto* reading = std::get_if<Protocol::IntegrityReading>(&answer.reading))
      {
        m_integrity.insert_or_assign(integrity->structure, *reading);
      }
    }
    else if (const auto* reach = std::get_if<Protocol::TileReach>(&query))
    {
      if (const auto* reading = std::get_if<Protocol::TileReachReading>(&answer.reading))
      {
        m_reach.insert_or_assign(std::pair{reach->tileX, reach->tileZ}, *reading);
      }
    }
  }
}

const Protocol::StateUpdate& Session::State() const noexcept
{
  return m_state;
}

const Protocol::StateUpdate& Session::PreviousState() const noexcept
{
  return m_previous;
}

std::vector<Protocol::ChunkSnapshot> Session::TakeSnapshots()
{
  return std::exchange(m_snapshots, {});
}

std::vector<std::vector<std::byte>> Session::TakeCommands()
{
  m_takenSources = std::exchange(m_sources, {});
  return std::exchange(m_commands, {});
}

std::vector<std::vector<std::byte>> Session::TakeQueries()
{
  return std::exchange(m_queries, {});
}

void Session::Submit(const Protocol::Command& _command, std::string_view _source)
{
  Core::ByteWriter writer;
  Protocol::Encode(_command, writer);
  m_commands.push_back(writer.TakeBytes());
  m_sources.emplace_back(_source);
}

void Session::Ask(const Protocol::Query& _query)
{
  const std::uint32_t id = m_nextQueryId++;
  Core::ByteWriter writer;
  Protocol::Encode(Protocol::QueryRequest{.id = id, .query = _query}, writer);
  m_queries.push_back(writer.TakeBytes());
  m_asked.insert_or_assign(id, _query);
}

const Protocol::AimReading* Session::Aim(Protocol::EntityId _engine, Protocol::VoxelPosition _target) const noexcept
{
  const auto found = m_aims.find(AimKey{_engine, _target.xVoxels, _target.yVoxels, _target.zVoxels});
  return found == m_aims.end() ? nullptr : &found->second;
}

const Protocol::IntegrityReading* Session::Integrity(Protocol::EntityId _structure) const noexcept
{
  const auto found = m_integrity.find(_structure);
  return found == m_integrity.end() ? nullptr : &found->second;
}

const Protocol::TileReachReading* Session::Reach(std::int32_t _tileX, std::int32_t _tileZ) const noexcept
{
  const auto found = m_reach.find(std::pair{_tileX, _tileZ});
  return found == m_reach.end() ? nullptr : &found->second;
}

const std::vector<std::string>& Session::TakenSources() const noexcept
{
  return m_takenSources;
}

} // namespace Client
