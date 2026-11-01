#include "pch.h"

#include "BreachLab.h"
#include "BreachRun.h"
#include "ByteWriter.h"
#include "Camera.h"
#include "Catalog.h"
#include "Command.h"
#include "CommandQueue.h"
#include "Debris.h"
#include "FrameRing.h"
#include "FrameTimer.h"
#include "GpuDevice.h"
#include "Hash.h"
#include "FireMode.h"
#include "FireTally.h"
#include "Hud.h"
#include "HudAlerts.h"
#include "HudModel.h"
#include "KeyboardInput.h"
#include "Look.h"
#include "ModelPlacement.h"
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

// Earthrise.World (ADR-020): the executable that joins the client and the simulation in one process. Each frame the
// client's clock gives the ticks due; for each, the executable hands the simulation the session's command and query
// bytes, ticks it, and hands the session back a copy of its state update, the chunks it published and its answers
// (ADR-020, decision 2). The client draws from the session alone.
//
// In M3-C3 it runs the M2 breach run live on the breach lab's site (M3-B7): the trebuchet's five scripted shots, the
// slot's warning and fall with its debris, and the squad walking through. From M3-E4 the player fires: select the
// trebuchet, press F and right-click a target. `--script` plays the five scripted shots instead.
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
  std::optional<float> uiScale; // --ui-scale, over the display's band
  bool scripted;                // --script: the M2 run's shots, as in M3-C3
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

