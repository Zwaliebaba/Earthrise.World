#pragma once

#include "VoxelGrid.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace Content
{

/// The client view of a template (ADR-008, decision 2): what the client draws a building from, and shows in placement
/// previews and the HUD. Baked from GameData/Server/Templates/. The simulation reads its own view of the same file
/// from ServerData, and nothing it decides by is here.
struct TemplateView
{
  std::string_view id;   // T1 to T9
  std::string_view name; // the display name, such as Keep
  VoxelGrid grid;        // on the world grid, 1 m
  /// The counted cost (ADR-005, decision 7): decorations count as their host, and stock and foundation are left out.
  std::uint32_t stoneM3;
  std::uint32_t timberM3;
};

/// A model (ADR-005, decision 8): one unit figure or one engine pose, at 0.5 m. It sets no cost and no rule; the
/// simulation knows a unit from its stat table, and this is only how the client draws it.
struct ModelView
{
  std::string_view id;   // U1 to U5, S1 to S3
  std::string_view name; // the display name, such as Spearman
  VoxelGrid grid;
};

/// One segment of a line model, along its axis from its tail: where it starts and ends, in millimetres of the drawn
/// length, the colour it is drawn in, and how wide it is on screen, in pixels at every zoom (ADR-018).
struct LineSegmentView
{
  std::uint32_t fromMillimeters;
  std::uint32_t toMillimeters;
  std::uint32_t colorSrgb; // 0xRRGGBB; unused when factionColor
  bool factionColor;       // drawn in its side's faction colour instead
  std::uint32_t widthPixels;
  bool tapers; // narrows to a point at its end
};

/// A line model (ADR-018; the one exception to ADR-006, decision 2): a thing too thin for a 0.5 m voxel, drawn as
/// coloured segments along one axis, turned freely along its velocity, with its width fixed in screen pixels. The arrow
/// is the first. Baked from GameData/Client/Lines/. Like a model, it sets no cost and no rule.
struct LineModelView
{
  std::string_view id;                       // X2
  std::string_view name;                     // the display name, such as Arrow
  std::uint32_t drawnLengthMillimeters;      // from its tail to its head
  std::uint32_t dotBelowPixels;              // drawn shorter than this on screen, it is a dot instead
  std::uint32_t dotWidthPixels;              // the dot's width
  std::span<const LineSegmentView> segments; // contiguous, from 0 to drawnLengthMillimeters
};

/// Every template, in ascending id, compared byte by byte.
[[nodiscard]] std::span<const TemplateView> Templates() noexcept;

/// Every model in GameData/Client/Models/, in ascending id.
[[nodiscard]] std::span<const ModelView> Models() noexcept;

/// Every line model in GameData/Client/Lines/, in ascending id.
[[nodiscard]] std::span<const LineModelView> LineModels() noexcept;

/// The template with this id, or nullptr when there is none.
[[nodiscard]] const TemplateView* FindTemplate(std::string_view _id) noexcept;

/// The model with this id, or nullptr when there is none.
[[nodiscard]] const ModelView* FindModel(std::string_view _id) noexcept;

/// The line model with this id, or nullptr when there is none.
[[nodiscard]] const LineModelView* FindLineModel(std::string_view _id) noexcept;

} // namespace Content
