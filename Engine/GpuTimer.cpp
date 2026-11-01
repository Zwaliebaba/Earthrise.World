#include "pch.h"

#include "GpuTimer.h"

namespace Engine
{

namespace
{

constexpr std::uint32_t QUERIES_PER_SPAN = 2; // its opening and its closing timestamp

} // namespace

GpuTimer::GpuTimer(ID3D12Device* _device, ID3D12CommandQueue* _queue, const Desc& _desc)
  : m_slotCount(_desc.slotCount),
    m_spanCount(_desc.spanCount)
{
  if (m_slotCount == 0 || m_spanCount == 0)
  {
    throw std::invalid_argument("a GPU timer needs at least one slot and one span");
  }
  const std::uint32_t queryCount = m_slotCount * m_spanCount * QUERIES_PER_SPAN;
  const D3D12_QUERY_HEAP_DESC heapDesc{.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP, .Count = queryCount, .NodeMask = 0};
  winrt::check_hresult(_device->CreateQueryHeap(&heapDesc, IID_PPV_ARGS(m_queryHeap.put())));

  const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_READBACK};
  const D3D12_RESOURCE_DESC bufferDesc{.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                       .Width = sizeof(std::uint64_t) * queryCount,
                                       .Height = 1,
                                       .DepthOrArraySize = 1,
                                       .MipLevels = 1,
                                       .Format = DXGI_FORMAT_UNKNOWN,
                                       .SampleDesc = {.Count = 1},
                                       .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR};
  winrt::check_hresult(_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST,
                                                        nullptr, IID_PPV_ARGS(m_readback.put())));

  UINT64 ticksPerSecond = 0;
  winrt::check_hresult(_queue->GetTimestampFrequency(&ticksPerSecond));
  m_ticksPerMillisecond = static_cast<double>(ticksPerSecond) / 1000.0;
}

std::uint32_t GpuTimer::QueryIndex(std::uint32_t _slot, std::uint32_t _span) const
{
  if (_slot >= m_slotCount || _span >= m_spanCount)
  {
    throw std::out_of_range("a GPU timer span or slot beyond its counts");
  }
  return (_slot * m_spanCount + _span) * QUERIES_PER_SPAN;
}

void GpuTimer::Begin(ID3D12GraphicsCommandList* _commandList, std::uint32_t _slot, std::uint32_t _span) const
{
  _commandList->EndQuery(m_queryHeap.get(), D3D12_QUERY_TYPE_TIMESTAMP, QueryIndex(_slot, _span));
}

void GpuTimer::End(ID3D12GraphicsCommandList* _commandList, std::uint32_t _slot, std::uint32_t _span) const
{
  _commandList->EndQuery(m_queryHeap.get(), D3D12_QUERY_TYPE_TIMESTAMP, QueryIndex(_slot, _span) + 1);
}

void GpuTimer::Resolve(ID3D12GraphicsCommandList* _commandList, std::uint32_t _slot) const
{
  const std::uint32_t first = QueryIndex(_slot, 0);
  const std::uint32_t count = m_spanCount * QUERIES_PER_SPAN;
  _commandList->ResolveQueryData(m_queryHeap.get(), D3D12_QUERY_TYPE_TIMESTAMP, first, count, m_readback.get(),
                                 sizeof(std::uint64_t) * first);
}

std::vector<double> GpuTimer::ReadMilliseconds(std::uint32_t _slot) const
{
  const std::uint32_t first = QueryIndex(_slot, 0);
  const std::uint32_t count = m_spanCount * QUERIES_PER_SPAN;
  const D3D12_RANGE readRange{.Begin = sizeof(std::uint64_t) * first, .End = sizeof(std::uint64_t) * (first + count)};
  void* mapped = nullptr;
  winrt::check_hresult(m_readback->Map(0, &readRange, &mapped));
  std::vector<std::uint64_t> ticks(count);
  std::memcpy(ticks.data(), static_cast<const std::byte*>(mapped) + readRange.Begin, sizeof(std::uint64_t) * count);
  const D3D12_RANGE noWrite{.Begin = 0, .End = 0};
  m_readback->Unmap(0, &noWrite);

  std::vector<double> milliseconds;
  milliseconds.reserve(m_spanCount);
  for (std::uint32_t span = 0; span < m_spanCount; ++span)
  {
    const std::uint64_t opened = ticks[static_cast<std::size_t>(span) * QUERIES_PER_SPAN];
    const std::uint64_t closed = ticks[static_cast<std::size_t>(span) * QUERIES_PER_SPAN + 1];
    // Unsigned, so a span the frame never closed shows as huge rather than negative.
    milliseconds.push_back(static_cast<double>(closed - opened) / m_ticksPerMillisecond);
  }
  return milliseconds;
}

} // namespace Engine
