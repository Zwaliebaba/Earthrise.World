#include "pch.h"

#include "DescriptorHeap.h"

namespace Engine
{

DescriptorHeap::DescriptorHeap(ID3D12Device* _device, const Desc& _desc)
  : m_capacityDescriptors(_desc.capacityDescriptors)
{
  const D3D12_DESCRIPTOR_HEAP_DESC heapDesc{.Type = _desc.type,
                                            .NumDescriptors = _desc.capacityDescriptors,
                                            .Flags = _desc.shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                                                         : D3D12_DESCRIPTOR_HEAP_FLAG_NONE};
  winrt::check_hresult(_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_heap.put())));
  m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();
  if (_desc.shaderVisible)
  {
    m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
  }
  m_incrementBytes = _device->GetDescriptorHandleIncrementSize(_desc.type);
}

DescriptorHeap::Handle DescriptorHeap::Allocate(std::uint32_t _countDescriptors)
{
  if (_countDescriptors == 0)
  {
    throw std::invalid_argument("allocate at least one descriptor");
  }
  if (_countDescriptors > m_capacityDescriptors - m_allocatedDescriptors)
  {
    throw std::length_error("the descriptor heap is full");
  }
  const Handle first = At(m_allocatedDescriptors);
  m_allocatedDescriptors += _countDescriptors;
  return first;
}

DescriptorHeap::Handle DescriptorHeap::At(std::uint32_t _index) const
{
  if (_index >= m_capacityDescriptors)
  {
    throw std::out_of_range("descriptor index beyond the heap");
  }
  const SIZE_T offsetBytes = static_cast<SIZE_T>(_index) * m_incrementBytes;
  return {
    .cpu = {.ptr = m_cpuStart.ptr + offsetBytes}, .gpu = {.ptr = m_gpuStart.ptr == 0 ? 0 : m_gpuStart.ptr + offsetBytes}, .index = _index};
}

void DescriptorHeap::Reset() noexcept
{
  m_allocatedDescriptors = 0;
}

ID3D12DescriptorHeap* DescriptorHeap::Heap() const noexcept
{
  return m_heap.get();
}

std::uint32_t DescriptorHeap::CapacityDescriptors() const noexcept
{
  return m_capacityDescriptors;
}

std::uint32_t DescriptorHeap::AllocatedDescriptors() const noexcept
{
  return m_allocatedDescriptors;
}

} // namespace Engine
