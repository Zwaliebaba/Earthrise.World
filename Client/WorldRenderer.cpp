#include "pch.h"

#include "WorldRenderer.h"

#include "Catalog.h"
#include "GlyphAtlas.h"
#include "Look.h"
#include "OrthographicCamera.h"
#include "Palette.h"
#include "Portraits.h"
#include "ResourceBarrier.h"
#include "SunProjection.h"

#include <tuple>

namespace Client
{

namespace
{

// The scene's target: 8 bits a channel through an sRGB view, as the back buffer, because the output is sRGB with no
// tone mapping (RendererLook.md). The upscale reads it through the same view.
constexpr DXGI_FORMAT SCENE_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
constexpr float PI = 3.14159265F;

float SrgbToLinear(float _value) noexcept
{
  return _value <= 0.04045F ? _value / 12.92F : std::pow((_value + 0.055F) / 1.055F, 2.4F);
}

DirectX::XMFLOAT3 LinearOf(std::uint32_t _srgb) noexcept
{
  const auto channel = [_srgb](int _shift) { return SrgbToLinear(static_cast<float>((_srgb >> _shift) & 0xffU) / 255.0F); };
  return {channel(16), channel(8), channel(0)};
}

/// A light's radiance as the voxel pass takes it: linear, times its intensity, over pi.
DirectX::XMFLOAT4 Radiance(std::uint32_t _srgb, float _intensity) noexcept
{
  const DirectX::XMFLOAT3 color = LinearOf(_srgb);
  const float scale = _intensity / PI;
  return {color.x * scale, color.y * scale, color.z * scale, 0.0F};
}

/// A linear colour with its HSL lightness scaled, hue and saturation kept: three.js r160's getHSL and setHSL, as the
/// voxel pass's jitter uses them (Shader/VoxelCommon.hlsli).
DirectX::XMFLOAT4 ScaledLightness(DirectX::XMFLOAT4 _color, float _scale) noexcept
{
  const float maxChannel = std::max({_color.x, _color.y, _color.z});
  const float minChannel = std::min({_color.x, _color.y, _color.z});
  const float lightness = std::clamp(0.5F * (maxChannel + minChannel) * _scale, 0.0F, 1.0F);
  if (maxChannel == minChannel)
  {
    return {lightness, lightness, lightness, _color.w};
  }
  const float delta = maxChannel - minChannel;
  const float oldLightness = 0.5F * (maxChannel + minChannel);
  const float saturation = oldLightness <= 0.5F ? delta / (maxChannel + minChannel) : delta / (2.0F - maxChannel - minChannel);
  float hue = 0.0F;
  if (maxChannel == _color.x)
  {
    hue = (_color.y - _color.z) / delta + (_color.y < _color.z ? 6.0F : 0.0F);
  }
  else if (maxChannel == _color.y)
  {
    hue = (_color.z - _color.x) / delta + 2.0F;
  }
  else
  {
    hue = (_color.x - _color.y) / delta + 4.0F;
  }
  hue /= 6.0F;
  const float p = lightness <= 0.5F ? lightness * (1.0F + saturation) : lightness + saturation - lightness * saturation;
  const float q = 2.0F * lightness - p;
  const auto channel = [p, q](float _t)
  {
    const float t = _t < 0.0F ? _t + 1.0F : (_t > 1.0F ? _t - 1.0F : _t);
    if (t < 1.0F / 6.0F)
    {
      return q + (p - q) * 6.0F * t;
    }
    if (t < 0.5F)
    {
      return p;
    }
    if (t < 2.0F / 3.0F)
    {
      return q + (p - q) * 6.0F * (2.0F / 3.0F - t);
    }
    return q;
  };
  return {channel(hue + 1.0F / 3.0F), channel(hue), channel(hue - 1.0F / 3.0F), _color.w};
}

std::array<DirectX::XMFLOAT4, Engine::VoxelPass::PALETTE_ENTRIES> LinearPalette() noexcept
{
  std::array<DirectX::XMFLOAT4, Engine::VoxelPass::PALETTE_ENTRIES> colors{};
  for (std::size_t index = 0; index < colors.size(); ++index)
  {
    const DirectX::XMFLOAT3 color = LinearOf(Content::PaletteColorSrgb(static_cast<std::uint8_t>(index)));
    colors[index] = {color.x, color.y, color.z, 1.0F};
  }
  // The two sides' faction colours, in entries beyond the materials'.
  for (const auto& [index, srgb] :
       {std::pair{ATTACKER_COLOR_INDEX, ATTACKER_COLOR_SRGB}, std::pair{DEFENDER_COLOR_INDEX, DEFENDER_COLOR_SRGB}})
  {
    const DirectX::XMFLOAT3 color = LinearOf(srgb);
    colors[index] = {color.x, color.y, color.z, 1.0F};
  }
  // Fords: water one world voxel deep, drawn lighter (RendererLook.md §3).
  colors[FORD_COLOR_INDEX] = ScaledLightness(colors[WATER_INDEX], FORD_LIGHTNESS_SCALE);
  // The overlays' entries, in Hud.md §2's colours (ADR-021, decision 2).
  for (const auto& [index, srgb] :
       {std::pair{OVERLAY_GOOD_INDEX, GOOD_SRGB}, std::pair{OVERLAY_BAD_INDEX, BAD_SRGB}, std::pair{OVERLAY_ACCENT_INDEX, ACCENT_SRGB},
        std::pair{OVERLAY_DIM_INDEX, DIM_SRGB}, std::pair{OVERLAY_TEXT_INDEX, TEXT_SRGB}})
  {
    const DirectX::XMFLOAT3 color = LinearOf(srgb);
    colors[index] = {color.x, color.y, color.z, 1.0F};
  }
  return colors;
}

/// Towards the sun, or towards it mirrored east to west, as the mirrored castle has it (MvpPlan.md, M4, input 28).
DirectX::XMVECTOR SunTowards(bool _mirrored) noexcept
{
  const float east = _mirrored ? -SUN_TOWARDS[0] : SUN_TOWARDS[0];
  return DirectX::XMVector3Normalize(DirectX::XMVectorSet(east, SUN_TOWARDS[1], SUN_TOWARDS[2], 0.0F));
}

/// The sun's frame for a shadow map _edgeTexels across: its view-projection, the ray away from the sun, and the map as
/// the viewport.
Engine::VoxelPass::FrameConstants MakeSunFrame(const DirectX::XMFLOAT4X4& _sunViewProjection, std::uint32_t _edgeTexels,
                                               bool _sunMirrored) noexcept
{
  Engine::VoxelPass::FrameConstants frame{};
  frame.viewProjection = _sunViewProjection;
  DirectX::XMStoreFloat4x4(&frame.inverseViewProjection, DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&_sunViewProjection)));
  DirectX::XMStoreFloat4(&frame.rayDirection, DirectX::XMVectorNegate(SunTowards(_sunMirrored)));
  const auto edge = static_cast<float>(_edgeTexels);
  frame.viewportPixels = {edge, edge, 1.0F / edge, 1.0F / edge};
  return frame;
}

