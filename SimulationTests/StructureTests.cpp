#include "pch.h"

#include "FlatWorld.h"
#include "Save.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Structures as entities (M3-B1; ADR-005, decision 10): a template stamped by edits, turned clockwise
// seen from above as the voxel pass turns a batch, recorded with its name, saved and named by the
// collapses of its voxels.

namespace SimulationTests
{

namespace
{

constexpr std::uint8_t OWNER = 2;
constexpr std::int32_t GROUND = FlatWorld::GROUND_VOXELS;
constexpr std::int32_t ORIGIN_X = 20;
constexpr std::int32_t ORIGIN_Z = 20;
constexpr std::int32_t BOX_VOXELS = 12; // larger than T3 either way, turned or not

Protocol::VoxelPosition At(std::int32_t _x, std::int32_t _y, std::int32_t _z)
{
  return {.xVoxels = _x, .yVoxels = _y, .zVoxels = _z};
}

/// The voxels the M2 breach run's PlaceSection stamped for a template at (_x, GROUND, _z), turned
/// _anticlockwise quarter turns anticlockwise seen from above: the reference the structure is held to.
std::map<std::tuple<std::int32_t, std::int32_t, std::int32_t>, std::uint8_t>
PlaceSectionVoxels(const Simulation::TemplateShape& _shape, std::int32_t _x, std::int32_t _z, std::uint32_t _anticlockwise)
{
  std::map<std::tuple<std::int32_t, std::int32_t, std::int32_t>, std::uint8_t> voxels;
  const auto width = static_cast<std::int32_t>(_shape.widthVoxels);
  const auto depth = static_cast<std::int32_t>(_shape.depthVoxels);
  for (std::uint32_t y = 0; y < _shape.heightVoxels; ++y)
  {
    for (std::int32_t z = 0; z < depth; ++z)
    {
      for (std::int32_t x = 0; x < width; ++x)
      {
        const std::uint8_t index = _shape.At(static_cast<std::uint32_t>(x), y, static_cast<std::uint32_t>(z));
        if (index == 0)
        {
          continue;
        }
        std::int32_t turnedX = x;
        std::int32_t turnedZ = z;
        switch (_anticlockwise % 4)
        {
        case 1:
          turnedX = depth - 1 - z;
          turnedZ = x;
          break;
        case 2:
          turnedX = width - 1 - x;
          turnedZ = depth - 1 - z;
          break;
        case 3:
          turnedX = z;
          turnedZ = width - 1 - x;
          break;
        default:
          break;
        }
        voxels[{_x + turnedX, GROUND + static_cast<std::int32_t>(y), _z + turnedZ}] = index;
      }
    }
  }
  return voxels;
}

/// Every voxel above the ground in the box about the origin that is not empty.
std::map<std::tuple<std::int32_t, std::int32_t, std::int32_t>, std::uint8_t> Stamped(const Simulation::World& _world)
{
  std::map<std::tuple<std::int32_t, std::int32_t, std::int32_t>, std::uint8_t> voxels;
  for (std::int32_t y = GROUND; y < GROUND + BOX_VOXELS; ++y)
  {
    for (std::int32_t z = ORIGIN_Z - 2; z < ORIGIN_Z + BOX_VOXELS; ++z)
    {
      for (std::int32_t x = ORIGIN_X - 2; x < ORIGIN_X + BOX_VOXELS; ++x)
      {
        const std::uint8_t index = _world.Voxels().At(At(x, y, z));
        if (index != 0)
        {
          voxels[{x, y, z}] = index;
        }
      }
    }
  }
  return voxels;
}

} // namespace

TEST_CLASS(StructureTests)
{
public:
  TEST_METHOD(StampsItsTemplateTurnByTurnAsPlaceSectionDid)
  {
    // PlaceSection turned anticlockwise and a structure turns clockwise, as the voxel pass turns a
    // batch, so clockwise turn t is PlaceSection's turn (4 - t) mod 4.
    const Simulation::TemplateShape shape = BakedServerData().FindTemplate("T3")->Shape();
    for (std::uint32_t turns = 0; turns < 4; ++turns)
    {
      Simulation::World world(FlatWorld::Desc());
      const Protocol::EntityId id = world.AddStructure(OWNER, "T3", At(ORIGIN_X, GROUND, ORIGIN_Z), turns, "W7");
      Assert::IsTrue(Stamped(world) == PlaceSectionVoxels(shape, ORIGIN_X, ORIGIN_Z, (4 - turns) % 4));
      const Simulation::Structure& structure = world.Structures().at(id);
      Assert::AreEqual(std::string("T3"), structure.templateId);
      Assert::AreEqual(std::string("W7"), structure.name);
      Assert::AreEqual(turns, structure.quarterTurns);
      Assert::IsTrue(structure.originVoxels == At(ORIGIN_X, GROUND, ORIGIN_Z));
    }
  }

  TEST_METHOD(OneTurnTakesItsFrontEast)
  {
    // T3's front is +z, the side its battlements are higher on (MvpAssets.md); one clockwise turn
    // takes it to +x, so the footprint runs 2 along x and 8 along z.
    Simulation::World world(FlatWorld::Desc());
    static_cast<void>(world.AddStructure(OWNER, "T3", At(ORIGIN_X, GROUND, ORIGIN_Z), 1, "W1"));
    for (std::int32_t z = ORIGIN_Z; z < ORIGIN_Z + 8; ++z)
    {
      Assert::IsTrue(world.Voxels().At(At(ORIGIN_X, GROUND, z)) != 0);
      Assert::IsTrue(world.Voxels().At(At(ORIGIN_X + 1, GROUND, z)) != 0);
      Assert::AreEqual(std::uint8_t{0}, world.Voxels().At(At(ORIGIN_X + 2, GROUND, z)));
    }
    Assert::AreEqual(std::uint8_t{0}, world.Voxels().At(At(ORIGIN_X, GROUND, ORIGIN_Z + 8)));
  }

  TEST_METHOD(RefusesATemplateTheDataLacks)
  {
    Simulation::World world(FlatWorld::Desc());
    Assert::ExpectException<std::invalid_argument>([&world]
                                                   { static_cast<void>(world.AddStructure(OWNER, "T99", At(4, GROUND, 4), 0, "W1")); });
    Assert::IsTrue(world.Structures().empty());
  }

  TEST_METHOD(ACollapseNamesTheStructureItFellFrom)
  {
    // The hand calculation of M2's risks: a hole 5 wide through a T3 section's base drops the slot
    // above its middle, and the collapse names the section.
    Simulation::World world(FlatWorld::Desc());
    static_cast<void>(world.AddStructure(OWNER, "T3", At(4, GROUND, 4), 0, "W1"));
    const Protocol::EntityId section = world.AddStructure(OWNER, "T3", At(ORIGIN_X, GROUND, ORIGIN_Z), 0, "W2");
    world.Tick();
    Assert::IsTrue(world.Collapses().empty());
    for (std::int32_t y = GROUND; y <= GROUND + 1; ++y)
    {
      for (std::int32_t z = ORIGIN_Z; z <= ORIGIN_Z + 1; ++z)
      {
        for (std::int32_t x = ORIGIN_X + 1; x <= ORIGIN_X + 5; ++x)
        {
          world.Voxels().Edit(At(x, y, z), 0);
        }
      }
    }
    world.Tick();
    Assert::AreEqual(std::size_t{1}, world.Collapses().size());
    Assert::AreEqual(section, world.Collapses().begin()->second.structure);
    Assert::AreEqual(std::string("W2"), world.Structures().at(section).name);
  }

  TEST_METHOD(ASaveHoldsItsStructures)
  {
    Simulation::World world(FlatWorld::Desc());
    static_cast<void>(world.AddStructure(OWNER, "T3", At(ORIGIN_X, GROUND, ORIGIN_Z), 3, "W10"));
    static_cast<void>(world.AddStructure(OWNER, "T6", At(4, GROUND, 4), 1, ""));
    world.Tick();
    const std::vector<std::byte> save = Simulation::WriteSave(world);
    const Simulation::World loaded = Simulation::LoadSave(save, FlatWorld::Desc());
    Assert::AreEqual(world.Structures().size(), loaded.Structures().size());
    for (const auto& [id, structure] : world.Structures())
    {
      const Simulation::Structure& other = loaded.Structures().at(id);
      Assert::AreEqual(structure.templateId, other.templateId);
      Assert::AreEqual(structure.name, other.name);
      Assert::AreEqual(structure.quarterTurns, other.quarterTurns);
      Assert::IsTrue(structure.originVoxels == other.originVoxels);
    }
    Assert::IsTrue(save == Simulation::WriteSave(loaded));
    Assert::AreEqual(Simulation::StateHash(world), Simulation::StateHash(loaded));
  }
};

} // namespace SimulationTests
