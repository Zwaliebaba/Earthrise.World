#pragma once

#include "framework.h"

#include <unknwn.h>

#include <DirectXMath.h>
#include <d3d12.h>
#include <winrt/base.h>

#include <array>
#include <cstdint>

namespace Engine
{

/// One segment of a line model, along its axis from the tail (ADR-018, decision 2). Its layout is load-bearing:
/// Shader/LineCommon.hlsli reads the same 40 bytes.
struct LineSegmentRecord
{
  float fromMeters, toMeters;
  DirectX::XMFLOAT4 color;    // linear RGBA; unused when factionColor is 1
  float widthPixels;          // in the scene target's pixels, whatever the zoom
  std::uint32_t factionColor; // 1: drawn in its instance's faction colour
  std::uint32_t tapers;       // 1: it narrows to a point at its end
  float reserved;
};

static_assert(sizeof(LineSegmentRecord) == 40, "the shaders read a segment as 40 bytes");

/// A line model: a run of segment records and its dot rule. Its layout is load-bearing, as LineSegmentRecord's is.
struct LineModelRecord
{
  std::uint32_t firstSegment; // its first segment record's index
  std::uint32_t segmentCount; // at most LinePass::MAX_SEGMENTS are drawn
  float drawnLengthMeters;    // from its tail to its head
  float dotBelowPixels;       // drawn shorter than this on screen, the whole line is a dot
  float dotWidthPixels;       // the dot's width and height
  std::array<std::uint32_t, 3> reserved;
};

static_assert(sizeof(LineModelRecord) == 32, "the shaders read a line model as 32 bytes");

/// One line drawn: its model, where its middle is, the unit direction from its tail to its head, and its side's
/// colour. Its layout is load-bearing, as LineSegmentRecord's is.
struct LineInstance
{
  DirectX::XMFLOAT3 positionMeters; // the middle of its drawn length
  std::uint32_t lineModel;          // its model record's index
  DirectX::XMFLOAT3 direction;      // unit length
  float reserved;
  DirectX::XMFLOAT4 factionColor; // linear RGBA, for its faction segments
};

static_assert(sizeof(LineInstance) == 48, "the shaders read an instance as 48 bytes");

/// The line pass of ADR-018, decision 3: one instanced draw of lines after the voxel pass, into the same scene target
/// and depth buffer. The vertex shader widens each segment of each instance's model to a quad of its width in the
/// target's pixels, a tapering one narrowing to a point at its end, and draws the whole line as a dot when it is shorter
/// on screen than its model's dotBelowPixels. Lines are unlit and unshadowed, and are tested against the scene's depth
/// with GREATER_EQUAL under reversed Z and write it. It knows lines, colours and pixels, never arrows (R9).
class LinePass
{
public:
  /// The segments a line model may have that are drawn; a model's segments past them are not.
  static constexpr std::uint32_t MAX_SEGMENTS = 4;

  struct Desc
  {
    DXGI_FORMAT colorFormat;
    DXGI_FORMAT depthFormat;
  };

  /// Throws winrt::hresult_error when the device refuses the root signature or the pipeline.
  LinePass(ID3D12Device* _device, const Desc& _desc);

  /// Binds the pipeline, the frame's constants, VoxelPass::FrameConstants at a 256-byte-aligned GPU address, and the
  /// model and segment tables, arrays of LineModelRecord and LineSegmentRecord the GPU can read. The caller has set the
  /// render target, the depth target the voxel pass wrote, the viewport and the scissor.
  void Begin(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _frameConstants, D3D12_GPU_VIRTUAL_ADDRESS _models,
             D3D12_GPU_VIRTUAL_ADDRESS _segments) const;

  /// Draws _instanceCount LineInstances at _instances, after Begin.
  void Draw(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _instances, std::uint32_t _instanceCount) const;

private:
  winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
  winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
};

} // namespace Engine
