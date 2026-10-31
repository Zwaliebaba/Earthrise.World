#include "pch.h"

#include "ColorTarget.h"

namespace Engine
{

ColorTarget::ColorTarget(ID3D12Device* _device, const Desc& _desc, std::uint32_t _widthPixels, std::uint32_t _heightPixels)
  : m_device(_device),
    m_desc(_desc),
    m_viewHeap(_device, {.type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV, .capacityDescriptors = 1, .shaderVisible = false})
{
  m_view = m_viewHeap.Allocate().cpu;
  Create(_widthPixels, _heightPixels);
}

void ColorTarget::Resize(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  m_texture = nullptr;
  Create(_widthPixels, _heightPixels);
}

void ColorTarget::Create(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_DEFAULT};
  const D3D12_RESOURCE_DESC desc{.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                 .Width = _widthPixels,
                                 .Height = _heightPixels,
                                 .DepthOrArraySize = 1,
                                 .MipLevels = 1,
                                 .Format = m_desc.format,
                                 .SampleDesc = {.Count = 1},
                                 .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
                                 .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET};
  // The clear value is filled by member: designators cannot name an anonymous union's.
  D3D12_CLEAR_VALUE clear{};
  clear.Format = m_desc.format;
  std::copy(m_desc.clearColor.begin(), m_desc.clearColor.end(), std::begin(clear.Color));
  winrt::check_hresult(
    m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, IDLE_STATE, &clear, IID_PPV_ARGS(m_texture.put())));
  m_device->CreateRenderTargetView(m_texture.get(), nullptr, m_view);
  m_widthPixels = _widthPixels;
  m_heightPixels = _heightPixels;
}

ID3D12Resource* ColorTarget::Texture() const noexcept
{
  return m_texture.get();
}

D3D12_CPU_DESCRIPTOR_HANDLE ColorTarget::View() const noexcept
{
  return m_view;
}

std::uint32_t ColorTarget::WidthPixels() const noexcept
{
  return m_widthPixels;
}

std::uint32_t ColorTarget::HeightPixels() const noexcept
{
  return m_heightPixels;
}

void ColorTarget::Clear(ID3D12GraphicsCommandList* _commandList) const noexcept
{
  _commandList->ClearRenderTargetView(m_view, m_desc.clearColor.data(), 0, nullptr);
}

} // namespace Engine
