#include "pch.h"

#include "FrameRing.h"

namespace Engine
{

FrameRing::FrameRing(ID3D12Device* _device, CommandQueue& _queue)
  : m_queue(_queue)
{
  for (winrt::com_ptr<ID3D12CommandAllocator>& allocator : m_allocators)
  {
    winrt::check_hresult(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(allocator.put())));
  }
  // A command list is created open; it is closed here so that every Begin can reset it.
  winrt::check_hresult(
    _device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_allocators[0].get(), nullptr, IID_PPV_ARGS(m_commandList.put())));
  winrt::check_hresult(m_commandList->Close());
}

ID3D12GraphicsCommandList* FrameRing::Begin()
{
  m_queue.WaitFor(m_fenceValues[m_slot]);
  winrt::check_hresult(m_allocators[m_slot]->Reset());
  winrt::check_hresult(m_commandList->Reset(m_allocators[m_slot].get(), nullptr));
  return m_commandList.get();
}

std::uint64_t FrameRing::End()
{
  winrt::check_hresult(m_commandList->Close());
  ID3D12CommandList* const lists[] = {m_commandList.get()};
  m_queue.Queue()->ExecuteCommandLists(static_cast<UINT>(std::size(lists)), lists);
  const std::uint64_t fenceValue = m_queue.Signal();
  m_fenceValues[m_slot] = fenceValue;
  m_slot = (m_slot + 1) % FRAMES_IN_FLIGHT;
  return fenceValue;
}

std::uint32_t FrameRing::Slot() const noexcept
{
  return m_slot;
}

} // namespace Engine
