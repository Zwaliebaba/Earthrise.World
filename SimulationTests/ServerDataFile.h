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

/// The baked file's bytes with one RULE key taken out, its section, directory and the header's hash
/// rewritten, for a test that the reader refuses a file lacking a key it needs (ADR-016, decision 6).
inline std::vector<std::byte> BakedServerDataWithout(std::string_view _key)
{
  std::ifstream stream(ServerDataFile(), std::ios::binary);
  const std::vector<char> raw{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
  std::vector<std::byte> file(raw.size());
  std::ranges::transform(raw, file.begin(), [](char _value) { return static_cast<std::byte>(_value); });
  const auto u32At = [&file](std::size_t _at)
  {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4; ++index)
    {
      value |= std::to_integer<std::uint32_t>(file[_at + index]) << (8 * index);
    }
    return value;
  };
  const auto setU32 = [&file](std::size_t _at, std::uint32_t _value)
  {
    for (std::size_t index = 0; index < 4; ++index)
    {
      file[_at + index] = static_cast<std::byte>((_value >> (8 * index)) & 0xffU);
    }
  };
  constexpr std::size_t HEADER_BYTES = 16;
  constexpr std::size_t ENTRY_BYTES = 16;
  const std::size_t body = HEADER_BYTES;
  const std::uint32_t sections = u32At(body + 8);
  std::size_t ruleEntry = 0;
  for (std::uint32_t section = 0; section < sections; ++section)
  {
    const std::size_t entry = body + 12 + section * ENTRY_BYTES;
    if (std::to_integer<char>(file[entry]) == 'R' && std::to_integer<char>(file[entry + 1]) == 'U')
    {
      ruleEntry = entry;
    }
  }
  if (ruleEntry == 0)
  {
    throw std::invalid_argument("the baked server data has no RULE section");
  }
  const std::uint32_t ruleOffset = u32At(ruleEntry + 8);
  std::size_t at = body + ruleOffset + 4;
  const std::uint32_t values = u32At(body + ruleOffset);
  for (std::uint32_t value = 0; value < values; ++value)
  {
    const std::uint32_t length = u32At(at);
    std::string key(length, '\0');
    for (std::uint32_t index = 0; index < length; ++index)
    {
      key[index] = std::to_integer<char>(file[at + 4 + index]);
    }
    const std::size_t next = at + 4 + length + 8;
    if (key == _key)
    {
      file.erase(file.begin() + static_cast<std::ptrdiff_t>(at), file.begin() + static_cast<std::ptrdiff_t>(next));
      setU32(body + ruleOffset, values - 1);
      const std::uint32_t removed = static_cast<std::uint32_t>(next - at);
      setU32(ruleEntry + 12, u32At(ruleEntry + 12) - removed);
      for (std::uint32_t section = 0; section < sections; ++section)
      {
        const std::size_t entry = body + 12 + section * ENTRY_BYTES;
        if (u32At(entry + 8) > ruleOffset)
        {
          setU32(entry + 8, u32At(entry + 8) - removed);
        }
      }
      const std::uint64_t hash = Core::Fnv1a64(std::span<const std::byte>(file).subspan(HEADER_BYTES));
      for (std::size_t index = 0; index < 8; ++index)
      {
        file[8 + index] = static_cast<std::byte>((hash >> (8 * index)) & 0xffU);
      }
      return file;
    }
    at = next;
  }
  throw std::invalid_argument("the baked server data holds no such RULE key");
}

} // namespace SimulationTests
