#pragma once

#include "framework.h"

#include <unknwn.h>

#include <d3d12.h>
#include <winrt/base.h>

#include <cstdint>

namespace Engine
{

/// A descriptor heap handed out front to back. Descriptors are never freed one by one, only all at
/// once by Reset: there is no free list (R15).
class DescriptorHeap
{
public:
  struct Desc
  {
    D3D12_DESCRIPTOR_HEAP_TYPE type;
    std::uint32_t capacityDescriptors;
    bool shaderVisible; // never for render-target or depth-stencil heaps
  };

  struct Handle
  {
    D3D12_CPU_DESCRIPTOR_HANDLE cpu;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu; // zero unless the heap is shader-visible
    std::uint32_t index;
  };

  /// Throws winrt::hresult_error when the device refuses.
  DescriptorHeap(ID3D12Device* _device, const Desc& _desc);

  /// The first of _countDescriptors consecutive descriptors. Throws std::length_error when they do
  /// not fit, and std::invalid_argument for a count of zero.
  [[nodiscard]] Handle Allocate(std::uint32_t _countDescriptors = 1);

  /// The descriptor at _index. Throws std::out_of_range beyond the capacity.
  [[nodiscard]] Handle At(std::uint32_t _index) const;

  /// Forgets every allocation. The caller makes sure the GPU no longer reads them.
  void Reset() noexcept;

  [[nodiscard]] ID3D12DescriptorHeap* Heap() const noexcept;
  [[nodiscard]] std::uint32_t CapacityDescriptors() const noexcept;
  [[nodiscard]] std::uint32_t AllocatedDescriptors() const noexcept;

private:
  winrt::com_ptr<ID3D12DescriptorHeap> m_heap;
  D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
  D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart{};
  std::uint32_t m_incrementBytes = 0;
  std::uint32_t m_capacityDescriptors = 0;
  std::uint32_t m_allocatedDescriptors = 0;
};

} // namespace Engine