/// The frame's constants for a target _widthPixels by _heightPixels.
Engine::VoxelPass::FrameConstants MakeFrame(const DirectX::XMFLOAT4X4& _viewProjection, const DirectX::XMFLOAT3& _forward,
                                            std::uint32_t _widthPixels, std::uint32_t _heightPixels,
                                            const DirectX::XMFLOAT4X4& _sunViewProjection, bool _sunMirrored)
{
  const auto width = static_cast<float>(_widthPixels);
  const auto height = static_cast<float>(_heightPixels);
  const DirectX::XMMATRIX viewProjection = DirectX::XMLoadFloat4x4(&_viewProjection);
  Engine::VoxelPass::FrameConstants frame{};
  frame.viewProjection = _viewProjection;
  DirectX::XMStoreFloat4x4(&frame.inverseViewProjection, DirectX::XMMatrixInverse(nullptr, viewProjection));
  DirectX::XMStoreFloat4(&frame.rayDirection, DirectX::XMVectorSet(_forward.x, _forward.y, _forward.z, 0.0F));
  frame.viewportPixels = {width, height, 1.0F / width, 1.0F / height};
  DirectX::XMStoreFloat4(&frame.lightDirection, SunTowards(_sunMirrored));
  frame.shadowViewProjection = _sunViewProjection;
  frame.shadowParameters = {SHADOW_NORMAL_OFFSET_METERS, SHADOW_DEPTH_BIAS, 1.0F / static_cast<float>(STATIC_SHADOW_TEXELS),
                            1.0F / static_cast<float>(DYNAMIC_SHADOW_TEXELS)};
  frame.sunRadiance = Radiance(SUN_SRGB, SUN_INTENSITY);
  frame.skyRadiance = Radiance(SKY_SRGB, HEMISPHERE_INTENSITY);
  frame.groundRadiance = Radiance(GROUND_SRGB, HEMISPHERE_INTENSITY);
  // The record's shade-table bit is Content's corner-shade set: 0 structure, 1 terrain.
  for (const Content::CornerShadeSet set : {Content::CornerShadeSet::Structure, Content::CornerShadeSet::Terrain})
  {
    const std::array<float, Content::CORNER_SHADE_LEVELS> shades = Content::CornerShades(set);
    frame.cornerShades.at(static_cast<std::size_t>(set)) = {shades[0], shades[1], shades[2], shades[3]};
  }
  return frame;
}

/// Sets the viewport and the scissor to the whole of a target _widthPixels by _heightPixels.
void SetViewport(ID3D12GraphicsCommandList* _commandList, std::uint32_t _widthPixels, std::uint32_t _heightPixels) noexcept
{
  const D3D12_VIEWPORT viewport{.TopLeftX = 0.0F,
                                .TopLeftY = 0.0F,
                                .Width = static_cast<float>(_widthPixels),
                                .Height = static_cast<float>(_heightPixels),
                                .MinDepth = 0.0F,
                                .MaxDepth = 1.0F};
  const D3D12_RECT scissor{.left = 0, .top = 0, .right = static_cast<LONG>(_widthPixels), .bottom = static_cast<LONG>(_heightPixels)};
  _commandList->RSSetViewports(1, &viewport);
  _commandList->RSSetScissorRects(1, &scissor);
}

