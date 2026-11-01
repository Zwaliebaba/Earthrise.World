#pragma once

#include "ServerData.h"

#include <filesystem>

namespace SimulationTests
{

/// Where the build wrote ServerData.bin, <repository>\<Platform>\<Configuration>\ServerData
/// (ADR-010, decision 3; ADR-016, decision 5). It is found from this file's own folder, the
/// platform and the configuration, so the tests include no Windows header to find their own DLL.
inline std::filesystem::path ServerDataFile()
{
#if defined(_M_X64)
  constexpr const char* PLATFORM = "x64";
#elif defined(_M_ARM64)
  constexpr const char* PLATFORM = "ARM64";
#else
#   error "SimulationTests builds for x64 and ARM64 only (ADR-007)"
#endif
#if defined(_DEBUG)
  constexpr const char* CONFIGURATION = "Debug";
#else
  constexpr const char* CONFIGURATION = "Release";
#endif
  return std::filesystem::path(__FILE__).parent_path().parent_path() / PLATFORM / CONFIGURATION / "ServerData" / "ServerData.bin";
}

/// The baked server data, read once for every suite that needs it.
inline const Simulation::ServerData& BakedServerData()
{
  static const Simulation::ServerData DATA = Simulation::ServerData::Load(ServerDataFile());
  return DATA;
}

} // namespace SimulationTests
