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

/// Every template, in ascending id, compared byte by byte.
[[nodiscard]] std::span<const TemplateView> Templates() noexcept;

/// Every model in GameData/Client/Models/, in ascending id.
[[nodiscard]] std::span<const ModelView> Models() noexcept;

/// The template with this id, or nullptr when there is none.
[[nodiscard]] const TemplateView* FindTemplate(std::string_view _id) noexcept;

/// The model with this id, or nullptr when there is none.
[[nodiscard]] const ModelView* FindModel(std::string_view _id) noexcept;

} // namespace Content
