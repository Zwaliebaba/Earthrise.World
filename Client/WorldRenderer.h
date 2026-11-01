#pragma once

#include "framework.h"

#include <unknwn.h>

#include <DirectXMath.h>
#include <d3d12.h>
#include <winrt/base.h>

#include "ChunkSnapshot.h"
#include "ColorTarget.h"
#include "DepthTarget.h"
#include "FontAtlas.h"
#include "FrameRing.h"
#include "GpuTimer.h"
#include "InterfacePass.h"
#include "ModelPlacement.h"
#include "OrientedVoxel.h"
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
/// world's chunks, the models and the debris, scaled to the back buffer, and the GPU timer over the voxel pass; then the
/// interface over it at the back buffer's size (M3-E1).
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
    std::span<const Engine::OrientedVoxel> debris;
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

  /// Remakes the scene's targets at a new size. The caller first makes sure the GPU no longer uses them (a flush).
  void Resize(std::uint32_t _widthPixels, std::uint32_t _heightPixels);

  /// Records one frame. Reads the GPU timer's last result for _frame.slot first, which FrameRing::Begin has waited for.
  void Draw(const Frame& _frame);

  /// The voxel pass's time on the GPU, a few frames late.
  [[nodiscard]] double VoxelPassMilliseconds() const noexcept;
  [[nodiscard]] std::uint64_t RecordsDrawn() const noexcept;

  void Retire(std::uint64_t _fenceValue);
  void Release(std::uint64_t _completedFenceValue);

private:
  static constexpr std::uint32_t VOXEL_PASS_SPAN = 0;
  static constexpr std::uint32_t STATIC_SHADOW_SPAN = 1;
  static constexpr std::uint32_t DYNAMIC_SHADOW_SPAN = 2;
  static constexpr std::uint32_t SPAN_COUNT = 3;

  Engine::ColorTarget m_sceneColor;
  Engine::DepthTarget m_depth;
  Engine::VoxelPass m_pass;
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
  bool m_worldChanged = true;
};

} // namespace Client
