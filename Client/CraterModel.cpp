#include "pch.h"

#include "CraterModel.h"

#include "LabModel.h"
#include "Strings.h"

namespace Client
{

bool CraterItemRow::Correct() const noexcept
{
  return (answer == CraterAnswer::Ram) == keyRam;
}

CraterModel::CraterModel(std::vector<CraterItemSpec> _items, std::uint64_t _seed)
  : m_items(std::move(_items)),
    m_order(LabModel::Order(m_items.size(), _seed))
{
  if (m_items.empty())
  {
    throw std::invalid_argument("the crater run needs its items");
  }
}

const CraterItemSpec* CraterModel::Current() const noexcept
{
  return Finished() ? nullptr : &m_items[m_order[m_started - 1]];
}

std::size_t CraterModel::Started() const noexcept
{
  return m_started;
}

bool CraterModel::Asking() const noexcept
{
  return m_asking;
}

bool CraterModel::Finished() const noexcept
{
  return m_started > m_order.size();
}

void CraterModel::Ready(double _nowSeconds)
{
  if (!Finished() && !m_asking)
  {
    m_asking = true;
    m_askedAtSeconds = _nowSeconds;
  }
}

std::optional<CraterItemRow> CraterModel::Answer(CraterAnswer _answer, double _nowSeconds)
{
  const CraterItemSpec* item = Current();
  if (!m_asking || item == nullptr)
  {
    return std::nullopt;
  }
  CraterItemRow row{.id = item->id, .keyRam = item->keyRam, .answer = _answer, .secondsToAnswer = _nowSeconds - m_askedAtSeconds};
  m_asking = false;
  ++m_started;
  return row;
}

HudMode CraterModel::Mode() const
{
  if (Finished())
  {
    return {.command = "DONE", .banner = std::string(CRATER_FINISHED_BANNER), .buttons = {}};
  }
  if (!m_asking)
  {
    return {.command = "SETUP", .banner = std::string(CRATER_SETTING_UP_BANNER), .buttons = {}};
  }
  return {.command = "ASK",
          .banner = std::string(CRATER_QUESTION_BANNER),
          .buttons = {{.control = HudControl::AnswerOpen, .label = std::string(RAM_BUTTON)},
                      {.control = HudControl::AnswerShut, .label = std::string(TREBUCHET_BUTTON)}},
          .hint = std::string(VIEWS_HINT)};
}

HudObjective CraterModel::Objective() const
{
  if (Finished())
  {
    return {.title = std::string(FINISHED_OBJECTIVE), .detail = "", .detailOff = false};
  }
  if (m_started <= 1)
  {
    return {.title = std::string(PRACTICE_OBJECTIVE), .detail = "", .detailOff = false};
  }
  return {.title = std::format(ITEM_OBJECTIVE, m_started - 1, m_order.size() - 1), .detail = "", .detailOff = false};
}

std::string CraterModel::LogHeader(const CraterLogInfo& _info)
{
  std::string text;
  text += std::format("# participant: {}\n", _info.participant);
  text += std::format("# build id: {:016x}\n", _info.buildId);
  text += std::format("# server-data hash: {:016x}\n", _info.serverDataHash);
  text += std::format("# item-set hash: {:016x}\n", _info.itemsHash);
  text += std::format("# start time: {}\n", _info.startTime);
  text += "kind,id,key,answer,correct,secondsToAnswer\n";
  return text;
}

std::string CraterModel::ItemRowText(const CraterItemRow& _row)
{
  const auto engine = [](bool _ram) { return _ram ? "ram" : "trebuchet"; };
  return std::format("item,{},{},{},{},{:.3f}\n", _row.id, engine(_row.keyRam), engine(_row.answer == CraterAnswer::Ram),
                     _row.Correct() ? 1 : 0, _row.secondsToAnswer);
}

} // namespace Client
