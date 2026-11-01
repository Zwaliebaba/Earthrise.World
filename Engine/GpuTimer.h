#pragma once

#include "framework.h"

#include <unknwn.h>

#include <d3d12.h>
#include <winrt/base.h>

#include <cstdint>
#include <vector>

namespace Engine
{

/// GPU time spans from timestamp queries on one queue, built in for the pass mark (ADR-011,
/// decision 10). Each frame slot of a FrameRing has its own queries and its own part of one
/// readback buffer. A frame's spans are read once its fence has passed, which FrameRing::Begin
/// waits for before it reuses the slot, while the frame after it records.
class GpuTimer
{
public:
  struct Desc
  {
    std::uint32_t slotCount; // frames in flight, such as FrameRing::FRAMES_IN_FLIGHT
    std::uint32_t spanCount; // spans each frame measures
  };

  /// Throws winrt::hresult_error when the device refuses, and std::invalid_argument for a count of
  /// zero.
  GpuTimer(ID3D12Device* _device, ID3D12CommandQueue* _queue, const Desc& _desc);

  /// Records the timestamps that open and close _span in _slot's frame. Throws std::out_of_range
  /// beyond the Desc's counts.
  void Begin(ID3D12GraphicsCommandList* _commandList, std::uint32_t _slot, std::uint32_t _span) const;
  void End(ID3D12GraphicsCommandList* _commandList, std::uint32_t _slot, std::uint32_t _span) const;

  /// Records the copy of _slot's timestamps into the readback buffer: last in the frame's list,
  /// after every span is closed.
  void Resolve(ID3D12GraphicsCommandList* _commandList, std::uint32_t _slot) const;

  /// Each span of _slot's last resolved frame, in milliseconds. The caller first makes sure that
  /// frame's fence has passed. A span the frame did not both begin and end reads as nonsense.
  [[nodiscard]] std::vector<double> ReadMilliseconds(std::uint32_t _slot) const;

private:
  [[nodiscard]] std::uint32_t QueryIndex(std::uint32_t _slot, std::uint32_t _span) const;

  winrt::com_ptr<ID3D12QueryHeap> m_queryHeap;
  winrt::com_ptr<ID3D12Resource> m_readback;
  double m_ticksPerMillisecond = 0.0;
  std::uint32_t m_slotCount = 0;
  std::uint32_t m_spanCount = 0;
};

} // namespace Engine
