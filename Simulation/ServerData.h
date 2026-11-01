#pragma once

#include "MaterialRules.h"
#include "Rules.h"
#include "TestWorld.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Simulation
{

/// A template as the server sees it (ADR-008, decision 2; ADR-016, TMPL): the client's view, its
/// palette indices x fastest, then z, then y, mirrored in x from the layer file (ADR-017), with each
/// voxel's part and the name of its critical part.
struct ServerTemplate
{
  /// A voxel's part, as the layer files number them.
  enum class Part : std::uint8_t
  {
    None,
    Critical,
    Foundation,
    Stock
  };

  std::string id;
  std::string name;
  std::string criticalPart; // empty when the template names none
  std::uint32_t widthVoxels;
  std::uint32_t heightVoxels;
  std::uint32_t depthVoxels;
  std::uint32_t voxelEdgeMillimeters;
  std::uint32_t stoneM3;
  std::uint32_t timberM3;
  std::vector<std::uint8_t> indices;
  std::vector<Part> parts;

  [[nodiscard]] TemplateShape Shape() const noexcept;
};

/// The server's data, read from <outputdir>\ServerData\ServerData.bin (ADR-016): each palette
/// index's rule, the templates and the tuning values, with the server-data hash that saves and
/// replays record and the shared hash that both halves hold (ADR-008, decision 5).
///
/// The reader checks the magic number, the format version, the server-data hash, recomputed over
/// the body, and each section's version and layout, and refuses anything else with a
/// std::runtime_error naming the check that failed. It runs none of the content checks again: those
/// ran at bake time (ADR-016, decision 6).
class ServerData
{
public:
  static constexpr std::uint32_t FORMAT_VERSION = 1;

  /// Reads a file's bytes. Throws std::runtime_error naming the failed check.
  [[nodiscard]] static ServerData Read(std::span<const std::byte> _file);

  /// Reads the file at _file, which the caller finds: Simulation includes no Windows header, so it
  /// cannot find its own executable (ADR-016, decision 6). Throws std::runtime_error when the file
  /// cannot be read, or as Read does.
  [[nodiscard]] static ServerData Load(const std::filesystem::path& _file);

  /// The server-data hash: Core::Fnv1a64 over the body. Saves and replays record it (ADR-014).
  [[nodiscard]] std::uint64_t Hash() const noexcept;

  /// The shared hash of the palette and the templates, which Content compiles in too.
  [[nodiscard]] std::uint64_t SharedHash() const noexcept;

  [[nodiscard]] const MaterialRules& Materials() const noexcept;
  [[nodiscard]] const Rules& Tuning() const noexcept;

  /// Every template, in ascending id.
  [[nodiscard]] std::span<const ServerTemplate> TemplateList() const noexcept;

  /// The template with _id, or null.
  [[nodiscard]] const ServerTemplate* FindTemplate(std::string_view _id) const noexcept;

  /// The templates as the test world and the generator take them. The lookup refers to this object,
  /// which must outlive it and not move.
  [[nodiscard]] TemplateLookup Templates() const;

private:
  ServerData() = default;

  std::uint64_t m_hash = 0;
  std::uint64_t m_sharedHash = 0;
  MaterialRules m_materials{};
  Rules m_rules{};
  std::vector<ServerTemplate> m_templates;
};

} // namespace Simulation
