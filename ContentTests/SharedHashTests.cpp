#include "pch.h"

#include "Catalog.h"
#include "Hash.h"
#include "Palette.h"
#include "SharedHash.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ContentTests
{

namespace
{

void AppendU8(std::vector<std::byte>& _bytes, std::uint8_t _value)
{
  _bytes.push_back(static_cast<std::byte>(_value));
}

void AppendU32(std::vector<std::byte>& _bytes, std::uint32_t _value)
{
  for (std::uint32_t shiftBits = 0; shiftBits < 32; shiftBits += 8)
  {
    _bytes.push_back(static_cast<std::byte>((_value >> shiftBits) & 0xFFU));
  }
}

void AppendText(std::vector<std::byte>& _bytes, std::string_view _text)
{
  AppendU32(_bytes, static_cast<std::uint32_t>(_text.size()));
  const std::span<const std::byte> raw = std::as_bytes(std::span(_text.data(), _text.size()));
  _bytes.insert(_bytes.end(), raw.begin(), raw.end());
}

/// The canonical encoding of Content/SharedHash.h, written from that comment and from the compiled tables alone.
std::vector<std::byte> SharedEncoding()
{
  std::vector<std::byte> bytes;
  for (std::size_t index = 0; index < Content::PALETTE_ENTRIES; ++index)
  {
    const auto paletteIndex = static_cast<std::uint8_t>(index);
    AppendU8(bytes, static_cast<std::uint8_t>(Content::PaletteHasEntry(paletteIndex) ? 1 : 0));
    AppendU32(bytes, Content::PaletteColorSrgb(paletteIndex));
  }
  const std::span<const Content::TemplateView> templates = Content::Templates();
  AppendU32(bytes, static_cast<std::uint32_t>(templates.size()));
  for (const Content::TemplateView& view : templates)
  {
    AppendText(bytes, view.id);
    AppendText(bytes, view.name);
    AppendU32(bytes, view.grid.widthVoxels);
    AppendU32(bytes, view.grid.heightVoxels);
    AppendU32(bytes, view.grid.depthVoxels);
    AppendU32(bytes, view.grid.voxelEdgeMillimeters);
    AppendU32(bytes, view.stoneM3);
    AppendU32(bytes, view.timberM3);
    for (const std::uint8_t paletteIndex : view.grid.indices)
    {
      AppendU8(bytes, paletteIndex);
    }
  }
  return bytes;
}

} // namespace

// ADR-008, decision 5: the bake writes the shared hash into the client's tables and, from M2, into ServerData. If the
// Python encoding and the tables ever drift apart, this fails before a client and a server disagree on a template.
TEST_CLASS(SharedHashTests)
{
public:
  TEST_METHOD(RecomputesFromTheCompiledTables)
  {
    Assert::AreEqual(Content::SharedHash(), Core::Fnv1a64(SharedEncoding()));
  }
};

} // namespace ContentTests
