#include "pch.h"

#include "WorldRenderer.h"

#include "GlyphAtlas.h"
#include "Look.h"
#include "Palette.h"
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

DirectX::XMVECTOR SunTowards() noexcept
{
  return DirectX::XMVector3Normalize(DirectX::XMVectorSet(SUN_TOWARDS[0], SUN_TOWARDS[1], SUN_TOWARDS[2], 0.0F));
}

/// The sun's frame for a shadow map _edgeTexels across: its view-projection, the ray away from the sun, and the map as
/// the viewport.
Engine::VoxelPass::FrameConstants MakeSunFrame(const DirectX::XMFLOAT4X4& _sunViewProjection, std::uint32_t _edgeTexels) noexcept
{
  Engine::VoxelPass::FrameConstants frame{};
  frame.viewProjection = _sunViewProjection;
  DirectX::XMStoreFloat4x4(&frame.inverseViewProjection, DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&_sunViewProjection)));
  DirectX::XMStoreFloat4(&frame.rayDirection, DirectX::XMVectorNegate(SunTowards()));
  const auto edge = static_cast<float>(_edgeTexels);
  frame.viewportPixels = {edge, edge, 1.0F / edge, 1.0F / edge};
  return frame;
}

/// The frame's constants for a target _widthPixels by _heightPixels.
Engine::VoxelPass::FrameConstants MakeFrame(const DirectX::XMFLOAT4X4& _viewProjection, const DirectX::XMFLOAT3& _forward,
                                            std::uint32_t _widthPixels, std::uint32_t _heightPixels,
                                            const DirectX::XMFLOAT4X4& _sunViewProjection)
{
  const auto width = static_cast<float>(_widthPixels);
  const auto height = static_cast<float>(_heightPixels);
  const DirectX::XMMATRIX viewProjection = DirectX::XMLoadFloat4x4(&_viewProjection);
  Engine::VoxelPass::FrameConstants frame{};
  frame.viewProjection = _viewProjection;
  DirectX::XMStoreFloat4x4(&frame.inverseViewProjection, DirectX::XMMatrixInverse(nullptr, viewProjection));
  DirectX::XMStoreFloat4(&frame.rayDirection, DirectX::XMVectorSet(_forward.x, _forward.y, _forward.z, 0.0F));
  frame.viewportPixels = {width, height, 1.0F / width, 1.0F / height};
  DirectX::XMStoreFloat4(&frame.lightDirection, SunTowards());
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
  : m_sceneColor(_device, {.format = SCENE_FORMAT, .clearColor = CLEAR_COLOR}, _widthPixels, _heightPixels),
    m_depth(_device, _widthPixels, _heightPixels),
    m_pass(_device, {.colorFormat = SCENE_FORMAT, .depthFormat = Engine::DepthTarget::FORMAT}),
    m_staticShadow(_device, STATIC_SHADOW_TEXELS),
    m_dynamicShadow(_device, DYNAMIC_SHADOW_TEXELS),
    m_upscale(_device, {.targetFormat = _desc.targetFormat}),
    m_gpuTimer(_device, _queue, {.slotCount = Engine::FrameRing::FRAMES_IN_FLIGHT, .spanCount = SPAN_COUNT}),
    m_worldView(_device, 1.0F),
    m_interface(_device, {.targetFormat = _desc.targetFormat}),
    m_fontAtlas(_device),
    m_palette(LinearPalette())
{
  m_pass.SetShadowMaps(m_staticShadow.Texture(), m_dynamicShadow.Texture());
  m_upscale.SetSource(m_sceneColor.Texture());
  m_interface.SetAtlas(m_fontAtlas.Texture(), GlyphAtlas::WIDTH_TEXELS, GlyphAtlas::HEIGHT_TEXELS);
  // The sun's view over the whole world, its height included, for both shadow maps.
  DirectX::XMStoreFloat4x4(&m_sunViewProjection,
                           Engine::SunViewProjection(SunTowards(), DirectX::XMFLOAT3{0.0F, 0.0F, 0.0F},
                                                     DirectX::XMFLOAT3{_desc.widthMeters, _desc.heightMeters, _desc.depthMeters},
                                                     SHADOW_MARGIN_METERS));
}

void WorldRenderer::ResetWorld()
{
  m_worldView.Forget();
  m_worldChanged = true;
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
  const std::uint32_t slot = _frame.slot;
  // FrameRing::Begin waited for this slot's last frame, so its timestamps are back.
  if (m_timed.at(slot))
  {
    m_voxelPassMilliseconds = m_gpuTimer.ReadMilliseconds(slot)[VOXEL_PASS_SPAN];
  }
  if (!_frame.snapshots.empty())
  {
    m_worldView.TakeSnapshots(commandList, ring, _frame.snapshots);
    m_worldChanged = true;
  }
  const float modelVoxelPixels = MODEL_VOXEL_METERS * static_cast<float>(m_sceneColor.HeightPixels()) / _frame.viewHeightMeters;
  m_worldView.PlaceModels(ring, std::vector<ModelPlacement>(_frame.placements.begin(), _frame.placements.end()), modelVoxelPixels);
  const D3D12_GPU_VIRTUAL_ADDRESS frameConstants = UploadConstants(
    ring, MakeFrame(_frame.viewProjection, _frame.forward, m_sceneColor.WidthPixels(), m_sceneColor.HeightPixels(), m_sunViewProjection));
  const D3D12_GPU_VIRTUAL_ADDRESS paletteConstants = UploadConstants(ring, m_palette);

  // The sun's shadow maps: the world's when it changed, the models' every frame (ADR-011, decision 9).
  m_gpuTimer.Begin(commandList, slot, STATIC_SHADOW_SPAN);
  if (m_worldChanged || !m_staticShadow.Readable())
  {
    const D3D12_GPU_VIRTUAL_ADDRESS staticSunFrame = UploadConstants(ring, MakeSunFrame(m_sunViewProjection, STATIC_SHADOW_TEXELS));
    m_staticShadow.BeginDrawing(commandList);
    m_pass.BeginShadow(commandList, staticSunFrame, paletteConstants);
    m_worldView.DrawChunks(commandList, m_pass);
    m_staticShadow.EndDrawing(commandList);
    m_worldChanged = false;
  }
  m_gpuTimer.End(commandList, slot, STATIC_SHADOW_SPAN);
  m_gpuTimer.Begin(commandList, slot, DYNAMIC_SHADOW_SPAN);
  const D3D12_GPU_VIRTUAL_ADDRESS dynamicSunFrame = UploadConstants(ring, MakeSunFrame(m_sunViewProjection, DYNAMIC_SHADOW_TEXELS));
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
  m_worldView.Draw(commandList, m_pass);
  if (debrisCount != 0)
  {
    m_pass.BeginOriented(commandList, frameConstants, paletteConstants);
    m_pass.DrawOriented(commandList, debrisAddress, debrisCount);
  }
  m_gpuTimer.End(commandList, slot, VOXEL_PASS_SPAN);
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
                       .textured = quad.textured ? 1U : 0U,
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
