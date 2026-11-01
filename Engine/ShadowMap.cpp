#include "pch.h"

#include "ShadowMap.h"

#include "ResourceBarrier.h"

namespace Engine
{

ShadowMap::ShadowMap(ID3D12Device* _device, std::uint32_t _edgeTexels)
  : m_viewHeap(_device, {.type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV, .capacityDescriptors = 1, .shaderVisible = false}),
    m_edgeTexels(_edgeTexels)
{
  if (_edgeTexels == 0)
  {
    throw std::invalid_argument("a shadow map needs at least one texel");
  }
  const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_DEFAULT};
  const D3D12_RESOURCE_DESC desc{.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                 .Width = _edgeTexels,
                                 .Height = _edgeTexels,
                                 .DepthOrArraySize = 1,
                                 .MipLevels = 1,
                                 .Format = DXGI_FORMAT_R32_TYPELESS, // so it can be viewed both as depth and as R32_FLOAT
                                 .SampleDesc = {.Count = 1},
                                 .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
                                 .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL};
  // The clear value is filled by member: designators cannot name an anonymous union's.
  D3D12_CLEAR_VALUE clear{};
  clear.Format = DEPTH_FORMAT;
  clear.DepthStencil.Depth = 0.0F;
  clear.DepthStencil.Stencil = 0;
  winrt::check_hresult(_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                                        &clear, IID_PPV_ARGS(m_texture.put())));
  m_view = m_viewHeap.Allocate().cpu;
  D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc{};
  viewDesc.Format = DEPTH_FORMAT;
  viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
  viewDesc.Flags = D3D12_DSV_FLAG_NONE;
  viewDesc.Texture2D.MipSlice = 0;
  _device->CreateDepthStencilView(m_texture.get(), &viewDesc, m_view);
}

void ShadowMap::BeginDrawing(ID3D12GraphicsCommandList* _commandList)
{
  if (m_readable)
  {
    const D3D12_RESOURCE_BARRIER toWrite =
      TransitionBarrier(m_texture.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    _commandList->ResourceBarrier(1, &toWrite);
    m_readable = false;
  }
  _commandList->OMSetRenderTargets(0, nullptr, FALSE, &m_view);
  _commandList->ClearDepthStencilView(m_view, D3D12_CLEAR_FLAG_DEPTH, 0.0F, 0, 0, nullptr);
  const auto edge = static_cast<float>(m_edgeTexels);
  const D3D12_VIEWPORT viewport{.TopLeftX = 0.0F, .TopLeftY = 0.0F, .Width = edge, .Height = edge, .MinDepth = 0.0F, .MaxDepth = 1.0F};
  const D3D12_RECT scissor{.left = 0, .top = 0, .right = static_cast<LONG>(m_edgeTexels), .bottom = static_cast<LONG>(m_edgeTexels)};
  _commandList->RSSetViewports(1, &viewport);
  _commandList->RSSetScissorRects(1, &scissor);
}

void ShadowMap::EndDrawing(ID3D12GraphicsCommandList* _commandList)
{
  if (!m_readable)
  {
    const D3D12_RESOURCE_BARRIER toRead =
      TransitionBarrier(m_texture.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    _commandList->ResourceBarrier(1, &toRead);
    m_readable = true;
  }
}

ID3D12Resource* ShadowMap::Texture() const noexcept
{
  return m_texture.get();
}

std::uint32_t ShadowMap::EdgeTexels() const noexcept
{
  return m_edgeTexels;
}

bool ShadowMap::Readable() const noexcept
{
  return m_readable;
}

} // namespace Engine