int Run(const Options& _options)
{
  const std::filesystem::path executable = ExecutablePath();
  const std::uint64_t buildId = BuildIdOf(executable);
  const Simulation::ServerData serverData = LoadServerData(executable.parent_path());

  // The simulation's side: the lab's site with the M2 breach run's script (M3-B7).
  const Simulation::World::Desc desc = Simulation::LabDesc(serverData, buildId);
  Simulation::World world(desc);
  const Simulation::BreachRun run = Simulation::BuildBreachRun(world);
  Client::Session session;
  Client::TickClock clock;
  Client::FireTally tally;
  Presentation presentation;

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

  // Looking at W2, the target, from the default azimuth and view height (GameDesign.md, M3's values, item 14).
  const Protocol::VoxelPosition goal = run.goalVoxels;
  Client::Camera camera({static_cast<float>(goal.xVoxels) + CELL_CENTER_METERS, static_cast<float>(Simulation::BreachRun::GROUND_VOXELS),
                         static_cast<float>(Simulation::BreachRun::WALL_SOUTH_VOXELS)});
  Engine::PointerInput pointer;
  Engine::KeyboardInput keyboard;
  Engine::FrameTimer timer;
  std::vector<Client::ModelPlacement> placements;
  std::vector<Engine::OrientedVoxel> debris;
  std::vector<Client::ScreenQuad> interfaceQuads;
  Client::Hud hud;
  Client::HudAlerts alerts;
  Client::HudModel hudModel; // as the HUD was last built, which the clicks land on
  Client::WorldPicker picker;
  Protocol::EntityId selected = Protocol::NO_ENTITY;
  Client::FireMode fire;
  bool worldChanged = false;                              // the last frame took a changed chunk
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

    swapChain.WaitForFrame();
    const double lapSeconds = std::min(timer.LapSeconds(), LONGEST_FRAME_SECONDS);
    seconds += lapSeconds;

    // The simulation's ticks: the session's bytes in, the world ticked, a copy of what it told back out.
    const std::uint32_t ticks = clock.Advance(lapSeconds, _options.speed);
    for (std::uint32_t tick = 0; tick < ticks; ++tick)
    {
      for (const std::vector<std::byte>& command : session.TakeCommands())
      {
        world.Submit(PLAYER, command);
      }
      for (const std::vector<std::byte>& query : session.TakeQueries())
      {
        world.Ask(PLAYER, query);
      }
      for (const Simulation::ScriptedCommand& scripted : run.script)
      {
        // The squad's order always; the shots only with --script, since the player fires (M3-E4).
        if (scripted.tick == world.TickCount() && (_options.scripted || !std::holds_alternative<Protocol::FireEngine>(scripted.command)))
        {
          Core::ByteWriter writer;
          Protocol::Encode(scripted.command, writer);
          world.Submit(scripted.player, writer.Bytes());
        }
      }
      world.Tick();
      const std::vector<Protocol::ChunkSnapshot> published = world.Publish();
      const std::vector<std::vector<std::byte>> answers = world.TakeAnswers();
      session.Receive(world.State(), published, answers);
      tally.Observe(session.State());
      presentation.Update(session, static_cast<float>(seconds));
    }

    // A left click on the HUD is the HUD's (M3-E2): the camera buttons turn and zoom, and JUMP, or Space for the
    // newest alert with a place, looks at the alert's place. One on the world selects the engine whose footprint it
    // lands on, or nothing, and Esc deselects (M3-E3).
    if (pointer.WasPressed(Engine::PointerInput::Button::Left))
    {
      const std::optional<Client::HudTarget> hit = hud.HitTest(pointer.XPixels(), pointer.YPixels());
      if (!hit.has_value() && !hud.Covers(pointer.XPixels(), pointer.YPixels()) && drawnViewProjection.has_value() && !fire.Active())
      {
        const Engine::PickRay ray = Client::WorldPicker::RayThrough(*drawnViewProjection, pointer.XPixels(), pointer.YPixels(),
                                                                    swapChain.WidthPixels(), swapChain.HeightPixels());
        selected = picker.Pick(ray, EngineFootprints(session, tally)).engine;
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
          fire.Cancel();
          break;
        case Client::HudControl::Jump:
          if (hit->alert < hudModel.alerts.size() && hudModel.alerts.at(hit->alert).location.has_value())
          {
            camera.LookAt(FloorMiddle(*hudModel.alerts.at(hit->alert).location));
          }
          break;
        }
      }
    }
    // The selection, while its engine is there.
    const auto selectedEngine = std::ranges::find(session.State().engines, selected, &Protocol::EngineView::id);
    const Protocol::EngineView* selectedView = selectedEngine != session.State().engines.end() ? &*selectedEngine : nullptr;
    if (selectedView == nullptr)
    {
      selected = Protocol::NO_ENTITY;
    }
    // FIRE (M3-E4), from the world voxel under the pointer, off the HUD, while an engine is selected. Esc leaves the
    // mode, and with none on deselects.
    const bool pointerOnHud = hud.Covers(pointer.XPixels(), pointer.YPixels());
    std::optional<Protocol::VoxelPosition> pointed;
    if (selectedView != nullptr && !pointerOnHud && drawnViewProjection.has_value())
    {
      const Engine::PickRay ray = Client::WorldPicker::RayThrough(*drawnViewProjection, pointer.XPixels(), pointer.YPixels(),
                                                                  swapChain.WidthPixels(), swapChain.HeightPixels());
      const Client::WorldPick pick = picker.Pick(ray, {});
      if (pick.voxel.has_value())
      {
        pointed = Protocol::VoxelPosition{pick.voxel->cell[0], pick.voxel->cell[1], pick.voxel->cell[2]};
      }
    }
    const bool usedEscape = fire.Update({.fPressed = keyboard.WasPressed('F'),
                                         .escapePressed = keyboard.WasPressed(VK_ESCAPE),
                                         .rightClicked = pointer.WasPressed(Engine::PointerInput::Button::Right) && !pointerOnHud,
                                         .shiftHeld = keyboard.IsDown(VK_SHIFT),
                                         .pointedVoxels = pointed,
                                         .worldChanged = worldChanged},
                                        selectedView, session);
    if (keyboard.WasPressed(VK_ESCAPE) && !usedEscape)
    {
      selected = Protocol::NO_ENTITY;
      fire.Update({}, nullptr, session);
    }
    window.ShowCrosshair(fire.Active());
    if (keyboard.WasPressed(VK_SPACE))
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
    presentation.Place(session, tally, clock.TickFraction(), placements, debris, static_cast<float>(seconds));

    const std::uint64_t completed = device.DirectQueue().CompletedValue();
    ring.Reclaim(completed);
    renderer.Release(completed);
    const std::uint32_t slot = frames.Slot();
    ID3D12GraphicsCommandList* commandList = frames.Begin();
    const float aspect = static_cast<float>(swapChain.WidthPixels()) / static_cast<float>(swapChain.HeightPixels());
    // The UI scale: --ui-scale, or the display's band (Hud.md §2).
    const float uiScale = _options.uiScale.value_or(Client::UiScaleForDpi(window.Dpi()));
    const float topBarPixels = Client::TOP_BAR_LOGICAL_PIXELS * uiScale;
    // The HUD, built from the session (M3-E2), and the frame's figures under it in the bottom-left corner.
    alerts.Expire(seconds);
    // The objective line names the selection while there is one, with FIRE's key, or RELOADING in off text.
    const bool ready = selectedView != nullptr && selected != Protocol::NO_ENTITY && Client::FireMode::Ready(*selectedView);
    const Client::HudObjective objective =
      selected != Protocol::NO_ENTITY
        ? Client::HudObjective{.title = std::string(Client::TREBUCHET_NAME),
                               .detail = std::string(ready ? Client::FIRE_READY_DETAIL : Client::RELOADING_DETAIL),
                               .detailOff = !ready}
        : Client::HudObjective{.title = std::string(Client::BREACH_OBJECTIVE_TITLE),
                               .detail = std::string(Client::BREACH_OBJECTIVE_DETAIL),
                               .detailOff = false};
    hudModel = {.resources = Client::BreachResources(session.State().tick, tally.Fired(PLAYER)),
                .objective = objective,
                .mode = fire.Mode(),
                .alerts = alerts.Shown(),
                .selectedEngine = selected,
                .integrityView = true,
                .passabilityView = false};
    interfaceQuads.clear();
    const std::uint32_t figuresScale = Client::TextLayout::PixelsPerFontPixel(Client::TextSize::Label, uiScale);
    const std::int32_t margin = Client::ScaledPixels(FIGURES_MARGIN_LOGICAL_PIXELS, uiScale);
    Client::TextLayout::Lay(std::format(Client::FIGURES, renderer.VoxelPassMilliseconds(), session.State().tick, _options.speed), margin,
                            static_cast<std::int32_t>(swapChain.HeightPixels()) - margin -
                              static_cast<std::int32_t>(Client::TextLayout::CellPixels(figuresScale)),
                            figuresScale, figuresColor, interfaceQuads);
    hud.Build(hudModel, swapChain.WidthPixels(), uiScale, interfaceQuads);
    const std::optional<Client::AttackCursor> cursor = fire.Cursor(session);
    if (cursor.has_value() && !pointerOnHud)
    {
      hud.LayCursor(*cursor, pointer.XPixels(), pointer.YPixels(), uiScale, interfaceQuads);
    }
    // The world overlays (ADR-021): FIRE's strike mark.
    const std::array<Client::OverlayLayer, 1> overlays = {{{.alpha = Client::STRIKE_MARK_ALPHA, .boxes = fire.StrikeMark(session)}}};
    Client::WorldRenderer::Frame frame{.commandList = commandList,
                                       .slot = slot,
                                       .ring = &ring,
                                       .snapshots = {},
                                       .placements = placements,
                                       .debris = debris,
                                       .overlays = overlays,
                                       .viewProjection = {},
                                       .forward = {},
                                       .viewHeightMeters = camera.ViewHeightMeters(),
                                       .backBuffer = swapChain.BackBuffer(),
                                       .backBufferView = swapChain.BackBufferView(),
                                       .backWidthPixels = swapChain.WidthPixels(),
                                       .backHeightPixels = swapChain.HeightPixels(),
                                       .interfaceQuads = interfaceQuads};
    const std::vector<Protocol::ChunkSnapshot> snapshots = session.TakeSnapshots();
    frame.snapshots = snapshots;
    picker.TakeSnapshots(snapshots);
    worldChanged = !snapshots.empty();
    DirectX::XMStoreFloat4x4(&frame.viewProjection, camera.ViewProjection(aspect, swapChain.HeightPixels(), topBarPixels));
    drawnViewProjection = frame.viewProjection;
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
        device.AdapterName(), 1000.0 * titleSeconds / titleFrames, renderer.VoxelPassMilliseconds(), session.State().tick, _options.speed,
        renderer.RecordsDrawn(), camera.ViewHeightMeters(), DirectX::XMConvertToDegrees(camera.AzimuthRadians()));
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
    return Run({.windowed = HasFlag(commandLine, L"--windowed"),
                .speed = SpeedFrom(commandLine),
                .uiScale = uiScale ? std::optional<float>(Client::UiScaleFromOption(*uiScale)) : std::nullopt,
                .scripted = HasFlag(commandLine, L"--script")});
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
