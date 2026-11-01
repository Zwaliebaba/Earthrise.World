#include "pch.h"

#include "Catalog.h"

#include "LineModelTable.h"
#include "ModelTable.h"
#include "TemplateTable.h"

namespace Content
{

namespace
{

template <typename View> const View* FindById(std::span<const View> _views, std::string_view _id) noexcept
{
  const auto found = std::ranges::find(_views, _id, &View::id);
  return found == _views.end() ? nullptr : &*found;
}

} // namespace

std::span<const TemplateView> Templates() noexcept
{
  return Generated::TEMPLATES;
}

std::span<const ModelView> Models() noexcept
{
  return Generated::MODELS;
}

std::span<const LineModelView> LineModels() noexcept
{
  return Generated::LINE_MODELS;
}

const TemplateView* FindTemplate(std::string_view _id) noexcept
{
  return FindById(Templates(), _id);
}

const ModelView* FindModel(std::string_view _id) noexcept
{
  return FindById(Models(), _id);
}

const LineModelView* FindLineModel(std::string_view _id) noexcept
{
  return FindById(LineModels(), _id);
}

} // namespace Content
