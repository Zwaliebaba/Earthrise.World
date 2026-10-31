#pragma once

#include "framework.h"

#include <unknwn.h>

#include <d3d12.h>
#include <winrt/base.h>

#include "CommandQueue.h"

#include <array>
#include <cstdint>

namespace Engine
{

/// The frames in flight (ADR-011, decision 2): one command allocator for each, and the one command
/// list they take turns with. Begin waits until the GPU has finished the frame that last used the
/// slot, so the CPU never runs more than FRAMES_IN_FLIGHT frames ahead.
class FrameRing
{
public:
  static constexpr std::uint32_t FRAMES_IN_FLIGHT = 2;

  /// Throws winrt::hresult_error when the device refuses.
  FrameRing(ID3D12Device* _device, CommandQueue& _queue);

  /// The command list, reset onto this frame's allocator and open for recording.
  [[nodiscard]] ID3D12GraphicsCommandList* Begin();

  /// Closes the list, submits it, and returns the fence value that marks the frame finished.
  std::uint64_t End();

  /// Which slot the next Begin uses.
  [[nodiscard]] std::uint32_t Slot() const noexcept;

private:
  CommandQueue& m_queue;
  std::array<winrt::com_ptr<ID3D12CommandAllocator>, FRAMES_IN_FLIGHT> m_allocators;
  std::array<std::uint64_t, FRAMES_IN_FLIGHT> m_fenceValues{};
  winrt::com_ptr<ID3D12GraphicsCommandList> m_commandList;
  std::uint32_t m_slot = 0;
};

} // namespace Engine
