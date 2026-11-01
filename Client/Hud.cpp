#include "pch.h"

#include "Hud.h"

#include "Font.h"
#include "HudAlerts.h"
#include "Look.h"
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
  std::vector<ScreenQuad>& m_quads;
  std::vector<HudText>& m_texts;
};

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

void Hud::Build(const HudModel& _model, std::uint32_t _widthPixels, float _uiScale, std::vector<ScreenQuad>& _quads)
{
  m_panels.clear();
  m_targets.clear();
  m_texts.clear();
  Painter paint(_uiScale, _quads, m_texts);
  const auto width = static_cast<std::int32_t>(_widthPixels);

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

  // The objective, in the middle of the room between them: its title in accent and its detail in sub. Without the
  // room for both, the title alone, and without room for that, nothing; the fit test keeps M3's from either.
  const std::int32_t roomLeft = x + resourceGap;
  const std::int32_t room = buttonsLeft - resourceGap - roomLeft;
  const std::int32_t titleWidth = _model.objective.title.empty() ? 0 : paint.Width(_model.objective.title);
  const std::int32_t detailWidth =
    _model.objective.detail.empty() ? 0 : paint.Pixels(OBJECTIVE_GAP_LOGICAL_PIXELS) + paint.Width(_model.objective.detail);
  const bool withDetail = titleWidth != 0 && detailWidth != 0 && titleWidth + detailWidth <= room;
  if (titleWidth != 0 && titleWidth <= room)
  {
    const std::int32_t objectiveWidth = titleWidth + (withDetail ? detailWidth : 0);
    const std::int32_t objectiveX = roomLeft + ((room - objectiveWidth) / 2);
    paint.Text(_model.objective.title, objectiveX, barTextTop, ACCENT_SRGB, bar);
    if (withDetail)
    {
      paint.Text(_model.objective.detail, objectiveX + objectiveWidth - paint.Width(_model.objective.detail), barTextTop,
                 _model.objective.detailOff ? OFF_SRGB : SUB_SRGB, bar);
    }
  }

  // The mode banner, centred under the top bar: its text in accent and its buttons after it, or under it, centred,
  // when one row would not fit the screen.
  if (!_model.mode.command.empty())
  {
    const std::int32_t control = paint.Pixels(CONTROL_HEIGHT_LOGICAL_PIXELS);
    const std::int32_t paddingY = paint.Pixels(BANNER_PADDING_Y_LOGICAL_PIXELS);
    const std::int32_t paddingLeft = paint.Pixels(BANNER_PADDING_LEFT_LOGICAL_PIXELS);
    const std::int32_t paddingRight = paint.Pixels(BANNER_PADDING_RIGHT_LOGICAL_PIXELS);
    const std::int32_t gap = paint.Pixels(BANNER_GAP_LOGICAL_PIXELS);
    const std::int32_t textWidth = paint.Width(_model.mode.banner);
    std::vector<std::int32_t> buttonWidths;
    std::int32_t buttonsWidth = 0;
    for (const HudModeButton& button : _model.mode.buttons)
    {
      buttonWidths.push_back(paint.Width(button.label) + (2 * paint.Pixels(CANCEL_PADDING_LOGICAL_PIXELS)));
      buttonsWidth += buttonWidths.back() + (buttonWidths.size() > 1 ? gap : 0);
    }
    const bool hasButtons = !buttonWidths.empty();
    const std::int32_t chrome = (2 * border) + paddingLeft + paddingRight;
    const std::int32_t oneRow = textWidth + (hasButtons ? gap + buttonsWidth : 0);
    const bool stacked = hasButtons && chrome + oneRow > width - (2 * paint.Pixels(ALERTS_LEFT_LOGICAL_PIXELS));
    const std::int32_t contentWidth = stacked ? std::max(textWidth, buttonsWidth) : oneRow;
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
    std::int32_t x = stacked ? contentLeft + ((contentWidth - buttonsWidth) / 2) : contentLeft + textWidth + gap;
    const std::int32_t buttonsTop = stacked ? textTop + control + paddingY : textTop;
    for (std::size_t index = 0; index < buttonWidths.size(); ++index)
    {
      const PixelRect button{x, buttonsTop, buttonWidths.at(index), control};
      paint.Button(button, _model.mode.buttons.at(index).label);
      m_targets.push_back({.control = _model.mode.buttons.at(index).control, .alert = 0, .rect = button});
      x += buttonWidths.at(index) + gap;
    }
  }

  // The alerts, newest first, down the left: the text in accent, or bad when urgent, a second line in sub, and JUMP
  // when the alert has a place.
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

} // namespace Client
