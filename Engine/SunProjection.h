#pragma once

#include <DirectXMath.h>

namespace Engine
{

/// The sun's orthographic view-projection over a box of the world (ADR-011, decision 9). It looks
/// along -_towardsSun, which need not be unit length, and its view is fitted to the box's eight
/// corners with _marginMeters about them on every side. Its depth is reversed, as the camera's is
/// (ADR-011, decision 4): 1 at the plane nearest the sun, 0 at the farthest. For row vectors, as
/// VoxelPass::FrameConstants takes its matrices.
[[nodiscard]] DirectX::XMMATRIX SunViewProjection(DirectX::FXMVECTOR _towardsSun, const DirectX::XMFLOAT3& _boxMinMeters,
                                                  const DirectX::XMFLOAT3& _boxMaxMeters, float _marginMeters) noexcept;

} // namespace Engine
