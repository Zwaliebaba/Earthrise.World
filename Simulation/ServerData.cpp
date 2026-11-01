#include "pch.h"

#include "ServerData.h"

#include "ByteReader.h"
#include "Hash.h"

namespace Simulation
{

namespace
{

constexpr std::array<std::byte, 4> MAGIC = {std::byte{'E'}, std::byte{'R'}, std::byte{'S'}, std::byte{'D'}};
constexpr std::size_t HEADER_BYTES = 16;
constexpr std::uint32_t SECTION_VERSION = 1;
constexpr std::size_t PALETTE_ENTRIES = 256;
constexpr std::size_t MATERIAL_RECORD_BYTES = 6;
constexpr std::uint8_t GROUND_FLAG = 1U << 0;
constexpr std::uint8_t LIQUID_FLAG = 1U << 1;
constexpr std::uint8_t LAST_BEHAVIOR = static_cast<std::uint8_t>(Behavior::WasteRubble);
constexpr std::uint8_t LAST_SUPPORT = static_cast<std::uint8_t>(Support::Anchors);
constexpr std::uint8_t LAST_PART = static_cast<std::uint8_t>(ServerTemplate::Part::Stock);

/// One RULE key and the field it sets: a whole number, or a Core::Fixed raw value.
struct RuleField
{
  std::string_view key;
  std::int32_t Rules::*whole;
  Core::Fixed Rules::*fixed;
};

// Every key the simulation reads, in ascending order, as the bake writes them. A key the file holds
// and this table does not is refused, and so is one this table needs and the file lacks.
constexpr std::array<RuleField, 43> RULE_FIELDS = {{
  {"bedrockCraterVoxels", &Rules::bedrockCraterVoxels, nullptr},
  {"buildingStoneCraterVoxels", &Rules::buildingStoneCraterVoxels, nullptr},
  {"buildingStoneSpanVoxels", &Rules::buildingStoneSpanVoxels, nullptr},
  {"cavalryClimbRunCells", &Rules::cavalryClimbRunCells, nullptr},
  {"cavalryFordDepthVoxels", &Rules::cavalryFordDepthVoxels, nullptr},
  {"cavalryFordFactor", &Rules::cavalryFordFactor, nullptr},
  {"cavalryHeadroomVoxels", &Rules::cavalryHeadroomVoxels, nullptr},
  {"cavalryRubbleFactor", &Rules::cavalryRubbleFactor, nullptr},
  {"cavalrySpeedMetersPerTick", nullptr, &Rules::cavalrySpeedMetersPerTick},
  {"cavalryWidthVoxels", &Rules::cavalryWidthVoxels, nullptr},
  {"commonRockCraterVoxels", &Rules::commonRockCraterVoxels, nullptr},
  {"commonRockSpanVoxels", &Rules::commonRockSpanVoxels, nullptr},
  {"earthCraterVoxels", &Rules::earthCraterVoxels, nullptr},
  {"footClimbRunCells", &Rules::footClimbRunCells, nullptr},
  {"footFordDepthVoxels", &Rules::footFordDepthVoxels, nullptr},
  {"footFordFactor", &Rules::footFordFactor, nullptr},
  {"footHeadroomVoxels", &Rules::footHeadroomVoxels, nullptr},
  {"footRubbleFactor", &Rules::footRubbleFactor, nullptr},
  {"footSpeedMetersPerTick", nullptr, &Rules::footSpeedMetersPerTick},
  {"footWidthVoxels", &Rules::footWidthVoxels, nullptr},
  {"gravityMetersPerTickSquared", nullptr, &Rules::gravityMetersPerTickSquared},
  {"leavesCraterVoxels", &Rules::leavesCraterVoxels, nullptr},
  {"leavesSpanVoxels", &Rules::leavesSpanVoxels, nullptr},
  {"looseRestBelowTopVoxels", &Rules::looseRestBelowTopVoxels, nullptr},
  {"machinesClimbRunCells", &Rules::machinesClimbRunCells, nullptr},
  {"machinesFordDepthVoxels", &Rules::machinesFordDepthVoxels, nullptr},
  {"machinesFordFactor", &Rules::machinesFordFactor, nullptr},
  {"machinesHeadroomVoxels", &Rules::machinesHeadroomVoxels, nullptr},
  {"machinesRubbleFactor", &Rules::machinesRubbleFactor, nullptr},
  {"machinesSpeedMetersPerTick", nullptr, &Rules::machinesSpeedMetersPerTick},
  {"machinesWidthVoxels", &Rules::machinesWidthVoxels, nullptr},
  {"pathNodesPerTick", &Rules::pathNodesPerTick, nullptr},
  {"shotLaunchHeightVoxels", &Rules::shotLaunchHeightVoxels, nullptr},
  {"shotMaxRangeVoxels", &Rules::shotMaxRangeVoxels, nullptr},
  {"shotMinRangeVoxels", &Rules::shotMinRangeVoxels, nullptr},
  {"shotReloadTicks", &Rules::shotReloadTicks, nullptr},
  {"slumpVoxelsPerTick", &Rules::slumpVoxelsPerTick, nullptr},
  {"stoneRubbleCraterVoxels", &Rules::stoneRubbleCraterVoxels, nullptr},
  {"timberCraterVoxels", &Rules::timberCraterVoxels, nullptr},
  {"timberSpanVoxels", &Rules::timberSpanVoxels, nullptr},
  {"warningBaseTicks", &Rules::warningBaseTicks, nullptr},
  {"wasteRubbleCraterVoxels", &Rules::wasteRubbleCraterVoxels, nullptr},
  {"waterCraterVoxels", &Rules::waterCraterVoxels, nullptr},
}};

[[noreturn]] void Refuse(const std::string& _check)
{
  throw std::runtime_error("ServerData: " + _check);
}

std::string Text(Core::ByteReader& _reader)
{
  const std::uint32_t length = _reader.ReadU32();
  const std::span<const std::byte> bytes = _reader.ReadBytes(length);
  std::string text(bytes.size(), '\0');
  std::ranges::transform(bytes, text.begin(), [](std::byte _byte) { return static_cast<char>(_byte); });
  return text;
}

MaterialRules ReadMaterials(std::span<const std::byte> _section)
{
  if (_section.size() != PALETTE_ENTRIES * MATERIAL_RECORD_BYTES)
  {
    Refuse("MATL holds " + std::to_string(_section.size()) + " bytes, not 256 records of 6");
  }
  Core::ByteReader reader(_section);
  std::array<bool, PALETTE_ENTRIES> exists{};
  MaterialRules rules{};
  for (std::size_t index = 0; index < PALETTE_ENTRIES; ++index)
  {
    const std::uint8_t present = reader.ReadU8();
    const std::uint8_t behavior = reader.ReadU8();
    const std::uint8_t support = reader.ReadU8();
    const std::uint8_t span = reader.ReadU8();
    const std::uint8_t flags = reader.ReadU8();
    const std::uint8_t rubble = reader.ReadU8();
    if (present > 1 || behavior > LAST_BEHAVIOR || support > LAST_SUPPORT || (flags & ~(GROUND_FLAG | LIQUID_FLAG)) != 0 ||
        span == UINT8_MAX)
    {
      Refuse("MATL's record for index " + std::to_string(index) + " holds a value this build does not know");
    }
    exists.at(index) = present == 1;
    rules.at(index) = {.behavior = static_cast<Behavior>(behavior),
                       .support = static_cast<Support>(support),
                       .spanVoxels = span,
                       .walkableGround = (flags & GROUND_FLAG) != 0,
                       .liquid = (flags & LIQUID_FLAG) != 0,
                       .rubbleIndex = rubble};
  }
  for (std::size_t index = 0; index < PALETTE_ENTRIES; ++index)
  {
    if (rules.at(index).rubbleIndex != 0 && !exists.at(rules.at(index).rubbleIndex))
    {
      Refuse("MATL gives index " + std::to_string(index) + " a rubble index the palette does not have");
    }
  }
  return rules;
}

std::vector<ServerTemplate> ReadTemplates(std::span<const std::byte> _section)
{
  Core::ByteReader reader(_section);
  const std::uint32_t count = reader.ReadU32();
  std::vector<ServerTemplate> templates;
  for (std::uint32_t number = 0; number < count; ++number)
  {
    ServerTemplate shape{.id = Text(reader),
                         .name = Text(reader),
                         .criticalPart = Text(reader),
                         .widthVoxels = reader.ReadU32(),
                         .heightVoxels = reader.ReadU32(),
                         .depthVoxels = reader.ReadU32(),
                         .voxelEdgeMillimeters = reader.ReadU32(),
                         .stoneM3 = reader.ReadU32(),
                         .timberM3 = reader.ReadU32(),
                         .indices = {},
                         .parts = {}};
    const std::uint64_t volume = std::uint64_t{shape.widthVoxels} * shape.heightVoxels * shape.depthVoxels;
    if (volume == 0 || 2 * volume > reader.Remaining())
    {
      Refuse("TMPL's template " + shape.id + " is larger than its section");
    }
    const std::span<const std::byte> indices = reader.ReadBytes(static_cast<std::size_t>(volume));
    shape.indices.resize(indices.size());
    std::ranges::transform(indices, shape.indices.begin(), [](std::byte _byte) { return static_cast<std::uint8_t>(_byte); });
    for (const std::byte part : reader.ReadBytes(static_cast<std::size_t>(volume)))
    {
      if (static_cast<std::uint8_t>(part) > LAST_PART)
      {
        Refuse("TMPL's template " + shape.id + " holds a part this build does not know");
      }
      shape.parts.push_back(static_cast<ServerTemplate::Part>(part));
    }
    if (!templates.empty() && templates.back().id >= shape.id)
    {
      Refuse("TMPL's templates are not in ascending id at " + shape.id);
    }
    templates.push_back(std::move(shape));
  }
  if (!reader.AtEnd())
  {
    Refuse("TMPL holds bytes after its last template");
  }
  return templates;
}

Rules ReadRules(std::span<const std::byte> _section)
{
  Core::ByteReader reader(_section);
  const std::uint32_t count = reader.ReadU32();
  Rules rules{};
  std::vector<std::string_view> found;
  std::string previous;
  for (std::uint32_t number = 0; number < count; ++number)
  {
    const std::string key = Text(reader);
    const std::int64_t value = reader.ReadI64();
    if (number > 0 && key <= previous)
    {
      Refuse("RULE's keys are not in ascending order at " + key);
    }
    previous = key;
    const auto field = std::ranges::find(RULE_FIELDS, std::string_view(key), &RuleField::key);
    if (field == RULE_FIELDS.end())
    {
      Refuse("RULE holds the key " + key + ", which this build does not know");
    }
    if (value < INT32_MIN || value > INT32_MAX || (field->whole != nullptr && value < 0))
    {
      Refuse("RULE's " + key + " is " + std::to_string(value) + ", outside what its field holds");
    }
    if (field->whole != nullptr)
    {
      rules.*(field->whole) = static_cast<std::int32_t>(value);
    }
    else
    {
      rules.*(field->fixed) = Core::Fixed::FromRaw(static_cast<std::int32_t>(value));
    }
    found.push_back(field->key);
  }
  if (!reader.AtEnd())
  {
    Refuse("RULE holds bytes after its last value");
  }
  for (const RuleField& field : RULE_FIELDS)
  {
    if (std::ranges::find(found, field.key) == found.end())
    {
      Refuse("RULE lacks the key " + std::string(field.key) + ", which this build needs");
    }
  }
  return rules;
}

} // namespace

TemplateShape ServerTemplate::Shape() const noexcept
{
  return {.widthVoxels = widthVoxels, .heightVoxels = heightVoxels, .depthVoxels = depthVoxels, .indices = indices};
}

ServerData ServerData::Read(std::span<const std::byte> _file)
{
  if (_file.size() < HEADER_BYTES || !std::ranges::equal(_file.first(MAGIC.size()), MAGIC))
  {
    Refuse("the file does not begin with the magic number ERSD");
  }
  Core::ByteReader header(_file.subspan(MAGIC.size(), HEADER_BYTES - MAGIC.size()));
  const std::uint32_t version = header.ReadU32();
  if (version != FORMAT_VERSION)
  {
    Refuse("the format version is " + std::to_string(version) + ", and this build reads " + std::to_string(FORMAT_VERSION));
  }
  ServerData data;
  data.m_hash = header.ReadU64();
  const std::span<const std::byte> body = _file.subspan(HEADER_BYTES);
  if (Core::Fnv1a64(body) != data.m_hash)
  {
    Refuse("the server-data hash does not match the body, so the file is corrupt");
  }

  // The body: the shared hash, then a directory of sections whose offsets count from the body's start.
  try
  {
    Core::ByteReader reader(body);
    data.m_sharedHash = reader.ReadU64();
    const std::uint32_t count = reader.ReadU32();
    std::map<std::string, std::span<const std::byte>> sections;
    for (std::uint32_t entry = 0; entry < count; ++entry)
    {
      const std::span<const std::byte> tagBytes = reader.ReadBytes(4);
      std::string tag(4, '\0');
      std::ranges::transform(tagBytes, tag.begin(), [](std::byte _byte) { return static_cast<char>(_byte); });
      const std::uint32_t sectionVersion = reader.ReadU32();
      const std::uint32_t offset = reader.ReadU32();
      const std::uint32_t length = reader.ReadU32();
      if (sectionVersion != SECTION_VERSION)
      {
        Refuse("section " + tag + " is at version " + std::to_string(sectionVersion) + ", and this build reads " +
               std::to_string(SECTION_VERSION));
      }
      if (std::uint64_t{offset} + length > body.size())
      {
        Refuse("section " + tag + " runs past the end of the file");
      }
      if (!sections.emplace(tag, body.subspan(offset, length)).second)
      {
        Refuse("section " + tag + " appears twice");
      }
    }
    for (const char* tag : {"MATL", "TMPL", "RULE"})
    {
      if (!sections.contains(tag))
      {
        Refuse(std::string("the file has no ") + tag + " section");
      }
    }
    if (sections.size() != 3)
    {
      Refuse("the file holds a section this build does not know");
    }
    data.m_materials = ReadMaterials(sections.at("MATL"));
    data.m_templates = ReadTemplates(sections.at("TMPL"));
    data.m_rules = ReadRules(sections.at("RULE"));
  }
  catch (const std::out_of_range&)
  {
    Refuse("a section ends before its records do");
  }
  return data;
}

ServerData ServerData::Load(const std::filesystem::path& _file)
{
  std::ifstream stream(_file, std::ios::binary);
  if (!stream)
  {
    Refuse("cannot open " + _file.generic_string() + "; the server bake writes it into the build's ServerData folder");
  }
  const std::vector<char> raw{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
  if (stream.bad())
  {
    Refuse("cannot read " + _file.generic_string());
  }
  return Read(std::as_bytes(std::span(raw)));
}

std::uint64_t ServerData::Hash() const noexcept
{
  return m_hash;
}

std::uint64_t ServerData::SharedHash() const noexcept
{
  return m_sharedHash;
}

const MaterialRules& ServerData::Materials() const noexcept
{
  return m_materials;
}

const Rules& ServerData::Tuning() const noexcept
{
  return m_rules;
}

std::span<const ServerTemplate> ServerData::TemplateList() const noexcept
{
  return m_templates;
}

const ServerTemplate* ServerData::FindTemplate(std::string_view _id) const noexcept
{
  const auto found = std::ranges::find(m_templates, _id, &ServerTemplate::id);
  return found == m_templates.end() ? nullptr : &*found;
}

TemplateLookup ServerData::Templates() const
{
  return [this](std::string_view _id) -> std::optional<TemplateShape>
  {
    const ServerTemplate* found = FindTemplate(_id);
    if (found == nullptr)
    {
      return std::nullopt;
    }
    return found->Shape();
  };
}

} // namespace Simulation
