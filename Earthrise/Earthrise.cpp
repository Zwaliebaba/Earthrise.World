#include "pch.h"

#include "BreachLab.h"
#include "BreachRun.h"
#include "ByteWriter.h"
#include "Camera.h"
#include "Catalog.h"
#include "Command.h"
#include "CommandQueue.h"
#include "Debris.h"
#include "FireMode.h"
#include "FireTally.h"
#include "FrameRing.h"
#include "FrameTimer.h"
#include "GpuDevice.h"
#include "Hash.h"
#include "Hud.h"
#include "HudAlerts.h"
#include "HudModel.h"
#include "KeyboardInput.h"
#include "Lab.h"
#include "Look.h"
#include "ModelPlacement.h"
#include "Overlays.h"
#include "PointerInput.h"
#include "Script.h"
#include "ServerData.h"
#include "Session.h"
#include "SharedHash.h"
#include "Strings.h"
#include "SwapChain.h"
#include "TextLayout.h"
#include "TickClock.h"
#include "UiScale.h"
#include "UploadRing.h"
#include "Window.h"
#include "World.h"
#include "WorldPicker.h"
#include "WorldRenderer.h"

#include <cctype>

// Earthrise.World (ADR-020): the executable that joins the client and the simulation in one process. Each frame the
// client's clock gives the ticks due; for each, the executable hands the simulation the session's command and query
// bytes, ticks it, and hands the session back a copy of its state update, the chunks it published and its answers
// (ADR-020, decision 2). The client draws from the session alone.
//
// In M3-C3 it runs the M2 breach run live on the breach lab's site (M3-B7): the trebuchet's five scripted shots, the
// slot's warning and fall with its debris, and the squad walking through. From M3-E4 the player fires: select the
// trebuchet, press F and right-click a target. `--script` plays the five scripted shots instead.
//
// `--lab <participant> [--seed n]` runs the breach lab (M3-G1, Lab.h) instead, and writes its log to Logs\ beside the
// executable.
//
// It starts borderless over the whole monitor (ADR-011, decision 1), and Alt+Enter switches between that and a window.
// `--windowed` starts it in a window. `--speed 10` runs the clock at ten times. `--ui-scale 1.5` sets the UI scale over
// the display's band (Hud.md §2).

namespace
{

constexpr const wchar_t* TITLE = L"Earthrise.World";
constexpr std::uint64_t UPLOAD_RING_BYTES = std::uint64_t{32} * 1024 * 1024;
constexpr double TITLE_REFRESH_SECONDS = 0.5;
// A frame longer than this, such as one held up by a drag of the window, counts as this long.
constexpr double LONGEST_FRAME_SECONDS = 0.25;
constexpr double MAX_SPEED = 100.0;
constexpr std::uint8_t PLAYER = Simulation::BreachRun::PLAYER;
constexpr float CELL_CENTER_METERS = 0.5F;
constexpr std::uint32_t QUARTER_TURNS = 4;
// The frame's figures, in the bottom-left corner (M3-E1), in logical pixels.
constexpr float FIGURES_MARGIN_LOGICAL_PIXELS = 8.0F;

struct Options
{
  bool windowed;
  double speed;
  std::optional<float> uiScale;           // --ui-scale, over the display's band
  bool scripted;                          // --script: the M2 run's shots, as in M3-C3
  std::optional<std::string> participant; // --lab: the breach lab, for this participant (M3-G1)
  std::uint64_t seed;                     // --seed, the lab's item order; by default the participant's hash
};

/// Whether the command line holds _flag as a word of its own.
bool HasFlag(std::wstring_view _commandLine, std::wstring_view _flag)
{
  std::wstring_view rest = _commandLine;
  while (!rest.empty())
  {
    const std::size_t end = rest.find(L' ');
    if (rest.substr(0, end) == _flag)
    {
      return true;
    }
    rest.remove_prefix(end == std::wstring_view::npos ? rest.size() : end + 1);
  }
  return false;
}

/// The value after _option, as `--option value` or `--option=value`, or nothing when the command line lacks it.
std::optional<std::string> OptionValue(std::wstring_view _commandLine, std::wstring_view _option)
{
  const std::size_t at = _commandLine.find(_option);
  if (at == std::wstring_view::npos)
  {
    return std::nullopt;
  }
  std::wstring_view rest = _commandLine.substr(at + _option.size());
  if (rest.starts_with(L'='))
  {
    rest.remove_prefix(1);
  }
  while (rest.starts_with(L' '))
  {
    rest.remove_prefix(1);
  }
  std::string value;
  for (const wchar_t character : rest.substr(0, rest.find(L' ')))
  {
    value.push_back(character < 128 ? static_cast<char>(character) : '?');
  }
  return value;
}

/// The speed after --speed, as `--speed 10` or `--speed=10`, and 1 when it asks for none. Throws std::invalid_argument
/// for anything but a number above 0 and up to MAX_SPEED.
double SpeedFrom(std::wstring_view _commandLine)
{
  const std::optional<std::string> given = OptionValue(_commandLine, L"--speed");
  if (!given)
  {
    return 1.0;
  }
  const std::string& value = *given;
  double speed = 0.0;
  const std::from_chars_result parsed = std::from_chars(value.data(), value.data() + value.size(), speed);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !(speed > 0.0) || speed > MAX_SPEED)
  {
    throw std::invalid_argument(std::format("--speed takes a number above 0 and up to {}, and was given \"{}\".", MAX_SPEED, value));
  }
  return speed;
}

