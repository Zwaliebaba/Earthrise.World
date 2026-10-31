#include "pch.h"

#include "VoxelPass.h"

#include "CompiledShader/VoxelPS.h"
#include "CompiledShader/VoxelVS.h"

namespace Engine
{

namespace
{

// The root parameters, in the order VoxelCommon.hlsli's registers expect.
enum RootParameter : UINT
{
  RootFrame,   // b0, a root CBV
  RootPalette, // b1, a root CBV
  RootBatch,   // b2, four root constants
  RootRecords, // t0, a root SRV
  RootCount
};

constexpr UINT BATCH_CONSTANT_COUNT = 4;

winrt::com_ptr<ID3D12RootSignature> MakeRootSignature(ID3D12Device* _device)
{
  // Written in code, not in HLSL (ADR-011, decision 7).
  std::array<D3D12_ROOT_PARAMETER, RootCount> parameters{};
  parameters[RootFrame].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  parameters[RootFrame].Descriptor = {.ShaderRegister = 0, .RegisterSpace = 0};
  parameters[RootFrame].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  parameters[RootPalette].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  parameters[RootPalette].Descriptor = {.ShaderRegister = 1, .RegisterSpace = 0};
  parameters[RootPalette].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  parameters[RootBatch].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  parameters[RootBatch].Constants = {.ShaderRegister = 2, .RegisterSpace = 0, .Num32BitValues = BATCH_CONSTANT_COUNT};
  parameters[RootBatch].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  parameters[RootRecords].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
  parameters[RootRecords].Descriptor = {.ShaderRegister = 0, .RegisterSpace = 0};
  parameters[RootRecords].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

  const D3D12_ROOT_SIGNATURE_DESC desc{.NumParameters = RootCount,
                                       .pParameters = parameters.data(),
                                       .NumStaticSamplers = 0,
                                       .pStaticSamplers = nullptr,
                                       .Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                                                D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                                                D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS};
  winrt::com_ptr<ID3DBlob> blob;
  winrt::com_ptr<ID3DBlob> error;
  const HRESULT serialized = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, blob.put(), error.put());
  if (FAILED(serialized))
  {
    const std::string message = error ? std::string(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize()) : "";
    throw std::runtime_error("the voxel pass's root signature did not serialize: " + message);
  }
  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::check_hresult(_device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(rootSignature.put())));
  return rootSignature;
}

} // namespace

VoxelPass::VoxelPass(ID3D12Device* _device, const Desc& _desc)
  : m_rootSignature(MakeRootSignature(_device))
{
  // Every enum is named, because several of D3D12's have no zero value (no d3dx12.h defaults, R14).
  constexpr D3D12_RENDER_TARGET_BLEND_DESC OPAQUE_TARGET{.BlendEnable = FALSE,
                                                         .LogicOpEnable = FALSE,
                                                         .SrcBlend = D3D12_BLEND_ONE,
                                                         .DestBlend = D3D12_BLEND_ZERO,
                                                         .BlendOp = D3D12_BLEND_OP_ADD,
                                                         .SrcBlendAlpha = D3D12_BLEND_ONE,
                                                         .DestBlendAlpha = D3D12_BLEND_ZERO,
                                                         .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                                                         .LogicOp = D3D12_LOGIC_OP_NOOP,
                                                         .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL};
  constexpr D3D12_DEPTH_STENCILOP_DESC NO_STENCIL{.StencilFailOp = D3D12_STENCIL_OP_KEEP,
                                                  .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
                                                  .StencilPassOp = D3D12_STENCIL_OP_KEEP,
                                                  .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS};
  const D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{
    .pRootSignature = m_rootSignature.get(),
    .VS = {.pShaderBytecode = g_VoxelVS, .BytecodeLength = sizeof(g_VoxelVS)},
    .PS = {.pShaderBytecode = g_VoxelPS, .BytecodeLength = sizeof(g_VoxelPS)},
    .BlendState = {.AlphaToCoverageEnable = FALSE,
                   .IndependentBlendEnable = FALSE,
                   .RenderTarget = {OPAQUE_TARGET, OPAQUE_TARGET, OPAQUE_TARGET, OPAQUE_TARGET, OPAQUE_TARGET, OPAQUE_TARGET, OPAQUE_TARGET,
                                    OPAQUE_TARGET}},
    .SampleMask = UINT_MAX,
    .RasterizerState = {.FillMode = D3D12_FILL_MODE_SOLID,
                        .CullMode = D3D12_CULL_MODE_NONE,
                        .FrontCounterClockwise = FALSE,
                        .DepthBias = 0,
                        .DepthBiasClamp = 0.0F,
                        .SlopeScaledDepthBias = 0.0F,
                        .DepthClipEnable = TRUE,
                        .MultisampleEnable = FALSE,
                        .AntialiasedLineEnable = FALSE,
                        .ForcedSampleCount = 0,
                        .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF},
    // Reversed Z: cleared to 0, nearer is larger (ADR-011, decision 4).
    .DepthStencilState = {.DepthEnable = TRUE,
                          .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
                          .DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL,
                          .StencilEnable = FALSE,
                          .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
                          .StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
                          .FrontFace = NO_STENCIL,
                          .BackFace = NO_STENCIL},
    .InputLayout = {.pInputElementDescs = nullptr, .NumElements = 0},
    .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    .NumRenderTargets = 1,
    .RTVFormats = {_desc.colorFormat},
    .DSVFormat = _desc.depthFormat,
    .SampleDesc = {.Count = 1, .Quality = 0}, // no MSAA in M1 (ADR-011, decision 5)
    .NodeMask = 0,
    .CachedPSO = {.pCachedBlob = nullptr, .CachedBlobSizeInBytes = 0},
    .Flags = D3D12_PIPELINE_STATE_FLAG_NONE};
  winrt::check_hresult(_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(m_pipelineState.put())));
}

void VoxelPass::Begin(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _frameConstants,
                      D3D12_GPU_VIRTUAL_ADDRESS _palette) const
{
  _commandList->SetGraphicsRootSignature(m_rootSignature.get());
  _commandList->SetPipelineState(m_pipelineState.get());
  _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  _commandList->SetGraphicsRootConstantBufferView(RootFrame, _frameConstants);
  _commandList->SetGraphicsRootConstantBufferView(RootPalette, _palette);
}

void VoxelPass::Draw(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _records, std::uint32_t _recordCount,
                     const DirectX::XMFLOAT3& _originMeters, float _voxelEdgeMeters) const
{
  if (_recordCount == 0)
  {
    return;
  }
  const std::array<float, BATCH_CONSTANT_COUNT> batch{_originMeters.x, _originMeters.y, _originMeters.z, _voxelEdgeMeters};
  _commandList->SetGraphicsRoot32BitConstants(RootBatch, BATCH_CONSTANT_COUNT, batch.data(), 0);
  _commandList->SetGraphicsRootShaderResourceView(RootRecords, _records);
  _commandList->DrawInstanced(4, _recordCount, 0, 0);
}

} // namespace Engine
