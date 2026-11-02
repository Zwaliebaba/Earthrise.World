#include "pch.h"

#include "Font.h"
#include "Strings.h"
#include "TextLayout.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The glyph coverage test Hud.md §9 proposes (ADR-019, decision 4): every code point of every string the client can
// draw has a glyph in Earthrise Mason. A format's {} and what is inside them take figures, which are digits.

namespace ClientTests
{

TEST_CLASS(StringsTests)
{
public:
  TEST_METHOD(HasAGlyphForEveryCodePointOfEveryString)
  {
    for (const std::string_view text : Client::ALL_STRINGS)
    {
      std::string outsideFields;
      bool inField = false;
      for (const char character : text)
      {
        inField = character == '{' ? true : (character == '}' ? false : inField);
        if (!inField && character != '}')
        {
          outsideFields.push_back(character);
        }
      }
      for (const char32_t codePoint : Client::TextLayout::CodePoints(outsideFields))
      {
        if (Content::FindGlyph(codePoint) == nullptr)
        {
          Assert::Fail(std::format(L"no glyph for U+{:04X} in a string", static_cast<std::uint32_t>(codePoint)).c_str());
        }
      }
    }
  }
};

} // namespace ClientTests
