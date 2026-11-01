#include "pch.h"

#include "Debris.h"

#include "Palette.h"

namespace RendererSpike
{

namespace
{

// The pieces' sizes (RendererLook.md §1): half of them a loose block of 0.5 to 1 m, the rest 0.5 m
// pebbles, and one or two pebbles more for each voxel that fell, so the rubble is strewn.
constexpr float PEBBLE_HALF_EDGE_METERS = 0.25F;
constexpr float SMALLEST_BLOCK_HALF_EDGE_METERS = 0.25F;
constexpr float LARGEST_BLOCK_HALF_EDGE_METERS = 0.5F;
constexpr float BLOCK_SHARE = 0.5F;
// How long the last of them waits, and how deep a piece at rest sits in the rubble, as a share of
// its half edge.
constexpr float LONGEST_DELAY_SECONDS = 0.25F;
constexpr float SUNK_SHARE = 0.4F;
constexpr std::uint8_t RUBBLE_OF_STONE = 8;

/// SplitMix64: the pinned mixer every piece is drawn from (AGENTS.md R16).
constexpr std::uint64_t Mix(std::uint64_t _value) noexcept
{
  _value += 0x9e3779b97f4a7c15ULL;
  _value = (_value ^ (_value >> 30)) * 0xbf58476d1ce4e5b9ULL;
  _value = (_value ^ (_value >> 27)) * 0x94d049bb133111ebULL;
  return _value ^ (_value >> 31);
}

/// A stream of numbers in [0, 1) for one piece, from the collapse's id and the piece's number.
class Draws
{
public:
  Draws(std::uint64_t _collapseId, std::uint64_t _piece) noexcept
    : m_state(Mix(_collapseId ^ Mix(_piece)))
  {
  }

  float Next() noexcept
  {
    m_state = Mix(m_state);
    return static_cast<float>(m_state >> 40) / static_cast<float>(std::uint64_t{1} << 24);
  }

private:
  std::uint64_t m_state;
};

/// A uniformly random unit quaternion from three draws (Shoemake 1992).
DirectX::XMFLOAT4 RandomRotation(Draws& _draws) noexcept
{
  const float first = _draws.Next();
  const float second = DirectX::XM_2PI * _draws.Next();
  const float third = DirectX::XM_2PI * _draws.Next();
  const float a = std::sqrt(1.0F - first);
  const float b = std::sqrt(first);
  return {a * std::sin(second), a * std::cos(second), b * std::sin(third), b * std::cos(third)};
}

} // namespace

void Debris::Scatter(std::uint64_t _collapseId, std::span<const Simulation::FallenVoxel> _fallen,
                     std::span<const Protocol::VoxelPosition> _rubble, const RestingTop& _restingTop, float _startSeconds)
{
  m_pieces.clear();
  m_startSeconds = _startSeconds;
  if (_rubble.empty())
  {
    return; // nothing for debris to lie on
  }
  // Each fallen voxel draws how many pieces it makes from a stream of its own, and each piece from
  // one of its own, numbered across the collapse.
  const std::uint64_t countStream = Mix(~_collapseId);
  std::uint64_t number = 0;
  for (std::size_t index = 0; index < _fallen.size(); ++index)
  {
    const Simulation::FallenVoxel& fallen = _fallen[index];
    Draws count(countStream, index);
    const std::uint32_t pieces = count.Next() < 0.5F ? 2U : 3U;
    for (std::uint32_t piece = 0; piece < pieces; ++piece, ++number)
    {
      Draws draws(_collapseId, number);
      const bool block = piece == 0 && draws.Next() < BLOCK_SHARE;
      const float halfEdge =
        block ? SMALLEST_BLOCK_HALF_EDGE_METERS + (LARGEST_BLOCK_HALF_EDGE_METERS - SMALLEST_BLOCK_HALF_EDGE_METERS) * draws.Next()
              : PEBBLE_HALF_EDGE_METERS;
      // From inside the voxel that fell, to a column near it on the rubble.
      const DirectX::XMFLOAT3 from = {static_cast<float>(fallen.position.xVoxels) + draws.Next(),
                                      static_cast<float>(fallen.position.yVoxels) + draws.Next(),
                                      static_cast<float>(fallen.position.zVoxels) + draws.Next()};
      // Onto one of the rubble's columns, anywhere within it.
      const auto column = std::min(static_cast<std::size_t>(draws.Next() * static_cast<float>(_rubble.size())), _rubble.size() - 1);
      const float toX = static_cast<float>(_rubble[column].xVoxels) + draws.Next();
      const float toZ = static_cast<float>(_rubble[column].zVoxels) + draws.Next();
      const float toY = _restingTop(toX, toZ) + halfEdge * (1.0F - SUNK_SHARE);
      // A block is the stone it fell from, a pebble the rubble it became.
      const std::uint8_t paletteIndex = block ? fallen.paletteIndex : RUBBLE_OF_STONE;
      const float lightness = (draws.Next() - 0.5F) * Content::PaletteJitter(paletteIndex);
      const DirectX::XMFLOAT4 fromRotation = RandomRotation(draws);
      const DirectX::XMFLOAT4 toRotation = RandomRotation(draws);
      m_pieces.push_back({.fromMeters = from,
                          .toMeters = {toX, toY, toZ},
                          .fromRotation = fromRotation,
                          .toRotation = toRotation,
                          .halfEdgeMeters = halfEdge,
                          .delaySeconds = LONGEST_DELAY_SECONDS * draws.Next(),
                          .paletteIndex = paletteIndex,
                          .lightnessOffset = lightness});
    }
  }
}

void Debris::Pose(float _seconds, std::vector<Engine::OrientedVoxel>& _voxels) const
{
  for (const Piece& piece : m_pieces)
  {
    // Along the fall, 0 to 1: across at a steady rate and down as it falls, faster as it goes,
    // turning from its first angle to its last; then at rest.
    const float along = std::clamp((_seconds - m_startSeconds - piece.delaySeconds) / FALL_SECONDS, 0.0F, 1.0F);
    const DirectX::XMVECTOR from = DirectX::XMLoadFloat3(&piece.fromMeters);
    const DirectX::XMVECTOR to = DirectX::XMLoadFloat3(&piece.toMeters);
    DirectX::XMFLOAT3 center{};
    DirectX::XMStoreFloat3(&center, DirectX::XMVectorLerp(from, to, along));
    center.y = piece.fromMeters.y + (piece.toMeters.y - piece.fromMeters.y) * along * along;
    DirectX::XMFLOAT4 rotation{};
    DirectX::XMStoreFloat4(
      &rotation, DirectX::XMQuaternionSlerp(DirectX::XMLoadFloat4(&piece.fromRotation), DirectX::XMLoadFloat4(&piece.toRotation), along));
    _voxels.push_back({.centerMeters = center,
                       .halfEdgeMeters = piece.halfEdgeMeters,
                       .rotation = rotation,
                       .paletteIndex = piece.paletteIndex,
                       .lightnessOffset = piece.lightnessOffset,
                       .reserved = {}});
  }
}

std::size_t Debris::Pieces() const noexcept
{
  return m_pieces.size();
}

} // namespace RendererSpike
