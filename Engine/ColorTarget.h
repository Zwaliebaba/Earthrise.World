#pragma once

#include "framework.h"

#include <unknwn.h>

#include <d3d12.h>
#include <winrt/base.h>

#include "DescriptorHeap.h"

#include <array>
#include <cstdint>

namespace Engine
{

/// A colour texture that one pass draws into and a later pass reads, such as the scene at its
/// render scale before the upscale (ADR-011, decision 3). Between frames it waits in IDLE_STATE,
/// readable by a pixel shader; the drawing pass moves it to a render target and back.
class ColorTarget
{
public:
  static constexpr D3D12_RESOURCE_STATES IDLE_STATE = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

  struct Desc
  {
    DXGI_FORMAT format;              // the texture's and both of its views', such as R8G8B8A8_UNORM_SRGB
    std::array<float, 4> clearColor; // what Clear writes, also given to the device as the fast clear
  };

  /// Throws winrt::hresult_error when the device refuses.
  ColorTarget(ID3D12Device* _device, const Desc& _desc, std::uint32_t _widthPixels, std::uint32_t _heightPixels);

  /// Remakes the texture at a new size, in IDLE_STATE. The caller first makes sure the GPU no
  /// longer uses it, and gives a reader's shader resource view the new texture.
  void Resize(std::uint32_t _widthPixels, std::uint32_t _heightPixels);

  [[nodiscard]] ID3D12Resource* Texture() const noexcept;
  [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE View() const noexcept;
  [[nodiscard]] std::uint32_t WidthPixels() const noexcept;
  [[nodiscard]] std::uint32_t HeightPixels() const noexcept;

  /// Records the clear to the Desc's colour. The texture is a render target at the time.
  void Clear(ID3D12GraphicsCommandList* _commandList) const noexcept;

private:
  void Create(std::uint32_t _widthPixels, std::uint32_t _heightPixels);

  ID3D12Device* m_device;
  Desc m_desc;
  DescriptorHeap m_viewHeap;
  winrt::com_ptr<ID3D12Resource> m_texture;
  D3D12_CPU_DESCRIPTOR_HANDLE m_view{};
  std::uint32_t m_widthPixels = 0;
  std::uint32_t m_heightPixels = 0;
};

} // namespace Engine
