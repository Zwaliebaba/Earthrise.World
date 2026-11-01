#include "pch.h"

#include "Debris.h"

#include "Catalog.h"
#include "DebrisChoice.h"
#include "Palette.h"
#include "SplitMix64.h"

namespace Client
{

namespace
{

constexpr float MODEL_VOXEL_METERS = 0.5F;
constexpr float OFFSET_STEP_METERS = 0.25F; // a piece sits 0, 0.25 or 0.5 m into its cell
constexpr std::uint32_t OFFSET_STEPS = 3;
constexpr std::uint32_t QUARTER_TURNS = 4;
// How long the last of them waits after the fall starts.
constexpr float LONGEST_DELAY_SECONDS = 0.25F;

// The salts naming each draw's purpose (ADR-013, decision 4).
constexpr std::uint64_t SALT_COLUMN = 1;
constexpr std::uint64_t SALT_MODEL = 2;
constexpr std::uint64_t SALT_TURN = 3;
constexpr std::uint64_t SALT_OFFSET_X = 4;
constexpr std::uint64_t SALT_OFFSET_Z = 5;
constexpr std::uint64_t SALT_FALLEN = 6;
constexpr std::uint64_t SALT_START = 7;
constexpr std::uint64_t SALT_ROTATION = 8;
constexpr std::uint64_t SALT_DELAY = 9;
constexpr std::uint64_t SALT_LIGHTNESS = 10;

/// A keyed draw as a number in [0, 1).
float Unit(std::uint64_t _draw) noexcept
{
  return static_cast<float>(_draw >> 40) / static_cast<float>(std::uint64_t{1} << 24);
}

/// A uniformly random unit quaternion from three draws (Shoemake 1992).
DirectX::XMFLOAT4 RandomRotation(float _first, float _second, float _third) noexcept
{
  const float second = DirectX::XM_2PI * _second;
  const float third = DirectX::XM_2PI * _third;
  const float a = std::sqrt(1.0F - _first);
  const float b = std::sqrt(_first);
  return {a * std::sin(second), a * std::cos(second), b * std::sin(third), b * std::cos(third)};
}

/// The top voxel of each column of a landing, in ascending z, then x.
std::map<std::pair<std::int32_t, std::int32_t>, Protocol::PlacedVoxel> Tops(std::span<const Protocol::PlacedVoxel> _landing)
{
  std::map<std::pair<std::int32_t, std::int32_t>, Protocol::PlacedVoxel> tops;
  for (const Protocol::PlacedVoxel& voxel : _landing)
  {
    const std::pair<std::int32_t, std::int32_t> column{voxel.position.zVoxels, voxel.position.xVoxels};
    const auto found = tops.find(column);
    if (found == tops.end() || found->second.position.yVoxels < voxel.position.yVoxels)
    {
      tops.insert_or_assign(column, voxel);
    }
  }
  return tops;
}

} // namespace

void Debris::Scatter(std::uint64_t _collapseId, std::span<const Protocol::PlacedVoxel> _fallen,
                     std::span<const Protocol::PlacedVoxel> _landing, float _startSeconds)
{
  m_pieces.clear();
  m_voxels.clear();
  m_startSeconds = _startSeconds;
  // The columns of each rubble that has debris, in ascending palette index.
  std::map<std::uint8_t, std::vector<Protocol::PlacedVoxel>> columnsByRubble;
  for (const auto& [column, top] : Tops(_landing))
  {
    if (Content::FindDebrisChoice(top.paletteIndex) != nullptr)
    {
      columnsByRubble[top.paletteIndex].push_back(top);
    }
  }
  for (const auto& [rubble, columns] : columnsByRubble)
  {
    const Content::DebrisChoice* found = Content::FindDebrisChoice(rubble);
    if (found == nullptr)
    {
      continue;
    }
    const Content::DebrisChoice& choice = *found;
    // One piece a COLUMNS_PER_PIECE columns, to the nearest.
    const auto count = static_cast<std::uint32_t>((columns.size() + COLUMNS_PER_PIECE / 2) / COLUMNS_PER_PIECE);
    for (std::uint32_t number = 0; number < count; ++number)
    {
      // Each piece is keyed by its rubble and its number, so that a piece on one rubble never shifts another's.
      const std::uint64_t key = (std::uint64_t{rubble} << 32) | number;
      const auto draw = [_collapseId, key](std::uint64_t _salt) { return Core::KeyedDraw(_collapseId, _salt, key); };
      const Protocol::PlacedVoxel& top = columns[Core::DrawBelow(draw(SALT_COLUMN), static_cast<std::uint32_t>(columns.size()))];
      const std::string_view modelId = choice.models[Core::DrawBelow(draw(SALT_MODEL), static_cast<std::uint32_t>(choice.models.size()))];
      const Content::ModelView* model = Content::FindModel(modelId);
      if (model == nullptr)
      {
        throw std::runtime_error("the debris table names a model the catalog lacks");
      }
      const std::uint32_t turns = Core::DrawBelow(draw(SALT_TURN), QUARTER_TURNS);
      const Content::VoxelGrid& grid = model->grid;
      // The model's extent along x and z after its turn, in meters.
      const bool across = turns % 2 == 1;
      const float widthMeters = MODEL_VOXEL_METERS * static_cast<float>(across ? grid.depthVoxels : grid.widthVoxels);
      const float depthMeters = MODEL_VOXEL_METERS * static_cast<float>(across ? grid.widthVoxels : grid.depthVoxels);
      const float heightMeters = MODEL_VOXEL_METERS * static_cast<float>(grid.heightVoxels);
      const float offsetX = OFFSET_STEP_METERS * static_cast<float>(Core::DrawBelow(draw(SALT_OFFSET_X), OFFSET_STEPS));
      const float offsetZ = OFFSET_STEP_METERS * static_cast<float>(Core::DrawBelow(draw(SALT_OFFSET_Z), OFFSET_STEPS));
      const auto bottom = static_cast<float>(top.position.yVoxels + 1);
      const DirectX::XMFLOAT3 restingCenter = {static_cast<float>(top.position.xVoxels) + offsetX + (0.5F * widthMeters),
                                               bottom + (0.5F * heightMeters),
                                               static_cast<float>(top.position.zVoxels) + offsetZ + (0.5F * depthMeters)};
      // From inside one of the voxels that fell, or from above the column when none is given.
      DirectX::XMFLOAT3 from = {restingCenter.x, restingCenter.y + 1.0F, restingCenter.z};
      if (!_fallen.empty())
      {
        const Protocol::VoxelPosition fell =
          _fallen[Core::DrawBelow(draw(SALT_FALLEN), static_cast<std::uint32_t>(_fallen.size()))].position;
        const std::uint64_t start = draw(SALT_START);
        from = {static_cast<float>(fell.xVoxels) + Unit(Core::SplitMix64Mix(start)),
                static_cast<float>(fell.yVoxels) + Unit(Core::SplitMix64Mix(start + 1)),
                static_cast<float>(fell.zVoxels) + Unit(Core::SplitMix64Mix(start + 2))};
      }
      const std::uint64_t spin = draw(SALT_ROTATION);
      DirectX::XMFLOAT4 restingRotation{};
      DirectX::XMStoreFloat4(&restingRotation, DirectX::XMQuaternionRotationNormal(DirectX::XMVectorSet(0.0F, 1.0F, 0.0F, 0.0F),
                                                                                   DirectX::XM_PIDIV2 * static_cast<float>(turns)));
      m_pieces.push_back({.modelId = model->id,
                          .cellXVoxels = top.position.xVoxels,
                          .cellZVoxels = top.position.zVoxels,
                          .restingBottomMeters = bottom,
                          .quarterTurns = turns,
                          .tintIndex = choice.tintIndex,
                          .fromMeters = from,
                          .restingCenterMeters = restingCenter,
                          .fromRotation = RandomRotation(Unit(Core::SplitMix64Mix(spin)), Unit(Core::SplitMix64Mix(spin + 1)),
                                                         Unit(Core::SplitMix64Mix(spin + 2))),
                          .restingRotation = restingRotation,
                          .delaySeconds = LONGEST_DELAY_SECONDS * Unit(draw(SALT_DELAY))});
      // Its voxels about the model's center, each with its own jitter.
      std::vector<PieceVoxel>& voxels = m_voxels.emplace_back();
      const std::uint64_t lightness = draw(SALT_LIGHTNESS);
      for (std::uint32_t y = 0; y < grid.heightVoxels; ++y)
      {
        for (std::uint32_t z = 0; z < grid.depthVoxels; ++z)
        {
          for (std::uint32_t x = 0; x < grid.widthVoxels; ++x)
          {
            const std::uint8_t own = grid.At(x, y, z);
            if (own == Content::VoxelGrid::EMPTY)
            {
              continue;
            }
            const std::uint8_t index = choice.tintIndex != 0 ? choice.tintIndex : own;
            const float jitter = Unit(Core::SplitMix64Mix(lightness + voxels.size()));
            voxels.push_back(
              {.offsetMeters = {MODEL_VOXEL_METERS * (static_cast<float>(x) + 0.5F) - (0.5F * MODEL_VOXEL_METERS * grid.widthVoxels),
                                MODEL_VOXEL_METERS * (static_cast<float>(y) + 0.5F) - (0.5F * heightMeters),
                                MODEL_VOXEL_METERS * (static_cast<float>(z) + 0.5F) - (0.5F * MODEL_VOXEL_METERS * grid.depthVoxels)},
               .paletteIndex = index,
               .lightnessOffset = (jitter - 0.5F) * Content::PaletteJitter(index)});
          }
        }
      }
    }
  }
}

void Debris::Pose(float _seconds, std::vector<Engine::OrientedVoxel>& _voxels) const
{
  for (std::size_t index = 0; index < m_pieces.size(); ++index)
  {
    const Piece& piece = m_pieces[index];
    // Along the fall, 0 to 1: across at a steady rate and down as it falls, faster as it goes, turning from its first
    // angle to its resting one; then at rest.
    const float along = std::clamp((_seconds - m_startSeconds - piece.delaySeconds) / FALL_SECONDS, 0.0F, 1.0F);
    DirectX::XMFLOAT3 center{};
    DirectX::XMStoreFloat3(
      &center, DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&piece.fromMeters), DirectX::XMLoadFloat3(&piece.restingCenterMeters), along));
    center.y = piece.fromMeters.y + ((piece.restingCenterMeters.y - piece.fromMeters.y) * along * along);
    const DirectX::XMVECTOR rotation = along >= 1.0F ? DirectX::XMLoadFloat4(&piece.restingRotation)
                                                     : DirectX::XMQuaternionSlerp(DirectX::XMLoadFloat4(&piece.fromRotation),
                                                                                  DirectX::XMLoadFloat4(&piece.restingRotation), along);
    DirectX::XMFLOAT4 stored{};
    DirectX::XMStoreFloat4(&stored, rotation);
    const DirectX::XMVECTOR origin = DirectX::XMLoadFloat3(&center);
    for (const PieceVoxel& voxel : m_voxels[index])
    {
      DirectX::XMFLOAT3 voxelCenter{};
      DirectX::XMStoreFloat3(&voxelCenter,
                             DirectX::XMVectorAdd(origin, DirectX::XMVector3Rotate(DirectX::XMLoadFloat3(&voxel.offsetMeters), rotation)));
      _voxels.push_back({.centerMeters = voxelCenter,
                         .halfEdgeMeters = 0.5F * MODEL_VOXEL_METERS,
                         .rotation = stored,
                         .paletteIndex = voxel.paletteIndex,
                         .lightnessOffset = voxel.lightnessOffset,
                         .reserved = {}});
    }
  }
}

std::span<const Debris::Piece> Debris::Pieces() const noexcept
{
  return m_pieces;
}

} // namespace Client
