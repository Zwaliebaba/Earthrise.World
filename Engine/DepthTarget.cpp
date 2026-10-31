#include "pch.h"

#include "DepthTarget.h"

namespace Engine
{

DepthTarget::DepthTarget(ID3D12Device* _device, std::uint32_t _widthPixels, std::uint32_t _heightPixels)
  : m_device(_device),
    m_viewHeap(_device, {.type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV, .capacityDescriptors = 1, .shaderVisible = false})
{
  m_view = m_viewHeap.Allocate().cpu;
  Create(_widthPixels, _heightPixels);
}

void DepthTarget::Resize(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  m_texture = nullptr;
  Create(_widthPixels, _heightPixels);
}

void DepthTarget::Create(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_DEFAULT};
  const D3D12_RESOURCE_DESC desc{.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                 .Width = _widthPixels,
                                 .Height = _heightPixels,
                                 .DepthOrArraySize = 1,
                                 .MipLevels = 1,
                                 .Format = FORMAT,
                                 .SampleDesc = {.Count = 1},
                                 .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
                                 .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE};
  // The clear value is filled by member: designators cannot name an anonymous union's.
  D3D12_CLEAR_VALUE clear{};
  clear.Format = FORMAT;
  clear.DepthStencil.Depth = 0.0F;
  clear.DepthStencil.Stencil = 0;
  winrt::check_hresult(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                                         &clear, IID_PPV_ARGS(m_texture.put())));
  m_device->CreateDepthStencilView(m_texture.get(), nullptr, m_view);
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthTarget::View() const noexcept
{
  return m_view;
}

void DepthTarget::Clear(ID3D12GraphicsCommandList* _commandList) const noexcept
{
  _commandList->ClearDepthStencilView(m_view, D3D12_CLEAR_FLAG_DEPTH, 0.0F, 0, 0, nullptr);
}

} // namespace Engine
