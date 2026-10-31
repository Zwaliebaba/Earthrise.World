#pragma once

#include "framework.h"

#include <unknwn.h>

#include <d3d12.h>
#include <winrt/base.h>

#include <cstdint>

namespace Engine
{

/// A command queue and the fence that tracks it. M1 has one, the direct queue, and uploads go
/// through it too (ADR-011, decision 8).
class CommandQueue
{
public:
  /// Throws winrt::hresult_error when the device refuses.
  CommandQueue(ID3D12Device* _device, D3D12_COMMAND_LIST_TYPE _type);

  [[nodiscard]] ID3D12CommandQueue* Queue() const noexcept;

  /// Signals the next fence value after everything submitted so far, and returns it.
  std::uint64_t Signal();

  /// The highest fence value the GPU has passed.
  [[nodiscard]] std::uint64_t CompletedValue() const noexcept;

  /// Blocks until the GPU has passed _fenceValue.
  void WaitFor(std::uint64_t _fenceValue);

  /// Signals and waits, so that everything submitted so far has finished when it returns.
  void Flush();

private:
  winrt::com_ptr<ID3D12CommandQueue> m_queue;
  winrt::com_ptr<ID3D12Fence> m_fence;
  winrt::handle m_completion;
  std::uint64_t m_lastSignaled = 0;
};

} // namespace Engine
