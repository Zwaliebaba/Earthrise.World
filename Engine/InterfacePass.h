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

/// A screen-space quad of the interface pass: a solid colour, or a rectangle of the atlas whose red channel is the
/// alpha. Its layout is load-bearing: Shader/InterfaceCommon.hlsli reads the same 64 bytes.
struct InterfaceQuad
{
  float xPixels, yPixels, widthPixels, heightPixels; // top-left origin, in the target's pixels
  float u0, v0, u1, v1;                              // the atlas rectangle, in texels; unused when solid
  DirectX::XMFLOAT4 color;                           // linear RGBA
  std::uint32_t textured;                            // 1: alpha is the atlas's red channel times color.a; 0: solid
  std::array<std::uint32_t, 3> reserved;
};

static_assert(sizeof(InterfaceQuad) == 64, "the shaders read a quad as 64 bytes");

/// The HUD's and every text's pass (ADR-019, decision 3): one instanced draw of screen-space quads, in the order given,
/// which is their stacking order, alpha-blended over what is there, with no depth test, at the target's size. It knows
/// nothing of fonts, scales or layouts.
class InterfacePass
{
public:
  struct Desc
  {
    DXGI_FORMAT targetFormat; // the render-target view drawn into, such as the back buffer's
  };

  /// Throws winrt::hresult_error when the device refuses.
  InterfacePass(ID3D12Device* _device, const Desc& _desc);

  /// Reads _atlas, R8_UNORM, _widthTexels by _heightTexels, from now on. The caller first makes sure the GPU no longer
  /// reads the old one, because the view is rewritten in place.
  void SetAtlas(ID3D12Resource* _atlas, std::uint32_t _widthTexels, std::uint32_t _heightTexels);

  /// Draws _quadCount quads at _quads, a buffer the GPU can read, in order, blended over the target, no depth. The caller
  /// has set the target, viewport and scissor to the whole of a target _targetWidthPixels by _targetHeightPixels, and
  /// the atlas is readable by a pixel shader. Sets this pass's descriptor heap on _commandList. Throws
  /// std::logic_error when no atlas was set.
  void Draw(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _quads, std::uint32_t _quadCount,
            std::uint32_t _targetWidthPixels, std::uint32_t _targetHeightPixels) const;

private:
  ID3D12Device* m_device;
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
  DescriptorHeap m_viewHeap;
  DescriptorHeap::Handle m_atlasView{};
  bool m_hasAtlas = false;
};

} // namespace Engine
