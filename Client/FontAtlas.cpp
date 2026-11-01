#include "pch.h"

#include "FontAtlas.h"

#include "GlyphAtlas.h"
#include "ResourceBarrier.h"

namespace Client
{

FontAtlas::FontAtlas(ID3D12Device* _device)
  : m_device(_device)
{
  const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_DEFAULT};
  const D3D12_RESOURCE_DESC desc{.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                 .Width = GlyphAtlas::WIDTH_TEXELS,
                                 .Height = GlyphAtlas::HEIGHT_TEXELS,
                                 .DepthOrArraySize = 1,
                                 .MipLevels = 1,
                                 .Format = DXGI_FORMAT_R8_UNORM,
                                 .SampleDesc = {.Count = 1},
                                 .Flags = D3D12_RESOURCE_FLAG_NONE};
  winrt::check_hresult(_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
                                                        nullptr, IID_PPV_ARGS(m_texture.put())));
}

void FontAtlas::Upload(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring)
{
  if (m_uploaded)
  {
    return;
  }
  const D3D12_RESOURCE_DESC desc = m_texture->GetDesc();
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  UINT64 totalBytes = 0;
  m_device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &totalBytes);
  const std::optional<Engine::UploadRing::Allocation> allocation = _ring.Allocate(totalBytes, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
  if (!allocation)
  {
    throw std::runtime_error("the upload ring has no room for the glyph atlas");
  }
  const std::vector<std::uint8_t> texels = GlyphAtlas::Texels();
  for (std::uint32_t row = 0; row < GlyphAtlas::HEIGHT_TEXELS; ++row)
  {
    std::memcpy(allocation->bytes.data() + (static_cast<std::size_t>(row) * footprint.Footprint.RowPitch),
                texels.data() + (static_cast<std::size_t>(row) * GlyphAtlas::WIDTH_TEXELS), GlyphAtlas::WIDTH_TEXELS);
  }
  footprint.Offset = allocation->offsetBytes;
  // The copy locations' unions are filled by member: designators cannot name an anonymous union's.
  D3D12_TEXTURE_COPY_LOCATION source{};
  source.pResource = allocation->buffer;
  source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  source.PlacedFootprint = footprint;
  D3D12_TEXTURE_COPY_LOCATION destination{};
  destination.pResource = m_texture.get();
  destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  destination.SubresourceIndex = 0;
  _commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
  const D3D12_RESOURCE_BARRIER toRead =
    Engine::TransitionBarrier(m_texture.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  _commandList->ResourceBarrier(1, &toRead);
  m_uploaded = true;
}

ID3D12Resource* FontAtlas::Texture() const noexcept
{
  return m_texture.get();
}

} // namespace Client