/// Copies _value into the ring for this frame, at the alignment constant buffers need.
template <typename T> D3D12_GPU_VIRTUAL_ADDRESS UploadConstants(Engine::UploadRing& _ring, const T& _value)
{
  const std::optional<Engine::UploadRing::Allocation> allocation =
    _ring.Allocate(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
  if (!allocation)
  {
    throw std::runtime_error("the upload ring is full");
  }
  std::memcpy(allocation->bytes.data(), &_value, sizeof(T));
  return allocation->gpuAddress;
}

/// Copies _values into the ring for this frame, for a root shader-resource view; 0 for none.
template <typename T> D3D12_GPU_VIRTUAL_ADDRESS UploadArray(Engine::UploadRing& _ring, std::span<const T> _values)
{
  if (_values.empty())
  {
    return 0;
  }
  const std::optional<Engine::UploadRing::Allocation> allocation = _ring.Allocate(_values.size_bytes(), 256);
  if (!allocation)
  {
    throw std::runtime_error("the upload ring is full");
  }
  std::memcpy(allocation->bytes.data(), _values.data(), _values.size_bytes());
  return allocation->gpuAddress;
}

} // namespace

WorldRenderer::WorldRenderer(ID3D12Device* _device, ID3D12CommandQueue* _queue, const Desc& _desc, std::uint32_t _widthPixels,
                             std::uint32_t _heightPixels)
  : m_device(_device),
    m_sceneColor(_device, {.format = SCENE_FORMAT, .clearColor = CLEAR_COLOR}, _widthPixels, _heightPixels),
    m_depth(_device, _widthPixels, _heightPixels),
    m_pass(_device, {.colorFormat = SCENE_FORMAT, .depthFormat = Engine::DepthTarget::FORMAT}),
    m_lines(_device, {.colorFormat = SCENE_FORMAT, .depthFormat = Engine::DepthTarget::FORMAT}),
    m_staticShadow(_device, STATIC_SHADOW_TEXELS),
    m_dynamicShadow(_device, DYNAMIC_SHADOW_TEXELS),
    m_upscale(_device, {.targetFormat = _desc.targetFormat}),
    m_gpuTimer(_device, _queue, {.slotCount = Engine::FrameRing::FRAMES_IN_FLIGHT, .spanCount = SPAN_COUNT}),
    m_worldView(_device, 1.0F),
    m_interface(_device, {.targetFormat = _desc.targetFormat}),
    m_fontAtlas(_device),
    m_palette(LinearPalette()),
    m_portraitTarget(_device, {.format = SCENE_FORMAT, .clearColor = {0.0F, 0.0F, 0.0F, 0.0F}}, PORTRAIT_TEXELS, PORTRAIT_TEXELS),
    m_portraitDepth(_device, PORTRAIT_TEXELS, PORTRAIT_TEXELS)
{
  // The portraits' atlas, in the scene target's format, so that a copy of a portrait is a copy of its texels; drawn on
  // the first frame (DrawPortraits).
  const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_DEFAULT};
  const D3D12_RESOURCE_DESC atlasDesc{.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                      .Width = PORTRAIT_ATLAS_WIDTH_TEXELS,
                                      .Height = IMAGE_ATLAS_HEIGHT_TEXELS,
                                      .DepthOrArraySize = 1,
                                      .MipLevels = 1,
                                      .Format = SCENE_FORMAT,
                                      .SampleDesc = {.Count = 1},
                                      .Flags = D3D12_RESOURCE_FLAG_NONE};
  winrt::check_hresult(_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &atlasDesc, D3D12_RESOURCE_STATE_COPY_DEST,
                                                        nullptr, IID_PPV_ARGS(m_portraitAtlas.put())));
  m_interface.SetImageAtlas(m_portraitAtlas.get(), PORTRAIT_ATLAS_WIDTH_TEXELS, IMAGE_ATLAS_HEIGHT_TEXELS);
  m_pass.SetShadowMaps(m_staticShadow.Texture(), m_dynamicShadow.Texture());
  m_upscale.SetSource(m_sceneColor.Texture());
  // Content's line models (ADR-018, decision 2), in linear colour, the faction segments left to each instance.
  bool arrowFound = false;
  for (const Content::LineModelView& model : Content::LineModels())
  {
    if (model.id == "X2")
    {
      m_arrowModel = static_cast<std::uint32_t>(m_lineModels.size());
      arrowFound = true;
    }
    m_lineModels.push_back({.firstSegment = static_cast<std::uint32_t>(m_lineSegments.size()),
                            .segmentCount = static_cast<std::uint32_t>(model.segments.size()),
                            .drawnLengthMeters = static_cast<float>(model.drawnLengthMillimeters) / 1000.0F,
                            .dotBelowPixels = static_cast<float>(model.dotBelowPixels),
                            .dotWidthPixels = static_cast<float>(model.dotWidthPixels),
                            .reserved = {}});
    for (const Content::LineSegmentView& segment : model.segments)
    {
      const DirectX::XMFLOAT3 color = LinearOf(segment.colorSrgb);
      m_lineSegments.push_back({.fromMeters = static_cast<float>(segment.fromMillimeters) / 1000.0F,
                                .toMeters = static_cast<float>(segment.toMillimeters) / 1000.0F,
                                .color = {color.x, color.y, color.z, 1.0F},
                                .widthPixels = static_cast<float>(segment.widthPixels),
                                .factionColor = segment.factionColor ? 1U : 0U,
                                .tapers = segment.tapers ? 1U : 0U,
                                .reserved = 0.0F});
    }
  }
  if (!arrowFound)
  {
    throw std::runtime_error("Content has no line model X2, the arrow");
  }
  m_interface.SetAtlas(m_fontAtlas.Texture(), GlyphAtlas::WIDTH_TEXELS, GlyphAtlas::HEIGHT_TEXELS);
  m_sunMirrored = _desc.sunMirrored;
  // The sun's view over the whole world, its height included, for both shadow maps.
  DirectX::XMStoreFloat4x4(&m_sunViewProjection,
                           Engine::SunViewProjection(SunTowards(_desc.sunMirrored), DirectX::XMFLOAT3{0.0F, 0.0F, 0.0F},
                                                     DirectX::XMFLOAT3{_desc.widthMeters, _desc.heightMeters, _desc.depthMeters},
                                                     SHADOW_MARGIN_METERS));
}

