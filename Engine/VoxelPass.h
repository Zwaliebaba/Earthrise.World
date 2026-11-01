#pragma once

#include "framework.h"

#include <unknwn.h>

#include <DirectXMath.h>
#include <d3d12.h>
#include <winrt/base.h>

#include "DescriptorHeap.h"

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
    /// The sun's view-projection, for both shadow maps (SunProjection.h).
    DirectX::XMFLOAT4X4 shadowViewProjection;
    /// x: how far a hit is moved out along its normal before the maps are read, in meters; y: a
    /// depth bias added to its depth in the sun's view; z: one texel of the static map, 1 over its
    /// edge; w: one texel of the dynamic map, or 0, which turns shadows off.
    DirectX::XMFLOAT4 shadowParameters;
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
  /// addresses, and the shadow maps. The caller has set the render target, depth target, viewport
  /// and scissor, and the shadow maps are readable by a pixel shader. Sets this pass's descriptor
  /// heap on _commandList.
  void Begin(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _frameConstants, D3D12_GPU_VIRTUAL_ADDRESS _palette) const;

  /// Binds the shadow pipeline, which writes only depth, with the sun's frame constants: its
  /// view-projection, the ray direction away from the sun, and the shadow map's size as the
  /// viewport (ADR-001 decision 5, ADR-011 decision 9). Draw then draws into the map the caller
  /// has set up with ShadowMap::BeginDrawing.
  void BeginShadow(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _sunFrameConstants,
                   D3D12_GPU_VIRTUAL_ADDRESS _palette) const;

  /// Binds the pipeline for oriented voxels (OrientedVoxel.h), lit and shadowed as Begin's are,
  /// with no corner shade; DrawOriented then draws them.
  void BeginOriented(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _frameConstants,
                     D3D12_GPU_VIRTUAL_ADDRESS _palette) const;

  /// Binds the shadow pipeline for oriented voxels, as BeginShadow does for axis-aligned ones.
  void BeginOrientedShadow(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _sunFrameConstants,
                           D3D12_GPU_VIRTUAL_ADDRESS _palette) const;

  /// Draws _voxelCount OrientedVoxels at _voxels, in world space, through whichever oriented
  /// pipeline was last begun.
  void DrawOriented(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _voxels, std::uint32_t _voxelCount) const;

  /// The two shadow maps the pass reads from now on: the world's, drawn when it changes, and the
  /// models', drawn each frame (ADR-011, decision 9). Null views read nothing. The caller first
  /// makes sure the GPU no longer reads the previous maps, because the views are rewritten in place.
  void SetShadowMaps(ID3D12Resource* _static, ID3D12Resource* _dynamic);

  /// Draws _recordCount VoxelRecords at _records as one batch: a chunk or a model. Throws
  /// std::invalid_argument for a turn of four or more, or a turned batch's extent outside 1 to 64.
  void Draw(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _records, std::uint32_t _recordCount,
            const Batch& _batch) const;

private:
  void Bind(ID3D12GraphicsCommandList* _commandList, ID3D12PipelineState* _pipelineState, D3D12_GPU_VIRTUAL_ADDRESS _frameConstants,
            D3D12_GPU_VIRTUAL_ADDRESS _palette) const;

  ID3D12Device* m_device;
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
  winrt::com_ptr<ID3D12PipelineState> m_shadowPipelineState;
  winrt::com_ptr<ID3D12PipelineState> m_orientedPipelineState;
  winrt::com_ptr<ID3D12PipelineState> m_orientedShadowPipelineState;
  DescriptorHeap m_shadowViews;
  DescriptorHeap::Handle m_shadowTable{};
};

} // namespace Engine
