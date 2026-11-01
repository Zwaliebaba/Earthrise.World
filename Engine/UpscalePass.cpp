#include "pch.h"

#include "UpscalePass.h"

#include "CompiledShader/UpscalePS.h"
#include "CompiledShader/UpscaleVS.h"

namespace Engine
{

namespace
{

// The root parameters, in the order UpscaleCommon.hlsli's registers expect.
enum RootParameter : UINT
{
  RootSource, // t0, a descriptor table of one shader resource view
  RootCount
};

winrt::com_ptr<ID3D12RootSignature> MakeRootSignature(ID3D12Device* _device)
{
  // Written in code, not in HLSL (ADR-011, decision 7).
  const D3D12_DESCRIPTOR_RANGE sourceRange{.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                           .NumDescriptors = 1,
                                           .BaseShaderRegister = 0,
                                           .RegisterSpace = 0,
                                           .OffsetInDescriptorsFromTableStart = 0};
  std::array<D3D12_ROOT_PARAMETER, RootCount> parameters{};
  parameters[RootSource].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[RootSource].DescriptorTable = {.NumDescriptorRanges = 1, .pDescriptorRanges = &sourceRange};
  parameters[RootSource].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  // Bilinear, clamped at the edges, so the border pixels repeat the source's edge.
  const D3D12_STATIC_SAMPLER_DESC bilinear{.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                                           .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                           .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                           .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                                           .MipLODBias = 0.0F,
                                           .MaxAnisotropy = 1,
                                           .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
                                           .BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
                                           .MinLOD = 0.0F,
                                           .MaxLOD = 0.0F,
                                           .ShaderRegister = 0,
                                           .RegisterSpace = 0,
                                           .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL};

  const D3D12_ROOT_SIGNATURE_DESC desc{.NumParameters = RootCount,
                                       .pParameters = parameters.data(),
                                       .NumStaticSamplers = 1,
                                       .pStaticSamplers = &bilinear,
                                       .Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                                                D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                                                D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS};
  winrt::com_ptr<ID3DBlob> blob;
  winrt::com_ptr<ID3DBlob> error;
  const HRESULT serialized = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, blob.put(), error.put());
  if (FAILED(serialized))
  {
    const std::string message = error ? std::string(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize()) : "";
    throw std::runtime_error("the upscale pass's root signature did not serialize: " + message);
  }
  winrt::com_ptr<ID3D12RootSignature> rootSignature;
  winrt::check_hresult(_device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(rootSignature.put())));
  return rootSignature;
}

} // namespace

std::uint32_t ScaledPixels(std::uint32_t _displayPixels, float _renderScale) noexcept
{
  const double scaled = std::round(static_cast<double>(_displayPixels) * static_cast<double>(_renderScale));
  return scaled < 1.0 ? 1U : static_cast<std::uint32_t>(scaled);
}

UpscalePass::UpscalePass(ID3D12Device* _device, const Desc& _desc)
  : m_device(_device),
    m_rootSignature(MakeRootSignature(_device)),
    m_viewHeap(_device, {.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, .capacityDescriptors = 1, .shaderVisible = true})
{
  m_sourceView = m_viewHeap.Allocate();

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
    .VS = {.pShaderBytecode = g_UpscaleVS, .BytecodeLength = sizeof(g_UpscaleVS)},
    .PS = {.pShaderBytecode = g_UpscalePS, .BytecodeLength = sizeof(g_UpscalePS)},
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
    // No depth: the scene's depth stays at the render scale, and the interface draws after this.
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

void UpscalePass::SetSource(ID3D12Resource* _source)
{
  // A null description views the whole texture in its own format, one mip level.
  m_device->CreateShaderResourceView(_source, nullptr, m_sourceView.cpu);
  m_hasSource = true;
}

void UpscalePass::Draw(ID3D12GraphicsCommandList* _commandList) const
{
  if (!m_hasSource)
  {
    throw std::logic_error("the upscale pass was drawn before it was given a source");
  }
  ID3D12DescriptorHeap* heap = m_viewHeap.Heap();
  _commandList->SetDescriptorHeaps(1, &heap);
  _commandList->SetGraphicsRootSignature(m_rootSignature.get());
  _commandList->SetPipelineState(m_pipelineState.get());
  _commandList->SetGraphicsRootDescriptorTable(RootSource, m_sourceView.gpu);
  _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  _commandList->DrawInstanced(3, 1, 0, 0);
}

} // namespace Engine
