#include "pch.h"

#include "InterfacePass.h"

#include "CompiledShader/InterfacePS.h"
#include "CompiledShader/InterfaceVS.h"

namespace Engine
{

namespace
{

// The root parameters, in the order InterfaceCommon.hlsli's registers expect.
enum RootParameter : UINT
{
  RootConstants, // b0: the target's size, four 32-bit values
  RootQuads,     // t0: the quads, a root shader-resource view
  RootAtlas,     // t1: a descriptor table of one shader-resource view
  RootCount
};

constexpr UINT CONSTANT_VALUES = 4;
constexpr UINT QUAD_VERTICES = 4;

winrt::com_ptr<ID3D12RootSignature> MakeRootSignature(ID3D12Device* _device)
{
  // Written in code, not in HLSL (ADR-011, decision 7).
  const D3D12_DESCRIPTOR_RANGE atlasRange{.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                          .NumDescriptors = 1,
                                          .BaseShaderRegister = 1,
                                          .RegisterSpace = 0,
                                          .OffsetInDescriptorsFromTableStart = 0};
  std::array<D3D12_ROOT_PARAMETER, RootCount> parameters{};
  parameters[RootConstants].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  parameters[RootConstants].Constants = {.ShaderRegister = 0, .RegisterSpace = 0, .Num32BitValues = CONSTANT_VALUES};
  parameters[RootConstants].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  parameters[RootQuads].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
  parameters[RootQuads].Descriptor = {.ShaderRegister = 0, .RegisterSpace = 0};
  parameters[RootQuads].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
  parameters[RootAtlas].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[RootAtlas].DescriptorTable = {.NumDescriptorRanges = 1, .pDescriptorRanges = &atlasRange};
  parameters[RootAtlas].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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
    throw std::runtime_error("the interface pass's root signature did not serialize: " + message);
  }
  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::check_hresult(_device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(rootSignature.put())));
  return rootSignature;
}

} // namespace

InterfacePass::InterfacePass(ID3D12Device* _device, const Desc& _desc)
  : m_device(_device),
    m_rootSignature(MakeRootSignature(_device)),
    m_viewHeap(_device, {.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, .capacityDescriptors = 1, .shaderVisible = true})
{
  m_atlasView = m_viewHeap.Allocate();

  // Blended over what is there, alpha not premultiplied. Every enum is named, because several of D3D12's have no zero
  // value (no d3dx12.h defaults, R14).
  constexpr D3D12_RENDER_TARGET_BLEND_DESC BLENDED_TARGET{.BlendEnable = TRUE,
                                                          .LogicOpEnable = FALSE,
                                                          .SrcBlend = D3D12_BLEND_SRC_ALPHA,
                                                          .DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
                                                          .BlendOp = D3D12_BLEND_OP_ADD,
                                                          .SrcBlendAlpha = D3D12_BLEND_ONE,
                                                          .DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
                                                          .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                                                          .LogicOp = D3D12_LOGIC_OP_NOOP,
                                                          .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL};
  constexpr D3D12_DEPTH_STENCILOP_DESC NO_STENCIL{.StencilFailOp = D3D12_STENCIL_OP_KEEP,
                                                  .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
                                                  .StencilPassOp = D3D12_STENCIL_OP_KEEP,
                                                  .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS};
  const D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{
    .pRootSignature = m_rootSignature.get(),
    .VS = {.pShaderBytecode = g_InterfaceVS, .BytecodeLength = sizeof(g_InterfaceVS)},
    .PS = {.pShaderBytecode = g_InterfacePS, .BytecodeLength = sizeof(g_InterfacePS)},
    .BlendState = {.AlphaToCoverageEnable = FALSE,
                   .IndependentBlendEnable = FALSE,
                   .RenderTarget = {BLENDED_TARGET, BLENDED_TARGET, BLENDED_TARGET, BLENDED_TARGET, BLENDED_TARGET, BLENDED_TARGET,
                                    BLENDED_TARGET, BLENDED_TARGET}},
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
    // No depth: the interface draws over everything, after the upscale.
    .DepthStencilState = {.DepthEnable = FALSE,
                          .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
                          .DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS,
                          .StencilEnable = FALSE,
                          .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
                          .StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
                          .FrontFace = NO_STENCIL,
                          .BackFace = NO_STENCIL},
    .InputLayout = {.pInputElementDescs = nullptr, .NumElements = 0},
    .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    .NumRenderTargets = 1,
    .RTVFormats = {_desc.targetFormat},
    .DSVFormat = DXGI_FORMAT_UNKNOWN,
    .SampleDesc = {.Count = 1, .Quality = 0},
    .NodeMask = 0,
    .CachedPSO = {.pCachedBlob = nullptr, .CachedBlobSizeInBytes = 0},
    .Flags = D3D12_PIPELINE_STATE_FLAG_NONE};
  winrt::check_hresult(_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(m_pipelineState.put())));
}

void InterfacePass::SetAtlas(ID3D12Resource* _atlas, std::uint32_t _widthTexels, std::uint32_t _heightTexels)
{
  if (_widthTexels == 0 || _heightTexels == 0)
  {
    throw std::invalid_argument("the interface pass's atlas has no texels");
  }
  // A null description views the whole texture in its own format, one mip level.
  m_device->CreateShaderResourceView(_atlas, nullptr, m_atlasView.cpu);
  m_hasAtlas = true;
}

void InterfacePass::Draw(ID3D12GraphicsCommandList* _commandList, D3D12_GPU_VIRTUAL_ADDRESS _quads, std::uint32_t _quadCount,
                         std::uint32_t _targetWidthPixels, std::uint32_t _targetHeightPixels) const
{
  if (!m_hasAtlas)
  {
    throw std::logic_error("the interface pass was drawn before it was given an atlas");
  }
  if (_quadCount == 0)
  {
    return;
  }
  const std::array<float, CONSTANT_VALUES> constants = {static_cast<float>(_targetWidthPixels), static_cast<float>(_targetHeightPixels),
                                                        0.0F, 0.0F};
  ID3D12DescriptorHeap* heap = m_viewHeap.Heap();
  _commandList->SetDescriptorHeaps(1, &heap);
  _commandList->SetGraphicsRootSignature(m_rootSignature.get());
  _commandList->SetPipelineState(m_pipelineState.get());
  _commandList->SetGraphicsRoot32BitConstants(RootConstants, CONSTANT_VALUES, constants.data(), 0);
  _commandList->SetGraphicsRootShaderResourceView(RootQuads, _quads);
  _commandList->SetGraphicsRootDescriptorTable(RootAtlas, m_atlasView.gpu);
  _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  _commandList->DrawInstanced(QUAD_VERTICES, _quadCount, 0, 0);
}

} // namespace Engine
