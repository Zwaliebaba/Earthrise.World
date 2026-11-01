#include "pch.h"

#include "SwapChain.h"

namespace Engine
{

SwapChain::SwapChain(GpuDevice& _device, HWND _window)
  : m_device(_device.Device()),
    m_viewHeap(_device.Device(), {.type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV, .capacityDescriptors = BUFFER_COUNT, .shaderVisible = false}),
    m_tearingAllowed(_device.TearingSupported())
{
  m_flags = static_cast<UINT>(DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) |
            (m_tearingAllowed ? static_cast<UINT>(DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) : 0U);
  // A width and height of zero take the window's client area.
  const DXGI_SWAP_CHAIN_DESC1 swapChainDesc{.Width = 0,
                                            .Height = 0,
                                            .Format = BUFFER_FORMAT,
                                            .Stereo = FALSE,
                                            .SampleDesc = {.Count = 1, .Quality = 0},
                                            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
                                            .BufferCount = BUFFER_COUNT,
                                            .Scaling = DXGI_SCALING_STRETCH,
                                            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
                                            .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
                                            .Flags = m_flags};
  winrt::com_ptr<IDXGISwapChain1> swapChain;
  winrt::check_hresult(
    _device.Factory()->CreateSwapChainForHwnd(_device.DirectQueue().Queue(), _window, &swapChainDesc, nullptr, nullptr, swapChain.put()));
  m_swapChain = swapChain.as<IDXGISwapChain3>();
  // Full screen is a borderless window (ADR-011, decision 1), so DXGI's Alt+Enter stays out of it.
  winrt::check_hresult(_device.Factory()->MakeWindowAssociation(_window, DXGI_MWA_NO_ALT_ENTER));
  winrt::check_hresult(m_swapChain->SetMaximumFrameLatency(MAX_FRAME_LATENCY));
  m_frameLatencyWaitable.attach(m_swapChain->GetFrameLatencyWaitableObject());
  AcquireBuffers();
}

void SwapChain::WaitForFrame() const noexcept
{
  WaitForSingleObjectEx(m_frameLatencyWaitable.get(), 1000, TRUE);
}

void SwapChain::Resize(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  for (winrt::com_ptr<ID3D12Resource>& buffer : m_buffers)
  {
    buffer = nullptr;
  }
  winrt::check_hresult(m_swapChain->ResizeBuffers(BUFFER_COUNT, _widthPixels, _heightPixels, BUFFER_FORMAT, m_flags));
  AcquireBuffers();
}

void SwapChain::AcquireBuffers()
{
  DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
  winrt::check_hresult(m_swapChain->GetDesc1(&swapChainDesc));
  m_widthPixels = swapChainDesc.Width;
  m_heightPixels = swapChainDesc.Height;

  // The buffers are UNORM and the views sRGB, because a flip-model swap chain cannot be sRGB itself.
  const D3D12_RENDER_TARGET_VIEW_DESC viewDesc{.Format = VIEW_FORMAT, .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D};
  m_viewHeap.Reset();
  for (UINT index = 0; index < BUFFER_COUNT; ++index)
  {
    winrt::check_hresult(m_swapChain->GetBuffer(index, IID_PPV_ARGS(m_buffers[index].put())));
    m_views[index] = m_viewHeap.Allocate().cpu;
    m_device->CreateRenderTargetView(m_buffers[index].get(), &viewDesc, m_views[index]);
  }
}

ID3D12Resource* SwapChain::BackBuffer() const noexcept
{
  return m_buffers[m_swapChain->GetCurrentBackBufferIndex()].get();
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::BackBufferView() const noexcept
{
  return m_views[m_swapChain->GetCurrentBackBufferIndex()];
}

std::uint32_t SwapChain::WidthPixels() const noexcept
{
  return m_widthPixels;
}

std::uint32_t SwapChain::HeightPixels() const noexcept
{
  return m_heightPixels;
}

void SwapChain::Present(bool _vsync)
{
  const UINT syncInterval = _vsync ? 1U : 0U;
  const UINT flags = (!_vsync && m_tearingAllowed) ? static_cast<UINT>(DXGI_PRESENT_ALLOW_TEARING) : 0U;
  winrt::check_hresult(m_swapChain->Present(syncInterval, flags));
}

std::uint32_t SwapChain::LastPresentCount() const
{
  UINT count = 0;
  winrt::check_hresult(m_swapChain->GetLastPresentCount(&count));
  return count;
}

std::optional<DXGI_FRAME_STATISTICS> SwapChain::FrameStatistics() const
{
  DXGI_FRAME_STATISTICS statistics{};
  // A failure here means only that there are no statistics yet, such as DXGI_ERROR_FRAME_STATISTICS_DISJOINT.
  if (FAILED(m_swapChain->GetFrameStatistics(&statistics)))
  {
    return std::nullopt;
  }
  return statistics;
}

} // namespace Engine
