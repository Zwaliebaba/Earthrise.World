#pragma once

#include "framework.h"

#include <unknwn.h>

#include <DirectXMath.h>
#include <d3d12.h>
#include <winrt/base.h>

#include <cstdint>

namespace Engine
{

/// The voxel pass of ADR-001 and ADR-011 decision 6: one instanced draw per batch of voxel records,
/// four vertices each as a strip. The vertex shader bounds each voxel's box on screen, and the
/// pixel shader casts the ray through the pixel at the box (Majercik et al. 2018, Listing 5) and
/// writes the hit's depth with SV_DepthLessEqual under reversed Z (ADR-011, decision 4).
class VoxelPass
{
public:
  /// The frame's constants, as Shader/VoxelCommon.hlsli reads them. Matrices are for row vectors.
  struct FrameConstants
  {
    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMFLOAT4X4 inverseViewProjection;
    DirectX::XMFLOAT4 rayDirection;   // world, unit length: the orthographic camera's forward
    DirectX::XMFLOAT4 viewportPixels; // width, height, 1 / width, 1 / height
    DirectX::XMFLOAT4 lightDirection; // world, unit length, towards the light
  };

  /// One linear colour per palette index.
  static constexpr std::uint32_t PALETTE_ENTRIES = 256;

  struct Desc
  {
    DXGI_FORMAT colorFormat;
    DXGI_FORMAT depthFormat;
  };

  /// Throws winrt::hresult_error when the device refuses the root signature or the pipeline.
  VoxelPass(ID3D12Device* _device, const Desc& _desc);

  /// Binds the pipeline, the frame's constants and the palette, both at 256-byte-aligned GPU
  /// addresses. The caller has set the render target, depth target, viewport and scissor.
  void Begin(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _frameConstants, D3D12_GPU_VIRTUAL_ADDRESS _palette) const;

  /// Draws _recordCount VoxelRecords at _records, one batch: a chunk or a model, whose minimum
  /// corner is _originMeters and whose voxels are _voxelEdgeMeters on each edge.
  void Draw(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _records, std::uint32_t _recordCount,
            const DirectX::XMFLOAT3& _originMeters, float _voxelEdgeMeters) const;

private:
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
};

} // namespace Engine
