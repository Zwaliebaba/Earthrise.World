#pragma once

#include "framework.h"

#include <unknwn.h>

#include <d3d12.h>
#include <winrt/base.h>

#include "DescriptorHeap.h"

#include <cstdint>

namespace Engine
{

/// The scene's depth buffer: D32_FLOAT, cleared to 0 for reversed Z (ADR-011, decision 4).
class DepthTarget
{
public:
  static constexpr DXGI_FORMAT FORMAT = DXGI_FORMAT_D32_FLOAT;

  /// Throws winrt::hresult_error when the device refuses.
  DepthTarget(ID3D12Device* _device, std::uint32_t _widthPixels, std::uint32_t _heightPixels);

  /// Remakes the buffer at a new size. The caller first makes sure the GPU no longer uses it.
  void Resize(std::uint32_t _widthPixels, std::uint32_t _heightPixels);

  [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE View() const noexcept;

  /// Records the clear to 0: the far plane, under reversed Z.
  void Clear(ID3D12GraphicsCommandList* _commandList) const noexcept;

private:
  void Create(std::uint32_t _widthPixels, std::uint32_t _heightPixels);

  ID3D12Device* m_device;
  DescriptorHeap m_viewHeap;
  winrt::com_ptr<ID3D12Resource> m_texture;
  D3D12_CPU_DESCRIPTOR_HANDLE m_view{};
};

} // namespace Engine
