#include "pch.h"

#include "Hud.h"

#include "Font.h"
#include "HudAlerts.h"
#include "Look.h"
#include "Portraits.h"
#include "Strings.h"
#include "TextLayout.h"
#include "UiScale.h"

namespace Client
{

namespace
{

// Hud.md §3's layout and the HUD prototype's spacing, in logical pixels.
constexpr float BORDER_LOGICAL_PIXELS = 2.0F;
constexpr float TOP_BAR_LEFT_LOGICAL_PIXELS = 12.0F;
constexpr float TOP_BAR_RIGHT_LOGICAL_PIXELS = 6.0F;
constexpr float RESOURCE_GAP_LOGICAL_PIXELS = 20.0F; // between resources, and around the objective
constexpr float KEY_GAP_LOGICAL_PIXELS = 8.0F;
constexpr float OBJECTIVE_GAP_LOGICAL_PIXELS = 10.0F;
constexpr float CONTROL_HEIGHT_LOGICAL_PIXELS = 44.0F; // CANCEL · ESC and JUMP
constexpr float BANNER_TOP_LOGICAL_PIXELS = 56.0F;
constexpr float BANNER_PADDING_Y_LOGICAL_PIXELS = 6.0F;
constexpr float BANNER_PADDING_LEFT_LOGICAL_PIXELS = 14.0F;
constexpr float BANNER_PADDING_RIGHT_LOGICAL_PIXELS = 6.0F;
constexpr float BANNER_GAP_LOGICAL_PIXELS = 12.0F;
constexpr float CANCEL_PADDING_LOGICAL_PIXELS = 12.0F;
constexpr float ALERTS_LEFT_LOGICAL_PIXELS = 8.0F;
constexpr float ALERTS_TOP_LOGICAL_PIXELS = 110.0F;
constexpr float ALERTS_GAP_LOGICAL_PIXELS = 6.0F;
constexpr float ALERT_PADDING_X_LOGICAL_PIXELS = 10.0F;
constexpr float ALERT_PADDING_Y_LOGICAL_PIXELS = 8.0F;
constexpr float ALERT_GAP_LOGICAL_PIXELS = 10.0F;
constexpr float ALERT_LINE_GAP_LOGICAL_PIXELS = 4.0F;
constexpr float ALERT_MAX_WIDTH_LOGICAL_PIXELS = 440.0F;
constexpr float JUMP_PADDING_LOGICAL_PIXELS = 10.0F;
constexpr float RING_LOGICAL_PIXELS = 24.0F;
constexpr float CURSOR_LABEL_OFFSET_LOGICAL_PIXELS = 18.0F; // from the pointer to the label's panel
constexpr float CURSOR_PADDING_X_LOGICAL_PIXELS = 6.0F;
constexpr float CURSOR_PADDING_Y_LOGICAL_PIXELS = 4.0F;
constexpr float CHIP_PADDING_LOGICAL_PIXELS = 10.0F; // the selection's chip, on each side of its text (M3-H3)
// The integrity readout (M3-H1): docked in the bottom-left corner, at least Hud.md §3's readout width, and as wide as
// the paper test's longest row.
constexpr float READOUT_LEFT_LOGICAL_PIXELS = 8.0F;
constexpr float READOUT_BOTTOM_LOGICAL_PIXELS = 8.0F;
constexpr float READOUT_MIN_WIDTH_LOGICAL_PIXELS = 340.0F;
constexpr float READOUT_PADDING_LOGICAL_PIXELS = 8.0F;
constexpr float READOUT_LINE_GAP_LOGICAL_PIXELS = 4.0F;
constexpr float READOUT_BAR_GAP_LOGICAL_PIXELS = 3.0F;
constexpr float READOUT_BAR_HEIGHT_LOGICAL_PIXELS = 4.0F;
// The UI scales the readout steps down through where it would be too tall (Hud.md §2's bands).
constexpr std::array<float, 3> READOUT_SCALES = {2.0F, 1.5F, 1.0F};
// Layout A (Hud.md §3; M4-I1): the deck's padding and the gap between its columns, the minimap's well, the selection at
// its narrowest, the card's 4 x 3 cells, 100 to 112 wide and 60 high with gaps of 4, and the strip of groups 1 to 5 and
// SET above it, at left 212 and bottom 212. With the deck, the readout stands at right 416 and bottom 212, as M6's
// placement and dig readouts will, and above the strip where the two would meet, as they do at 1280 px.
constexpr float DECK_PADDING_LOGICAL_PIXELS = 8.0F;
constexpr float DECK_GAP_LOGICAL_PIXELS = 8.0F;
constexpr float MINIMAP_LOGICAL_PIXELS = 188.0F;
constexpr float SELECTION_MIN_LOGICAL_PIXELS = 340.0F;
constexpr std::int32_t CARD_COLUMNS = 4;
constexpr std::int32_t CARD_ROWS = 3;
constexpr float CELL_MIN_WIDTH_LOGICAL_PIXELS = 100.0F;
constexpr float CELL_MAX_WIDTH_LOGICAL_PIXELS = 112.0F;
constexpr float CELL_HEIGHT_LOGICAL_PIXELS = 60.0F;
constexpr float CELL_GAP_LOGICAL_PIXELS = 4.0F;
constexpr float STRIP_LEFT_LOGICAL_PIXELS = 212.0F;
constexpr float STRIP_BOTTOM_LOGICAL_PIXELS = 212.0F;
constexpr float GROUP_WIDTH_LOGICAL_PIXELS = 120.0F;
constexpr float GROUP_HEIGHT_LOGICAL_PIXELS = 44.0F;
constexpr float SET_WIDTH_LOGICAL_PIXELS = 64.0F;
constexpr float DECK_READOUT_RIGHT_LOGICAL_PIXELS = 416.0F;
constexpr float DECK_READOUT_BOTTOM_LOGICAL_PIXELS = 212.0F;
constexpr float ABOVE_STRIP_GAP_LOGICAL_PIXELS = 8.0F;
// The selection panel (Hud.md §5; M4-I2): its padding; one thing's 96 px portrait, the hit-point bar 120 x 10 and the
// gaps between lines; several things' tiles, 80 x 84, with a 40 px portrait.
constexpr float PANEL_PADDING_LOGICAL_PIXELS = 8.0F;
constexpr float PORTRAIT_LOGICAL_PIXELS = 96.0F;
constexpr float HIT_BAR_WIDTH_LOGICAL_PIXELS = 120.0F;
constexpr float HIT_BAR_HEIGHT_LOGICAL_PIXELS = 10.0F;
constexpr float PANEL_LINE_GAP_LOGICAL_PIXELS = 4.0F;
constexpr float TILE_WIDTH_LOGICAL_PIXELS = 80.0F;
constexpr float TILE_HEIGHT_LOGICAL_PIXELS = 84.0F;
constexpr float TILE_PORTRAIT_LOGICAL_PIXELS = 40.0F;
constexpr float TILE_GAP_LOGICAL_PIXELS = 8.0F;
constexpr float TILE_PADDING_LOGICAL_PIXELS = 4.0F;
constexpr float TILE_BAR_HEIGHT_LOGICAL_PIXELS = 6.0F;
// The command card's cells (Hud.md §7; M4-I3): the hotkey's inset from the top-left corner and the label's from the
// bottom; and the tooltip above the card, right 8 and bottom 212, 400 wide, its hotkey and label on one line and its
// rule on the next.
constexpr float CELL_INSET_LOGICAL_PIXELS = 4.0F;
constexpr float TOOLTIP_RIGHT_LOGICAL_PIXELS = 8.0F;
constexpr float TOOLTIP_BOTTOM_LOGICAL_PIXELS = 212.0F;
constexpr float TOOLTIP_WIDTH_LOGICAL_PIXELS = 400.0F;
constexpr float TOOLTIP_PADDING_LOGICAL_PIXELS = 8.0F;
constexpr std::string_view TOOLTIP_SEPARATOR = " · ";

// A cap stands on rows 1 to 7 of the font's cell (Hud.md §9), so text is centred on that band.
constexpr std::int32_t CAP_TOP_ROW = 1;
constexpr std::int32_t CAP_ROWS = static_cast<std::int32_t>(Content::FONT_BASELINE_ROW);

/// Draws the HUD's pieces at one UI scale and records what it drew.
class Painter
{
public:
  Painter(float _uiScale, std::vector<ScreenQuad>& _quads, std::vector<HudText>& _texts)
    : m_uiScale(_uiScale),
      m_scale(TextLayout::PixelsPerFontPixel(TextSize::Label, _uiScale)),
      m_nameScale(TextLayout::PixelsPerFontPixel(TextSize::Name, _uiScale)),
      m_quads(_quads),
      m_texts(_texts)
  {
  }

