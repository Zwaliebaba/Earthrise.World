#pragma once

#include "framework.h"

#include <unknwn.h>

#include <DirectXMath.h>
#include <d3d12.h>
#include <winrt/base.h>

#include <array>
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
    /// The light's radiance in linear RGB, with its intensity and Lambert's 1 / pi already
    /// multiplied in: the sun's, and the hemisphere's from straight up and straight down
    /// (RendererLook.md §6). The fourth component is unused.
    DirectX::XMFLOAT4 sunRadiance;
    DirectX::XMFLOAT4 skyRadiance;
    DirectX::XMFLOAT4 groundRadiance;
    /// Each corner-shade table's shade at corner levels 0 to 3 (RendererLook.md §5), which a
    /// record's shade-table bit chooses between.
    std::array<DirectX::XMFLOAT4, 2> cornerShades;
  };

  /// One linear colour per palette index.
  static constexpr std::uint32_t PALETTE_ENTRIES = 256;

  /// Where one batch lands and how it is drawn, as Shader/VoxelCommon.hlsli reads its root
  /// constants. A chunk is drawn as it is; a model may be turned and have one colour swapped.
  struct Batch
  {
    DirectX::XMFLOAT3 originMeters; // the minimum corner of the batch's footprint, after any turn
    float voxelEdgeMeters;
    /// Quarter turns about the vertical, clockwise seen from above, so that one turns +z towards
    /// +x (ADR-011, decision 6). The turn keeps the footprint's minimum corner at the origin, which
    /// is why it needs the batch's extent, before the turn, in cells along x and z: 1 to 64 each.
    std::uint32_t quarterTurns;
    std::uint32_t widthCells;
    std::uint32_t depthCells;
    /// Voxels of palette index swapFromIndex are drawn as swapToIndex, such as a model's faction
    /// colour as its owner's. Index 0 is never drawn, so 0 swaps nothing.
    std::uint8_t swapFromIndex;
    std::uint8_t swapToIndex;
  };

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

  /// Draws _recordCount VoxelRecords at _records as one batch: a chunk or a model. Throws
  /// std::invalid_argument for a turn of four or more, or a turned batch's extent outside 1 to 64.
  void Draw(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _records, std::uint32_t _recordCount,
            const Batch& _batch) const;

private:
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
};

} // namespace Engine
