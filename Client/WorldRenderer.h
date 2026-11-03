#pragma once

#include "framework.h"

#include <unknwn.h>

#include <DirectXMath.h>
#include <d3d12.h>
#include <winrt/base.h>

#include "ArrowDraw.h"
#include "ChunkSnapshot.h"
#include "ColorTarget.h"
#include "DepthTarget.h"
#include "FontAtlas.h"
#include "FrameRing.h"
#include "GpuTimer.h"
#include "InterfacePass.h"
#include "LinePass.h"
#include "ModelPlacement.h"
#include "OrientedVoxel.h"
#include "OverlayBox.h"
#include "ScreenQuad.h"
#include "ShadowMap.h"
#include "UploadRing.h"
#include "UpscalePass.h"
#include "VoxelPass.h"
#include "WorldView.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace Client
{

/// One frame of the world as the renderer spike draws it (M3-C3): the sun's two shadow maps, the voxel pass with the
/// world's chunks, the models and the debris, and the line pass with the arrows (M4-H2), each timed on the GPU; the
/// world overlays over them (ADR-021); all of it scaled to the back buffer; then the interface over it at the back
/// buffer's size (M3-E1).
class WorldRenderer
{
public:
  /// The world's extent in meters from its origin, which the sun's shadow maps cover, and the format of the target it
  /// draws into.
  struct Desc
  {
    float widthMeters;
    float heightMeters;
    float depthMeters;
    DXGI_FORMAT targetFormat;
  };

  /// What a frame draws, and into what.
  struct Frame
  {
    ID3D12GraphicsCommandList* commandList; // open, after FrameRing::Begin
    std::uint32_t slot;                     // FrameRing::Slot, for the GPU timer
    Engine::UploadRing* ring;
    std::span<const Protocol::ChunkSnapshot> snapshots; // published since the last frame
    std::span<const ModelPlacement> placements;
    std::span<const Protocol::VoxelPosition> hiddenVoxels; // left out of the world's batches, such as an open gate's
    std::span<const Engine::OrientedVoxel> debris;
    std::span<const ArrowDrawn> arrows;     // the line pass's, after the voxel pass (ADR-018, decision 3)
    std::span<const OverlayLayer> overlays; // in order, after the scene and before the upscale
    /// The fog's column mask (FogMask.h), fogWidthCells by fogDepthCells, uploaded when it changes; empty for no fog.
    std::span<const std::uint8_t> fogMask;
    std::int32_t fogWidthCells;
    std::int32_t fogDepthCells;
    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMFLOAT3 forward;  // the camera's, along its rays
    float viewHeightMeters;     // for how large a model voxel is on screen
    ID3D12Resource* backBuffer; // in PRESENT, and left there
    D3D12_CPU_DESCRIPTOR_HANDLE backBufferView;
    std::uint32_t backWidthPixels;
    std::uint32_t backHeightPixels;
    std::span<const ScreenQuad> interfaceQuads; // drawn over the scaled scene, in order
  };

  /// Throws winrt::hresult_error when the device refuses.
  WorldRenderer(ID3D12Device* _device, ID3D12CommandQueue* _queue, const Desc& _desc, std::uint32_t _widthPixels,
                std::uint32_t _heightPixels);

  /// Forgets the world drawn so far, its fog included, for a fresh one such as the lab's next item. The caller first
  /// makes sure the GPU no longer reads it (a flush).
  void ResetWorld();

  /// Remakes the scene's targets at a new size. The caller first makes sure the GPU no longer uses them (a flush).
  void Resize(std::uint32_t _widthPixels, std::uint32_t _heightPixels);

  /// Records one frame. Reads the GPU timer's last result for _frame.slot first, which FrameRing::Begin has waited for.
  void Draw(const Frame& _frame);

  /// The voxel pass's time on the GPU, a few frames late.
  [[nodiscard]] double VoxelPassMilliseconds() const noexcept;
  /// The line pass's, likewise: what a volley costs.
  [[nodiscard]] double LinePassMilliseconds() const noexcept;
  [[nodiscard]] std::uint64_t RecordsDrawn() const noexcept;

  void Retire(std::uint64_t _fenceValue);
  void Release(std::uint64_t _completedFenceValue);

private:
  /// Draws each of Portraits.h's models once, on the first frame, after the shadow maps: lit as the world is, with no
  /// shadow, through the voxel pass into a PORTRAIT_TEXELS square target, copied into its cell of the image atlas,
  /// which the interface pass reads from then on (M4-I2; Hud.md §5). Each stands far outside the map, where the fog's
  /// mask shades nothing.
  void DrawPortraits(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring, D3D12_GPU_VIRTUAL_ADDRESS _palette);

  /// Uploads the frame's fog mask when it differs from the one drawn, into one texture for the world, which the voxel
  /// pass shades the columns not seen by (ADR-025, decision 8). The first mask makes the texture; the world's map keeps
  /// its size, so a mask of another size before ResetWorld throws std::logic_error.
  void UpdateFog(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring, const Frame& _frame);

  /// The world overlays, layer by layer, through the voxel pass's overlay pipeline, into the scene's targets.
  void DrawOverlays(ID3D12GraphicsCommandList* _commandList, Engine::UploadRing& _ring, D3D12_GPU_VIRTUAL_ADDRESS _frameConstants,
                    D3D12_GPU_VIRTUAL_ADDRESS _palette, std::span<const OverlayLayer> _layers) const;

  static constexpr std::uint32_t VOXEL_PASS_SPAN = 0;
  static constexpr std::uint32_t STATIC_SHADOW_SPAN = 1;
  static constexpr std::uint32_t DYNAMIC_SHADOW_SPAN = 2;
  static constexpr std::uint32_t LINE_PASS_SPAN = 3;
  static constexpr std::uint32_t SPAN_COUNT = 4;

  ID3D12Device* m_device;
  Engine::ColorTarget m_sceneColor;
  Engine::DepthTarget m_depth;
  Engine::VoxelPass m_pass;
  Engine::LinePass m_lines;
  /// Content's line models and their segments as the line pass reads them, and the arrow's model among them.
  std::vector<Engine::LineModelRecord> m_lineModels;
  std::vector<Engine::LineSegmentRecord> m_lineSegments;
  std::uint32_t m_arrowModel = 0;
  Engine::ShadowMap m_staticShadow;
  Engine::ShadowMap m_dynamicShadow;
  Engine::UpscalePass m_upscale;
  Engine::GpuTimer m_gpuTimer;
  WorldView m_worldView;
  Engine::InterfacePass m_interface;
  FontAtlas m_fontAtlas;
  std::array<DirectX::XMFLOAT4, Engine::VoxelPass::PALETTE_ENTRIES> m_palette;
  DirectX::XMFLOAT4X4 m_sunViewProjection{};
  std::array<bool, Engine::FrameRing::FRAMES_IN_FLIGHT> m_timed{};
  double m_voxelPassMilliseconds = 0.0;
  double m_linePassMilliseconds = 0.0;
  Engine::ColorTarget m_portraitTarget; // one portrait at a time, cleared to nothing
  Engine::DepthTarget m_portraitDepth;
  winrt::com_ptr<ID3D12Resource> m_portraitAtlas; // the image atlas: the portraits side by side
  bool m_portraitsDrawn = false;
  winrt::com_ptr<ID3D12Resource> m_fogMask; // R8, one texel a cell, once the world has fog
  std::vector<std::uint8_t> m_fogTexels;    // as last uploaded
  std::int32_t m_fogWidthCells = 0;
  std::int32_t m_fogDepthCells = 0;
  bool m_worldChanged = true;
};

} // namespace Client
