#include "pch.h"

#include "Scenario.h"

#include "Generator.h"

namespace Simulation
{

namespace
{

std::string ReadText(Core::ByteReader& _reader)
{
  const std::span<const std::byte> bytes = _reader.ReadBytes(_reader.ReadU32());
  std::string text(bytes.size(), '\0');
  std::ranges::transform(bytes, text.begin(), [](std::byte _byte) { return static_cast<char>(_byte); });
  return text;
}

Protocol::VoxelPosition ReadPosition(Core::ByteReader& _reader)
{
  // Read in order: braced initialisation evaluates its elements left to right.
  return {.xVoxels = _reader.ReadI32(), .yVoxels = _reader.ReadI32(), .zVoxels = _reader.ReadI32()};
}

template <typename Enum> Enum ReadEnum(Core::ByteReader& _reader, Enum _last, const char* _what)
{
  const std::uint8_t value = _reader.ReadU8();
  if (value > static_cast<std::uint8_t>(_last))
  {
    throw std::runtime_error(std::string("ServerData: SCEN holds a ") + _what + " this build does not know");
  }
  return static_cast<Enum>(value);
}

ScenarioBox ReadBox(Core::ByteReader& _reader)
{
  ScenarioBox box{.material = _reader.ReadU8(), .minVoxels = {}, .maxVoxels = {}};
  box.minVoxels = ReadPosition(_reader);
  box.maxVoxels = ReadPosition(_reader);
  return box;
}

std::vector<ScenarioBox> ReadBoxes(Core::ByteReader& _reader)
{
  const std::uint32_t count = _reader.ReadU32();
  constexpr std::size_t BOX_BYTES = 25;
  std::vector<ScenarioBox> boxes;
  boxes.reserve(std::min<std::size_t>(count, _reader.Remaining() / BOX_BYTES)); // at most what is left
  for (std::uint32_t number = 0; number < count; ++number)
  {
    boxes.push_back(ReadBox(_reader));
  }
  return boxes;
}

bool Holds(const ScenarioBox& _box, std::int32_t _x, std::int32_t _z) noexcept
{
  return _x >= _box.minVoxels.xVoxels && _x <= _box.maxVoxels.xVoxels && _z >= _box.minVoxels.zVoxels && _z <= _box.maxVoxels.zVoxels;
}

const Scenario& Find(const ServerData& _data, std::string_view _scenarioId)
{
  const Scenario* scenario = _data.FindScenario(_scenarioId);
  if (scenario == nullptr)
  {
    throw std::invalid_argument("the server data holds no scenario " + std::string(_scenarioId));
  }
  return *scenario;
}

/// The site: each column's voxels below the level from the ground boxes, the last that holds a voxel winning, and
/// nothing above; then the earthworks over them. Only the voxels that differ from the generated world are edited.
void BuildSite(World& _world, const Scenario& _scenario)
{
  const std::int32_t height = _world.Description().heightChunks * Protocol::CHUNK_EDGE_VOXELS;
  std::vector<std::uint8_t> column(static_cast<std::size_t>(height));
  for (std::int32_t z = _scenario.siteMinZVoxels; z <= _scenario.siteMaxZVoxels; ++z)
  {
    for (std::int32_t x = _scenario.siteMinXVoxels; x <= _scenario.siteMaxXVoxels; ++x)
    {
      std::ranges::fill(column, Protocol::Chunk::EMPTY);
      for (const ScenarioBox& box : _scenario.ground)
      {
        if (!Holds(box, x, z))
        {
          continue;
        }
        const std::int32_t top = std::min(box.maxVoxels.yVoxels, _scenario.groundVoxels - 1);
        for (std::int32_t y = std::max(box.minVoxels.yVoxels, 0); y <= top; ++y)
        {
          column.at(static_cast<std::size_t>(y)) = box.material;
        }
      }
      for (const ScenarioBox& box : _scenario.earthworks)
      {
        if (!Holds(box, x, z))
        {
          continue;
        }
        for (std::int32_t y = std::max(box.minVoxels.yVoxels, 0); y <= std::min(box.maxVoxels.yVoxels, height - 1); ++y)
        {
          column.at(static_cast<std::size_t>(y)) = box.material;
        }
      }
      for (std::int32_t y = 0; y < height; ++y)
      {
        const Protocol::VoxelPosition voxel{.xVoxels = x, .yVoxels = y, .zVoxels = z};
        if (_world.Voxels().At(voxel) != column.at(static_cast<std::size_t>(y)))
        {
          _world.Voxels().Edit(voxel, column.at(static_cast<std::size_t>(y)));
        }
      }
    }
  }
}

} // namespace

Scenario ReadScenario(Core::ByteReader& _reader)
{
  Scenario scenario{};
  scenario.id = ReadText(_reader);
  scenario.worldSeed = _reader.ReadU64();
  scenario.resourceSeed = _reader.ReadU64();
  scenario.matchSeed = _reader.ReadU64();
  scenario.siteMinXVoxels = _reader.ReadI32();
  scenario.siteMinZVoxels = _reader.ReadI32();
  scenario.siteMaxXVoxels = _reader.ReadI32();
  scenario.siteMaxZVoxels = _reader.ReadI32();
  scenario.groundVoxels = _reader.ReadI32();
  scenario.ground = ReadBoxes(_reader);
  const std::uint32_t structures = _reader.ReadU32();
  for (std::uint32_t number = 0; number < structures; ++number)
  {
    ScenarioStructure structure{};
    structure.templateId = ReadText(_reader);
    structure.name = ReadText(_reader);
    structure.owner = _reader.ReadU8();
    structure.originVoxels = ReadPosition(_reader);
    structure.quarterTurns = _reader.ReadU8() % 4;
    structure.foundationDepthVoxels = _reader.ReadI32();
    scenario.structures.push_back(std::move(structure));
  }
  scenario.earthworks = ReadBoxes(_reader);
  const std::uint32_t players = _reader.ReadU32();
  for (std::uint32_t number = 0; number < players; ++number)
  {
    ScenarioPlayer player{};
    player.id = _reader.ReadU8();
    player.side = ReadEnum(_reader, Protocol::Side::Defender, "side");
    player.stoneHundredthsCubicMeters = _reader.ReadI32();
    player.timberHundredthsCubicMeters = _reader.ReadI32();
    player.commander = ReadText(_reader);
    player.revealed = _reader.ReadU8() == 1;
    scenario.players.push_back(std::move(player));
  }
  const std::uint32_t squads = _reader.ReadU32();
  for (std::uint32_t number = 0; number < squads; ++number)
  {
    ScenarioSquad squad{};
    squad.kind = ReadEnum(_reader, Protocol::UnitKind::Trebuchet, "kind of unit");
    squad.owner = _reader.ReadU8();
    const std::uint32_t cells = _reader.ReadU32();
    for (std::uint32_t cell = 0; cell < cells; ++cell)
    {
      squad.cellsVoxels.push_back(ReadPosition(_reader));
    }
    scenario.squads.push_back(std::move(squad));
  }
  const std::uint32_t engines = _reader.ReadU32();
  for (std::uint32_t number = 0; number < engines; ++number)
  {
    ScenarioEngine engine{};
    engine.kind = ReadEnum(_reader, Protocol::EngineKind::Ram, "kind of engine");
    engine.owner = _reader.ReadU8();
    engine.cellVoxels = ReadPosition(_reader);
    engine.quarterTurns = _reader.ReadU8() % 4;
    engine.state = ReadEnum(_reader, Protocol::EngineState::Packing, "engine state");
    scenario.engines.push_back(engine);
  }
  const std::uint32_t garrisons = _reader.ReadU32();
  for (std::uint32_t number = 0; number < garrisons; ++number)
  {
    ScenarioGarrison garrison{};
    garrison.kind = ReadEnum(_reader, Protocol::UnitKind::Trebuchet, "kind of unit");
    garrison.owner = _reader.ReadU8();
    garrison.structure = ReadText(_reader);
    scenario.garrisons.push_back(std::move(garrison));
  }
  ScenarioTrial& trial = scenario.trial;
  trial.version = ReadEnum(_reader, Protocol::TrialVersion::Control, "trial version");
  trial.attacker = _reader.ReadU8();
  trial.defender = _reader.ReadU8();
  trial.keep = ReadText(_reader);
  trial.timeLimitTicks = _reader.ReadU32();
  trial.holdTicks = _reader.ReadU32();
  trial.ringCells = _reader.ReadI32();
  trial.campMinVoxels = ReadPosition(_reader);
  trial.campMaxVoxels = ReadPosition(_reader);
  trial.nearest = ReadText(_reader);
  trial.weak = ReadText(_reader);
  return scenario;
}

World::Desc ScenarioDesc(const ServerData& _data, std::string_view _scenarioId, std::uint64_t _buildId)
{
  const Scenario& scenario = Find(_data, _scenarioId);
  const auto generator = std::make_shared<const Generator>(Generator::Desc{.worldSeed = scenario.worldSeed,
                                                                           .resourceSeed = scenario.resourceSeed,
                                                                           .widthVoxels = Generator::MAP_WIDTH_VOXELS,
                                                                           .heightVoxels = Generator::MAP_HEIGHT_VOXELS,
                                                                           .depthVoxels = Generator::MAP_DEPTH_VOXELS});
  const std::int32_t minX = scenario.siteMinXVoxels;
  const std::int32_t minZ = scenario.siteMinZVoxels;
  const std::int32_t maxX = scenario.siteMaxXVoxels;
  const std::int32_t maxZ = scenario.siteMaxZVoxels;
  const std::int32_t level = scenario.groundVoxels;
  return {.serverData = &_data,
          .buildId = _buildId,
          .worldSeed = scenario.worldSeed,
          .resourceSeed = scenario.resourceSeed,
          .matchSeed = scenario.matchSeed,
          .generatorVersion = GENERATOR_VERSION,
          .widthChunks = generator->WidthChunks(),
          .heightChunks = generator->HeightChunks(),
          .depthChunks = generator->DepthChunks(),
          .generate = [generator](Protocol::ChunkCoordinate _coordinate, Protocol::Chunk& _chunk)
          { generator->GenerateChunk(_coordinate, _chunk); },
          .surfaceVoxels = [generator, minX, minZ, maxX, maxZ, level](std::int32_t _x, std::int32_t _z)
          { return _x >= minX && _x <= maxX && _z >= minZ && _z <= maxZ ? level : generator->SurfaceAt(_x, _z); }};
}

void LoadScenario(World& _world, std::string_view _scenarioId)
{
  const Scenario& scenario = Find(_world.Data(), _scenarioId);
  BuildSite(_world, scenario);
  std::map<std::string, Protocol::EntityId, std::less<>> named;
  for (const ScenarioStructure& structure : scenario.structures)
  {
    named.emplace(structure.name, _world.AddStructure(structure.owner, structure.templateId, structure.originVoxels, structure.quarterTurns,
                                                      structure.name, structure.foundationDepthVoxels));
  }
  for (const ScenarioPlayer& player : scenario.players)
  {
    _world.AddPlayer({.id = player.id,
                      .side = player.side,
                      .stoneHundredthsCubicMeters = player.stoneHundredthsCubicMeters,
                      .timberHundredthsCubicMeters = player.timberHundredthsCubicMeters});
  }
  for (const ScenarioSquad& squad : scenario.squads)
  {
    static_cast<void>(_world.AddSquad(squad.owner, _world.Data().Stats(squad.kind).navigationClass, squad.cellsVoxels, squad.kind));
  }
  for (const ScenarioEngine& engine : scenario.engines)
  {
    static_cast<void>(_world.AddSiegeEngine(engine.owner, engine.kind, engine.cellVoxels, engine.quarterTurns, engine.state));
  }
  for (const ScenarioGarrison& garrison : scenario.garrisons)
  {
    // Its figures start at the building's origin, which they leave the grid at (ADR-024, decision 5).
    const Protocol::EntityId building = named.at(garrison.structure);
    const Structure& structure = _world.Structures().at(building);
    const std::vector<Protocol::VoxelPosition> cells(_world.Data().Stats(garrison.kind).squadFigures, structure.originVoxels);
    const Protocol::EntityId squad =
      _world.AddSquad(garrison.owner, _world.Data().Stats(garrison.kind).navigationClass, cells, garrison.kind);
    _world.PlaceInGarrison(squad, building);
  }
  const ScenarioTrial& trial = scenario.trial;
  _world.SetTrial({.version = trial.version,
                   .attacker = trial.attacker,
                   .defender = trial.defender,
                   .keep = named.at(trial.keep),
                   .timeLimitTicks = trial.timeLimitTicks,
                   .holdTicks = trial.holdTicks,
                   .ringCells = trial.ringCells,
                   .campMinVoxels = trial.campMinVoxels,
                   .campMaxVoxels = trial.campMaxVoxels});
}

} // namespace Simulation
