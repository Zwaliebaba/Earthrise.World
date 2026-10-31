#pragma once

#include "framework.h"

#include <unknwn.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <winrt/base.h>

#include "DescriptorHeap.h"
#include "GpuDevice.h"

#include <array>
#include <cstdint>

namespace Engine
{

/// The window's swap chain (ADR-011, decision 2): flip model with three back buffers, a frame
/// latency of two, R8G8B8A8_UNORM buffers drawn through sRGB render-target views, and tearing when
/// the system offers it and vsync is off.
class SwapChain
{
public:
  static constexpr std::uint32_t BUFFER_COUNT = 3;
  static constexpr std::uint32_t MAX_FRAME_LATENCY = 2;
  static constexpr DXGI_FORMAT BUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;
  static constexpr DXGI_FORMAT VIEW_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

  /// Sized to the window's client area. Throws winrt::hresult_error when DXGI refuses.
  SwapChain(GpuDevice& _device, HWND _window);

  /// Blocks until the swap chain will take another frame, or a second has passed.
  void WaitForFrame() const noexcept;

  /// Resizes the buffers. The caller first makes sure the GPU no longer uses them (a flush).
  void Resize(std::uint32_t _widthPixels, std::uint32_t _heightPixels);

  [[nodiscard]] ID3D12Resource* BackBuffer() const noexcept;
  [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE BackBufferView() const noexcept;
  [[nodiscard]] std::uint32_t WidthPixels() const noexcept;
  [[nodiscard]] std::uint32_t HeightPixels() const noexcept;

  /// Shows the back buffer: on the next vertical blank with _vsync, at once otherwise.
  void Present(bool _vsync);

private:
  void AcquireBuffers();

  ID3D12Device* m_device;
  winrt::com_ptr<IDXGISwapChain3> m_swapChain;
  winrt::handle m_frameLatencyWaitable;
  DescriptorHeap m_viewHeap;
  std::array<winrt::com_ptr<ID3D12Resource>, BUFFER_COUNT> m_buffers;
  std::array<D3D12_CPU_DESCRIPTOR_HANDLE, BUFFER_COUNT> m_views{};
  UINT m_flags = 0;
  std::uint32_t m_widthPixels = 0;
  std::uint32_t m_heightPixels = 0;
  bool m_tearingAllowed = false;
};

} // namespace Engine