  [[nodiscard]] std::int32_t Pixels(float _logicalPixels) const noexcept
  {
    return ScaledPixels(_logicalPixels, m_uiScale);
  }

  [[nodiscard]] std::int32_t Width(std::string_view _text) const
  {
    return static_cast<std::int32_t>(TextLayout::Measure(_text, m_scale));
  }

  [[nodiscard]] std::int32_t Cell() const noexcept
  {
    return static_cast<std::int32_t>(TextLayout::CellPixels(m_scale));
  }

  /// How far _text moves the pen, its last glyph's gap included: where the text after it starts.
  [[nodiscard]] std::int32_t Advance(std::string_view _text) const
  {
    return static_cast<std::int32_t>(TextLayout::Advance(_text, m_scale));
  }

  [[nodiscard]] std::int32_t NameWidth(std::string_view _text) const
  {
    return static_cast<std::int32_t>(TextLayout::Measure(_text, m_nameScale));
  }

  [[nodiscard]] std::int32_t NameCell() const noexcept
  {
    return static_cast<std::int32_t>(TextLayout::CellPixels(m_nameScale));
  }

  /// The top of a line's cell whose caps are centred in the band _heightPixels high from _topPixels.
  [[nodiscard]] std::int32_t CenteredCellTop(std::int32_t _topPixels, std::int32_t _heightPixels) const noexcept
  {
    const auto scale = static_cast<std::int32_t>(m_scale);
    return _topPixels + ((_heightPixels - (CAP_ROWS * scale)) / 2) - (CAP_TOP_ROW * scale);
  }

  void Fill(const PixelRect& _rect, std::uint32_t _srgb, float _alpha = 1.0F)
  {
    m_quads.push_back({.xPixels = static_cast<float>(_rect.xPixels),
                       .yPixels = static_cast<float>(_rect.yPixels),
                       .widthPixels = static_cast<float>(_rect.widthPixels),
                       .heightPixels = static_cast<float>(_rect.heightPixels),
                       .u0 = 0.0F,
                       .v0 = 0.0F,
                       .u1 = 0.0F,
                       .v1 = 0.0F,
                       .color = LinearColor(_srgb, _alpha),
                       .textured = false});
  }

  /// A box with a border: the inside filled, and the border drawn as four strips, so a translucent inside never shows
  /// the border's colour through it. Returns the inside.
  PixelRect Box(const PixelRect& _rect, std::uint32_t _borderSrgb, std::uint32_t _fillSrgb, float _fillAlpha)
  {
    const std::int32_t border = Pixels(BORDER_LOGICAL_PIXELS);
    const PixelRect inside{_rect.xPixels + border, _rect.yPixels + border, _rect.widthPixels - (2 * border),
                           _rect.heightPixels - (2 * border)};
    Fill(inside, _fillSrgb, _fillAlpha);
    Fill({_rect.xPixels, _rect.yPixels, _rect.widthPixels, border}, _borderSrgb);
    Fill({_rect.xPixels, _rect.yPixels + _rect.heightPixels - border, _rect.widthPixels, border}, _borderSrgb);
    Fill({_rect.xPixels, inside.yPixels, border, inside.heightPixels}, _borderSrgb);
    Fill({inside.xPixels + inside.widthPixels, inside.yPixels, border, inside.heightPixels}, _borderSrgb);
    return inside;
  }

  /// _text with its cell's top-left corner at (_xPixels, _yPixels), which has to fit in _element.
  void Text(std::string_view _text, std::int32_t _xPixels, std::int32_t _yPixels, std::uint32_t _srgb, const PixelRect& _element)
  {
    TextLayout::Lay(_text, _xPixels, _yPixels, m_scale, LinearColor(_srgb), m_quads);
    m_texts.push_back({.cell = {_xPixels, _yPixels, Width(_text), Cell()}, .element = _element});
  }

  /// _text at the names' size.
  void NameText(std::string_view _text, std::int32_t _xPixels, std::int32_t _yPixels, std::uint32_t _srgb, const PixelRect& _element)
  {
    TextLayout::Lay(_text, _xPixels, _yPixels, m_nameScale, LinearColor(_srgb), m_quads);
    m_texts.push_back({.cell = {_xPixels, _yPixels, NameWidth(_text), NameCell()}, .element = _element});
  }

  /// The image atlas's _cell, a portrait (Portraits.h), drawn over _rect.
  void Image(const PixelRect& _rect, std::uint32_t _cell)
  {
    const auto left = static_cast<float>(_cell * PORTRAIT_TEXELS);
    m_quads.push_back({.xPixels = static_cast<float>(_rect.xPixels),
                       .yPixels = static_cast<float>(_rect.yPixels),
                       .widthPixels = static_cast<float>(_rect.widthPixels),
                       .heightPixels = static_cast<float>(_rect.heightPixels),
                       .u0 = left,
                       .v0 = 0.0F,
                       .u1 = left + static_cast<float>(PORTRAIT_TEXELS),
                       .v1 = static_cast<float>(PORTRAIT_TEXELS),
                       .color = LinearColor(IMAGE_TINT_SRGB),
                       .textured = false,
                       .image = true});
  }