/// The participant after --lab, or nothing without it. Throws std::invalid_argument when --lab has none, or one that is
/// not letters, digits, - and _, since it names the log's file.
std::optional<std::string> ParticipantFrom(std::wstring_view _commandLine)
{
  if (!HasFlag(_commandLine, L"--lab") && _commandLine.find(L"--lab=") == std::wstring_view::npos)
  {
    return std::nullopt;
  }
  const std::string participant = OptionValue(_commandLine, L"--lab").value_or("");
  const bool named =
    !participant.empty() &&
    std::ranges::all_of(participant, [](char _character)
                        { return std::isalnum(static_cast<unsigned char>(_character)) != 0 || _character == '-' || _character == '_'; });
  if (!named || participant.starts_with('-'))
  {
    throw std::invalid_argument("--lab needs a participant, of letters, digits, - and _, such as --lab P03");
  }
  return participant;
}

/// The lab's item order's seed: the number after --seed, or else the participant's hash, so that each participant's
/// order is their own. Throws std::invalid_argument for a --seed that is not a whole number.
std::uint64_t SeedFrom(std::wstring_view _commandLine, std::string_view _participant)
{
  const std::optional<std::string> given = OptionValue(_commandLine, L"--seed");
  if (!given)
  {
    return Core::Fnv1a64(std::as_bytes(std::span(_participant.data(), _participant.size())));
  }
  std::uint64_t seed = 0;
  const std::from_chars_result parsed = std::from_chars(given->data(), given->data() + given->size(), seed);
  if (parsed.ec != std::errc{} || parsed.ptr != given->data() + given->size())
  {
    throw std::invalid_argument(std::format("--seed takes a whole number, and was given \"{}\".", *given));
  }
  return seed;
}

