#pragma once

#include "framework.h"

#include <unknwn.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <winrt/base.h>

#include "CommandQueue.h"

#include <memory>
#include <string>
#include <vector>

namespace Engine
{

// Debug builds turn the debug layer on (ADR-011, decision 8).
#ifdef _DEBUG
inline constexpr bool DEBUG_BUILD = true;
#else
inline constexpr bool DEBUG_BUILD = false;
#endif

/// The DXGI factory, the adapter, the Direct3D 12 device and its one direct queue (ADR-011,
/// decision 8). The adapter is the first, by the system's high-performance order, that offers
/// feature level 12_0 and Shader Model 6.0.
class GpuDevice
{
public:
  struct Desc
  {
    bool enableDebugLayer; // asks for it; DebugLayerActive says whether Windows has it installed
    bool useWarp;          // the software adapter, for tests on machines without a GPU
  };

  /// Throws std::runtime_error, with a message meant for the player, when no adapter offers feature
  /// level 12_0 and Shader Model 6.0, and winrt::hresult_error for any other refusal.
  explicit GpuDevice(const Desc& _desc);

  [[nodiscard]] ID3D12Device* Device() const noexcept;
  [[nodiscard]] IDXGIFactory6* Factory() const noexcept;
  [[nodiscard]] CommandQueue& DirectQueue() noexcept;
  [[nodiscard]] const std::wstring& AdapterName() const noexcept;
  /// The adapter's user-mode driver version, as a.b.c.d, or empty when DXGI does not give one.
  [[nodiscard]] const std::wstring& DriverVersion() const noexcept;
  [[nodiscard]] bool TearingSupported() const noexcept;
  [[nodiscard]] bool DebugLayerActive() const noexcept;

  /// The debug layer's errors and corruption reports since the last call, which it then clears.
  /// Always empty when the debug layer is not active.
  [[nodiscard]] std::vector<std::string> TakeDebugErrors();

private:
  winrt::com_ptr<IDXGIFactory6> m_factory;
  winrt::com_ptr<IDXGIAdapter1> m_adapter;
  winrt::com_ptr<ID3D12Device> m_device;
  winrt::com_ptr<ID3D12InfoQueue> m_infoQueue;
  std::unique_ptr<CommandQueue> m_directQueue; // made once the device exists
  std::wstring m_adapterName;
  std::wstring m_driverVersion;
  bool m_tearingSupported = false;
  bool m_debugLayerActive = false;
};

} // namespace Engine
