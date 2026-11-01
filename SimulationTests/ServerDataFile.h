#pragma once

#include "Hash.h"
#include "ServerData.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <vector>

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

/// The baked server data with one RULE value changed and the header's hash recomputed, for a test
/// that needs another budget or that turns a step off.
inline Simulation::ServerData BakedServerDataWith(std::string_view _key, std::int64_t _value)
{
  std::ifstream stream(ServerDataFile(), std::ios::binary);
  std::vector<char> raw{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
  const std::string_view text(raw.data(), raw.size());
  const std::size_t at = text.find(_key);
  if (at == std::string_view::npos || at < 4 || static_cast<unsigned char>(raw[at - 4]) != _key.size())
  {
    throw std::invalid_argument("the baked server data holds no such RULE key");
  }
  const std::size_t value = at + _key.size();
  for (std::size_t index = 0; index < 8; ++index)
  {
    raw[value + index] = static_cast<char>((static_cast<std::uint64_t>(_value) >> (8 * index)) & 0xffU);
  }
  const std::span<const std::byte> bytes = std::as_bytes(std::span(raw));
  const std::uint64_t hash = Core::Fnv1a64(bytes.subspan(16));
  for (std::size_t index = 0; index < 8; ++index)
  {
    raw[8 + index] = static_cast<char>((hash >> (8 * index)) & 0xffU);
  }
  return Simulation::ServerData::Read(std::as_bytes(std::span(raw)));
}

} // namespace SimulationTests
