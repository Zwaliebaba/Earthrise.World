#include "pch.h"

#include "BreachLab.h"
#include "BreachRun.h"
#include "ByteWriter.h"
#include "FlatWorld.h"
#include "Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// The breach lab's items (M3-B7; MvpPlan.md, M3, *Done when*): the 16 items split 8 open and 8 shut
// by the simulation's own key, each key stable, and the squad reaches the courtyard if and only if the
// key is open. Every item runs once, on its own copy of one built site, and the tests read what those
// runs recorded; the save and the second resolution run one item again.

namespace SimulationTests
{

namespace
{

struct Key
{
  const char* id;
  bool open;
};

// The keys, as the simulation gave them on 2026-11-01, computed by clang 18 and GCC 13 on Linux, which
// agreed; CI's MSVC build checks them on Windows. A change fails here and prints the new table: a
// changed rule is a changed item set, so an intended one comes with new keys, checked for the 8 and 8
// split, and a new LabItemsHash.
constexpr std::array<Key, 17> KEYS = {{
  {.id = "P0", .open = false},
  {.id = "L01", .open = true},
  {.id = "L02", .open = false},
  {.id = "L03", .open = true},
  {.id = "L04", .open = false},
  {.id = "L05", .open = true},
  {.id = "L06", .open = false},
  {.id = "L07", .open = false},
  {.id = "L08", .open = true},
  {.id = "L09", .open = true},
  {.id = "L10", .open = false},
  {.id = "L11", .open = false},
  {.id = "L12", .open = true},
  {.id = "L13", .open = true},
  {.id = "L14", .open = false},
  {.id = "L15", .open = false},
  {.id = "L16", .open = true},
}};
constexpr std::uint64_t ITEMS_HASH = 0xd2bbb4766d8d02ccULL;

std::string Hex(std::uint64_t _value)
{
  constexpr std::string_view DIGITS = "0123456789abcdef";
  std::string text = "0x";
  for (std::int32_t shift = 60; shift >= 0; shift -= 4)
  {
    text += DIGITS[(_value >> shift) & 0xfU];
  }
  return text;
}

/// What an item did.
struct Run
{
  std::string id;
  Simulation::LabOutcome outcome;
  bool shutAfterPrior; // the prior shots alone leave W2 shut
  bool reached;        // the squad's lead ends at the goal
  bool quiet;          // each phase ended quiet with the trebuchet loaded
};

struct Lab
{
  Simulation::World::Desc desc;
  std::unique_ptr<Simulation::World> site;
  Simulation::LabSetup setup;
  std::vector<Run> runs;
};

struct Phase
{
  bool timedOut;
  bool quiet;
};

Phase Play(Simulation::World& _world, const Simulation::LabSetup& _setup, std::span<const Protocol::VoxelPosition> _aims)
{
  Simulation::LabShots shots(_setup, _aims);
  while (!shots.Advance(_world))
  {
  }
  return {.timedOut = shots.TimedOut(), .quiet = _world.Quiet() && _world.SiegeEngines().at(_setup.engine).reloadTicks == 0};
}

std::vector<Protocol::VoxelPosition> Asked(const Simulation::LabItem& _item)
{
  return std::vector<Protocol::VoxelPosition>(_item.shotsAsked, _item.markVoxels);
}

Lab RunLab()
{
  Lab lab{.desc = Simulation::LabDesc(BakedServerData(), FlatWorld::BUILD_ID), .site = nullptr, .setup = {}, .runs = {}};
  lab.site = std::make_unique<Simulation::World>(lab.desc);
  lab.setup = Simulation::BuildLabSite(*lab.site);
  for (const Simulation::LabItem& item : Simulation::LabItems())
  {
    Simulation::World world = *lab.site;
    const Phase prior = Play(world, lab.setup, item.priorAimVoxels);
    const bool shutAfterPrior = !Simulation::CourtyardOpen(world, lab.setup);
    const Phase asked = Play(world, lab.setup, Asked(item));
    const Simulation::Squad& squad = world.Squads().at(lab.setup.squad);
    const Protocol::VoxelPosition lead = world.Units().at(squad.units.front()).cellVoxels;
    lab.runs.push_back({.id = item.id,
                        .outcome = {.open = Simulation::CourtyardOpen(world, lab.setup),
                                    .ticks = world.TickCount(),
                                    .stateHash = Simulation::StateHash(world),
                                    .timedOut = prior.timedOut || asked.timedOut},
                        .shutAfterPrior = shutAfterPrior,
                        .reached = lead.xVoxels == lab.setup.goalVoxels.xVoxels && lead.zVoxels == lab.setup.goalVoxels.zVoxels,
                        .quiet = prior.quiet && asked.quiet});
  }
  return lab;
}

const Lab& TheLab()
{
  static const Lab LAB = RunLab();
  return LAB;
}

/// The ids of the items for which _fails holds, logged, so that a failing test names them.
template <typename Fails> std::vector<std::string> Failing(const Fails& _fails)
{
  std::vector<std::string> failing;
  for (const Run& run : TheLab().runs)
  {
    if (_fails(run))
    {
      Logger::WriteMessage(("fails: " + run.id).c_str());
      failing.push_back(run.id);
    }
  }
  return failing;
}

/// The index of the item the save and the second resolution run: the first L item whose key is open.
std::size_t OpenItemIndex()
{
  for (std::size_t index = 1; index < TheLab().runs.size(); ++index)
  {
    if (TheLab().runs[index].outcome.open)
    {
      return index;
    }
  }
  throw std::logic_error("the lab has no open item");
}

} // namespace

TEST_CLASS(BreachLabTests)
{
public:
  TEST_METHOD(SplitsEightOpenAndEightShutWithTheCommittedKeys)
  {
    const Lab& lab = TheLab();
    const std::span<const Simulation::LabItem> items = Simulation::LabItems();
    Assert::AreEqual(KEYS.size(), items.size());
    Assert::AreEqual(std::string("P0"), items.front().id);
    std::string table;
    bool same = Simulation::LabItemsHash() == ITEMS_HASH;
    std::size_t open = 0;
    for (std::size_t index = 0; index < items.size(); ++index)
    {
      const Run& run = lab.runs.at(index);
      table += "  {.id = \"" + run.id + "\", .open = " + (run.outcome.open ? "true" : "false") + "},\n";
      same = same && run.id == KEYS.at(index).id && run.outcome.open == KEYS.at(index).open;
      open += index > 0 && run.outcome.open ? 1 : 0;
    }
    if (!same)
    {
      Logger::WriteMessage(
        ("The lab's keys changed. Item set " + Hex(Simulation::LabItemsHash()) + "ULL; the new table:\n" + table).c_str());
    }
    Assert::IsTrue(same);
    Assert::AreEqual(std::size_t{8}, open);
    // The practice item: an intact wall and two shots at its ground row, which stays shut.
    Assert::IsFalse(lab.runs.front().outcome.open);
  }

  TEST_METHOD(AsksEachQuestionOfAShutWall)
  {
    // The prior damage alone never opens W2: each item's question is whether its asked shots will.
    Assert::IsTrue(Failing([](const Run& _run) { return !_run.shutAfterPrior; }).empty());
  }

  TEST_METHOD(TheSquadReachesTheCourtyardIfAndOnlyIfTheKeyIsOpen)
  {
    Assert::IsTrue(Failing([](const Run& _run) { return _run.outcome.open != _run.reached; }).empty());
  }

  TEST_METHOD(EndsEveryPhaseQuietWithinItsLimit)
  {
    Assert::IsTrue(Failing([](const Run& _run) { return _run.outcome.timedOut || !_run.quiet; }).empty());
  }

  TEST_METHOD(ResolvesAnItemTheSameTwiceAndFromASaveBetweenItsHalves)
  {
    const Lab& lab = TheLab();
    const std::size_t index = OpenItemIndex();
    const Simulation::LabItem& item = Simulation::LabItems()[index];
    const Run& recorded = lab.runs.at(index);

    Simulation::World again = *lab.site;
    const Simulation::LabOutcome outcome = Simulation::ResolveLabItem(again, lab.setup, item);
    Assert::AreEqual(recorded.outcome.stateHash, outcome.stateHash);
    Assert::AreEqual(recorded.outcome.ticks, outcome.ticks);
    Assert::IsTrue(outcome.open);
    Assert::IsFalse(outcome.timedOut);

    // The two halves, with the world saved and loaded between them, as a session may stop at the
    // question.
    Simulation::World first = *lab.site;
    Assert::IsTrue(Play(first, lab.setup, item.priorAimVoxels).quiet);
    Simulation::World loaded = Simulation::LoadSave(Simulation::WriteSave(first), lab.desc);
    Assert::IsTrue(Play(loaded, lab.setup, Asked(item)).quiet);
    Assert::IsTrue(Simulation::CourtyardOpen(loaded, lab.setup));
    Assert::AreEqual(recorded.outcome.stateHash, Simulation::StateHash(loaded));
  }

  TEST_METHOD(GivesEachItemTheHeadlessHashRunLiveAsEarthriseRunsIt)
  {
    // Earthrise's loop (M3-G1): a tick at a time through LabShots, asking the session's queries before each, publishing
    // the chunks and taking the answers and the state after each, and answering between ticks while the question is up.
    // The mark's aim check is asked every tick, as Earthrise asks it, and W2's integrity after a tick that changed a
    // chunk, as the integrity view asks it.
    const Lab& lab = TheLab();
    const std::span<const Simulation::LabItem> items = Simulation::LabItems();
    std::vector<std::string> mismatched;
    for (std::size_t index = 0; index < items.size(); ++index)
    {
      const Simulation::LabItem& item = items[index];
      Simulation::World world = *lab.site;
      std::uint32_t id = 1;
      bool changed = true;
      const auto ask = [&world, &id, &lab, &item, &changed]
      {
        const auto query = [&world, &id](const Protocol::Query& _query)
        {
          Core::ByteWriter writer;
          Protocol::Encode(Protocol::QueryRequest{.id = id++, .query = _query}, writer);
          world.Ask(Simulation::BreachRun::PLAYER, writer.Bytes());
        };
        query(Protocol::AimCheck{.engine = lab.setup.engine, .targetVoxels = item.markVoxels});
        if (changed)
        {
          query(Protocol::SectionIntegrity{.structure = lab.setup.target});
        }
      };
      const auto live = [&world, &ask, &changed](Simulation::LabShots& _shots)
      {
        bool done = false;
        while (!done)
        {
          ask();
          done = _shots.Advance(world);
          changed = !world.Publish().empty();
          static_cast<void>(world.TakeAnswers());
          static_cast<void>(world.State());
        }
      };
      Simulation::LabShots prior(lab.setup, item.priorAimVoxels);
      live(prior);
      for (int frame = 0; frame < 3; ++frame) // the question up, the clock paused
      {
        ask();
        world.AnswerBetweenTicks();
        static_cast<void>(world.TakeAnswers());
      }
      Simulation::LabShots asked(lab.setup, Asked(item));
      live(asked);
      if (Simulation::StateHash(world) != lab.runs.at(index).outcome.stateHash)
      {
        Logger::WriteMessage(("fails: " + item.id).c_str());
        mismatched.push_back(item.id);
      }
    }
    Assert::IsTrue(mismatched.empty());
  }

  TEST_METHOD(KeepsTheBreachRunsSite)
  {
    // BuildBreachRun builds its site through BuildLabSite, so the two worlds agree before a tick.
    Simulation::World run(TheLab().desc);
    const Simulation::BreachRun breach = Simulation::BuildBreachRun(run);
    Assert::AreEqual(TheLab().setup.engine, breach.engine);
    Assert::AreEqual(TheLab().setup.squad, breach.squad);
    Assert::IsTrue(TheLab().setup.goalVoxels == breach.goalVoxels);
    Assert::AreEqual(Simulation::StateHash(*TheLab().site), Simulation::StateHash(run));
  }
};

} // namespace SimulationTests
