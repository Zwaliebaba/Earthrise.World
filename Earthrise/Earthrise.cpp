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
#include "Scenario.h"
#include "ServerData.h"
#include "Session.h"
#include "SharedHash.h"
#include "SiegeAttacker.h"
#include "Strings.h"
#include "SwapChain.h"
#include "TextLayout.h"
#include "TickClock.h"
#include "UiScale.h"
#include "UploadRing.h"
#include "Window.h"
#include "World.h"
#include "WorldModels.h"
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
// executable. `--siege` loads the Siege castle (M4-B4; ADR-026) instead and runs it with the defender's commander and the
// player's side unordered, so that the castle, its earthworks, the camp and the armies can be seen; M4-J1 builds the trial
// on it. `--siege --mirror` loads the mirrored castle, SiegeMirror, whose weak section is on the west (M4-B6).
// `--siege --watch` seats the scripted attacker (Simulation::SiegeAttacker, M4-F5) for the player, so that the owner can
// watch a trial before the HUD can order one (M4-H1); J1 keeps the flag.
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
constexpr std::uint8_t PLAYER = Simulation::BreachRun::PLAYER; // the Siege castle's attacker too
constexpr std::string_view SIEGE_SCENARIO = "Siege";
constexpr std::string_view SIEGE_MIRROR_SCENARIO = "SiegeMirror";
constexpr float CELL_CENTER_METERS = 0.5F;
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
  bool siege;                             // --siege: the Siege castle (M4-B4)
  bool mirror;                            // --mirror, with --siege: the mirrored castle (M4-B6)
  bool watch;                             // --watch, with --siege: the scripted attacker plays the player (M4-H1)
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
  std::string participant = OptionValue(_commandLine, L"--lab").value_or(""); // not const, so the return moves it
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

/// What the client draws from its session (ADR-020, decision 2): every structure is in the world's voxels, so the
/// models are the units, the engines in their poses, the stones and the open gates, between their last two updates
/// (Client::WorldModels, M4-H1); and each collapse's debris, scattered on the first tick of its fall and kept at rest
/// for good.
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
             std::vector<Client::ModelPlacement>& _placements, std::vector<Protocol::VoxelPosition>& _hidden,
             std::vector<Engine::OrientedVoxel>& _debris, float _seconds)
  {
    m_models.Place(_session.PreviousState(), _session.State(), _tally, _tickFraction, _placements, _hidden);
    _debris.clear();
    for (const auto& [id, debris] : m_debris)
    {
      debris.Pose(_seconds, _debris);
    }
  }

  /// Each engine's model's box as last drawn, where a click selects it (M3-E3; M3-H3).
  [[nodiscard]] const std::vector<Client::EngineBox>& EngineBoxes() const noexcept
  {
    return m_models.EngineBoxes();
  }