  /// A bar _rect wide of the track, with _fraction of it filled in _srgb.
  void Bar(const PixelRect& _rect, float _fraction, std::uint32_t _srgb)
  {
    Fill(_rect, LINE_SRGB);
    const auto filled = static_cast<std::int32_t>(std::lround(std::clamp(_fraction, 0.0F, 1.0F) * static_cast<float>(_rect.widthPixels)));
    if (filled > 0)
    {
      Fill({_rect.xPixels, _rect.yPixels, filled, _rect.heightPixels}, _srgb);
    }
  }

  /// A portrait in _rect: the figure's render, or the structure's icon centred at the names' size.
  void Portrait(const PixelRect& _rect, const HudPortrait& _portrait)
  {
    const PixelRect inside = Box(_rect, LINE_SRGB, BUTTON_SRGB, 1.0F);
    if (_portrait.cell.has_value())
    {
      Image(inside, *_portrait.cell);
    }
    else if (!_portrait.icon.empty())
    {
      NameText(_portrait.icon, inside.xPixels + ((inside.widthPixels - NameWidth(_portrait.icon)) / 2),
               inside.yPixels + ((inside.heightPixels - NameCell()) / 2), TEXT_SRGB, inside);
    }
  }

  /// A button: a bordered box with _text centred in it.
  void Button(const PixelRect& _rect, std::string_view _text)
  {
    const PixelRect inside = Box(_rect, LINE_SRGB, BUTTON_SRGB, 1.0F);
    Text(_text, _rect.xPixels + ((_rect.widthPixels - Width(_text)) / 2), CenteredCellTop(_rect.yPixels, _rect.heightPixels), TEXT_SRGB,
         inside);
  }

private:
  float m_uiScale;
  std::uint32_t m_scale;
  std::uint32_t m_nameScale;
  std::vector<ScreenQuad>& m_quads;
  std::vector<HudText>& m_texts;
};

/// The readout's size at one scale.
struct ReadoutSize
{
  std::int32_t widthPixels;
  std::int32_t heightPixels;
  std::int32_t contentWidthPixels;
};

ReadoutSize MeasureReadout(const HudReadout& _readout, const Painter& _paint)
{
  const std::int32_t border = _paint.Pixels(BORDER_LOGICAL_PIXELS);
  const std::int32_t padding = _paint.Pixels(READOUT_PADDING_LOGICAL_PIXELS);
  std::int32_t content = _paint.NameWidth(_readout.title);
  std::int32_t height = (2 * border) + (2 * padding) + _paint.NameCell();
  for (const HudReadoutRow& row : _readout.rows)
  {
    const std::int32_t rowWidth = row.value.empty() ? _paint.Width(row.key) : _paint.Advance(row.key + " ") + _paint.Width(row.value);
    content = std::max(content, rowWidth);
    height += _paint.Pixels(READOUT_LINE_GAP_LOGICAL_PIXELS) + _paint.Cell();
    if (row.fill.has_value())
    {
      height += _paint.Pixels(READOUT_BAR_GAP_LOGICAL_PIXELS) + _paint.Pixels(READOUT_BAR_HEIGHT_LOGICAL_PIXELS);
    }
  }
  const std::int32_t width = std::max(_paint.Pixels(READOUT_MIN_WIDTH_LOGICAL_PIXELS), (2 * border) + (2 * padding) + content);
  return {.widthPixels = width, .heightPixels = height, .contentWidthPixels = width - (2 * border) - (2 * padding)};
}

/// The selection panel in _well (Hud.md §5; M4-I2): NOTHING SELECTED and its hints; one thing's portrait beside its
/// name and owner tag, its figures and hit points, and its rows; or the several things' title and their tiles, row
/// after row, as many as the well holds.
void LayPanel(const HudPanel& _panel, const PixelRect& _well, Painter& _paint)
{
  const std::int32_t border = _paint.Pixels(BORDER_LOGICAL_PIXELS);
  const std::int32_t padding = _paint.Pixels(PANEL_PADDING_LOGICAL_PIXELS);
  const std::int32_t gap = _paint.Pixels(PANEL_LINE_GAP_LOGICAL_PIXELS);
  const PixelRect inside{_well.xPixels + border, _well.yPixels + border, _well.widthPixels - (2 * border),
                         _well.heightPixels - (2 * border)};
  const std::int32_t left = inside.xPixels + padding;
  const std::int32_t right = inside.xPixels + inside.widthPixels;
  const std::int32_t bottom = inside.yPixels + inside.heightPixels;
  std::int32_t y = inside.yPixels + padding;
  if (_panel.shows == HudPanel::Shows::Nothing)
  {
    _paint.Text(NOTHING_SELECTED, left, y, TEXT_SRGB, inside);
    for (const std::string_view hint : {HINT_CLICK, HINT_DRAG, HINT_GROUPS})
    {
      y += _paint.Cell() + gap;
      _paint.Text(hint, left, y, DIM_SRGB, inside);
    }
    return;
  }
  if (_panel.shows == HudPanel::Shows::One)
  {
    const std::int32_t side = _paint.Pixels(PORTRAIT_LOGICAL_PIXELS);
    _paint.Portrait({left, y, side, side}, _panel.portrait);
    // The column beside the portrait reaches the well's inner edge less half its padding, so that a 220 px row fits
    // the narrowest selection's 340 (Hud.md §5).
    const std::int32_t columnLeft = left + side + padding;
    const PixelRect column{columnLeft, y, right - (padding / 2) - columnLeft, bottom - y};
    std::int32_t line = column.yPixels;
    _paint.NameText(_panel.name, column.xPixels, line, TEXT_SRGB, column);
    const std::string_view tag = _panel.enemy ? ENEMY : YOURS;
    const std::uint32_t tagSrgb = _panel.enemy ? BAD_SRGB : ACCENT_SRGB;
    const std::int32_t tagLeft = column.xPixels + _paint.NameWidth(_panel.name) + _paint.Pixels(KEY_GAP_LOGICAL_PIXELS);
    if (tagLeft + _paint.Width(tag) <= column.xPixels + column.widthPixels)
    {
      // On the name's line, standing on its foot.
      _paint.Text(tag, tagLeft, line + _paint.NameCell() - _paint.Cell(), tagSrgb, column);
      line += _paint.NameCell() + gap;
    }
    else
    {
      line += _paint.NameCell() + gap;
      _paint.Text(tag, column.xPixels, line, tagSrgb, column);
      line += _paint.Cell() + gap;
    }
    if (!_panel.figures.empty() || _panel.hit.has_value())
    {
      std::int32_t x = column.xPixels;
      if (!_panel.figures.empty())
      {
        _paint.Text(_panel.figures, x, line, TEXT_SRGB, column);
        x += _paint.Width(_panel.figures) + _paint.Pixels(KEY_GAP_LOGICAL_PIXELS);
      }
      if (_panel.hit.has_value())
      {
        const std::int32_t barHeight = _paint.Pixels(HIT_BAR_HEIGHT_LOGICAL_PIXELS);
        const std::int32_t barWidth = std::min(_paint.Pixels(HIT_BAR_WIDTH_LOGICAL_PIXELS), column.xPixels + column.widthPixels - x);
        _paint.Bar({x, line + ((_paint.Cell() - barHeight) / 2), barWidth, barHeight}, *_panel.hit, GOOD_SRGB);
        x += barWidth + _paint.Pixels(KEY_GAP_LOGICAL_PIXELS);
        if (!_panel.hitText.empty() && x + _paint.Width(_panel.hitText) <= column.xPixels + column.widthPixels)
        {
          _paint.Text(_panel.hitText, x, line, SUB_SRGB, column);
        }
      }
      line += _paint.Cell() + gap;
    }
    for (std::size_t index = 0; index < std::min<std::size_t>(_panel.rows.size(), 3); ++index)
    {
      if (line + _paint.Cell() > bottom)
      {
        break;
      }
      const HudStatRow& row = _panel.rows.at(index);
      _paint.Text(row.key, column.xPixels, line, DIM_SRGB, column);
      _paint.Text(row.value, column.xPixels + _paint.Advance(row.key + " "), line, row.srgb, column);
      line += _paint.Cell() + gap;
    }
    return;
  }
  _paint.Text(_panel.title, left, y, TEXT_SRGB, inside);
  y += _paint.Cell() + gap;
  const std::int32_t tileWidth = _paint.Pixels(TILE_WIDTH_LOGICAL_PIXELS);
  const std::int32_t tileHeight = _paint.Pixels(TILE_HEIGHT_LOGICAL_PIXELS);
  const std::int32_t tileGap = _paint.Pixels(TILE_GAP_LOGICAL_PIXELS);
  const std::int32_t tilePadding = _paint.Pixels(TILE_PADDING_LOGICAL_PIXELS);
  const std::int32_t portrait = _paint.Pixels(TILE_PORTRAIT_LOGICAL_PIXELS);
  std::int32_t x = left;
  for (const HudPanelTile& tile : _panel.tiles)
  {
    if (x + tileWidth > right - padding)
    {
      x = left;
      y += tileHeight + tileGap;
    }
    if (y + tileHeight > bottom)
    {
      break; // the panel scrolls when its content overflows (Hud.md §3), which M4 leaves to its owner's look
    }
    const PixelRect rect{x, y, tileWidth, tileHeight};
    const PixelRect tileInside = _paint.Box(rect, tile.active ? ACCENT_SRGB : LINE_SRGB, PANEL_SRGB, PANEL_ALPHA);
    const PixelRect picture{tileInside.xPixels + tilePadding, tileInside.yPixels + tilePadding, portrait, portrait};
    _paint.Portrait(picture, tile.portrait);
    const std::int32_t countLeft = picture.xPixels + portrait + tilePadding;
    _paint.Text(tile.count, countLeft, picture.yPixels, ACCENT_SRGB, tileInside);
    _paint.Text(tile.figures, countLeft, picture.yPixels + _paint.Cell(), SUB_SRGB, tileInside);
    const std::int32_t barTop = picture.yPixels + portrait + tilePadding;
    if (tile.hit.has_value())
    {
      _paint.Bar({picture.xPixels, barTop, tileInside.widthPixels - (2 * tilePadding), _paint.Pixels(TILE_BAR_HEIGHT_LOGICAL_PIXELS)},
                 *tile.hit, GOOD_SRGB);
    }
    _paint.Text(tile.label, tileInside.xPixels + ((tileInside.widthPixels - _paint.Width(tile.label)) / 2),
                tileInside.yPixels + tileInside.heightPixels - tilePadding - _paint.Cell(), tile.active ? ACCENT_SRGB : TEXT_SRGB,
                tileInside);
    x += tileWidth + tileGap;
  }
}

} // namespace

bool PixelRect::Contains(std::int32_t _xPixels, std::int32_t _yPixels) const noexcept
{
  return _xPixels >= xPixels && _xPixels < xPixels + widthPixels && _yPixels >= yPixels && _yPixels < yPixels + heightPixels;
}

bool PixelRect::Inside(const PixelRect& _outer) const noexcept
{
  return xPixels >= _outer.xPixels && yPixels >= _outer.yPixels && xPixels + widthPixels <= _outer.xPixels + _outer.widthPixels &&
         yPixels + heightPixels <= _outer.yPixels + _outer.heightPixels;
}

bool PixelRect::Overlaps(const PixelRect& _other) const noexcept
{
  return xPixels < _other.xPixels + _other.widthPixels && _other.xPixels < xPixels + widthPixels &&
         yPixels < _other.yPixels + _other.heightPixels && _other.yPixels < yPixels + heightPixels;
}

void Hud::Build(const HudModel& _model, std::uint32_t _widthPixels, std::uint32_t _heightPixels, float _uiScale,
                std::vector<ScreenQuad>& _quads)
{
  m_panels.clear();
  m_targets.clear();
  m_texts.clear();
  m_readout.reset();
  m_deck.reset();
  Painter paint(_uiScale, _quads, m_texts);
  const auto width = static_cast<std::int32_t>(_widthPixels);
  const auto height = static_cast<std::int32_t>(_heightPixels);

  // The top bar, its border along its foot.
  const std::int32_t barHeight = paint.Pixels(TOP_BAR_LOGICAL_PIXELS);
  const std::int32_t border = paint.Pixels(BORDER_LOGICAL_PIXELS);
  const PixelRect bar{0, 0, width, barHeight - border};
  paint.Fill(bar, PANEL_SRGB, PANEL_ALPHA);
  paint.Fill({0, barHeight - border, width, border}, LINE_SRGB);
  m_panels.push_back({0, 0, width, barHeight});
  const std::int32_t barTextTop = paint.CenteredCellTop(0, bar.heightPixels);

  // The camera buttons, from the right.
  constexpr std::array<std::pair<HudControl, std::string_view>, 4> BUTTONS = {{{HudControl::TurnLeft, TURN_LEFT},
                                                                               {HudControl::TurnRight, TURN_RIGHT},
                                                                               {HudControl::ZoomOut, ZOOM_OUT},
                                                                               {HudControl::ZoomIn, ZOOM_IN}}};
  const std::int32_t button = paint.Pixels(BUTTON_LOGICAL_PIXELS);
  const std::int32_t hit = paint.Pixels(HIT_LOGICAL_PIXELS);
  const std::int32_t inset = (hit - button) / 2;
  const std::int32_t lastButtonX = width - paint.Pixels(TOP_BAR_RIGHT_LOGICAL_PIXELS) - button;
  const std::int32_t buttonsLeft = lastButtonX - (static_cast<std::int32_t>(BUTTONS.size() - 1) * hit) - inset;
  for (std::size_t index = 0; index < BUTTONS.size(); ++index)
  {
    const std::int32_t x = lastButtonX - (static_cast<std::int32_t>(BUTTONS.size() - 1 - index) * hit);
    paint.Button({x, (barHeight - button) / 2, button, button}, BUTTONS.at(index).second);
    m_targets.push_back({.control = BUTTONS.at(index).first, .alert = 0, .rect = {x - inset, (barHeight - hit) / 2, hit, hit}});
  }

  // The resources, from the left: each key in dim and its value.
  std::int32_t x = paint.Pixels(TOP_BAR_LEFT_LOGICAL_PIXELS);
  const std::int32_t resourceGap = paint.Pixels(RESOURCE_GAP_LOGICAL_PIXELS);
  for (std::size_t index = 0; index < _model.resources.size(); ++index)
  {
    const HudResource& resource = _model.resources.at(index);
    if (index != 0)
    {
      x += resourceGap;
    }
    paint.Text(resource.key, x, barTextTop, DIM_SRGB, bar);
    x += paint.Width(resource.key) + paint.Pixels(KEY_GAP_LOGICAL_PIXELS);
    paint.Text(resource.value, x, barTextTop, resource.warn ? BAD_SRGB : TEXT_SRGB, bar);
    x += paint.Width(resource.value);
  }

  // The selection's chip after the resources (M3-H3): its name, then FIRE's key in accent or RELOADING in off text, in
  // an accent box as tall as a camera button. Without the room for both, the name alone, and without room for that,
  // nothing.
  if (_model.selection.has_value())
  {
    const HudSelection& selection = *_model.selection;
    const std::int32_t chipPadding = paint.Pixels(CHIP_PADDING_LOGICAL_PIXELS);
    const std::int32_t chipGap = paint.Pixels(KEY_GAP_LOGICAL_PIXELS);
    const std::int32_t nameWidth = paint.Width(selection.name);
    const std::int32_t detailWidth = selection.detail.empty() ? 0 : chipGap + paint.Width(selection.detail);
    const std::int32_t chipLeft = x + (_model.resources.empty() ? 0 : resourceGap);
    const std::int32_t limit = buttonsLeft - resourceGap;
    const std::int32_t chrome = (2 * border) + (2 * chipPadding);
    const bool withDetail = detailWidth != 0 && chipLeft + chrome + nameWidth + detailWidth <= limit;
    if (chipLeft + chrome + nameWidth <= limit)
    {
      const PixelRect chip{chipLeft, (barHeight - button) / 2, chrome + nameWidth + (withDetail ? detailWidth : 0), button};
      const PixelRect inside = paint.Box(chip, ACCENT_SRGB, BUTTON_SRGB, 1.0F);
      const std::int32_t textTop = paint.CenteredCellTop(chip.yPixels, chip.heightPixels);
      paint.Text(selection.name, inside.xPixels + chipPadding, textTop, TEXT_SRGB, inside);
      if (withDetail)
      {
        paint.Text(selection.detail, inside.xPixels + chipPadding + nameWidth + chipGap, textTop,
                   selection.detailOff ? OFF_SRGB : ACCENT_SRGB, inside);
      }
      x = chip.xPixels + chip.widthPixels;
    }
  }

  // The objective, in the middle of the screen, or as near it as the room between the resources and the buttons lets
  // it: its title in accent and its detail in sub. Without the room for both, the title alone, and without room for
  // that, nothing; the fit test keeps M3's from either.
  const std::int32_t roomLeft = x + resourceGap;
  const std::int32_t room = buttonsLeft - resourceGap - roomLeft;
  const std::int32_t titleWidth = _model.objective.title.empty() ? 0 : paint.Width(_model.objective.title);
  const std::int32_t detailWidth =
    _model.objective.detail.empty() ? 0 : paint.Pixels(OBJECTIVE_GAP_LOGICAL_PIXELS) + paint.Width(_model.objective.detail);
  const bool withDetail = titleWidth != 0 && detailWidth != 0 && titleWidth + detailWidth <= room;
  if (titleWidth != 0 && titleWidth <= room)
  {
    const std::int32_t objectiveWidth = titleWidth + (withDetail ? detailWidth : 0);
    const std::int32_t objectiveX = std::clamp((width - objectiveWidth) / 2, roomLeft, roomLeft + room - objectiveWidth);
    paint.Text(_model.objective.title, objectiveX, barTextTop, ACCENT_SRGB, bar);
    if (withDetail)
    {
      paint.Text(_model.objective.detail, objectiveX + objectiveWidth - paint.Width(_model.objective.detail), barTextTop,
                 _model.objective.detailOff ? OFF_SRGB : SUB_SRGB, bar);
    }
  }

  // The mode banner, centred under the top bar: its text in accent, then its buttons and its hint in dim, after the
  // text or under it, centred, when one row would not fit the screen.
  if (!_model.mode.command.empty())
  {
    const std::int32_t control = paint.Pixels(CONTROL_HEIGHT_LOGICAL_PIXELS);
    const std::int32_t paddingY = paint.Pixels(BANNER_PADDING_Y_LOGICAL_PIXELS);
    const std::int32_t paddingLeft = paint.Pixels(BANNER_PADDING_LEFT_LOGICAL_PIXELS);
    const std::int32_t gap = paint.Pixels(BANNER_GAP_LOGICAL_PIXELS);
    const std::int32_t textWidth = paint.Width(_model.mode.banner);
    std::vector<std::int32_t> buttonWidths;
    std::int32_t buttonsWidth = 0;
    for (const HudModeButton& modeButton : _model.mode.buttons)
    {
      buttonWidths.push_back(paint.Width(modeButton.label) + (2 * paint.Pixels(CANCEL_PADDING_LOGICAL_PIXELS)));
      buttonsWidth += buttonWidths.back() + (buttonWidths.size() > 1 ? gap : 0);
    }
    const bool hasButtons = !buttonWidths.empty();
    const bool hasHint = !_model.mode.hint.empty();
    const std::int32_t hintWidth = hasHint ? paint.Width(_model.mode.hint) : 0;
    // What follows the text: the buttons, then the hint.
    const std::int32_t tailWidth = buttonsWidth + (hasHint ? (hasButtons ? gap : 0) + hintWidth : 0);
    const bool hasTail = hasButtons || hasHint;
    // The right's narrower padding is for a row that ends with a button; one that ends with text has the left's.
    const std::int32_t paddingRight = hasButtons && !hasHint ? paint.Pixels(BANNER_PADDING_RIGHT_LOGICAL_PIXELS) : paddingLeft;
    const std::int32_t chrome = (2 * border) + paddingLeft + paddingRight;
    const std::int32_t oneRow = textWidth + (hasTail ? gap + tailWidth : 0);
    const bool stacked = hasTail && chrome + oneRow > width - (2 * paint.Pixels(ALERTS_LEFT_LOGICAL_PIXELS));
    const std::int32_t contentWidth = stacked ? std::max(textWidth, tailWidth) : oneRow;
    const std::int32_t rowsHeight = stacked ? (2 * control) + paddingY : control;
    const std::int32_t bannerWidth = chrome + contentWidth;
    const PixelRect banner{(width - bannerWidth) / 2, paint.Pixels(BANNER_TOP_LOGICAL_PIXELS), bannerWidth,
                           (2 * border) + (2 * paddingY) + rowsHeight};
    const PixelRect inside = paint.Box(banner, ACCENT_SRGB, PANEL_SRGB, PANEL_ALPHA);
    m_panels.push_back(banner);
    const std::int32_t textTop = inside.yPixels + paddingY;
    const std::int32_t contentLeft = inside.xPixels + paddingLeft;
    paint.Text(_model.mode.banner, contentLeft + (stacked ? (contentWidth - textWidth) / 2 : 0), paint.CenteredCellTop(textTop, control),
               ACCENT_SRGB, inside);
    std::int32_t buttonX = stacked ? contentLeft + ((contentWidth - tailWidth) / 2) : contentLeft + textWidth + gap;
    const std::int32_t buttonsTop = stacked ? textTop + control + paddingY : textTop;
    for (std::size_t index = 0; index < buttonWidths.size(); ++index)
    {
      const PixelRect buttonRect{buttonX, buttonsTop, buttonWidths.at(index), control};
      paint.Button(buttonRect, _model.mode.buttons.at(index).label);
      m_targets.push_back({.control = _model.mode.buttons.at(index).control, .alert = 0, .rect = buttonRect});
      buttonX += buttonWidths.at(index) + gap;
    }
    if (hasHint)
    {
      paint.Text(_model.mode.hint, buttonX, paint.CenteredCellTop(buttonsTop, control), DIM_SRGB, inside);
    }
  }

  // Layout A's deck along the bottom, full width, its border along its top: the minimap's well on the left, the command
  // card's grid on the right, its cells as wide as the room lets them between 100 and 112, and the selection's panel
  // taking the rest; and the strip of the control groups above it, 1 to 5 and SET.
  std::optional<PixelRect> strip;
  if (_model.deck)
  {
    const std::int32_t deckHeight = paint.Pixels(DECK_LOGICAL_PIXELS);
    const std::int32_t padding = paint.Pixels(DECK_PADDING_LOGICAL_PIXELS);
    const std::int32_t gap = paint.Pixels(DECK_GAP_LOGICAL_PIXELS);
    const std::int32_t cellGap = paint.Pixels(CELL_GAP_LOGICAL_PIXELS);
    HudDeck deck{};
    deck.deck = {0, height - deckHeight, width, deckHeight};
    paint.Fill({0, deck.deck.yPixels + border, width, deckHeight - border}, PANEL_SRGB, PANEL_ALPHA);
    paint.Fill({0, deck.deck.yPixels, width, border}, LINE_SRGB);
    m_panels.push_back(deck.deck);
    const std::int32_t top = deck.deck.yPixels + padding;
    const std::int32_t inner = deckHeight - (2 * padding);
    const std::int32_t minimap = paint.Pixels(MINIMAP_LOGICAL_PIXELS);
    deck.minimap = {padding, top, minimap, inner};
    const std::int32_t cardRoom = width - (2 * padding) - minimap - paint.Pixels(SELECTION_MIN_LOGICAL_PIXELS) - (2 * gap);
    const std::int32_t cellWidth = std::clamp((cardRoom - ((CARD_COLUMNS - 1) * cellGap)) / CARD_COLUMNS,
                                              paint.Pixels(CELL_MIN_WIDTH_LOGICAL_PIXELS), paint.Pixels(CELL_MAX_WIDTH_LOGICAL_PIXELS));
    const std::int32_t cellHeight = paint.Pixels(CELL_HEIGHT_LOGICAL_PIXELS);
    const std::int32_t cardWidth = (CARD_COLUMNS * cellWidth) + ((CARD_COLUMNS - 1) * cellGap);
    deck.card = {width - padding - cardWidth, top, cardWidth, (CARD_ROWS * cellHeight) + ((CARD_ROWS - 1) * cellGap)};
    const std::int32_t selectionLeft = deck.minimap.xPixels + minimap + gap;
    deck.selection = {selectionLeft, top, deck.card.xPixels - gap - selectionLeft, inner};
    paint.Box(deck.minimap, LINE_SRGB, WELL_SRGB, 1.0F);
    paint.Box(deck.selection, LINE_SRGB, WELL_SRGB, 1.0F);
    for (std::int32_t row = 0; row < CARD_ROWS; ++row)
    {
      for (std::int32_t column = 0; column < CARD_COLUMNS; ++column)
      {
        const std::size_t index = (static_cast<std::size_t>(row) * CARD_COLUMNS) + static_cast<std::size_t>(column);
        const PixelRect cell{deck.card.xPixels + (column * (cellWidth + cellGap)), top + (row * (cellHeight + cellGap)), cellWidth,
                             cellHeight};
        deck.cells.at(index) = cell;
        // A slot: its hotkey top left in accent, its icon in the middle at the names' size, its label along the bottom;
        // off text while disabled, an accent border on active-bg while its mode is on; an empty slot very dim.
        const HudCommandSlot& slot = _model.commandCard.slots.at(index);
        if (slot.command.empty())
        {
          paint.Box(cell, LINE_SRGB, WELL_SRGB, 1.0F);
          continue;
        }
        const PixelRect inside =
          paint.Box(cell, slot.active ? ACCENT_SRGB : LINE_SRGB, slot.active ? ACTIVE_BACKGROUND_SRGB : BUTTON_SRGB, 1.0F);
        const std::int32_t cellInset = paint.Pixels(CELL_INSET_LOGICAL_PIXELS);
        const std::uint32_t textSrgb = slot.enabled ? TEXT_SRGB : OFF_SRGB;
        paint.Text(slot.hotkey, inside.xPixels + cellInset, inside.yPixels + cellInset, slot.enabled ? ACCENT_SRGB : OFF_SRGB, inside);
        paint.NameText(slot.icon, inside.xPixels + ((inside.widthPixels - paint.NameWidth(slot.icon)) / 2),
                       inside.yPixels + ((inside.heightPixels - paint.NameCell()) / 2) - cellInset, textSrgb, inside);
        paint.Text(slot.label, inside.xPixels + ((inside.widthPixels - paint.Width(slot.label)) / 2),
                   inside.yPixels + inside.heightPixels - cellInset - paint.Cell(), textSrgb, inside);
        m_targets.push_back({.control = HudControl::CardSlot, .alert = index, .rect = cell});
      }
    }
    // The strip: each group's slot with its number in dim, then SET.
    const std::int32_t slotHeight = paint.Pixels(GROUP_HEIGHT_LOGICAL_PIXELS);
    const std::int32_t slotTop = height - paint.Pixels(STRIP_BOTTOM_LOGICAL_PIXELS) - slotHeight;
    std::int32_t slotLeft = paint.Pixels(STRIP_LEFT_LOGICAL_PIXELS);
    for (std::size_t slot = 0; slot < HudDeck::GROUP_SLOTS; ++slot)
    {
      const bool set = slot + 1 == HudDeck::GROUP_SLOTS;
      const PixelRect rect{slotLeft, slotTop, paint.Pixels(set ? SET_WIDTH_LOGICAL_PIXELS : GROUP_WIDTH_LOGICAL_PIXELS), slotHeight};
      if (set)
      {
        paint.Button(rect, SET_GROUP);
      }
      else
      {
        const PixelRect inside = paint.Box(rect, LINE_SRGB, PANEL_SRGB, PANEL_ALPHA);
        const std::string number = std::to_string(slot + 1);
        paint.Text(number, inside.xPixels + paint.Pixels(KEY_GAP_LOGICAL_PIXELS), paint.CenteredCellTop(rect.yPixels, rect.heightPixels),
                   DIM_SRGB, inside);
      }
      deck.groups.at(slot) = rect;
      slotLeft += rect.widthPixels;
    }
    strip = PixelRect{deck.groups.front().xPixels, slotTop, slotLeft - deck.groups.front().xPixels, slotHeight};
    m_panels.push_back(*strip);
    m_deck = deck;
    LayPanel(_model.panel, deck.selection, paint);

    // The hovered slot's tooltip above the card: its hotkey and label, then its rule.
    if (_model.tooltip.has_value() && !_model.tooltip->empty())
    {
      const std::string_view text = *_model.tooltip;
      const std::size_t first = text.find(TOOLTIP_SEPARATOR);
      const std::size_t second = first == std::string_view::npos ? first : text.find(TOOLTIP_SEPARATOR, first + TOOLTIP_SEPARATOR.size());
      const std::string_view head = second == std::string_view::npos ? text : text.substr(0, second);
      const std::string_view rule = second == std::string_view::npos ? std::string_view{} : text.substr(second + TOOLTIP_SEPARATOR.size());
      const std::int32_t tipPadding = paint.Pixels(TOOLTIP_PADDING_LOGICAL_PIXELS);
      const std::int32_t lines = rule.empty() ? 1 : 2;
      const std::int32_t tipHeight = (2 * border) + (2 * tipPadding) + (lines * paint.Cell()) + ((lines - 1) * gap);
      const std::int32_t tipWidth = paint.Pixels(TOOLTIP_WIDTH_LOGICAL_PIXELS);
      PixelRect tip{width - paint.Pixels(TOOLTIP_RIGHT_LOGICAL_PIXELS) - tipWidth,
                    height - paint.Pixels(TOOLTIP_BOTTOM_LOGICAL_PIXELS) - tipHeight, tipWidth, tipHeight};
      if (tip.Overlaps(*strip))
      {
        tip.yPixels = strip->yPixels - paint.Pixels(ABOVE_STRIP_GAP_LOGICAL_PIXELS) - tipHeight; // as the readout, at 1280 px
      }
      const PixelRect tipInside = paint.Box(tip, LINE_SRGB, PANEL_SRGB, PANEL_ALPHA);
      m_panels.push_back(tip);
      paint.Text(head, tipInside.xPixels + tipPadding, tipInside.yPixels + tipPadding, ACCENT_SRGB, tipInside);
      if (!rule.empty())
      {
        paint.Text(rule, tipInside.xPixels + tipPadding, tipInside.yPixels + tipPadding + paint.Cell() + gap, SUB_SRGB, tipInside);
      }
    }
  }

  // The integrity readout: the focused section's name, then its rows, each key in dim and its value in its colour, both
  // in accent while the row flashes, and HOLE's and GAP's bars under them. It stands above the deck, or without one in
  // the bottom-left corner, and lays out at the UI scale, or at the largest band below it at which it is no taller than
  // READOUT_HEIGHT_FRACTION of the screen.
  if (_model.readout.has_value())
  {
    const HudReadout& readout = *_model.readout;
    const auto fits = [&](float _scale)
    {
      const Painter trial(_scale, _quads, m_texts);
      return static_cast<float>(MeasureReadout(readout, trial).heightPixels) <= READOUT_HEIGHT_FRACTION * static_cast<float>(height);
    };
    float readoutScale = _uiScale;
    for (const float band : READOUT_SCALES)
    {
      if (fits(readoutScale))
      {
        break;
      }
      readoutScale = std::min(readoutScale, band);
    }
    Painter readoutPaint(readoutScale, _quads, m_texts);
    const ReadoutSize size = MeasureReadout(readout, readoutPaint);
    const std::int32_t margin = paint.Pixels(READOUT_LEFT_LOGICAL_PIXELS);
    const std::int32_t boxWidth = std::min(size.widthPixels, width - (2 * margin));
    PixelRect box{margin, height - paint.Pixels(READOUT_BOTTOM_LOGICAL_PIXELS) - size.heightPixels, boxWidth, size.heightPixels};
    if (_model.deck)
    {
      box.xPixels = std::max(margin, width - paint.Pixels(DECK_READOUT_RIGHT_LOGICAL_PIXELS) - boxWidth);
      box.yPixels = height - paint.Pixels(DECK_READOUT_BOTTOM_LOGICAL_PIXELS) - size.heightPixels;
      if (strip.has_value() && box.Overlaps(*strip))
      {
        box.yPixels = strip->yPixels - paint.Pixels(ABOVE_STRIP_GAP_LOGICAL_PIXELS) - size.heightPixels;
      }
    }
    const PixelRect inside = readoutPaint.Box(box, LINE_SRGB, PANEL_SRGB, PANEL_ALPHA);
    m_panels.push_back(box);
    m_readout = box;
    const std::int32_t padding = readoutPaint.Pixels(READOUT_PADDING_LOGICAL_PIXELS);
    const std::int32_t left = inside.xPixels + padding;
    const std::int32_t barWidth = std::min(size.contentWidthPixels, inside.widthPixels - (2 * padding));
    std::int32_t y = inside.yPixels + padding;
    readoutPaint.NameText(readout.title, left, y, TEXT_SRGB, inside);
    y += readoutPaint.NameCell();
    for (const HudReadoutRow& row : readout.rows)
    {
      y += readoutPaint.Pixels(READOUT_LINE_GAP_LOGICAL_PIXELS);
      // A key alone, such as FALLS, is in the row's colour.
      const std::uint32_t keySrgb = row.flashing ? ACCENT_SRGB : (row.value.empty() ? row.srgb : DIM_SRGB);
      readoutPaint.Text(row.key, left, y, keySrgb, inside);
      if (!row.value.empty())
      {
        readoutPaint.Text(row.value, left + readoutPaint.Advance(row.key + " "), y, row.flashing ? ACCENT_SRGB : row.srgb, inside);
      }
      y += readoutPaint.Cell();
      if (row.fill.has_value())
      {
        // The track, and over it the width so far, in bad from the width at which the stone falls.
        y += readoutPaint.Pixels(READOUT_BAR_GAP_LOGICAL_PIXELS);
        const std::int32_t trackHeight = readoutPaint.Pixels(READOUT_BAR_HEIGHT_LOGICAL_PIXELS);
        readoutPaint.Fill({left, y, barWidth, trackHeight}, LINE_SRGB);
        const float fill = std::clamp(*row.fill, 0.0F, 1.0F);
        const auto filled = static_cast<std::int32_t>(std::lround(fill * static_cast<float>(barWidth)));
        if (filled > 0)
        {
          readoutPaint.Fill({left, y, filled, trackHeight}, *row.fill >= 1.0F ? BAD_SRGB : TEXT_SRGB);
        }
        y += trackHeight;
      }
    }
  }

  // The alerts, newest first, down the left: the text in accent, or bad when urgent, a second line in sub, and JUMP
  // when the alert has a place. One that would meet the readout is left out, with those older than it.
  std::int32_t y = paint.Pixels(ALERTS_TOP_LOGICAL_PIXELS);
  const std::int32_t paddingX = paint.Pixels(ALERT_PADDING_X_LOGICAL_PIXELS);
  const std::int32_t paddingY = paint.Pixels(ALERT_PADDING_Y_LOGICAL_PIXELS);
  for (std::size_t index = 0; index < std::min(_model.alerts.size(), HudAlerts::SHOWN); ++index)
  {
    const HudAlert& alert = _model.alerts.at(index);
    const std::uint32_t color = alert.urgent ? BAD_SRGB : ACCENT_SRGB;
    const bool hasSub = !alert.sub.empty();
    const bool hasJump = alert.location.has_value();
    const std::int32_t lines = hasSub ? (2 * paint.Cell()) + paint.Pixels(ALERT_LINE_GAP_LOGICAL_PIXELS) : paint.Cell();
    const std::int32_t control = paint.Pixels(CONTROL_HEIGHT_LOGICAL_PIXELS);
    const std::int32_t content = hasJump ? std::max(lines, control) : lines;
    const std::int32_t jumpWidth = hasJump ? paint.Width(JUMP) + (2 * paint.Pixels(JUMP_PADDING_LOGICAL_PIXELS)) : 0;
    const std::int32_t jumpRoom = hasJump ? paint.Pixels(ALERT_GAP_LOGICAL_PIXELS) + jumpWidth : 0;
    const std::int32_t textWidth = std::max(paint.Width(alert.text), hasSub ? paint.Width(alert.sub) : 0);
    const std::int32_t alertWidth =
      std::min((2 * border) + (2 * paddingX) + textWidth + jumpRoom, paint.Pixels(ALERT_MAX_WIDTH_LOGICAL_PIXELS));
    const PixelRect box{paint.Pixels(ALERTS_LEFT_LOGICAL_PIXELS), y, alertWidth, (2 * border) + (2 * paddingY) + content};
    const bool meets = (m_readout.has_value() && box.Overlaps(*m_readout)) || (strip.has_value() && box.Overlaps(*strip)) ||
                       (m_deck.has_value() && box.Overlaps(m_deck->deck));
    if (meets)
    {
      break;
    }
    const PixelRect inside = paint.Box(box, color, PANEL_SRGB, PANEL_ALPHA);
    m_panels.push_back(box);
    const PixelRect column{inside.xPixels + paddingX, inside.yPixels + paddingY, inside.widthPixels - (2 * paddingX) - jumpRoom, content};
    const std::int32_t linesTop = column.yPixels + ((content - lines) / 2);
    paint.Text(alert.text, column.xPixels, linesTop, color, column);
    if (hasSub)
    {
      paint.Text(alert.sub, column.xPixels, linesTop + paint.Cell() + paint.Pixels(ALERT_LINE_GAP_LOGICAL_PIXELS), SUB_SRGB, column);
    }
    if (hasJump)
    {
      const PixelRect jump{column.xPixels + column.widthPixels + paint.Pixels(ALERT_GAP_LOGICAL_PIXELS),
                           column.yPixels + ((content - control) / 2), jumpWidth, control};
      paint.Button(jump, JUMP);
      m_targets.push_back({.control = HudControl::Jump, .alert = index, .rect = jump});
    }
    y += box.heightPixels + paint.Pixels(ALERTS_GAP_LOGICAL_PIXELS);
  }
}

void Hud::LayCursor(const AttackCursor& _cursor, std::int32_t _xPixels, std::int32_t _yPixels, float _uiScale,
                    std::vector<ScreenQuad>& _quads)
{
  Painter paint(_uiScale, _quads, m_texts);
  const std::int32_t ring = paint.Pixels(RING_LOGICAL_PIXELS);
  const std::int32_t border = paint.Pixels(BORDER_LOGICAL_PIXELS);
  const PixelRect around{_xPixels - (ring / 2), _yPixels - (ring / 2), ring, ring};
  paint.Fill({around.xPixels, around.yPixels, ring, border}, _cursor.srgb);
  paint.Fill({around.xPixels, around.yPixels + ring - border, ring, border}, _cursor.srgb);
  paint.Fill({around.xPixels, around.yPixels + border, border, ring - (2 * border)}, _cursor.srgb);
  paint.Fill({around.xPixels + ring - border, around.yPixels + border, border, ring - (2 * border)}, _cursor.srgb);
  const std::int32_t paddingX = paint.Pixels(CURSOR_PADDING_X_LOGICAL_PIXELS);
  const std::int32_t paddingY = paint.Pixels(CURSOR_PADDING_Y_LOGICAL_PIXELS);
  const PixelRect panel{_xPixels + paint.Pixels(CURSOR_LABEL_OFFSET_LOGICAL_PIXELS), _yPixels - ((paint.Cell() / 2) + paddingY),
                        paint.Width(_cursor.label) + (2 * paddingX), paint.Cell() + (2 * paddingY)};
  paint.Fill(panel, PANEL_SRGB, PANEL_ALPHA);
  paint.Text(_cursor.label, panel.xPixels + paddingX, panel.yPixels + paddingY, _cursor.srgb, panel);
}

std::optional<HudTarget> Hud::HitTest(std::int32_t _xPixels, std::int32_t _yPixels) const noexcept
{
  const auto found =
    std::ranges::find_if(m_targets, [_xPixels, _yPixels](const HudTarget& _target) { return _target.rect.Contains(_xPixels, _yPixels); });
  if (found == m_targets.end())
  {
    return std::nullopt;
  }
  return *found;
}

bool Hud::Covers(std::int32_t _xPixels, std::int32_t _yPixels) const noexcept
{
  return std::ranges::any_of(m_panels, [_xPixels, _yPixels](const PixelRect& _panel) { return _panel.Contains(_xPixels, _yPixels); });
}

std::span<const HudTarget> Hud::Targets() const noexcept
{
  return m_targets;
}

std::span<const HudText> Hud::Texts() const noexcept
{
  return m_texts;
}

std::optional<PixelRect> Hud::Readout() const noexcept
{
  return m_readout;
}

std::optional<HudDeck> Hud::Deck() const noexcept
{
  return m_deck;
}

std::span<const PixelRect> Hud::Panels() const noexcept
{
  return m_panels;
}

} // namespace Client
