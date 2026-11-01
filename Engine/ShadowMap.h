#pragma once

#include "framework.h"

#include <unknwn.h>

#include <d3d12.h>
#include <winrt/base.h>

#include "DescriptorHeap.h"

#include <cstdint>

namespace Engine
{

/// A square depth map the sun's shadow passes draw into and the voxel pass reads (ADR-011,
/// decision 9): 32-bit, written through a D32_FLOAT view and read as R32_FLOAT, cleared to 0, the
/// far plane, for reversed Z (ADR-011, decision 4). It keeps track of its own state: it starts
/// writable, BeginDrawing makes it writable and clears it, and EndDrawing makes it readable by a
/// pixel shader, where it rests.
class ShadowMap
{
public:
  static constexpr DXGI_FORMAT DEPTH_FORMAT = DXGI_FORMAT_D32_FLOAT;
  static constexpr DXGI_FORMAT READ_FORMAT = DXGI_FORMAT_R32_FLOAT;

  /// Throws winrt::hresult_error when the device refuses, and std::invalid_argument for an edge of 0.
  ShadowMap(ID3D12Device* _device, std::uint32_t _edgeTexels);

  /// Records what makes the map writable, its clear, and the viewport and scissor over it, and
  /// sets it as the only target: no colour, its depth.
  void BeginDrawing(ID3D12GraphicsCommandList* _commandList);

  /// Records what makes the map readable by a pixel shader.
  void EndDrawing(ID3D12GraphicsCommandList* _commandList);

  [[nodiscard]] ID3D12Resource* Texture() const noexcept;
  [[nodiscard]] std::uint32_t EdgeTexels() const noexcept;
  /// Whether the map is readable now, as the commands recorded so far leave it.
  [[nodiscard]] bool Readable() const noexcept;

private:
  winrt::com_ptr<ID3D12Resource> m_texture;
  DescriptorHeap m_viewHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE m_view{};
  std::uint32_t m_edgeTexels;
  bool m_readable = false;
};

} // namespace Engine
