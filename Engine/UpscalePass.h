#pragma once

#include "framework.h"

#include <unknwn.h>

#include <d3d12.h>
#include <winrt/base.h>

#include "DescriptorHeap.h"

#include <cstdint>

namespace Engine
{

/// The pixels a target _displayPixels wide draws at _renderScale: rounded to the nearest pixel, and
/// never fewer than one.
[[nodiscard]] std::uint32_t ScaledPixels(std::uint32_t _displayPixels, float _renderScale) noexcept;

/// Scales the scene, drawn at its render scale, to the back buffer with a bilinear filter (ADR-011,
/// decision 3). One triangle covers the target, and each pixel samples the source at its centre,
/// clamped at the edges. The source is read through an sRGB view, so the filter blends linear light,
/// and the same size copies the source exactly.
class UpscalePass
{
public:
  struct Desc
  {
    DXGI_FORMAT targetFormat; // the render-target view drawn into, such as the back buffer's
  };

  /// Throws winrt::hresult_error when the device refuses.
  UpscalePass(ID3D12Device* _device, const Desc& _desc);

  /// Reads _source from now on, through its own format. The caller first makes sure the GPU no
  /// longer reads the previous source, because the view is rewritten in place.
  void SetSource(ID3D12Resource* _source);

  /// Records the draw. The caller has set the render target, its viewport and its scissor, and
  /// _source is readable by a pixel shader. Sets this pass's descriptor heap on _commandList. Throws
  /// std::logic_error when no source was set.
  void Draw(ID3D12GraphicsCommandList* _commandList) const;

private:
  ID3D12Device* m_device;
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
  DescriptorHeap m_viewHeap;
  DescriptorHeap::Handle m_sourceView{};
  bool m_hasSource = false;
};

} // namespace Engine