/// The folder the executable sits in: the build's output folder (ADR-010, decision 3).
std::filesystem::path ExecutablePath()
{
  std::wstring path(32768, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  if (length == 0 || length >= path.size())
  {
    throw std::runtime_error("Earthrise.World could not find where its executable is");
  }
  path.resize(length);
  return {path};
}

/// The build id (ADR-014, decision 6): Core::Fnv1a64 over the executable's own file.
std::uint64_t BuildIdOf(const std::filesystem::path& _executable)
{
  std::ifstream stream(_executable, std::ios::binary);
  if (!stream)
  {
    throw std::runtime_error("Earthrise.World could not read its own executable to compute its build id");
  }
  const std::vector<char> raw{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
  return Core::Fnv1a64(std::as_bytes(std::span(raw)));
}

/// The server's data, from ServerData\ beside the executable (ADR-016). It refuses to start when the two halves were
/// baked from different sources, naming both hashes (ADR-016, decision 7, amended by M3-C3).
Simulation::ServerData LoadServerData(const std::filesystem::path& _folder)
{
  Simulation::ServerData data = Simulation::ServerData::Load(_folder / L"ServerData" / L"ServerData.bin");
  if (data.SharedHash() != Content::SharedHash())
  {
    throw std::runtime_error(std::format("the server's data and Content were baked from different sources: ServerData's shared hash is "
                                         "{:016x}, Content's is {:016x}. Rebuild the solution.",
                                         data.SharedHash(), Content::SharedHash()));
  }
  return data;
}

/// Waits for the GPU before the resources it may still read are destroyed, also when the loop leaves by an exception.
struct FlushOnExit
{
  Engine::CommandQueue& queue;

  ~FlushOnExit()
  {
    try
    {
      queue.Flush();
    }
    catch (...)
    {
      // The device is lost, so there is nothing left to wait for; say so to a debugger.
      OutputDebugStringW(L"Earthrise.World: the GPU was lost before exit, so its work was not waited for.\n");
    }
  }
};

float Meters(Core::Fixed _value) noexcept
{
  return static_cast<float>(_value.Raw()) / static_cast<float>(Core::Fixed::ONE_RAW);
}

const Content::ModelView& Model(std::string_view _id)
{
  const Content::ModelView* model = Content::FindModel(_id);
  if (model == nullptr)
  {
    throw std::runtime_error(std::format("Content has no model {}", _id));
  }
  return *model;
}

/// A model's footprint along x and z in its own frame, in meters.
std::pair<float, float> Footprint(const Content::ModelView& _model) noexcept
{
  return {Client::MODEL_VOXEL_METERS * static_cast<float>(_model.grid.widthVoxels),
          Client::MODEL_VOXEL_METERS * static_cast<float>(_model.grid.depthVoxels)};
}

/// The minimum corner, after _turns quarter turns clockwise seen from above, of a footprint from _xMeters along x,
/// _widthMeters wide and _depthMeters deep, in a model's own frame: a turn takes +z towards +x.
std::pair<float, float> TurnedMinimum(float _xMeters, float _widthMeters, float _depthMeters, std::uint32_t _turns) noexcept
{
  const std::array<std::pair<float, float>, 4> corners = {
    {{_xMeters, 0.0F}, {_xMeters + _widthMeters, 0.0F}, {_xMeters, _depthMeters}, {_xMeters + _widthMeters, _depthMeters}}};
  std::pair<float, float> minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
  for (const auto& [x, z] : corners)
  {
    // x' = x cos + z sin, z' = -x sin + z cos, at a whole quarter turn.
    const std::array<std::pair<float, float>, QUARTER_TURNS> turned = {{{x, z}, {z, -x}, {-x, -z}, {-z, x}}};
    const auto [turnedX, turnedZ] = turned.at(_turns % QUARTER_TURNS);
    minimum = {std::min(minimum.first, turnedX), std::min(minimum.second, turnedZ)};
  }
  return minimum;
}

/// _id standing centred on the cell at _cellVoxels, turned _turns quarter turns.
Client::ModelPlacement Centered(std::string_view _id, Protocol::VoxelPosition _cellVoxels, std::uint32_t _turns, std::uint8_t _side)
{
  const Content::ModelView& model = Model(_id);
  const auto [width, depth] = Footprint(model);
  const bool across = _turns % 2 == 1;
  return {.modelId = model.id,
          .xMeters = static_cast<float>(_cellVoxels.xVoxels) + CELL_CENTER_METERS - (0.5F * (across ? depth : width)),
          .yMeters = static_cast<float>(_cellVoxels.yVoxels),
          .zMeters = static_cast<float>(_cellVoxels.zVoxels) + CELL_CENTER_METERS - (0.5F * (across ? width : depth)),
          .quarterTurns = _turns % QUARTER_TURNS,
          .sideColorIndex = _side};
}

/// The trebuchet at its cell, facing as it does: S3, released, centred on the cell, or S3c, cocked, at S3's placement
/// moved COCKED_POSE_OFFSET_X_METERS along the model's own x before its turn (Client/Look.h).
Client::ModelPlacement Trebuchet(const Protocol::EngineView& _engine, bool _released)
{
  const Client::ModelPlacement released = Centered("S3", _engine.cellVoxels, _engine.quarterTurns, Client::ATTACKER_COLOR_INDEX);
  if (_released)
  {
    return released;
  }
  const Content::ModelView& releasedModel = Model("S3");
  const Content::ModelView& cocked = Model("S3c");
  const auto [releasedWidth, releasedDepth] = Footprint(releasedModel);
  const auto [cockedWidth, cockedDepth] = Footprint(cocked);
  const std::pair<float, float> releasedMinimum = TurnedMinimum(0.0F, releasedWidth, releasedDepth, _engine.quarterTurns);
  const std::pair<float, float> cockedMinimum =
    TurnedMinimum(Client::COCKED_POSE_OFFSET_X_METERS, cockedWidth, cockedDepth, _engine.quarterTurns);
  return {.modelId = cocked.id,
          .xMeters = released.xMeters + (cockedMinimum.first - releasedMinimum.first),
          .yMeters = released.yMeters,
          .zMeters = released.zMeters + (cockedMinimum.second - releasedMinimum.second),
          .quarterTurns = released.quarterTurns,
          .sideColorIndex = released.sideColorIndex};
}

/// Whether _engine is drawn released at _tick: for RELEASED_POSE_TICKS after it fires.
bool Released(const Protocol::EngineView& _engine, std::uint32_t _tick, const Client::FireTally& _tally) noexcept
{
  const std::optional<std::uint32_t> fired = _tally.LastFiredTick(_engine.id);
  return fired.has_value() && _tick - *fired < Client::RELEASED_POSE_TICKS;
}

/// Each engine's footprint on the ground, in the pose it is drawn in, where a click selects it (M3-E3).
std::vector<Client::EngineFootprint> EngineFootprints(const Client::Session& _session, const Client::FireTally& _tally)
{
  std::vector<Client::EngineFootprint> footprints;
  for (const Protocol::EngineView& engine : _session.State().engines)
  {
    const Client::ModelPlacement placement = Trebuchet(engine, Released(engine, _session.State().tick, _tally));
    const auto [width, depth] = Footprint(Model(placement.modelId));
    const bool across = placement.quarterTurns % 2 == 1;
    footprints.push_back({.engine = engine.id,
                          .xMeters = placement.xMeters,
                          .zMeters = placement.zMeters,
                          .widthMeters = across ? depth : width,
                          .depthMeters = across ? width : depth,
                          .groundMeters = placement.yMeters});
  }
  return footprints;
}

std::string_view UnitModel(Protocol::UnitKind _kind) noexcept
{
  switch (_kind)
  {
  case Protocol::UnitKind::Laborer:
    return "U1";
  case Protocol::UnitKind::Spearman:
    return "U2";
  case Protocol::UnitKind::Archer:
    return "U3";
  case Protocol::UnitKind::Outrider:
    return "U4";
  case Protocol::UnitKind::ManAtArms:
    return "U5";
  case Protocol::UnitKind::Sapper:
    return "S2";
  }
  return "U2";
}

/// What the client draws from its session (ADR-020, decision 2): every structure is in the world's voxels, so the
/// models are the engines in their poses, the units, and each shot between its last two positions; and each collapse's
/// debris, scattered on the first tick of its fall and kept at rest for good.
class Presentation
{
public:
  void Update(const Client::Session& _session, float _seconds)
  {
    const Protocol::StateUpdate& state = _session.State();
    for (const Protocol::CollapseView& collapse : state.collapses)
    {
      if (!collapse.fallen.empty() && !m_debris.contains(collapse.id))
      {
        m_debris[collapse.id].Scatter(collapse.id, collapse.fallen, collapse.landing, _seconds);
      }
    }
  }

  void Place(const Client::Session& _session, const Client::FireTally& _tally, double _tickFraction,
             std::vector<Client::ModelPlacement>& _placements, std::vector<Engine::OrientedVoxel>& _debris, float _seconds) const
  {
    const Protocol::StateUpdate& state = _session.State();
    _placements.clear();
    for (const Protocol::EngineView& engine : state.engines)
    {
      _placements.push_back(Trebuchet(engine, Released(engine, state.tick, _tally)));
    }
    for (const Protocol::UnitView& unit : state.units)
    {
      // Facing north, into the courtyard, until units carry a facing.
      Client::ModelPlacement placement = Centered(UnitModel(unit.kind), unit.cellVoxels, 0, Client::ATTACKER_COLOR_INDEX);
      const auto [width, depth] = Footprint(Model(placement.modelId));
      placement.xMeters = Meters(unit.position.xMeters) - (0.5F * width);
      placement.yMeters = Meters(unit.position.yMeters);
      placement.zMeters = Meters(unit.position.zMeters) - (0.5F * depth);
      _placements.push_back(placement);
    }
    const Content::ModelView& shotModel = Model("X1");
    const auto [shotWidth, shotDepth] = Footprint(shotModel);
    const float shotHeight = Client::MODEL_VOXEL_METERS * static_cast<float>(shotModel.grid.heightVoxels);
    for (const Protocol::ShotView& shot : state.shots)
    {
      // Between its positions in the last two updates, a tick behind (GameDesign.md, M3's values, item 17).
      DirectX::XMFLOAT3 at{Meters(shot.position.xMeters), Meters(shot.position.yMeters), Meters(shot.position.zMeters)};
      const auto before = std::ranges::find(_session.PreviousState().shots, shot.id, &Protocol::ShotView::id);
      if (before != _session.PreviousState().shots.end())
      {
        const auto fraction = static_cast<float>(_tickFraction);
        at = {Meters(before->position.xMeters) + ((at.x - Meters(before->position.xMeters)) * fraction),
              Meters(before->position.yMeters) + ((at.y - Meters(before->position.yMeters)) * fraction),
              Meters(before->position.zMeters) + ((at.z - Meters(before->position.zMeters)) * fraction)};
      }
      _placements.push_back({.modelId = shotModel.id,
                             .xMeters = at.x - (0.5F * shotWidth),
                             .yMeters = at.y - (0.5F * shotHeight),
                             .zMeters = at.z - (0.5F * shotDepth),
                             .quarterTurns = 0,
                             .sideColorIndex = 0});
    }
    _debris.clear();
    for (const auto& [id, debris] : m_debris)
    {
      debris.Pose(_seconds, _debris);
    }
  }

private:
  std::map<Protocol::EntityId, Client::Debris> m_debris;
};

/// The middle of _cell's floor, where the camera looks when it jumps to an alert.
DirectX::XMFLOAT3 FloorMiddle(const Protocol::VoxelPosition& _cell) noexcept
{
  return {static_cast<float>(_cell.xVoxels) + CELL_CENTER_METERS, static_cast<float>(_cell.yVoxels),
          static_cast<float>(_cell.zVoxels) + CELL_CENTER_METERS};
}

/// The M2 breach run, live (M3-C3 to F3): the squad's order from the run's script, and the shots from the player, or
/// from the script with --script.
class BreachScenario final : public Earthrise::Scenario
{
public:
  BreachScenario(const Simulation::World::Desc& _desc, double _speed, bool _scripted)
    : m_world(_desc),
      m_run(Simulation::BuildBreachRun(m_world)),
      m_speed(_speed),
      m_scripted(_scripted)
  {
  }

  [[nodiscard]] Simulation::World& World() override
  {
    return m_world;
  }

  [[nodiscard]] std::size_t Generation() const noexcept override
  {
    return 0;
  }

  [[nodiscard]] Protocol::VoxelPosition Goal() const noexcept override
  {
    return m_run.goalVoxels;
  }

  bool Tick(Client::Session& _session, double /* _seconds */) override
  {
    for (const std::vector<std::byte>& command : _session.TakeCommands())
    {
      m_world.Submit(PLAYER, command);
    }
    for (const std::vector<std::byte>& query : _session.TakeQueries())
    {
      m_world.Ask(PLAYER, query);
    }
    for (const Simulation::ScriptedCommand& scripted : m_run.script)
    {
      // The squad's order always; the shots only with --script, since the player fires (M3-E4).
      if (scripted.tick == m_world.TickCount() && (m_scripted || !std::holds_alternative<Protocol::FireEngine>(scripted.command)))
      {
        Core::ByteWriter writer;
        Protocol::Encode(scripted.command, writer);
        m_world.Submit(scripted.player, writer.Bytes());
      }
    }
    m_world.Tick();
    return true;
  }

  void Frame(const Earthrise::ScenarioInput& /* _input */, Client::Session& /* _session */) override {}

  [[nodiscard]] double Speed() const noexcept override
  {
    return m_speed;
  }

  [[nodiscard]] bool Paused() const noexcept override
  {
    return false;
  }

  [[nodiscard]] bool FireAllowed() const noexcept override
  {
    return true;
  }

  [[nodiscard]] bool PassabilityForced() const noexcept override
  {
    return false;
  }

  [[nodiscard]] bool TakesSpace() const noexcept override
  {
    return false;
  }

  [[nodiscard]] std::optional<Client::HudMode> Mode() const override
  {
    return std::nullopt;
  }

  [[nodiscard]] std::optional<Client::HudObjective> Objective() const override
  {
    return std::nullopt;
  }

  [[nodiscard]] std::vector<Client::OverlayBox> Marks(const Client::Session& /* _session */) const override
  {
    return {};
  }

private:
  Simulation::World m_world;
  Simulation::BreachRun m_run;
  double m_speed;
  bool m_scripted;
};

/// The client's side of one world: what it was told, and what it draws and asks from it. The lab replaces it with
/// each item's world. It is made afresh and never moved: under MSVC's checked library a std::map's move may throw.
struct Play
{
  Client::Session session;
  Client::FireTally tally;
  Presentation presentation;
  Client::WorldPicker picker;
  Client::Overlays views;
  Client::FireMode fire;
  Client::HudAlerts alerts;
  Protocol::EntityId selected = Protocol::NO_ENTITY;
  bool worldChanged = false; // the last frame took a changed chunk
};

/// Looking at W2, the target, from the default azimuth and view height (GameDesign.md, M3's values, item 14).
Client::Camera DefaultCamera(const Protocol::VoxelPosition& _goal)
{
  return Client::Camera({static_cast<float>(_goal.xVoxels) + CELL_CENTER_METERS, static_cast<float>(Simulation::BreachRun::GROUND_VOXELS),
                         static_cast<float>(Simulation::BreachRun::WALL_SOUTH_VOXELS)});
}

int Run(const Options& _options)
{
  const std::filesystem::path executable = ExecutablePath();
  const std::uint64_t buildId = BuildIdOf(executable);
  const Simulation::ServerData serverData = LoadServerData(executable.parent_path());
  // The lab's site, the breach run's map and seeds (M3-B7).
  const Simulation::World::Desc desc = Simulation::LabDesc(serverData, buildId);

  Engine::Window window({.title = TITLE, .clientWidthPixels = 1280, .clientHeightPixels = 720, .fullScreen = !_options.windowed});
  Engine::GpuDevice device({.enableDebugLayer = Engine::DEBUG_BUILD, .useWarp = false});
  Engine::SwapChain swapChain(device, window.Handle());
  Engine::FrameRing frames(device.Device(), device.DirectQueue());
  Engine::UploadRing ring(device.Device(), UPLOAD_RING_BYTES);
  const auto edgeMeters = [](std::int32_t _chunks) { return static_cast<float>(_chunks * Protocol::CHUNK_EDGE_VOXELS); };
  Client::WorldRenderer renderer(device.Device(), device.DirectQueue().Queue(),
                                 {.widthMeters = edgeMeters(desc.widthChunks),
                                  .heightMeters = edgeMeters(desc.heightChunks),
                                  .depthMeters = edgeMeters(desc.depthChunks),
                                  .targetFormat = Engine::SwapChain::VIEW_FORMAT},
                                 swapChain.WidthPixels(), swapChain.HeightPixels());
  const FlushOnExit flushOnExit{device.DirectQueue()};

  // What runs: the breach lab with --lab (M3-G1), or the breach run live.
  std::unique_ptr<Earthrise::Scenario> scenario;
  if (_options.participant.has_value())
  {
    scenario = std::make_unique<Earthrise::Lab>(
      desc, Earthrise::LabOptions{.participant = *_options.participant,
                                  .seed = _options.seed,
                                  .logFolder = executable.parent_path() / L"Logs",
                                  .buildId = buildId,
                                  .serverDataHash = serverData.Hash(),
                                  .widthPixels = swapChain.WidthPixels(),
                                  .heightPixels = swapChain.HeightPixels(),
                                  .uiScale = _options.uiScale.value_or(Client::UiScaleForDpi(window.Dpi()))});
  }
  else
  {
    scenario = std::make_unique<BreachScenario>(desc, _options.speed, _options.scripted);
  }

  Client::TickClock clock;
  std::unique_ptr<Play> play;                          // never null after the first frame's reset
  std::size_t generation = scenario->Generation() + 1; // so that the first frame builds the play
  Client::Camera camera = DefaultCamera(scenario->Goal());
  Engine::PointerInput pointer;
  Engine::KeyboardInput keyboard;
  Engine::FrameTimer timer;
  std::vector<Client::ModelPlacement> placements;
  std::vector<Engine::OrientedVoxel> debris;
  std::vector<Client::ScreenQuad> interfaceQuads;
  Client::Hud hud;
  Client::HudModel hudModel;                              // as the HUD was last built, which the clicks land on
  bool integrityView = true;                              // F1, on by default (Hud.md §7)
  bool passabilityView = false;                           // F2
  std::optional<DirectX::XMFLOAT4X4> drawnViewProjection; // the last frame's, which the clicks land on
  const std::array<float, 4> figuresColor = Client::LinearColor(Client::DIM_SRGB);
  std::int32_t lastXPixels = 0;
  std::int32_t lastYPixels = 0;
  double seconds = 0.0;
  double titleSeconds = 0.0;
  std::uint32_t titleFrames = 0;
  while (Engine::Window::PumpMessages(pointer, keyboard))
  {
    // A window that is not in front does not see its keys and buttons released, so it forgets them.
    if (GetForegroundWindow() != window.Handle())
    {
      pointer.ReleaseAll();
      keyboard.ReleaseAll();
    }
    // Alt+Enter switches between borderless full screen and a window (ADR-011, decision 1).
    if (keyboard.WasPressed(VK_RETURN) && keyboard.IsDown(VK_MENU))
    {
      window.SetFullScreen(!window.IsFullScreen());
    }
    const Engine::Window::Size size = window.ClientSize();
    if (size.widthPixels == 0 || size.heightPixels == 0)
    {
      // Minimized: nothing to draw until the window comes back.
      WaitMessage();
      continue;
    }
    if (size.widthPixels != swapChain.WidthPixels() || size.heightPixels != swapChain.HeightPixels())
    {
      device.DirectQueue().Flush();
      swapChain.Resize(size.widthPixels, size.heightPixels);
      renderer.Resize(swapChain.WidthPixels(), swapChain.HeightPixels());
    }
    // A fresh world, the lab's next item: the client forgets the old one, and the camera goes back to W2.
    if (generation != scenario->Generation())
    {
      generation = scenario->Generation();
      device.DirectQueue().Flush();
      renderer.ResetWorld();
      play = std::make_unique<Play>();
      play->views.WatchBreach(scenario->Goal());
      camera = DefaultCamera(scenario->Goal());
      hudModel = {};
      drawnViewProjection.reset();
    }
    Play& client = *play;

    swapChain.WaitForFrame();
    const double lapSeconds = std::min(timer.LapSeconds(), LONGEST_FRAME_SECONDS);
    seconds += lapSeconds;

    // The simulation's ticks: the session's bytes in, the world ticked, a copy of what it told back out.
    clock.SetPaused(scenario->Paused());
    const std::uint32_t ticks = clock.Advance(lapSeconds, scenario->Speed());
    for (std::uint32_t tick = 0; tick < ticks; ++tick)
    {
      if (!scenario->Tick(client.session, seconds))
      {
        break;
      }
      Simulation::World& world = scenario->World();
      const std::vector<Protocol::ChunkSnapshot> published = world.Publish();
      const std::vector<std::vector<std::byte>> answers = world.TakeAnswers();
      client.session.Receive(world.State(), published, answers);
      client.tally.Observe(client.session.State());
      client.presentation.Update(client.session, static_cast<float>(seconds));
    }
    // Answers given between ticks while the clock is paused, as the lab's question has them (M3-G1).
    if (scenario->Paused())
    {
      Simulation::World& world = scenario->World();
      const std::vector<std::vector<std::byte>> answers = world.TakeAnswers();
      if (!answers.empty())
      {
        client.session.Receive(world.State(), {}, answers);
      }
    }

    // A left click on the HUD is the HUD's (M3-E2): the camera buttons turn and zoom, JUMP, or Space for the newest
    // alert with a place, looks at the alert's place, and the lab's question takes its answer. One on the world
    // selects the engine whose footprint it lands on, or nothing, and Esc deselects (M3-E3).
    bool answerOpen = false;
    bool answerShut = false;
    if (pointer.WasPressed(Engine::PointerInput::Button::Left))
    {
      const std::optional<Client::HudTarget> hit = hud.HitTest(pointer.XPixels(), pointer.YPixels());
      if (!hit.has_value() && !hud.Covers(pointer.XPixels(), pointer.YPixels()) && drawnViewProjection.has_value() && !client.fire.Active())
      {
        const Engine::PickRay ray = Client::WorldPicker::RayThrough(*drawnViewProjection, pointer.XPixels(), pointer.YPixels(),
                                                                    swapChain.WidthPixels(), swapChain.HeightPixels());
        client.selected = client.picker.Pick(ray, EngineFootprints(client.session, client.tally)).engine;
      }
      if (hit.has_value())
      {
        switch (hit->control)
        {
        case Client::HudControl::TurnLeft:
          camera.Turn(-1);
          break;
        case Client::HudControl::TurnRight:
          camera.Turn(1);
          break;
        case Client::HudControl::ZoomOut:
          camera.Zoom(-1.0F);
          break;
        case Client::HudControl::ZoomIn:
          camera.Zoom(1.0F);
          break;
        case Client::HudControl::Cancel:
          client.fire.Cancel();
          break;
        case Client::HudControl::Jump:
          if (hit->alert < hudModel.alerts.size() && hudModel.alerts.at(hit->alert).location.has_value())
          {
            camera.LookAt(FloorMiddle(*hudModel.alerts.at(hit->alert).location));
          }
          break;
        case Client::HudControl::AnswerOpen:
          answerOpen = true;
          break;
        case Client::HudControl::AnswerShut:
          answerShut = true;
          break;
        }
      }
    }
    // The selection, while its engine is there.
    const auto selectedEngine = std::ranges::find(client.session.State().engines, client.selected, &Protocol::EngineView::id);
    const Protocol::EngineView* selectedView = selectedEngine != client.session.State().engines.end() ? &*selectedEngine : nullptr;
    if (selectedView == nullptr)
    {
      client.selected = Protocol::NO_ENTITY;
    }
    // FIRE (M3-E4), from the world voxel under the pointer, off the HUD, while an engine is selected and the scenario
    // lets the player fire. Esc leaves the mode, and with none on deselects.
    const bool fireAllowed = scenario->FireAllowed();
    if (!fireAllowed && client.fire.Active())
    {
      client.fire.Cancel();
    }
    const bool pointerOnHud = hud.Covers(pointer.XPixels(), pointer.YPixels());
    std::optional<Protocol::VoxelPosition> pointed;
    if (selectedView != nullptr && !pointerOnHud && drawnViewProjection.has_value())
    {
      const Engine::PickRay ray = Client::WorldPicker::RayThrough(*drawnViewProjection, pointer.XPixels(), pointer.YPixels(),
                                                                  swapChain.WidthPixels(), swapChain.HeightPixels());
      const Client::WorldPick pick = client.picker.Pick(ray, {});
      if (pick.voxel.has_value())
      {
        pointed = Protocol::VoxelPosition{pick.voxel->cell[0], pick.voxel->cell[1], pick.voxel->cell[2]};
      }
    }
    const bool usedEscape =
      client.fire.Update({.fPressed = fireAllowed && keyboard.WasPressed('F'),
                          .escapePressed = keyboard.WasPressed(VK_ESCAPE),
                          .rightClicked = fireAllowed && pointer.WasPressed(Engine::PointerInput::Button::Right) && !pointerOnHud,
                          .shiftHeld = keyboard.IsDown(VK_SHIFT),
                          .pointedVoxels = pointed,
                          .worldChanged = client.worldChanged},
                         selectedView, client.session);
    if (keyboard.WasPressed(VK_ESCAPE) && !usedEscape)
    {
      client.selected = Protocol::NO_ENTITY;
      client.fire.Update({}, nullptr, client.session);
    }
    window.ShowCrosshair(client.fire.Active());
    if (keyboard.WasPressed(VK_F1))
    {
      integrityView = !integrityView;
    }
    if (keyboard.WasPressed(VK_F2))
    {
      passabilityView = !passabilityView;
    }
    const bool passabilityShown = passabilityView || scenario->PassabilityForced();
    if (keyboard.WasPressed(VK_SPACE) && !scenario->TakesSpace())
    {
      const auto newest = std::ranges::find_if(hudModel.alerts, [](const Client::HudAlert& _alert) { return _alert.location.has_value(); });
      if (newest != hudModel.alerts.end())
      {
        camera.LookAt(FloorMiddle(*newest->location));
      }
    }

    camera.Steer(keyboard, pointer, pointer.XPixels() - lastXPixels, pointer.YPixels() - lastYPixels, swapChain.HeightPixels(),
                 static_cast<float>(lapSeconds));
    lastXPixels = pointer.XPixels();
    lastYPixels = pointer.YPixels();
    // The scenario's own keys: the lab's answers, O and H (not S, which pans), and Space for the next item.
    const auto [quarterTurns, zoomNotches] = camera.TakeMoves();
    scenario->Frame({.nowSeconds = seconds,
                     .openPressed = answerOpen || keyboard.WasPressed('O'),
                     .shutPressed = answerShut || keyboard.WasPressed('H'),
                     .nextPressed = keyboard.WasPressed(VK_SPACE),
                     .quarterTurns = quarterTurns,
                     .zoomNotches = zoomNotches,
                     .integrityOn = integrityView,
                     .passabilityOn = passabilityShown},
                    client.session);
    client.presentation.Place(client.session, client.tally, clock.TickFraction(), placements, debris, static_cast<float>(seconds));

    const std::uint64_t completed = device.DirectQueue().CompletedValue();
    ring.Reclaim(completed);
    renderer.Release(completed);
    const std::uint32_t slot = frames.Slot();
    ID3D12GraphicsCommandList* commandList = frames.Begin();
    const float aspect = static_cast<float>(swapChain.WidthPixels()) / static_cast<float>(swapChain.HeightPixels());
    // The UI scale: --ui-scale, or the display's band (Hud.md §2).
    const float uiScale = _options.uiScale.value_or(Client::UiScaleForDpi(window.Dpi()));
    const float topBarPixels = Client::TOP_BAR_LOGICAL_PIXELS * uiScale;
    DirectX::XMFLOAT4X4 viewProjection{};
    DirectX::XMStoreFloat4x4(&viewProjection, camera.ViewProjection(aspect, swapChain.HeightPixels(), topBarPixels));
    // The chunks the frame takes: the world view's, the picker's, and the integrity view's asks (M3-F2).
    const std::vector<Protocol::ChunkSnapshot> snapshots = client.session.TakeSnapshots();
    client.picker.TakeSnapshots(snapshots);
    client.worldChanged = !snapshots.empty();
    client.views.Ask(client.session, snapshots, integrityView);
    // The passability view's tiles, those the view shows on the lab's ground (M3-F3).
    const std::vector<Client::Tile> visibleTiles = Client::Overlays::VisibleTiles(
      viewProjection, swapChain.WidthPixels(), swapChain.HeightPixels(), static_cast<float>(Simulation::BreachRun::GROUND_VOXELS));
    client.views.AskReach(client.session, snapshots, visibleTiles, PLAYER, passabilityShown);
    // The HUD, built from the session (M3-E2), and the frame's figures under it in the bottom-left corner, over the
    // integrity view's labels (M3-F2).
    client.views.Update(client.session, seconds, client.alerts);
    client.alerts.Expire(seconds);
    // The objective line: the scenario's, or the selection while there is one, with FIRE's key, or RELOADING in off
    // text, or the breach.
    const bool ready = selectedView != nullptr && client.selected != Protocol::NO_ENTITY && Client::FireMode::Ready(*selectedView);
    const Client::HudObjective objective = scenario->Objective().value_or(
      client.selected != Protocol::NO_ENTITY
        ? Client::HudObjective{.title = std::string(Client::TREBUCHET_NAME),
                               .detail = std::string(ready ? Client::FIRE_READY_DETAIL : Client::RELOADING_DETAIL),
                               .detailOff = !ready}
        : Client::HudObjective{.title = std::string(Client::BREACH_OBJECTIVE_TITLE),
                               .detail = std::string(Client::BREACH_OBJECTIVE_DETAIL),
                               .detailOff = false});
    // The banner: FIRE's while it is on, else the scenario's.
    const Client::HudMode mode = client.fire.Active() ? client.fire.Mode() : scenario->Mode().value_or(client.fire.Mode());
    hudModel = {.resources = Client::BreachResources(client.session.State().tick, client.tally.Fired(PLAYER)),
                .objective = objective,
                .mode = mode,
                .alerts = client.alerts.Shown(),
                .selectedEngine = client.selected,
                .integrityView = integrityView,
                .passabilityView = passabilityShown};
    interfaceQuads.clear();
    const std::uint32_t figuresScale = Client::TextLayout::PixelsPerFontPixel(Client::TextSize::Label, uiScale);
    const std::int32_t margin = Client::ScaledPixels(FIGURES_MARGIN_LOGICAL_PIXELS, uiScale);
    Client::TextLayout::Lay(std::format(Client::FIGURES, renderer.VoxelPassMilliseconds(), client.session.State().tick, scenario->Speed()),
                            margin,
                            static_cast<std::int32_t>(swapChain.HeightPixels()) - margin -
                              static_cast<std::int32_t>(Client::TextLayout::CellPixels(figuresScale)),
                            figuresScale, figuresColor, interfaceQuads);
    client.views.LayLabels(client.session, viewProjection, swapChain.WidthPixels(), swapChain.HeightPixels(),
                           static_cast<std::int32_t>(std::lround(topBarPixels)), uiScale, integrityView, interfaceQuads);
    hud.Build(hudModel, swapChain.WidthPixels(), uiScale, interfaceQuads);
    const std::optional<Client::AttackCursor> cursor = client.fire.Cursor(client.session);
    if (cursor.has_value() && !pointerOnHud)
    {
      hud.LayCursor(*cursor, pointer.XPixels(), pointer.YPixels(), uiScale, interfaceQuads);
    }
    // The world overlays (ADR-021): the views, then FIRE's strike mark and the scenario's marks.
    std::vector<Client::OverlayLayer> overlays = client.views.Layers(client.session, seconds, integrityView);
    overlays.push_back(client.views.PassabilityLayer(client.session, passabilityShown));
    overlays.push_back({.alpha = Client::STRIKE_MARK_ALPHA, .boxes = client.fire.StrikeMark(client.session)});
    overlays.push_back({.alpha = Client::STRIKE_MARK_ALPHA, .boxes = scenario->Marks(client.session)});
    Client::WorldRenderer::Frame frame{.commandList = commandList,
                                       .slot = slot,
                                       .ring = &ring,
                                       .snapshots = snapshots,
                                       .placements = placements,
                                       .debris = debris,
                                       .overlays = overlays,
                                       .viewProjection = viewProjection,
                                       .forward = {},
                                       .viewHeightMeters = camera.ViewHeightMeters(),
                                       .backBuffer = swapChain.BackBuffer(),
                                       .backBufferView = swapChain.BackBufferView(),
                                       .backWidthPixels = swapChain.WidthPixels(),
                                       .backHeightPixels = swapChain.HeightPixels(),
                                       .interfaceQuads = interfaceQuads};
    drawnViewProjection = viewProjection;
    DirectX::XMStoreFloat3(&frame.forward, camera.Forward());
    renderer.Draw(frame);
    const std::uint64_t fenceValue = frames.End();
    ring.Retire(fenceValue);
    renderer.Retire(fenceValue);
    swapChain.Present(true);

    titleSeconds += lapSeconds;
    ++titleFrames;
    if (titleSeconds >= TITLE_REFRESH_SECONDS)
    {
      const std::wstring title = std::format(
        L"{} | {} | {:.2f} ms a frame, voxel pass {:.2f} ms on the GPU | tick {} at {}x | {} voxels | view {:.0f} m, azimuth {:.0f}", TITLE,
        device.AdapterName(), 1000.0 * titleSeconds / titleFrames, renderer.VoxelPassMilliseconds(), client.session.State().tick,
        scenario->Speed(), renderer.RecordsDrawn(), camera.ViewHeightMeters(), DirectX::XMConvertToDegrees(camera.AzimuthRadians()));
      SetWindowTextW(window.Handle(), title.c_str());
      titleSeconds = 0.0;
      titleFrames = 0;
    }
    pointer.EndFrame();
    keyboard.EndFrame();
  }
  return 0;
}

} // namespace

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR _commandLine, _In_ int)
{
  try
  {
    const std::wstring_view commandLine = _commandLine != nullptr ? _commandLine : L"";
    const std::optional<std::string> uiScale = OptionValue(commandLine, L"--ui-scale");
    const std::optional<std::string> participant = ParticipantFrom(commandLine);
    return Run({.windowed = HasFlag(commandLine, L"--windowed"),
                .speed = SpeedFrom(commandLine),
                .uiScale = uiScale ? std::optional<float>(Client::UiScaleFromOption(*uiScale)) : std::nullopt,
                .scripted = HasFlag(commandLine, L"--script"),
                .participant = participant,
                .seed = SeedFrom(commandLine, participant.value_or(""))});
  }
  catch (const winrt::hresult_error& error)
  {
    MessageBoxW(nullptr, error.message().c_str(), TITLE, MB_ICONERROR | MB_OK);
  }
  catch (const std::exception& error)
  {
    MessageBoxA(nullptr, error.what(), "Earthrise.World", MB_ICONERROR | MB_OK);
  }
  catch (...)
  {
    MessageBoxW(nullptr, L"An unknown error stopped Earthrise.World.", TITLE, MB_ICONERROR | MB_OK);
  }
  return 1;
}
