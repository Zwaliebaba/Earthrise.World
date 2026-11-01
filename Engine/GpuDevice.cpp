#include "pch.h"

#include "GpuDevice.h"

// The libraries the SDK's headers need. windowsapp provides what winrt::hresult_error reports
// through.
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowsapp.lib")

namespace Engine
{

namespace
{

constexpr D3D_FEATURE_LEVEL MINIMUM_FEATURE_LEVEL = D3D_FEATURE_LEVEL_12_0;

constexpr const char* UNSUPPORTED_MESSAGE =
  "Earthrise.World needs a graphics card that supports Direct3D 12 at feature level 12_0 with Shader Model 6.0, and none was found. "
  "Updating the graphics driver may help.";

bool SupportsShaderModel60(ID3D12Device* _device) noexcept
{
  D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{.HighestShaderModel = D3D_SHADER_MODEL_6_0};
  return SUCCEEDED(_device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))) &&
         shaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_0;
}

} // namespace

GpuDevice::GpuDevice(const Desc& _desc)
{
  UINT factoryFlags = 0;
  if (_desc.enableDebugLayer)
  {
    // The debug layer ships with Windows' Graphics Tools feature. Without it the device runs
    // unvalidated, and DebugLayerActive says so.
    winrt::com_ptr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debug.put()))))
    {
      debug->EnableDebugLayer();
      m_debugLayerActive = true;
      factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
    }
  }
  if (FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(m_factory.put()))))
  {
    // DXGI's debug layer can be missing where Direct3D's is present.
    m_factory = nullptr;
    winrt::check_hresult(CreateDXGIFactory2(0, IID_PPV_ARGS(m_factory.put())));
  }

  if (_desc.useWarp)
  {
    winrt::check_hresult(m_factory->EnumWarpAdapter(IID_PPV_ARGS(m_adapter.put())));
    winrt::check_hresult(D3D12CreateDevice(m_adapter.get(), MINIMUM_FEATURE_LEVEL, IID_PPV_ARGS(m_device.put())));
  }
  else
  {
    for (UINT index = 0;; ++index)
    {
      winrt::com_ptr<IDXGIAdapter1> candidate;
      if (FAILED(m_factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(candidate.put()))))
      {
        break;
      }
      DXGI_ADAPTER_DESC1 candidateDesc{};
      winrt::check_hresult(candidate->GetDesc1(&candidateDesc));
      if ((candidateDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
      {
        continue;
      }
      winrt::com_ptr<ID3D12Device> device;
      if (SUCCEEDED(D3D12CreateDevice(candidate.get(), MINIMUM_FEATURE_LEVEL, IID_PPV_ARGS(device.put()))) &&
          SupportsShaderModel60(device.get()))
      {
        m_adapter = candidate;
        m_device = device;
        break;
      }
    }
    if (!m_device)
    {
      throw std::runtime_error(UNSUPPORTED_MESSAGE);
    }
  }
  if (!SupportsShaderModel60(m_device.get()))
  {
    throw std::runtime_error(UNSUPPORTED_MESSAGE);
  }

  DXGI_ADAPTER_DESC1 adapterDesc{};
  winrt::check_hresult(m_adapter->GetDesc1(&adapterDesc));
  m_adapterName = adapterDesc.Description;
  // DXGI answers the driver's version through this query, in four 16-bit parts.
  LARGE_INTEGER version{};
  if (SUCCEEDED(m_adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &version)))
  {
    const auto part = [&version](int _shift) { return (static_cast<std::uint64_t>(version.QuadPart) >> _shift) & 0xffffU; };
    m_driverVersion = std::format(L"{}.{}.{}.{}", part(48), part(32), part(16), part(0));
  }

  BOOL tearing = FALSE;
  m_tearingSupported =
    SUCCEEDED(m_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing))) && tearing == TRUE;

  if (m_debugLayerActive)
  {
    m_infoQueue = m_device.try_as<ID3D12InfoQueue>();
    if (m_infoQueue)
    {
      // Keep only what counts as a failure: errors and corruption.
      D3D12_MESSAGE_SEVERITY kept[] = {D3D12_MESSAGE_SEVERITY_CORRUPTION, D3D12_MESSAGE_SEVERITY_ERROR};
      D3D12_INFO_QUEUE_FILTER filter{};
      filter.AllowList.NumSeverities = static_cast<UINT>(std::size(kept));
      filter.AllowList.pSeverityList = kept;
      winrt::check_hresult(m_infoQueue->PushStorageFilter(&filter));
      if (IsDebuggerPresent())
      {
        winrt::check_hresult(m_infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
        winrt::check_hresult(m_infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
      }
    }
  }

  m_directQueue = std::make_unique<CommandQueue>(m_device.get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
}

ID3D12Device* GpuDevice::Device() const noexcept
{
  return m_device.get();
}

IDXGIFactory6* GpuDevice::Factory() const noexcept
{
  return m_factory.get();
}

CommandQueue& GpuDevice::DirectQueue() noexcept
{
  return *m_directQueue;
}

const std::wstring& GpuDevice::AdapterName() const noexcept
{
  return m_adapterName;
}

const std::wstring& GpuDevice::DriverVersion() const noexcept
{
  return m_driverVersion;
}

bool GpuDevice::TearingSupported() const noexcept
{
  return m_tearingSupported;
}

bool GpuDevice::DebugLayerActive() const noexcept
{
  return m_debugLayerActive;
}

std::vector<std::string> GpuDevice::TakeDebugErrors()
{
  std::vector<std::string> errors;
  if (!m_infoQueue)
  {
    return errors;
  }
  const UINT64 count = m_infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
  for (UINT64 index = 0; index < count; ++index)
  {
    SIZE_T lengthBytes = 0;
    winrt::check_hresult(m_infoQueue->GetMessage(index, nullptr, &lengthBytes));
    std::vector<std::byte> storage(lengthBytes);
    auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
    winrt::check_hresult(m_infoQueue->GetMessage(index, message, &lengthBytes));
    // DescriptionByteLength counts the terminating null.
    errors.emplace_back(message->pDescription, message->DescriptionByteLength > 0 ? message->DescriptionByteLength - 1 : 0);
  }
  m_infoQueue->ClearStoredMessages();
  return errors;
}

} // namespace Engine
