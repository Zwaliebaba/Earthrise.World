#include "pch.h"

#include "SunProjection.h"

namespace Engine
{

DirectX::XMMATRIX SunViewProjection(DirectX::FXMVECTOR _towardsSun, const DirectX::XMFLOAT3& _boxMinMeters,
                                    const DirectX::XMFLOAT3& _boxMaxMeters, float _marginMeters) noexcept
{
  const DirectX::XMVECTOR boxMin = DirectX::XMLoadFloat3(&_boxMinMeters);
  const DirectX::XMVECTOR boxMax = DirectX::XMLoadFloat3(&_boxMaxMeters);
  const DirectX::XMVECTOR center = DirectX::XMVectorScale(DirectX::XMVectorAdd(boxMin, boxMax), 0.5F);
  const DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(DirectX::XMVectorNegate(_towardsSun));
  // Up is the world's, unless the sun is nearly overhead, where up would be the direction of view.
  constexpr float NEARLY_VERTICAL = 0.99F;
  const DirectX::XMVECTOR up = std::abs(DirectX::XMVectorGetY(forward)) > NEARLY_VERTICAL ? DirectX::XMVectorSet(0.0F, 0.0F, 1.0F, 0.0F)
                                                                                          : DirectX::XMVectorSet(0.0F, 1.0F, 0.0F, 0.0F);
  const DirectX::XMMATRIX view = DirectX::XMMatrixLookToLH(center, forward, up);

  DirectX::XMVECTOR low = DirectX::XMVectorReplicate(1.0e30F);
  DirectX::XMVECTOR high = DirectX::XMVectorReplicate(-1.0e30F);
  for (std::uint32_t corner = 0; corner < 8; ++corner)
  {
    const DirectX::XMVECTOR point = DirectX::XMVectorSelect(
      boxMin, boxMax,
      DirectX::XMVectorSelectControl((corner & 1U) != 0 ? 1U : 0U, (corner & 2U) != 0 ? 1U : 0U, (corner & 4U) != 0 ? 1U : 0U, 0U));
    const DirectX::XMVECTOR inView = DirectX::XMVector3TransformCoord(point, view);
    low = DirectX::XMVectorMin(low, inView);
    high = DirectX::XMVectorMax(high, inView);
  }
  const DirectX::XMVECTOR margin = DirectX::XMVectorReplicate(_marginMeters);
  low = DirectX::XMVectorSubtract(low, margin);
  high = DirectX::XMVectorAdd(high, margin);
  // Near and far swapped: depth 1 at the side nearest the sun, which has the smallest view depth.
  const DirectX::XMMATRIX projection =
    DirectX::XMMatrixOrthographicOffCenterLH(DirectX::XMVectorGetX(low), DirectX::XMVectorGetX(high), DirectX::XMVectorGetY(low),
                                             DirectX::XMVectorGetY(high), DirectX::XMVectorGetZ(high), DirectX::XMVectorGetZ(low));
  return DirectX::XMMatrixMultiply(view, projection);
}

} // namespace Engine