private:
  Client::WorldModels m_models;
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
class BreachScenario final : public Earthrise::Run
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

  [[nodiscard]] Protocol::VoxelPosition CameraTarget() const noexcept override
  {
    // W2, the target, from the default azimuth and view height (GameDesign.md, M3's values, item 14).
    return {.xVoxels = m_run.goalVoxels.xVoxels,
            .yVoxels = Simulation::BreachRun::GROUND_VOXELS,
            .zVoxels = Simulation::BreachRun::WALL_SOUTH_VOXELS};
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

  void Frame(const Earthrise::RunInput& /* _input */, Client::Session& /* _session */) override {}

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

  [[nodiscard]] bool SelectionAllowed() const noexcept override
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

  [[nodiscard]] std::vector<Client::HudResource> Resources(std::uint32_t _tick, std::uint32_t _shotsFired) const override
  {
    return Client::BreachResources(_tick, _shotsFired);
  }

  [[nodiscard]] bool ShowsFigures() const noexcept override
  {
    return true;
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

/// The Siege castle (M4-B4; ADR-026), loaded in place of the lab's site and run with M3's client, so that the owner sees
/// the castle, its ditch, rampart and causeway, the camp and the armies. The player is the attacker, unordered, or with
/// _watch played by the scripted attacker (M4-H1); M4-J1 builds the trial on it.
class SiegeRun final : public Earthrise::Run
{
public:
  SiegeRun(const Simulation::World::Desc& _desc, std::string_view _scenarioId, double _speed, bool _watch)
    : m_world(_desc),
      m_speed(_speed)
  {
    Simulation::LoadScenario(m_world, _scenarioId);
    if (_watch)
    {
      m_world.SetCommander(PLAYER, std::make_unique<Simulation::SiegeAttacker>(nullptr));
    }
    const std::optional<Simulation::TrialRules>& trial = m_world.Trial();
    if (!trial.has_value())
    {
      throw std::runtime_error("the Siege scenario holds no trial");
    }
    const Simulation::Structure& keep = m_world.Structures().at(trial->keep);
    const Simulation::ServerTemplate* shape = m_world.Data().FindTemplate(keep.templateId);
    const bool across = keep.quarterTurns % 2 == 1;
    const auto width = static_cast<std::int32_t>(across ? shape->depthVoxels : shape->widthVoxels);
    const auto depth = static_cast<std::int32_t>(across ? shape->widthVoxels : shape->depthVoxels);
    m_keepMiddle = {.xVoxels = keep.originVoxels.xVoxels + (width / 2),
                    .yVoxels = keep.originVoxels.yVoxels,
                    .zVoxels = keep.originVoxels.zVoxels + (depth / 2)};
    // The keep's foot, south of its forebuilding, which the bailey's walls close until a breach.
    m_goal = {.xVoxels = m_keepMiddle.xVoxels, .yVoxels = keep.originVoxels.yVoxels, .zVoxels = keep.originVoxels.zVoxels - 2};
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
    return m_goal;
  }

  [[nodiscard]] Protocol::VoxelPosition CameraTarget() const noexcept override
  {
    return m_keepMiddle;
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
    m_world.Tick();
    return true;
  }

  void Frame(const Earthrise::RunInput& /* _input */, Client::Session& /* _session */) override {}

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

  [[nodiscard]] bool SelectionAllowed() const noexcept override
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
    return Client::HudObjective{.title = std::string(Client::SIEGE_OBJECTIVE_TITLE), .detail = {}, .detailOff = false};
  }

  /// The breach run's resources until M4-J1 gives the trial its own.
  [[nodiscard]] std::vector<Client::HudResource> Resources(std::uint32_t _tick, std::uint32_t _shotsFired) const override
  {
    return Client::BreachResources(_tick, _shotsFired);
  }

  [[nodiscard]] bool ShowsFigures() const noexcept override
  {
    return true;
  }

  [[nodiscard]] std::vector<Client::OverlayBox> Marks(const Client::Session& /* _session */) const override
  {
    return {};
  }

private:
  Simulation::World m_world;
  double m_speed;
  Protocol::VoxelPosition m_keepMiddle{};
  Protocol::VoxelPosition m_goal{};
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

/// Looking at the run's target, from the default azimuth and view height (GameDesign.md, M3's values, item 14).
Client::Camera DefaultCamera(const Protocol::VoxelPosition& _target)
{
  return Client::Camera(
    {static_cast<float>(_target.xVoxels) + CELL_CENTER_METERS, static_cast<float>(_target.yVoxels), static_cast<float>(_target.zVoxels)});
}

int Run(const Options& _options)
{
  const std::filesystem::path executable = ExecutablePath();
  const std::uint64_t buildId = BuildIdOf(executable);
  const Simulation::ServerData serverData = LoadServerData(executable.parent_path());
  // The lab's site, the breach run's map and seeds (M3-B7), or the Siege castle's (M4-B4) or its mirror's (M4-B6).
  const std::string_view siegeScenario = _options.mirror ? SIEGE_MIRROR_SCENARIO : SIEGE_SCENARIO;
  const Simulation::World::Desc desc =
    _options.siege ? Simulation::ScenarioDesc(serverData, siegeScenario, buildId) : Simulation::LabDesc(serverData, buildId);

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

  // What runs: the Siege castle with --siege (M4-B4), the breach lab with --lab (M3-G1), or the breach run live.
  std::unique_ptr<Earthrise::Run> run;
  if (_options.siege)
  {
    run = std::make_unique<SiegeRun>(desc, siegeScenario, _options.speed, _options.watch);
  }
  else if (_options.participant.has_value())
  {
    run = std::make_unique<Earthrise::Lab>(
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
    run = std::make_unique<BreachScenario>(desc, _options.speed, _options.scripted);
  }

  Client::TickClock clock;
  std::unique_ptr<Play> play;                     // never null after the first frame's reset
  std::size_t generation = run->Generation() + 1; // so that the first frame builds the play
  Client::Camera camera = DefaultCamera(run->CameraTarget());
  Engine::PointerInput pointer;
  Engine::KeyboardInput keyboard;
  Engine::FrameTimer timer;
  std::vector<Client::ModelPlacement> placements;
  std::vector<Protocol::VoxelPosition> hiddenVoxels; // an open gate's, which its open model is drawn over
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
    if (generation != run->Generation())
    {
      generation = run->Generation();
      device.DirectQueue().Flush();
      renderer.ResetWorld();
      play = std::make_unique<Play>();
      play->views.WatchBreach(run->Goal());
      camera = DefaultCamera(run->CameraTarget());
      hudModel = {};
      drawnViewProjection.reset();
    }
    Play& client = *play;

    swapChain.WaitForFrame();
    const double lapSeconds = std::min(timer.LapSeconds(), LONGEST_FRAME_SECONDS);
    seconds += lapSeconds;

    // The simulation's ticks: the session's bytes in, the world ticked, a copy of what it told back out.
    clock.SetPaused(run->Paused());
    const std::uint32_t ticks = clock.Advance(lapSeconds, run->Speed());
    for (std::uint32_t tick = 0; tick < ticks; ++tick)
    {
      if (!run->Tick(client.session, seconds))
      {
        break;
      }
      Simulation::World& world = run->World();
      // The chunks and the state the player sees (ADR-025): every change and everything, in a world without players.
      const std::vector<Protocol::ChunkSnapshot> published = world.Publish(PLAYER);
      const std::vector<std::vector<std::byte>> answers = world.TakeAnswers();
      client.session.Receive(world.State(PLAYER), published, answers);
      client.tally.Observe(client.session.State());
      client.presentation.Update(client.session, static_cast<float>(seconds));
    }
    // Answers given between ticks while the clock is paused, as the lab's question has them (M3-G1).
    if (run->Paused())
    {
      Simulation::World& world = run->World();
      const std::vector<std::vector<std::byte>> answers = world.TakeAnswers();
      if (!answers.empty())
      {
        client.session.Receive(world.State(PLAYER), {}, answers);
      }
    }

    // A left click on the HUD is the HUD's (M3-E2): the camera buttons turn and zoom, JUMP, or Space for the newest
    // alert with a place, looks at the alert's place, and the lab's question takes its answer. One on the world
    // selects the engine whose model's box it lands on, or nothing, where the scenario lets the player select, and Esc
    // deselects (M3-E3; M3-H3); one on a section gives it the integrity readout until the next click or Esc (M3-H1).
    const bool selectionAllowed = run->SelectionAllowed();
    const std::vector<Client::EngineBox> engineBoxes =
      selectionAllowed ? client.presentation.EngineBoxes() : std::vector<Client::EngineBox>{};
    bool answerOpen = false;
    bool answerShut = false;
    if (pointer.WasPressed(Engine::PointerInput::Button::Left))
    {
      const std::optional<Client::HudTarget> hit = hud.HitTest(pointer.XPixels(), pointer.YPixels());
      if (!hit.has_value() && !hud.Covers(pointer.XPixels(), pointer.YPixels()) && drawnViewProjection.has_value() && !client.fire.Active())
      {
        const Engine::PickRay ray = Client::WorldPicker::RayThrough(*drawnViewProjection, pointer.XPixels(), pointer.YPixels(),
                                                                    swapChain.WidthPixels(), swapChain.HeightPixels());
        const Client::WorldPick pick = client.picker.Pick(ray, engineBoxes);
        client.selected = pick.engine;
        Protocol::EntityId section = Protocol::NO_ENTITY;
        if (pick.engine == Protocol::NO_ENTITY && pick.voxel.has_value())
        {
          section = Client::Overlays::SectionAt(client.session.State(), {pick.voxel->cell[0], pick.voxel->cell[1], pick.voxel->cell[2]});
        }
        client.views.Select(section);
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
    if (selectedView == nullptr || !selectionAllowed)
    {
      client.selected = Protocol::NO_ENTITY;
      selectedView = nullptr;
    }
    // FIRE (M3-E4), from the world voxel under the pointer, off the HUD, while an engine is selected and the run
    // lets the player fire. Esc leaves the mode, and with none on deselects.
    const bool fireAllowed = run->FireAllowed();
    if (!fireAllowed && client.fire.Active())
    {
      client.fire.Cancel();
    }
    // The world voxel under the pointer, off the HUD: FIRE aims at it, and the section it is in has the readout.
    const bool pointerOnHud = hud.Covers(pointer.XPixels(), pointer.YPixels());
    std::optional<Protocol::VoxelPosition> underPointer;
    Protocol::EntityId hovered = Protocol::NO_ENTITY;
    Protocol::EntityId hoveredEngine = Protocol::NO_ENTITY; // the pointer answers with the hand and the outline (M3-H3)
    if (!pointerOnHud && drawnViewProjection.has_value())
    {
      const Engine::PickRay ray = Client::WorldPicker::RayThrough(*drawnViewProjection, pointer.XPixels(), pointer.YPixels(),
                                                                  swapChain.WidthPixels(), swapChain.HeightPixels());
      const Client::WorldPick pick = client.picker.Pick(ray, engineBoxes);
      hoveredEngine = pick.engine;
      if (pick.voxel.has_value())
      {
        const Protocol::VoxelPosition voxel{pick.voxel->cell[0], pick.voxel->cell[1], pick.voxel->cell[2]};
        underPointer = voxel;
        hovered = Client::Overlays::SectionAt(client.session.State(), voxel);
      }
    }
    client.views.Hover(hovered);
    const std::optional<Protocol::VoxelPosition> pointed = selectedView != nullptr ? underPointer : std::nullopt;
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
      client.views.Select(Protocol::NO_ENTITY);
      client.fire.Update({}, nullptr, client.session);
    }
    window.ShowPointer(client.fire.Active()                   ? Engine::PointerShape::Crosshair
                       : hoveredEngine != Protocol::NO_ENTITY ? Engine::PointerShape::Hand
                                                              : Engine::PointerShape::Arrow);
    if (keyboard.WasPressed(VK_F1))
    {
      integrityView = !integrityView;
    }
    if (keyboard.WasPressed(VK_F2))
    {
      passabilityView = !passabilityView;
    }
    const bool passabilityShown = passabilityView || run->PassabilityForced();
    if (keyboard.WasPressed(VK_SPACE) && !run->TakesSpace())
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
    // The run's own keys: the lab's answers, O and H (not S, which pans), and Space for the next item.
    const auto [quarterTurns, zoomNotches] = camera.TakeMoves();
    run->Frame({.nowSeconds = seconds,
                .openPressed = answerOpen || keyboard.WasPressed('O'),
                .shutPressed = answerShut || keyboard.WasPressed('H'),
                .nextPressed = keyboard.WasPressed(VK_SPACE),
                .quarterTurns = quarterTurns,
                .zoomNotches = zoomNotches,
                .integrityOn = integrityView,
                .passabilityOn = passabilityShown,
                .engineSelected = client.selected != Protocol::NO_ENTITY},
               client.session);
    client.presentation.Place(client.session, client.tally, clock.TickFraction(), placements, hiddenVoxels, debris,
                              static_cast<float>(seconds));

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
    // The HUD, built from the session (M3-E2), with the integrity readout (M3-H1), and the frame's figures under it,
    // outside the lab (M3-H4).
    client.views.Update(client.session, seconds, client.alerts);
    client.alerts.Expire(seconds);
    // The objective line: the run's, or the breach. The selection has its chip beside the resources, with FIRE's
    // key, or RELOADING in off text (M3-H3).
    const Client::HudObjective objective = run->Objective().value_or(Client::HudObjective{
      .title = std::string(Client::BREACH_OBJECTIVE_TITLE), .detail = std::string(Client::BREACH_OBJECTIVE_DETAIL), .detailOff = false});
    std::optional<Client::HudSelection> selection;
    if (selectedView != nullptr)
    {
      const bool ready = Client::FireMode::Ready(*selectedView);
      selection = Client::HudSelection{.name = std::string(Client::TREBUCHET_NAME),
                                       .detail = std::string(ready ? Client::FIRE_READY_DETAIL : Client::RELOADING_DETAIL),
                                       .detailOff = !ready};
    }
    // The banner: FIRE's while it is on, else the run's.
    const Client::HudMode mode = client.fire.Active() ? client.fire.Mode() : run->Mode().value_or(client.fire.Mode());
    hudModel = {.resources = run->Resources(client.session.State().tick, client.tally.Fired(PLAYER)),
                .objective = objective,
                .mode = mode,
                .alerts = client.alerts.Shown(),
                .selectedEngine = client.selected,
                .integrityView = integrityView,
                .passabilityView = passabilityShown,
                .readout = client.views.Readout(client.session, seconds, integrityView),
                .selection = selection};
    interfaceQuads.clear();
    if (run->ShowsFigures())
    {
      const std::uint32_t figuresScale = Client::TextLayout::PixelsPerFontPixel(Client::TextSize::Label, uiScale);
      const std::int32_t margin = Client::ScaledPixels(FIGURES_MARGIN_LOGICAL_PIXELS, uiScale);
      // In the bottom-right corner, since the readout is docked in the bottom-left (M3-H1).
      const std::string figures = std::format(Client::FIGURES, renderer.VoxelPassMilliseconds(), client.session.State().tick, run->Speed());
      Client::TextLayout::Lay(figures,
                              static_cast<std::int32_t>(swapChain.WidthPixels()) - margin -
                                static_cast<std::int32_t>(Client::TextLayout::Measure(figures, figuresScale)),
                              static_cast<std::int32_t>(swapChain.HeightPixels()) - margin -
                                static_cast<std::int32_t>(Client::TextLayout::CellPixels(figuresScale)),
                              figuresScale, figuresColor, interfaceQuads);
    }
    hud.Build(hudModel, swapChain.WidthPixels(), swapChain.HeightPixels(), uiScale, interfaceQuads);
    const std::optional<Client::AttackCursor> cursor = client.fire.Cursor(client.session);
    if (cursor.has_value() && !pointerOnHud)
    {
      hud.LayCursor(*cursor, pointer.XPixels(), pointer.YPixels(), uiScale, interfaceQuads);
    }
    // The world overlays (ADR-021): the views, the outline of the engine under the pointer and the selected one (M3-H3),
    // then FIRE's strike mark and the run's marks.
    std::vector<Client::OverlayLayer> overlays = client.views.Layers(client.session, seconds, integrityView);
    overlays.push_back(client.views.PassabilityLayer(client.session, passabilityShown));
    Client::OverlayLayer outlines{.alpha = Client::OUTLINE_ALPHA, .boxes = {}};
    for (const Client::EngineBox& box : engineBoxes)
    {
      if (box.engine == hoveredEngine || box.engine == client.selected)
      {
        std::ranges::copy(Client::WorldPicker::Outline(box, Client::OVERLAY_ACCENT_INDEX), std::back_inserter(outlines.boxes));
      }
    }
    overlays.push_back(std::move(outlines));
    overlays.push_back({.alpha = Client::STRIKE_MARK_ALPHA, .boxes = client.fire.StrikeMark(client.session)});
    overlays.push_back({.alpha = Client::STRIKE_MARK_ALPHA, .boxes = run->Marks(client.session)});
    Client::WorldRenderer::Frame frame{.commandList = commandList,
                                       .slot = slot,
                                       .ring = &ring,
                                       .snapshots = snapshots,
                                       .placements = placements,
                                       .hiddenVoxels = hiddenVoxels,
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
        run->Speed(), renderer.RecordsDrawn(), camera.ViewHeightMeters(), DirectX::XMConvertToDegrees(camera.AzimuthRadians()));
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
                .seed = SeedFrom(commandLine, participant.value_or("")),
                .siege = HasFlag(commandLine, L"--siege"),
                .mirror = HasFlag(commandLine, L"--mirror"),
                .watch = HasFlag(commandLine, L"--watch")});
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
