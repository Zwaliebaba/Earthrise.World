#pragma once

#include "framework.h"

#include <unknwn.h>

#include <d3d12.h>
#include <winrt/base.h>

#include "RingAllocator.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace Engine
{

/// Upload memory for the direct queue (ADR-011, decision 8): one persistently mapped upload-heap
/// buffer, handed out as a ring. Write through an allocation's bytes, record a copy from its buffer
/// and offset, then Retire with the fence value of the submission that carries the copy.
class UploadRing
{
public:
  struct Allocation
  {
    std::span<std::byte> bytes;
    ID3D12Resource* buffer;
    std::uint64_t offsetBytes;
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;
  };

  /// Throws winrt::hresult_error when the device cannot create or map the buffer.
  UploadRing(ID3D12Device* _device, std::uint64_t _capacityBytes);

  /// Room for _sizeBytes at _alignmentBytes, or nothing until the GPU finishes older uploads. Throws
  /// as RingAllocator::Allocate does.
  [[nodiscard]] std::optional<Allocation> Allocate(std::uint64_t _sizeBytes, std::uint64_t _alignmentBytes);

  void Retire(std::uint64_t _fenceValue);
  void Reclaim(std::uint64_t _completedFenceValue) noexcept;

  [[nodiscard]] std::uint64_t UsedBytes() const noexcept;

private:
  winrt::com_ptr<ID3D12Resource> m_buffer;
  std::byte* m_mappedBytes = nullptr;
  D3D12_GPU_VIRTUAL_ADDRESS m_gpuAddress = 0;
  RingAllocator m_ring;
};

} // namespace Engine