void WorldRenderer::ResetWorld()
{
  m_worldView.Forget();
  m_worldChanged = true;
  m_pass.SetColumnShade(nullptr, 0, 0, 1.0F);
  m_fogMask = nullptr;
  m_fogTexels.clear();
  m_fogWidthCells = 0;
  m_fogDepthCells = 0;
}

void WorldRenderer::DrawPortraits(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring, D3D12_GPU_VIRTUAL_ADDRESS _palette)
{
  // Seen from the front, +z, a little to its right and above, framing the model's largest side with a margin.
  constexpr float ELEVATION_DEGREES_PORTRAIT = 20.0F;
  constexpr float AZIMUTH_DEGREES_PORTRAIT = 30.0F;
  constexpr float MARGIN = 1.15F;
  constexpr DirectX::XMFLOAT3 FAR_CORNER{-200.0F, 0.0F, -200.0F}; // outside every map, where no column is shaded
  for (std::uint32_t cell = 0; cell < PORTRAIT_MODELS.size(); ++cell)
  {
    const Content::ModelView* model = Content::FindModel(PORTRAIT_MODELS.at(cell));
    if (model == nullptr)
    {
      continue;
    }
    const Content::VoxelGrid& grid = model->grid;
    const float edge = static_cast<float>(grid.voxelEdgeMillimeters) / 1000.0F;
    const DirectX::XMFLOAT3 size{edge * static_cast<float>(grid.widthVoxels), edge * static_cast<float>(grid.heightVoxels),
                                 edge * static_cast<float>(grid.depthVoxels)};
    const Engine::OrthographicCamera camera(
      {.elevationRadians = DirectX::XMConvertToRadians(ELEVATION_DEGREES_PORTRAIT),
       .azimuthRadians = DirectX::XMConvertToRadians(AZIMUTH_DEGREES_PORTRAIT),
       .viewHeightMeters = MARGIN * std::max({size.x, size.y, size.z}),
       .minViewHeightMeters = 0.1F,
       .maxViewHeightMeters = 1000.0F,
       .rotationSeconds = ROTATION_SECONDS,
       .targetMeters = {FAR_CORNER.x + (0.5F * size.x), FAR_CORNER.y + (0.5F * size.y), FAR_CORNER.z + (0.5F * size.z)}});
    DirectX::XMFLOAT4X4 viewProjection{};
    DirectX::XMStoreFloat4x4(&viewProjection, camera.ViewProjection(1.0F));
    DirectX::XMFLOAT3 forward{};
    DirectX::XMStoreFloat3(&forward, camera.Forward());
    Engine::VoxelPass::FrameConstants frame =
      MakeFrame(viewProjection, forward, PORTRAIT_TEXELS, PORTRAIT_TEXELS, m_sunViewProjection, m_sunMirrored);
    frame.shadowParameters.w = 0.0F; // no shadow
    const std::vector<Engine::VoxelRecord> records = Engine::BuildVisibleRecords(
      {.paletteIndices = grid.indices, .xVoxels = grid.widthVoxels, .yVoxels = grid.heightVoxels, .zVoxels = grid.depthVoxels},
      [](std::int32_t, std::int32_t, std::int32_t) { return false; },
      {.palette = &m_worldView.Shading(), .originCells = {0, 0, 0}, .jitterCellShift = 0, .alsoSolid = nullptr});
    const D3D12_RESOURCE_BARRIER toTarget =
      Engine::TransitionBarrier(m_portraitTarget.Texture(), Engine::ColorTarget::IDLE_STATE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    _commandList->ResourceBarrier(1, &toTarget);
    const D3D12_CPU_DESCRIPTOR_HANDLE targetView = m_portraitTarget.View();
    const D3D12_CPU_DESCRIPTOR_HANDLE depthView = m_portraitDepth.View();
    _commandList->OMSetRenderTargets(1, &targetView, FALSE, &depthView);
    m_portraitTarget.Clear(_commandList);
    m_portraitDepth.Clear(_commandList);
    SetViewport(_commandList, PORTRAIT_TEXELS, PORTRAIT_TEXELS);
    m_pass.Begin(_commandList, UploadConstants(_ring, frame), _palette);
    if (!records.empty())
    {
      m_pass.Draw(_commandList, UploadArray(_ring, std::span<const Engine::VoxelRecord>(records)),
                  static_cast<std::uint32_t>(records.size()),
                  {.originMeters = FAR_CORNER,
                   .voxelEdgeMeters = edge,
                   .quarterTurns = 0,
                   .widthCells = grid.widthVoxels,
                   .depthCells = grid.depthVoxels,
                   .swapFromIndex = FACTION_PALETTE_INDEX,
                   .swapToIndex = ATTACKER_COLOR_INDEX});
    }
    const D3D12_RESOURCE_BARRIER toCopy =
      Engine::TransitionBarrier(m_portraitTarget.Texture(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    _commandList->ResourceBarrier(1, &toCopy);
    // The copy locations' unions are filled by member: designators cannot name an anonymous union's.
    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = m_portraitTarget.Texture();
    source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = m_portraitAtlas.get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.SubresourceIndex = 0;
    _commandList->CopyTextureRegion(&destination, cell * PORTRAIT_TEXELS, 0, 0, &source, nullptr);
    const D3D12_RESOURCE_BARRIER toIdle =
      Engine::TransitionBarrier(m_portraitTarget.Texture(), D3D12_RESOURCE_STATE_COPY_SOURCE, Engine::ColorTarget::IDLE_STATE);
    _commandList->ResourceBarrier(1, &toIdle);
  }
  const D3D12_RESOURCE_BARRIER toRead =
    Engine::TransitionBarrier(m_portraitAtlas.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  _commandList->ResourceBarrier(1, &toRead);
}

void WorldRenderer::UploadMinimap(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring, const Frame& _frame)
{
  const std::uint32_t side = std::min(_frame.minimapSidePixels, MINIMAP_MAX_TEXELS);
  if (_frame.minimapBase.empty() || side == 0 ||
      _frame.minimapBase.size() < static_cast<std::size_t>(_frame.minimapSidePixels) * _frame.minimapSidePixels)
  {
    return;
  }
  constexpr std::uint32_t TEXEL_BYTES = 4;
  const std::uint32_t rowPitch =
    ((side * TEXEL_BYTES) + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
  const std::optional<Engine::UploadRing::Allocation> allocation =
    _ring.Allocate(static_cast<std::uint64_t>(rowPitch) * side, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
  if (!allocation)
  {
    throw std::runtime_error("the upload ring has no room for the minimap");
  }
  for (std::uint32_t row = 0; row < side; ++row)
  {
    std::memcpy(allocation->bytes.data() + (static_cast<std::size_t>(row) * rowPitch),
                _frame.minimapBase.data() + (static_cast<std::size_t>(row) * _frame.minimapSidePixels),
                static_cast<std::size_t>(side) * TEXEL_BYTES);
  }
  // The copy locations' unions are filled by member: designators cannot name an anonymous union's.
  D3D12_TEXTURE_COPY_LOCATION source{};
  source.pResource = allocation->buffer;
  source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  source.PlacedFootprint.Offset = allocation->offsetBytes;
  source.PlacedFootprint.Footprint = {.Format = SCENE_FORMAT, .Width = side, .Height = side, .Depth = 1, .RowPitch = rowPitch};
  D3D12_TEXTURE_COPY_LOCATION destination{};
  destination.pResource = m_portraitAtlas.get();
  destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  destination.SubresourceIndex = 0;
  const D3D12_RESOURCE_BARRIER toCopy =
    Engine::TransitionBarrier(m_portraitAtlas.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
  _commandList->ResourceBarrier(1, &toCopy);
  _commandList->CopyTextureRegion(&destination, 0, MINIMAP_ATLAS_TOP_TEXELS, 0, &source, nullptr);
  const D3D12_RESOURCE_BARRIER toRead =
    Engine::TransitionBarrier(m_portraitAtlas.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  _commandList->ResourceBarrier(1, &toRead);
}

void WorldRenderer::UpdateFog(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring, const Frame& _frame)
{
  if (_frame.fogMask.empty())
  {
    return; // a world without fog; one with fog has it every tick, and only ResetWorld takes it away
  }
  const auto width = static_cast<std::size_t>(std::max(0, _frame.fogWidthCells));
  const auto depth = static_cast<std::size_t>(std::max(0, _frame.fogDepthCells));
  if (_frame.fogMask.size() != width * depth)
  {
    throw std::invalid_argument("the fog's mask does not hold its cells");
  }
  if (!m_fogMask)
  {
    const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_DEFAULT};
    const D3D12_RESOURCE_DESC desc{.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                   .Width = width,
                                   .Height = static_cast<UINT>(depth),
                                   .DepthOrArraySize = 1,
                                   .MipLevels = 1,
                                   .Format = DXGI_FORMAT_R8_UNORM,
                                   .SampleDesc = {.Count = 1},
                                   .Flags = D3D12_RESOURCE_FLAG_NONE};
    winrt::check_hresult(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
                                                           nullptr, IID_PPV_ARGS(m_fogMask.put())));
    m_fogWidthCells = _frame.fogWidthCells;
    m_fogDepthCells = _frame.fogDepthCells;
    m_pass.SetColumnShade(m_fogMask.get(), m_fogWidthCells, m_fogDepthCells, FOG_SHADE);
  }
  else if (_frame.fogWidthCells != m_fogWidthCells || _frame.fogDepthCells != m_fogDepthCells)
  {
    throw std::logic_error("the fog's mask changed its size without ResetWorld");
  }
  else if (std::ranges::equal(_frame.fogMask, m_fogTexels))
  {
    return;
  }
  else
  {
    // Frames before this one on the queue read it; the barrier waits for them.
    const D3D12_RESOURCE_BARRIER toCopy =
      Engine::TransitionBarrier(m_fogMask.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    _commandList->ResourceBarrier(1, &toCopy);
  }
  const D3D12_RESOURCE_DESC desc = m_fogMask->GetDesc();
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  UINT64 totalBytes = 0;
  m_device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &totalBytes);
  const std::optional<Engine::UploadRing::Allocation> allocation = _ring.Allocate(totalBytes, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
  if (!allocation)
  {
    throw std::runtime_error("the upload ring has no room for the fog's mask");
  }
  for (std::size_t row = 0; row < depth; ++row)
  {
    std::memcpy(allocation->bytes.data() + (row * footprint.Footprint.RowPitch), _frame.fogMask.data() + (row * width), width);
  }
  footprint.Offset = allocation->offsetBytes;
  // The copy locations' unions are filled by member: designators cannot name an anonymous union's.
  D3D12_TEXTURE_COPY_LOCATION source{};
  source.pResource = allocation->buffer;
  source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  source.PlacedFootprint = footprint;
  D3D12_TEXTURE_COPY_LOCATION destination{};
  destination.pResource = m_fogMask.get();
  destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  destination.SubresourceIndex = 0;
  _commandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
  const D3D12_RESOURCE_BARRIER toRead =
    Engine::TransitionBarrier(m_fogMask.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  _commandList->ResourceBarrier(1, &toRead);
  m_fogTexels.assign(_frame.fogMask.begin(), _frame.fogMask.end());
}

void WorldRenderer::Resize(std::uint32_t _widthPixels, std::uint32_t _heightPixels)
{
  m_sceneColor.Resize(_widthPixels, _heightPixels);
  m_depth.Resize(m_sceneColor.WidthPixels(), m_sceneColor.HeightPixels());
  m_upscale.SetSource(m_sceneColor.Texture());
}

void WorldRenderer::Draw(const Frame& _frame)
{
  ID3D12GraphicsCommandList* commandList = _frame.commandList;
  Engine::UploadRing& ring = *_frame.ring;
  m_fontAtlas.Upload(commandList, ring);
  UpdateFog(commandList, ring, _frame);
  const std::uint32_t slot = _frame.slot;
  // FrameRing::Begin waited for this slot's last frame, so its timestamps are back.
  if (m_timed.at(slot))
  {
    const std::vector<double> spans = m_gpuTimer.ReadMilliseconds(slot);
    m_voxelPassMilliseconds = spans[VOXEL_PASS_SPAN];
    m_linePassMilliseconds = spans[LINE_PASS_SPAN];
  }
  if (m_worldView.Hide(commandList, ring, _frame.hiddenVoxels))
  {
    m_worldChanged = true;
  }
  if (!_frame.snapshots.empty())
  {
    m_worldView.TakeSnapshots(commandList, ring, _frame.snapshots);
    m_worldChanged = true;
  }
  const float modelVoxelPixels = MODEL_VOXEL_METERS * static_cast<float>(m_sceneColor.HeightPixels()) / _frame.viewHeightMeters;
  m_worldView.PlaceModels(ring, std::vector<ModelPlacement>(_frame.placements.begin(), _frame.placements.end()), modelVoxelPixels);
  const D3D12_GPU_VIRTUAL_ADDRESS frameConstants =
    UploadConstants(ring, MakeFrame(_frame.viewProjection, _frame.forward, m_sceneColor.WidthPixels(), m_sceneColor.HeightPixels(),
                                    m_sunViewProjection, m_sunMirrored));
  const D3D12_GPU_VIRTUAL_ADDRESS paletteConstants = UploadConstants(ring, m_palette);

  // The sun's shadow maps: the world's when it changed, the models' every frame (ADR-011, decision 9).
  m_gpuTimer.Begin(commandList, slot, STATIC_SHADOW_SPAN);
  if (m_worldChanged || !m_staticShadow.Readable())
  {
    const D3D12_GPU_VIRTUAL_ADDRESS staticSunFrame =
      UploadConstants(ring, MakeSunFrame(m_sunViewProjection, STATIC_SHADOW_TEXELS, m_sunMirrored));
    m_staticShadow.BeginDrawing(commandList);
    m_pass.BeginShadow(commandList, staticSunFrame, paletteConstants);
    m_worldView.DrawChunks(commandList, m_pass);
    m_staticShadow.EndDrawing(commandList);
    m_worldChanged = false;
  }
  m_gpuTimer.End(commandList, slot, STATIC_SHADOW_SPAN);
  m_gpuTimer.Begin(commandList, slot, DYNAMIC_SHADOW_SPAN);
  const D3D12_GPU_VIRTUAL_ADDRESS dynamicSunFrame =
    UploadConstants(ring, MakeSunFrame(m_sunViewProjection, DYNAMIC_SHADOW_TEXELS, m_sunMirrored));
  m_dynamicShadow.BeginDrawing(commandList);
  m_pass.BeginShadow(commandList, dynamicSunFrame, paletteConstants);
  m_worldView.DrawModels(commandList, m_pass);
  const D3D12_GPU_VIRTUAL_ADDRESS debrisAddress = UploadArray(ring, _frame.debris);
  const auto debrisCount = static_cast<std::uint32_t>(_frame.debris.size());
  if (debrisCount != 0)
  {
    m_pass.BeginOrientedShadow(commandList, dynamicSunFrame, paletteConstants);
    m_pass.DrawOriented(commandList, debrisAddress, debrisCount);
  }
  m_dynamicShadow.EndDrawing(commandList);
  m_gpuTimer.End(commandList, slot, DYNAMIC_SHADOW_SPAN);
  if (!m_portraitsDrawn)
  {
    DrawPortraits(commandList, ring, paletteConstants);
    m_portraitsDrawn = true;
  }
  UploadMinimap(commandList, ring, _frame);

  // The scene into its own target.
  const D3D12_RESOURCE_BARRIER toScene =
    Engine::TransitionBarrier(m_sceneColor.Texture(), Engine::ColorTarget::IDLE_STATE, D3D12_RESOURCE_STATE_RENDER_TARGET);
  commandList->ResourceBarrier(1, &toScene);
  const D3D12_CPU_DESCRIPTOR_HANDLE sceneView = m_sceneColor.View();
  const D3D12_CPU_DESCRIPTOR_HANDLE depthView = m_depth.View();
  commandList->OMSetRenderTargets(1, &sceneView, FALSE, &depthView);
  m_sceneColor.Clear(commandList);
  m_depth.Clear(commandList);
  SetViewport(commandList, m_sceneColor.WidthPixels(), m_sceneColor.HeightPixels());
  m_gpuTimer.Begin(commandList, slot, VOXEL_PASS_SPAN);
  m_pass.Begin(commandList, frameConstants, paletteConstants);
  m_worldView.DrawChunks(commandList, m_pass);
  // The silhouettes after the world and before the models (ADR-021, decision 6, amended): tested against the world's
  // depth alone, each shows only where the world stands in front of its model, which then covers it where it is seen.
  if (m_worldView.HasSilhouettes())
  {
    m_pass.BeginSilhouette(commandList, frameConstants, paletteConstants, SILHOUETTE_ALPHA);
    m_worldView.DrawSilhouettes(commandList, m_pass);
    m_pass.Begin(commandList, frameConstants, paletteConstants);
  }
  m_worldView.DrawModels(commandList, m_pass);
  if (debrisCount != 0)
  {
    m_pass.BeginOriented(commandList, frameConstants, paletteConstants);
    m_pass.DrawOriented(commandList, debrisAddress, debrisCount);
  }
  m_gpuTimer.End(commandList, slot, VOXEL_PASS_SPAN);
  // The arrows, after the voxel pass, into its target and against its depth (ADR-018, decision 3), each in its
  // shooter's faction colour.
  m_gpuTimer.Begin(commandList, slot, LINE_PASS_SPAN);
  if (!_frame.arrows.empty())
  {
    std::vector<Engine::LineInstance> instances;
    instances.reserve(_frame.arrows.size());
    for (const ArrowDrawn& arrow : _frame.arrows)
    {
      instances.push_back({.positionMeters = arrow.middleMeters,
                           .lineModel = m_arrowModel,
                           .direction = arrow.direction,
                           .reserved = 0.0F,
                           .factionColor = m_palette.at(arrow.sideColorIndex)});
    }
    m_lines.Begin(commandList, frameConstants, UploadArray(ring, std::span<const Engine::LineModelRecord>(m_lineModels)),
                  UploadArray(ring, std::span<const Engine::LineSegmentRecord>(m_lineSegments)));
    m_lines.Draw(commandList, UploadArray(ring, std::span<const Engine::LineInstance>(instances)),
                 static_cast<std::uint32_t>(instances.size()));
  }
  m_gpuTimer.End(commandList, slot, LINE_PASS_SPAN);
  DrawOverlays(commandList, ring, frameConstants, paletteConstants, _frame.overlays);

  // Then scaled to the back buffer, which the triangle covers whole, so it needs no clear.
  const std::array<D3D12_RESOURCE_BARRIER, 2> toUpscale = {
    Engine::TransitionBarrier(m_sceneColor.Texture(), D3D12_RESOURCE_STATE_RENDER_TARGET, Engine::ColorTarget::IDLE_STATE),
    Engine::TransitionBarrier(_frame.backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)};
  commandList->ResourceBarrier(static_cast<UINT>(toUpscale.size()), toUpscale.data());
  commandList->OMSetRenderTargets(1, &_frame.backBufferView, FALSE, nullptr);
  SetViewport(commandList, _frame.backWidthPixels, _frame.backHeightPixels);
  m_upscale.Draw(commandList);
  // The interface over it, at the back buffer's size (ADR-019, decision 3).
  if (!_frame.interfaceQuads.empty())
  {
    std::vector<Engine::InterfaceQuad> quads;
    quads.reserve(_frame.interfaceQuads.size());
    for (const ScreenQuad& quad : _frame.interfaceQuads)
    {
      quads.push_back({.xPixels = quad.xPixels,
                       .yPixels = quad.yPixels,
                       .widthPixels = quad.widthPixels,
                       .heightPixels = quad.heightPixels,
                       .u0 = quad.u0,
                       .v0 = quad.v0,
                       .u1 = quad.u1,
                       .v1 = quad.v1,
                       .color = {quad.color[0], quad.color[1], quad.color[2], quad.color[3]},
                       .textured = quad.image ? 2U : (quad.textured ? 1U : 0U),
                       .reserved = {}});
    }
    m_interface.Draw(commandList, UploadArray(ring, std::span<const Engine::InterfaceQuad>(quads)),
                     static_cast<std::uint32_t>(quads.size()), _frame.backWidthPixels, _frame.backHeightPixels);
  }
  const D3D12_RESOURCE_BARRIER toPresent =
    Engine::TransitionBarrier(_frame.backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  commandList->ResourceBarrier(1, &toPresent);
  m_gpuTimer.Resolve(commandList, slot);
  m_timed.at(slot) = true;
}

void WorldRenderer::DrawOverlays(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring,
                                 D3D12_GPU_VIRTUAL_ADDRESS _frameConstants, D3D12_GPU_VIRTUAL_ADDRESS _palette,
                                 std::span<const OverlayLayer> _layers) const
{
  // A batch holds cells 0 to 63 on each axis, so the boxes go into batches of 64-voxel blocks of the world, each with
  // its block's corner as its origin. The block's corner and the cell inside it are whole meters, so a box on a world
  // voxel is the same box, at the same depth, as the chunk's (ADR-021, decision 1).
  constexpr std::int32_t BLOCK_VOXELS = static_cast<std::int32_t>(Engine::VOXEL_RECORD_MAX_CELL) + 1;
  using Block = std::tuple<std::int32_t, std::int32_t, std::int32_t>;
  const auto blockOf = [](std::int32_t _voxels) { return _voxels >= 0 ? _voxels / BLOCK_VOXELS : ((_voxels + 1) / BLOCK_VOXELS) - 1; };
  for (const OverlayLayer& layer : _layers)
  {
    if (layer.boxes.empty())
    {
      continue;
    }
    std::map<Block, std::vector<Engine::VoxelRecord>> blocks;
    for (const OverlayBox& box : layer.boxes)
    {
      const Block block{blockOf(box.cellVoxels.xVoxels), blockOf(box.cellVoxels.yVoxels), blockOf(box.cellVoxels.zVoxels)};
      blocks[block].push_back(Engine::MakeVoxelRecord(
        static_cast<std::uint32_t>(box.cellVoxels.xVoxels - (std::get<0>(block) * BLOCK_VOXELS)),
        static_cast<std::uint32_t>(box.cellVoxels.yVoxels - (std::get<1>(block) * BLOCK_VOXELS)),
        static_cast<std::uint32_t>(box.cellVoxels.zVoxels - (std::get<2>(block) * BLOCK_VOXELS)), box.paletteIndex));
    }
    m_pass.BeginOverlay(_commandList, _frameConstants, _palette, layer.alpha);
    for (const auto& [block, records] : blocks)
    {
      const auto [x, y, z] = block;
      m_pass.Draw(
        _commandList, UploadArray(_ring, std::span<const Engine::VoxelRecord>(records)), static_cast<std::uint32_t>(records.size()),
        {.originMeters = {static_cast<float>(x * BLOCK_VOXELS), static_cast<float>(y * BLOCK_VOXELS), static_cast<float>(z * BLOCK_VOXELS)},
         .voxelEdgeMeters = 1.0F,
         .quarterTurns = 0,
         .widthCells = static_cast<std::uint32_t>(BLOCK_VOXELS),
         .depthCells = static_cast<std::uint32_t>(BLOCK_VOXELS),
         .swapFromIndex = 0,
         .swapToIndex = 0});
    }
  }
}

double WorldRenderer::VoxelPassMilliseconds() const noexcept
{
  return m_voxelPassMilliseconds;
}

double WorldRenderer::LinePassMilliseconds() const noexcept
{
  return m_linePassMilliseconds;
}

std::uint64_t WorldRenderer::RecordsDrawn() const noexcept
{
  return m_worldView.RecordsDrawn();
}

void WorldRenderer::Retire(std::uint64_t _fenceValue)
{
  m_worldView.Retire(_fenceValue);
}

void WorldRenderer::Release(std::uint64_t _completedFenceValue)
{
  m_worldView.Release(_completedFenceValue);
}

} // namespace Client
