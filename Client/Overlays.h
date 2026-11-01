#pragma once

#include <DirectXMath.h>

#include "ChunkSnapshot.h"
#include "EntityId.h"
#include "HudAlerts.h"
#include "OverlayBox.h"
#include "Query.h"
#include "ScreenQuad.h"
#include "Session.h"
#include "StateUpdate.h"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Client
{

/// A row of a section's integrity label: its text, a KEY and its value, and its colour.
struct IntegrityRow
{
  std::string text;
  std::uint32_t srgb;
};

/// Where a label is drawn, and what it shows, for the test that it stays on screen.
struct PlacedLabel
{
  Protocol::EntityId structure;
  std::int32_t xPixels;
  std::int32_t yPixels;
  std::int32_t widthPixels;
  std::int32_t heightPixels;
  bool nameOnly; // its rows would have covered a nearer label
};

/// A navigation tile, 32 x 32 cells: one chunk column (ADR-015).
using Tile = std::pair<std::int32_t, std::int32_t>;

/// The readability views, built from what the session was told (ADR-021, decisions 3 and 4): the collapse preview and
/// its alerts (M3-F1), the integrity view (M3-F2) and the passability view (M3-F3). The views are boxes for the overlay pipeline and labels for the
/// interface pass; the alerts are the HUD's, and they are raised whether or not a view is on.
class Overlays
{
public:
  /// Reads the session's state for the frame at _nowSeconds: raises each collapse's alert, W2 WILL FALL IN n S while it
  /// is in warning, counting down, and W2 HAS FALLEN once it falls, both urgent and at the piece.
  void Update(const Session& _session, double _nowSeconds, HudAlerts& _alerts);

  /// While _integrityView is on, asks each structure's integrity: the first time, when the view is turned on, when a
  /// chunk under or beside it changes, and when a collapse of it starts, falls or ends (ADR-021, decision 3).
  void Ask(Session& _session, std::span<const Protocol::ChunkSnapshot> _snapshots, bool _integrityView);

  /// The overlay layers for the frame at _seconds. While _integrityView is on, each collapse in warning has its piece
  /// in bad, pulsing over a second, and its strip's cells in bad on the ground under them, and each section's weak
  /// point has its box in accent.
  [[nodiscard]] std::vector<OverlayLayer> Layers(const Session& _session, double _seconds, bool _integrityView) const;

  /// While _integrityView is on, lays each section's label above its middle, on a target _widthPixels by _heightPixels
  /// whose top _topPixels the top bar covers, at _uiScale: its name at the names' size, then its rows at the labels'.
  /// Only a section whose anchor is on screen has one, kept inside the screen below the top bar. Labels are placed
  /// nearest the camera first, and one whose rows would cover a nearer one shows its name alone, or nothing when even
  /// that would. Returns where each went.
  std::vector<PlacedLabel> LayLabels(const Session& _session, const DirectX::XMFLOAT4X4& _viewProjection, std::uint32_t _widthPixels,
                                     std::uint32_t _heightPixels, std::int32_t _topPixels, float _uiScale, bool _integrityView,
                                     std::vector<ScreenQuad>& _quads) const;

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

  /// A reading's rows, in the paper test's words: HOLE, OPENING, GAP and GROUND, and FALLS in bad while it falls.
  [[nodiscard]] static std::vector<IntegrityRow> IntegrityRows(const Protocol::IntegrityReading& _reading);

private:
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
