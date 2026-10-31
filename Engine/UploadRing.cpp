#include "pch.h"

#include "UploadRing.h"

namespace Engine
{

UploadRing::UploadRing(ID3D12Device* _device, std::uint64_t _capacityBytes)
  : m_ring(_capacityBytes)
{
  // Written by hand: no d3dx12.h helpers (R14).
  const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_UPLOAD};
  const D3D12_RESOURCE_DESC bufferDesc{.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                       .Width = _capacityBytes,
                                       .Height = 1,
                                       .DepthOrArraySize = 1,
                                       .MipLevels = 1,
                                       .Format = DXGI_FORMAT_UNKNOWN,
                                       .SampleDesc = {.Count = 1},
                                       .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR};
  winrt::check_hresult(_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                                        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(m_buffer.put())));

  // The CPU never reads it, so the read range is empty, and it stays mapped for its lifetime.
  const D3D12_RANGE noRead{.Begin = 0, .End = 0};
  void* mapped = nullptr;
  winrt::check_hresult(m_buffer->Map(0, &noRead, &mapped));
  m_mappedBytes = static_cast<std::byte*>(mapped);
  m_gpuAddress = m_buffer->GetGPUVirtualAddress();
}

std::optional<UploadRing::Allocation> UploadRing::Allocate(std::uint64_t _sizeBytes, std::uint64_t _alignmentBytes)
{
  const std::optional<std::uint64_t> offsetBytes = m_ring.Allocate(_sizeBytes, _alignmentBytes);
  if (!offsetBytes)
  {
    return std::nullopt;
  }
  return Allocation{.bytes = std::span<std::byte>(m_mappedBytes + *offsetBytes, static_cast<std::size_t>(_sizeBytes)),
                    .buffer = m_buffer.get(),
                    .offsetBytes = *offsetBytes,
                    .gpuAddress = m_gpuAddress + *offsetBytes};
}

void UploadRing::Retire(std::uint64_t _fenceValue)
{
  m_ring.Retire(_fenceValue);
}

void UploadRing::Reclaim(std::uint64_t _completedFenceValue) noexcept
{
  m_ring.Reclaim(_completedFenceValue);
}

std::uint64_t UploadRing::UsedBytes() const noexcept
{
  return m_ring.UsedBytes();
}

} // namespace Engine
