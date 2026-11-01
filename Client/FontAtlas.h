#pragma once

#include "framework.h"

#include <unknwn.h>

#include <d3d12.h>
#include <winrt/base.h>

#include "UploadRing.h"

namespace Client
{

/// The glyph atlas on the GPU (ADR-019, decision 2): GlyphAtlas's 160 x 80 texels in an R8_UNORM texture, built once
/// and uploaded through the upload ring with the first frame, and never changed.
class FontAtlas
{
public:
  /// Creates the texture, waiting for its upload. Throws winrt::hresult_error when the device refuses.
  explicit FontAtlas(ID3D12Device* _device);

  /// Records the upload, the first time only: the texels copied in from the ring, and the texture left readable by a
  /// pixel shader. Throws std::runtime_error when the ring has no room.
  void Upload(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring);

  [[nodiscard]] ID3D12Resource* Texture() const noexcept;

private:
  ID3D12Device* m_device;
  winrt::com_ptr<ID3D12Resource> m_texture;
  bool m_uploaded = false;
};

} // namespace Client
