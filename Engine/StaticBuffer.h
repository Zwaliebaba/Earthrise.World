#pragma once

#include "framework.h"

#include <unknwn.h>

#include <d3d12.h>
#include <winrt/base.h>

#include "UploadRing.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace Engine
{

/// A buffer in GPU memory that the CPU fills once through the upload ring: a batch's voxel
/// records, for one (ADR-011, decision 6). It is written again only by a new Upload, which the
/// caller records after the GPU has finished reading the old contents.
class StaticBuffer
{
public:
  /// Throws winrt::hresult_error when the device refuses.
  StaticBuffer(ID3D12Device* _device, std::uint64_t _sizeBytes);

  /// Records a copy of _bytes into the buffer and leaves it readable by the vertex shader. Throws
  /// std::length_error when _bytes is larger than the buffer, and std::runtime_error when the ring
  /// has no room until earlier frames finish.
  void Upload(ID3D12GraphicsCommandList* _commandList, UploadRing& _ring, std::span<const std::byte> _bytes);

  [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS GpuAddress() const noexcept;
  [[nodiscard]] std::uint64_t SizeBytes() const noexcept;

private:
  winrt::com_ptr<ID3D12Resource> m_buffer;
  std::uint64_t m_sizeBytes;
  D3D12_RESOURCE_STATES m_state = D3D12_RESOURCE_STATE_COMMON;
};

} // namespace Engine
