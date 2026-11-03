#include "pch.h"

#include "ArrowDraw.h"

#include "Interpolation.h"
#include "WorldModels.h"

namespace Client
{

void PlaceArrows(const Protocol::StateUpdate& _previous, const Protocol::StateUpdate& _last, double _tickFraction,
                 std::vector<ArrowDrawn>& _arrows)
{
  _arrows.clear();
  for (const Protocol::ShotView& shot : _last.shots)
  {
    if (shot.kind != Protocol::ShotKind::Arrow)
    {
      continue;
    }
    const auto before = std::ranges::find(_previous.shots, shot.id, &Protocol::ShotView::id);
    if (before == _previous.shots.end())
    {
      continue; // one position gives no flight
    }
    const DirectX::XMFLOAT3 from = Meters(before->position);
    const DirectX::XMFLOAT3 to = Meters(shot.position);
    const DirectX::XMFLOAT3 along{to.x - from.x, to.y - from.y, to.z - from.z};
    const float length = std::sqrt((along.x * along.x) + (along.y * along.y) + (along.z * along.z));
    if (length <= 0.0F)
    {
      continue;
    }
    _arrows.push_back({.middleMeters = DrawnPosition(&before->position, shot.position, _tickFraction, NEVER_SNAP_METERS),
                       .direction = {along.x / length, along.y / length, along.z / length},
                       .sideColorIndex = WorldModels::SideColor(_last, shot.owner)});
  }
}

} // namespace Client
