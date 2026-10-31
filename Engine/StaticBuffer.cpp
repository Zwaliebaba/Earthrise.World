#include "pch.h"

#include "StaticBuffer.h"

#include "ResourceBarrier.h"

namespace Engine
{

StaticBuffer::StaticBuffer(ID3D12Device* _device, std::uint64_t _sizeBytes)
  : m_sizeBytes(_sizeBytes)
{
  const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_DEFAULT};
  const D3D12_RESOURCE_DESC bufferDesc{.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                       .Width = _sizeBytes,
                                       .Height = 1,
                                       .DepthOrArraySize = 1,
                                       .MipLevels = 1,
                                       .Format = DXGI_FORMAT_UNKNOWN,
                                       .SampleDesc = {.Count = 1},
                                       .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR};
  winrt::check_hresult(
    _device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, m_state, nullptr, IID_PPV_ARGS(m_buffer.put())));
}

void StaticBuffer::Upload(ID3D12GraphicsCommandList* _commandList, UploadRing& _ring, std::span<const std::byte> _bytes)
{
  if (_bytes.size() > m_sizeBytes)
  {
    throw std::length_error("more bytes than the buffer holds");
  }
  if (_bytes.empty())
  {
    return;
  }
  const std::optional<UploadRing::Allocation> staging = _ring.Allocate(_bytes.size(), 16);
  if (!staging)
  {
    throw std::runtime_error("the upload ring has no room for a static buffer");
  }
  std::memcpy(staging->bytes.data(), _bytes.data(), _bytes.size());

  if (m_state != D3D12_RESOURCE_STATE_COPY_DEST)
  {
    const D3D12_RESOURCE_BARRIER toCopy = TransitionBarrier(m_buffer.get(), m_state, D3D12_RESOURCE_STATE_COPY_DEST);
    _commandList->ResourceBarrier(1, &toCopy);
  }
  _commandList->CopyBufferRegion(m_buffer.get(), 0, staging->buffer, staging->offsetBytes, _bytes.size());
  const D3D12_RESOURCE_BARRIER toRead =
    TransitionBarrier(m_buffer.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  _commandList->ResourceBarrier(1, &toRead);
  m_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
}

D3D12_GPU_VIRTUAL_ADDRESS StaticBuffer::GpuAddress() const noexcept
{
  return m_buffer->GetGPUVirtualAddress();
}

std::uint64_t StaticBuffer::SizeBytes() const noexcept
{
  return m_sizeBytes;
}

} // namespace Engine
