#include "pch.h"

#include "CommandQueue.h"

namespace Engine
{

CommandQueue::CommandQueue(ID3D12Device* _device, D3D12_COMMAND_LIST_TYPE _type)
{
  const D3D12_COMMAND_QUEUE_DESC queueDesc{.Type = _type};
  winrt::check_hresult(_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_queue.put())));
  winrt::check_hresult(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.put())));
  m_completion.attach(CreateEventW(nullptr, FALSE, FALSE, nullptr));
  if (!m_completion)
  {
    winrt::throw_last_error();
  }
}

ID3D12CommandQueue* CommandQueue::Queue() const noexcept
{
  return m_queue.get();
}

std::uint64_t CommandQueue::Signal()
{
  ++m_lastSignaled;
  winrt::check_hresult(m_queue->Signal(m_fence.get(), m_lastSignaled));
  return m_lastSignaled;
}

std::uint64_t CommandQueue::CompletedValue() const noexcept
{
  return m_fence->GetCompletedValue();
}

void CommandQueue::WaitFor(std::uint64_t _fenceValue)
{
  if (m_fence->GetCompletedValue() >= _fenceValue)
  {
    return;
  }
  winrt::check_hresult(m_fence->SetEventOnCompletion(_fenceValue, m_completion.get()));
  if (WaitForSingleObject(m_completion.get(), INFINITE) != WAIT_OBJECT_0)
  {
    winrt::throw_last_error();
  }
}

void CommandQueue::Flush()
{
  WaitFor(Signal());
}

} // namespace Engine
