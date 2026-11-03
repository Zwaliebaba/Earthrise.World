#pragma once

#include <DirectXMath.h>

#include "ChunkSnapshot.h"
#include "EntityId.h"
#include "HudAlerts.h"
#include "HudModel.h"
#include "OverlayBox.h"
#include "Query.h"
#include "Session.h"
#include "StateUpdate.h"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Client
{

/// A row of a section's integrity reading: its text, a KEY and its value, and its colour.
struct IntegrityRow
{
  std::string text;
  std::uint32_t srgb;
};

/// A navigation tile, 32 x 32 cells: one chunk column (ADR-015).
using Tile = std::pair<std::int32_t, std::int32_t>;

/// The readability views, built from what the session was told (ADR-021, decisions 3 and 4): the collapse preview and
/// its alerts (M3-F1), the integrity view (M3-F2, and its readout, M3-H1) and the passability view (M3-F3). The views
/// are boxes for the overlay pipeline and the readout for the HUD; the alerts are the HUD's, and they are raised whether
/// or not a view is on.
class Overlays
{
public:
  /// How long a reading's row that changed shows in accent.
  static constexpr double FLASH_SECONDS = 2.0;
  /// The section the readout shows by default: the one M3's banners, questions and objective name.
  static constexpr std::string_view TARGET_NAME = "W2";

  /// Reads the session's state for the frame at _nowSeconds: raises each collapse's alert, W2 WILL FALL IN n S while it
  /// is in warning, counting down, and W2 HAS FALLEN once it falls, both urgent and at the piece; and notes each row of
  /// each reading that changed, which the readout flashes for FLASH_SECONDS. A section's first reading flashes nothing.
  void Update(const Session& _session, double _nowSeconds, HudAlerts& _alerts);

  /// While _integrityView is on, asks each structure's integrity: the first time, when the view is turned on, when a
  /// chunk under or beside it changes, and when a collapse of it starts, falls or ends (ADR-021, decision 3).
  void Ask(Session& _session, std::span<const Protocol::ChunkSnapshot> _snapshots, bool _integrityView);

  /// The overlay layers for the frame at _seconds. While _integrityView is on, each collapse in warning has its piece
  /// in bad, pulsing over a second, and its strip's cells in bad on the ground under them, and each section's weak
  /// point has its box in accent.
  [[nodiscard]] std::vector<OverlayLayer> Layers(const Session& _session, double _seconds, bool _integrityView) const;

  /// The section under the pointer this frame, or NO_ENTITY: the readout shows it while the pointer is over it.
  void Hover(Protocol::EntityId _section) noexcept;
  /// A click in the world: on _section, which the readout then keeps, or on anything else with NO_ENTITY, which lets
  /// it go back to the target. Esc lets it go too.
  void Select(Protocol::EntityId _section) noexcept;
  /// Layout A's focus (M4-I6; input 23, as the owner amended it): the structure selected, then the one under the
  /// pointer, and no target by default, so the selected one always reads in full. The lab keeps M3-H1's.
  void SelectedFirst(bool _selectedFirst) noexcept;
  /// The section the readout shows (M3-H1): the one under the pointer, else the one clicked, else TARGET_NAME's, each
  /// while the state holds it; NO_ENTITY when there is none. In layout A, the one clicked, else the one under the pointer.
  [[nodiscard]] Protocol::EntityId Focused(const Session& _session) const;
  /// While _integrityView is on, the readout of the focused section at _nowSeconds, once it has a reading: its name, and
  /// its rows split into KEY and value, HOLE's and GAP's with a bar of their width over their fall width when one falls,
  /// and those that changed within FLASH_SECONDS flashing; and while its repair is locked, REPAIR Locked · n s (M4-I6),
  /// which counts down and does not flash.
  [[nodiscard]] std::optional<HudReadout> Readout(const Session& _session, double _nowSeconds, bool _integrityView) const;
  /// The section whose box holds _voxels, or NO_ENTITY.
  [[nodiscard]] static Protocol::EntityId SectionAt(const Protocol::StateUpdate& _state, const Protocol::VoxelPosition& _voxels);

  /// Watches _goalVoxels, the courtyard's middle: Update raises BREACH OPEN AT W2 once, the first time its cell reads
  /// reached (M3-F3).
  void WatchBreach(const Protocol::VoxelPosition& _goalVoxels);

  /// Asks the reach of foot from _player's squad, from its first figure's cell (ADR-021, decision 3): while
  /// _passabilityView is on, for each of _visible, when it comes on screen, when the view is turned on, when the start
  /// moves and when a chunk in or beside it changes; and for the watched goal's tile the same way, whether or not the
  /// view is on, for the alert.
  void AskReach(Session& _session, std::span<const Protocol::ChunkSnapshot> _snapshots, std::span<const Tile> _visible,
                std::uint8_t _player, bool _passabilityView);

  /// The passability view while _passabilityView is on: for each visible tile's reading, each reached cell in good and
  /// each cell foot could stand in but not reach in dim, as a box in the cell a unit stands in, at its surface.
  [[nodiscard]] OverlayLayer PassabilityLayer(const Session& _session, bool _passabilityView) const;

  /// The tiles the view shows: those its footprint on the ground at _groundMeters overlaps, from a target _widthPixels by
  /// _heightPixels drawn with _viewProjection, at most the 36 nearest the footprint's middle. The default framing shows
  /// 35.
  [[nodiscard]] static std::vector<Tile> VisibleTiles(const DirectX::XMFLOAT4X4& _viewProjection, std::uint32_t _widthPixels,
                                                      std::uint32_t _heightPixels, float _groundMeters);

  /// A collapse's countdown, in whole seconds rounded up, named for its structure, or A WALL when it has none.
  [[nodiscard]] static std::string WillFallText(const Protocol::StateUpdate& _state, const Protocol::CollapseView& _collapse);
  [[nodiscard]] static std::string HasFallenText(const Protocol::StateUpdate& _state, const Protocol::CollapseView& _collapse);

  /// The view height above which a structure's line in the world is its name alone (input 23).
  static constexpr float NAMES_ALONE_METERS = 150.0F;

  /// The structures' lines in the world while _integrityView is on (input 23, as the owner amended it for M4-I6), for a
  /// target _widthPixels by _heightPixels drawn with _viewProjection: for each structure whose anchor, a metre above its
  /// box's middle, is in front of the camera, its name, then FALLS while a collapse of it runs, OPEN yes once a route
  /// runs through it, or else its weak reading against its fall, and then its lock while its repair is locked; its name
  /// alone above NAMES_ALONE_METERS of _viewHeightMeters, but for _selected. The selected first, then nearest the
  /// screen's middle.
  [[nodiscard]] static std::vector<HudWorldLabel> WorldLabels(const Session& _session, const DirectX::XMFLOAT4X4& _viewProjection,
                                                              std::uint32_t _widthPixels, std::uint32_t _heightPixels,
                                                              float _viewHeightMeters, Protocol::EntityId _selected, bool _integrityView);

  /// The control's health bars (ADR-028, decision 6; M4-J3): one over each wall section with hit points, its pool's
  /// share, kept from 0 to 1, at the anchor its line in the world has. None in the voxel version, whose structures carry
  /// no hit points.
  [[nodiscard]] static std::vector<HudHealthBar> HealthBars(const Session& _session, const DirectX::XMFLOAT4X4& _viewProjection,
                                                            std::uint32_t _widthPixels, std::uint32_t _heightPixels);

  /// A wall section's weak reading against its fall (input 22): HOLE w/f M or GAP r/f M, whichever is nearer its fall,
  /// HOLE on a tie, while it has a width and a fall width within the section; nothing otherwise.
  [[nodiscard]] static std::optional<std::string> WeakValue(const Protocol::IntegrityReading& _reading);
  /// Hud.md §5's REPAIR value: Ready, or Locked · n s, the lock in whole seconds rounded up.
  [[nodiscard]] static std::string RepairValue(std::uint32_t _lockTicks);

  /// A reading's rows, in the paper test's words: HOLE, OPENING, GAP and GROUND, and FALLS in bad while it falls.
  /// GROUND is dim, unless the section can be undermined and a gap lies under it, when it is bad (M3-H1).
  [[nodiscard]] static std::vector<IntegrityRow> IntegrityRows(const Protocol::IntegrityReading& _reading);

private:
  /// A section's rows as last read, and when each last changed.
  struct SeenRows
  {
    std::vector<std::string> texts;
    std::vector<double> changedSeconds;
  };

  Protocol::EntityId m_hovered = Protocol::NO_ENTITY;
  Protocol::EntityId m_selected = Protocol::NO_ENTITY;
  bool m_selectedFirst = false;
  std::map<Protocol::EntityId, SeenRows> m_seenRows;
  bool m_integrityAsked = false;                                                      // the view was on at the last Ask
  std::set<Protocol::EntityId> m_asked;                                               // structures asked about since
  std::map<Protocol::EntityId, std::pair<Protocol::EntityId, bool>> m_collapsePhases; // collapse: its structure, in warning
  bool m_reachAsked = false;                                                          // the view was on at the last AskReach
  std::optional<Protocol::VoxelPosition> m_reachStart;                                // the start the last asks were from
  std::set<Tile> m_reachTiles;                                                        // tiles asked about from it
  std::vector<Tile> m_visible;                                                        // as the last AskReach had them
  std::optional<Protocol::VoxelPosition> m_breachGoal;
  bool m_breachAnnounced = false;
};

} // namespace Client
