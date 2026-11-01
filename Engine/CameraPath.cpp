#include "pch.h"

#include "CameraPath.h"

namespace Engine
{

CameraPath::CameraPath(const Desc& _desc)
  : m_desc(_desc)
{
  // Written so that a NaN side fails it too.
  if (!(m_desc.sideMeters > 0.0F) || m_desc.legFrames == 0)
  {
    throw std::invalid_argument("a camera path needs a side above zero and legs of at least one frame");
  }
}

std::uint32_t CameraPath::FrameCount() const noexcept
{
  return LEG_COUNT * m_desc.legFrames;
}

CameraPath::Pose CameraPath::At(std::uint32_t _frame) const
{
  if (_frame >= FrameCount())
  {
    throw std::out_of_range("a frame beyond the camera path's end");
  }
  // The corners, clockwise seen from above with +z north and +x east: south-west, north-west,
  // north-east, south-east, and back to the start.
  const float half = 0.5F * m_desc.sideMeters;
  const std::array<DirectX::XMFLOAT2, LEG_COUNT> corners = {DirectX::XMFLOAT2{m_desc.centerMeters.x - half, m_desc.centerMeters.z - half},
                                                            DirectX::XMFLOAT2{m_desc.centerMeters.x - half, m_desc.centerMeters.z + half},
                                                            DirectX::XMFLOAT2{m_desc.centerMeters.x + half, m_desc.centerMeters.z + half},
                                                            DirectX::XMFLOAT2{m_desc.centerMeters.x + half, m_desc.centerMeters.z - half}};

  const std::uint32_t leg = _frame / m_desc.legFrames;
  const std::uint32_t frameInLeg = _frame % m_desc.legFrames;
  const float along = static_cast<float>(frameInLeg) / static_cast<float>(m_desc.legFrames);
  const DirectX::XMFLOAT2& from = corners[leg];
  const DirectX::XMFLOAT2& to = corners[(leg + 1) % LEG_COUNT];
  return {.targetMeters = {from.x + (to.x - from.x) * along, m_desc.centerMeters.y, from.y + (to.y - from.y) * along},
          .leg = leg,
          .turnStarts = leg > 0 && frameInLeg == 0};
}

} // namespace Engine
