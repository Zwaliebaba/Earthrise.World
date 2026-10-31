"""The breach-prediction paper test: one wall section, sixteen damage states, two readouts, one answer key.

Design review F-04 and F-47, cheapest test 3 (Design/Review/GameDescription-2026-10-31.md). The write-up,
with every value this script proposes and the reason for it, is Design/Review/PaperTests/BreachPrediction.md.

The script builds one standard wall section (template T3) on ground, between the ends of its two neighbouring
sections. It applies each damage state and resolves it with a stand-in for the collapse resolver: the support
rule of ADR-004, decision 2, applied until nothing more falls, with rubble that slumps to 45 degrees. It then
asks whether foot soldiers have a route through the section. That answer is the key; nothing here sets it by
hand, and the script refuses to write anything unless the sixteen states split eight open and eight shut.

For each state it also computes the two readouts shown to participants: the thinnest remaining thickness,
which is the integrity view as first written, and the three readings of the integrity view as revised
(GameDescription.md, *Readability*).

Usage:
  python Tools/PaperTests/BreachPrediction.py           write BreachPrediction.json and the page's items
  python Tools/PaperTests/BreachPrediction.py --check   exit 1 if either file is not what the script writes

Standard library only.
"""

import argparse
import collections
import hashlib
import itertools
import json
import math
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
PALETTE_PATH = ROOT / "GameData" / "Server" / "Palette.json"
WALL_TEMPLATE_PATH = ROOT / "GameData" / "Server" / "Templates" / "earthrise-t3-stone-wall-standard-1m.json"
JSON_PATH = HERE / "BreachPrediction.json"
HTML_PATH = HERE / "BreachPrediction.html"
HTML_BEGIN = '<script id="breach-items" type="application/json">'
HTML_END = "</script>"

# ── Materials, by palette index (GameData/Server/Palette.json) ────────────────────────────────

AIR = 0
EARTH = 2
ROCK = 3
BEDROCK = 5
MASONRY = 6
STONE_RUBBLE = 8
EARTH_RUBBLE = 26
ROCK_RUBBLE = 28

MATERIAL_CODES = {
  AIR: ".", EARTH: "E", ROCK: "R", BEDROCK: "B", MASONRY: "S", STONE_RUBBLE: "U", EARTH_RUBBLE: "D", ROCK_RUBBLE: "K",
}
LOOSE = {EARTH, STONE_RUBBLE, EARTH_RUBBLE, ROCK_RUBBLE}
STRUCTURE = {MASONRY}
FOOTING = {EARTH, ROCK, BEDROCK, STONE_RUBBLE, EARTH_RUBBLE, ROCK_RUBBLE}
RUBBLE_OF = {MASONRY: STONE_RUBBLE, EARTH: EARTH_RUBBLE, ROCK: ROCK_RUBBLE}

# ── Proposed values (each has its reason in the write-up) ─────────────────────────────────────

VOXEL_METERS = 1
STONE_SPAN_METERS = 2
ROCK_SPAN_METERS = 2
CLIMB_METERS = 1
UNIT_HEIGHT_METERS = 2
ANCHOR_DEPTH_METERS = 8
LOOSE_SET_HEIGHT_METERS = 1
ITEM_COUNT = 16

# ── The model ─────────────────────────────────────────────────────────────────────────────────

# T3, copied so that the instrument does not move if the template is redrawn. layers[row][templateZ];
# templateZ 0 is the back (inside) and 1 the front (outside), because the template's +z is its front.
T3_LAYERS = [
  ["ffffffff", "ffffffff"],
  ["ffffffff", "ffffffff"],
  ["ffffffff", "ffffffff"],
  ["ffffffff", "ffffffff"],
  ["ffffffff", "ffffffff"],
  ["ffffffff", "f.f.f.f."],
  ["f.f.f.f.", "........"],
]
SECTION_WIDTH = 8
WALL_THICKNESS = 2
WALL_HEIGHT = len(T3_LAYERS)
BODY_ROWS = 5  # the rows where T3 is solid through its whole thickness; above them is the crenellation
APRON = 7  # ground on each side of the wall, in voxels
GROUND = [BEDROCK, ROCK, EARTH, EARTH]  # y = 0 up to the surface
BASE_Y = len(GROUND)  # the wall's lowest row, and the cell a unit stands in on flat ground
X_SIZE = SECTION_WIDTH + 2  # one column of each neighbouring section
Y_SIZE = BASE_Y + WALL_HEIGHT + 5
Z_SIZE = 2 * APRON + WALL_THICKNESS
OUTER_Z = APRON  # the wall's outer layer, facing the attacker
INNER_Z = APRON + 1
FRONT_ROWS = BASE_Y + WALL_HEIGHT  # rows drawn in the front view: the ground and the wall
SIDES = ((0, -1), (0, 1), (-1, 0), (1, 0))  # (dx, dz) in a fixed order, so every run gives the same result


def Spans(_stoneSpan):
  return {MASONRY: _stoneSpan, ROCK: ROCK_SPAN_METERS}


def FallWidth(_stoneSpan):
  """The narrowest hole, bridged at both ends, whose middle is further than the span from either end."""
  return 2 * _stoneSpan + 1


def TemplateColumn(_column, _layer, _row):
  """True if T3 has stone at section column _column (0-7), layer 'outer' or 'inner', row _row."""
  return T3_LAYERS[_row][1 if _layer == "outer" else 0][_column] != "."


def LayerZ(_layer):
  return OUTER_Z if _layer == "outer" else INNER_Z


def NewGrid():
  grid = [[[AIR] * Z_SIZE for _ in range(Y_SIZE)] for _ in range(X_SIZE)]
  for x in range(X_SIZE):
    for y, material in enumerate(GROUND):
      for z in range(Z_SIZE):
        grid[x][y][z] = material
  for x in range(X_SIZE):
    # x 0 is the left neighbour's last column, x 9 the right neighbour's first; both are intact.
    column = 7 if x == 0 else 0 if x == X_SIZE - 1 else x - 1
    for row in range(WALL_HEIGHT):
      for layer in ("outer", "inner"):
        if TemplateColumn(column, layer, row):
          grid[x][BASE_Y + row][LayerZ(layer)] = MASONRY
  return grid


def CopyGrid(_grid):
  return [[list(row) for row in plane] for plane in _grid]


def InBounds(_x, _y, _z):
  return 0 <= _x < X_SIZE and 0 <= _y < Y_SIZE and 0 <= _z < Z_SIZE


def Solid(_grid, _x, _y, _z):
  return InBounds(_x, _y, _z) and _grid[_x][_y][_z] != AIR


# ── Damage ────────────────────────────────────────────────────────────────────────────────────


def Faces(_faces):
  return {"both": ("outer", "inner"), "outer": ("outer",), "inner": ("inner",)}[_faces]


def ApplyDamage(_grid, _damage):
  """_damage is a list of operations in section terms: columns 1-8 from the left as seen from outside,
  rows 0-6 counted up from the ground (row r spans r to r + 1 m)."""
  for operation in _damage:
    kind = operation["kind"]
    columns = range(operation["columns"][0], operation["columns"][1] + 1)
    if kind == "hole":
      for column in columns:
        for row in range(operation["rows"][0], operation["rows"][1] + 1):
          for layer in Faces(operation["faces"]):
            _grid[column][BASE_Y + row][LayerZ(layer)] = AIR
    elif kind == "undercut":
      # The ground under the foundation is gone, 1 m deep, under the given faces, and the sappers' trench
      # that took it runs along the outer foot, 2 m wide and 1 m deep, so every slope in it is 45 degrees.
      for column in columns:
        for layer in Faces(operation["faces"]):
          _grid[column][BASE_Y - 1][LayerZ(layer)] = AIR
        for z in (OUTER_Z - 2, OUTER_Z - 1):
          _grid[column][BASE_Y - 1][z] = AIR
    else:
      raise ValueError(f"unknown damage {kind}")


# ── The resolver's stand-in ───────────────────────────────────────────────────────────────────


def IsAnchor(_grid, _x, _y, _z):
  """ADR-004: a voxel of bedrock, or any voxel below the anchor depth. Ordinary terrain is not an anchor."""
  voxelTopDepth = BASE_Y - (_y + 1)
  return _grid[_x][_y][_z] == BEDROCK or voxelTopDepth >= ANCHOR_DEPTH_METERS


def SupportDistances(_grid):
  """ADR-004, decision 2: the fewest horizontal steps on any path from a voxel to an anchor that moves only
  down or sideways through solid voxels. Searched backwards from the anchors: up costs nothing, sideways one."""
  distances = {}
  queue = collections.deque()
  for x, y, z in itertools.product(range(X_SIZE), range(Y_SIZE), range(Z_SIZE)):
    if _grid[x][y][z] != AIR and IsAnchor(_grid, x, y, z):
      distances[(x, y, z)] = 0
      queue.append((0, (x, y, z)))
  while queue:
    distance, (x, y, z) = queue.popleft()
    if distance > distances[(x, y, z)]:
      continue
    above = (x, y + 1, z)
    if Solid(_grid, *above) and distances.get(above, math.inf) > distance:
      distances[above] = distance
      queue.appendleft((distance, above))
    for dx, dz in SIDES:
      side = (x + dx, y, z + dz)
      if Solid(_grid, *side) and distances.get(side, math.inf) > distance + 1:
        distances[side] = distance + 1
        queue.append((distance + 1, side))
  return distances


def Unsupported(_grid, _distances, _spans):
  """Voxels of a material judged by span whose support distance is more than its span, or that reach no
  anchor at all. Earth and rubble carry support but are not judged by span; they slump instead."""
  found = set()
  for x, y, z in itertools.product(range(X_SIZE), range(Y_SIZE), range(Z_SIZE)):
    material = _grid[x][y][z]
    if material in _spans and _distances.get((x, y, z), math.inf) > _spans[material]:
      found.add((x, y, z))
  return found


def Pieces(_voxels):
  """Connected pieces (face neighbours), ordered by their lowest voxel, which stands in for entity-id order."""
  remaining = set(_voxels)
  pieces = []
  while remaining:
    start = min(remaining, key=lambda _v: (_v[1], _v[0], _v[2]))
    piece, frontier = {start}, [start]
    remaining.discard(start)
    while frontier:
      x, y, z = frontier.pop()
      for neighbor in ((x + 1, y, z), (x - 1, y, z), (x, y + 1, z), (x, y - 1, z), (x, y, z + 1), (x, y, z - 1)):
        if neighbor in remaining:
          remaining.discard(neighbor)
          piece.add(neighbor)
          frontier.append(neighbor)
    pieces.append(sorted(piece, key=lambda _v: (_v[1], _v[0], _v[2])))
  return sorted(pieces, key=lambda _p: (_p[0][1], _p[0][0], _p[0][2]))


def Drop(_grid, _x, _y, _z, _material):
  """Places a loose voxel at (_x, _y, _z), or the first free cell above it, and lets it fall down its column."""
  y = _y
  while _grid[_x][y][_z] != AIR:
    y += 1
  while y > 0 and _grid[_x][y - 1][_z] == AIR:
    y -= 1
  _grid[_x][y][_z] = _material


def DropFromSky(_grid, _x, _z, _material):
  y = Y_SIZE - 1
  while y > 0 and _grid[_x][y - 1][_z] == AIR:
    y -= 1
  _grid[_x][y][_z] = _material


def Collapse(_grid, _pieces, _fall, _fallDamage):
  """Every piece found unsupported on this tick falls together. _fall is 'down' (laid as rubble in the
  columns it fell from), or 'outward' or 'inward' (laid along the line of the fall, as far out as it is tall).
  With _fallDamage, a piece falling down knocks out the first voxel of structure under each of its columns."""
  materials = {voxel: _grid[voxel[0]][voxel[1]][voxel[2]] for piece in _pieces for voxel in piece}
  for x, y, z in materials:
    _grid[x][y][z] = AIR
  falling = []
  for piece in _pieces:
    if _fall == "down" and _fallDamage:
      for x, z in sorted({(voxel[0], voxel[2]) for voxel in piece}):
        y = min(voxel[1] for voxel in piece if voxel[0] == x and voxel[2] == z) - 1
        while y > 0 and _grid[x][y][z] == AIR:
          y -= 1
        if _grid[x][y][z] in STRUCTURE:
          falling.append(((x, y, z), _grid[x][y][z], piece[0][1]))
          _grid[x][y][z] = AIR
    falling.extend((voxel, materials[voxel], piece[0][1]) for voxel in piece)
  for (x, y, z), material, baseY in sorted(falling, key=lambda _f: (_f[0][1], _f[0][0], _f[0][2])):
    rubble = RUBBLE_OF.get(material, material)
    if _fall == "down":
      Drop(_grid, x, y, z, rubble)
    else:
      faceZ, step = (OUTER_Z, -1) if _fall == "outward" else (INNER_Z, 1)
      targetZ = min(max(faceZ + step * (1 + y - baseY), 0), Z_SIZE - 1)
      DropFromSky(_grid, x, targetZ, rubble)


def SettleOne(_grid, _x, _y, _z):
  """One move of one loose voxel, or none. A loose voxel falls into an empty cell below it; slides down to a
  side whose top is two or more below it, which is the 45-degree slump of GameDescription.md; and never rests
  on top of a standing structure. Returns True if it moved."""
  if _y == 0:
    return False
  below = _grid[_x][_y - 1][_z]
  if below == AIR:
    _grid[_x][_y - 1][_z], _grid[_x][_y][_z] = _grid[_x][_y][_z], AIR
    return True
  onStructure = below in STRUCTURE
  best = None
  for order, (dx, dz) in enumerate(SIDES):
    x, z = _x + dx, _z + dz
    if not InBounds(x, _y, z) or _grid[x][_y][z] != AIR:
      continue
    landing = _y
    while landing > 0 and _grid[x][landing - 1][z] == AIR:
      landing -= 1
    if landing < _y or (onStructure and _grid[x][_y - 1][z] not in STRUCTURE):
      candidate = (landing, order, x, z)
      best = candidate if best is None or candidate < best else best
  if best is None:
    return False
  landing, _, x, z = best
  _grid[x][landing][z], _grid[_x][_y][_z] = _grid[_x][_y][_z], AIR
  return True


def Settle(_grid):
  """Moves loose voxels, sweeping bottom to top in a fixed order, until a whole sweep moves nothing."""
  moves = 0
  while True:
    moved = 0
    for y, x, z in itertools.product(range(1, Y_SIZE), range(X_SIZE), range(Z_SIZE)):
      if _grid[x][y][z] in LOOSE and SettleOne(_grid, x, y, z):
        moved += 1
    moves += moved
    if moved == 0:
      return moves
    if moves > 100000:
      raise RuntimeError("loose material did not settle")


def Resolve(_grid, _stoneSpan=STONE_SPAN_METERS, _fall="down", _fallDamage=False):
  """Settles loose material, then collapses unsupported pieces tick by tick until nothing more falls."""
  grid = CopyGrid(_grid)
  spans = Spans(_stoneSpan)
  history = {"settledBeforeSupport": Settle(grid), "ticks": []}
  firstUnsupported = None
  while True:
    unsupported = Unsupported(grid, SupportDistances(grid), spans)
    if firstUnsupported is None:
      firstUnsupported = unsupported
    if not unsupported:
      break
    pieces = Pieces(unsupported)
    Collapse(grid, pieces, _fall, _fallDamage)
    history["ticks"].append({"pieces": len(pieces), "voxels": len(unsupported), "settleMoves": Settle(grid)})
    if len(history["ticks"]) > 50:
      raise RuntimeError("the collapse did not end")
  return grid, history, firstUnsupported


# ── Routes ────────────────────────────────────────────────────────────────────────────────────


def Surface(_grid, _x, _z):
  """The column's one walkable cell: the highest empty cell with footing below it (ground or rubble, never
  structure) and room above it for a unit 2 m tall. None if the column has none."""
  for y in range(Y_SIZE - UNIT_HEIGHT_METERS, 0, -1):
    if _grid[_x][y - 1][_z] in FOOTING and all(_grid[_x][y + k][_z] == AIR for k in range(UNIT_HEIGHT_METERS)):
      return y
  return None


def Route(_grid):
  """A route for foot soldiers from the outer edge of the ground to the inner edge, crossing the wall line
  inside this section's 8 m. Steps go to a face neighbour at most 1 m up or down, with 2 m of room above the
  higher of the two cells in both columns. Returns the route's columns, or None."""
  columns = range(1, SECTION_WIDTH + 1)
  surfaces = {(x, z): Surface(_grid, x, z) for x in columns for z in range(Z_SIZE)}
  start = [(x, 0) for x in columns if surfaces[(x, 0)] is not None]
  previous = {cell: None for cell in start}
  queue = collections.deque(start)
  while queue:
    x, z = queue.popleft()
    if z == Z_SIZE - 1:
      path = [(x, z)]
      while previous[path[-1]] is not None:
        path.append(previous[path[-1]])
      return [[cellX, cellZ] for cellX, cellZ in reversed(path)]
    here = surfaces[(x, z)]
    for dx, dz in SIDES:
      cell = (x + dx, z + dz)
      if cell not in surfaces or cell in previous or surfaces[cell] is None:
        continue
      there = surfaces[cell]
      if abs(here - there) > CLIMB_METERS:
        continue
      top = max(here, there)
      if all(_grid[cx][top + k][cz] == AIR for cx, cz in ((x, z), cell) for k in range(UNIT_HEIGHT_METERS)):
        previous[cell] = (x, z)
        queue.append(cell)
  return None


# ── Readouts ──────────────────────────────────────────────────────────────────────────────────


def Thickness(_grid, _column, _row):
  return sum(1 for z in (OUTER_Z, INNER_Z) if _grid[_column][BASE_Y + _row][z] != AIR)


def Regions(_cells):
  """4-connected regions of (column, row) cells, ordered by their lowest, then leftmost, cell."""
  remaining, regions = set(_cells), []
  while remaining:
    start = min(remaining, key=lambda _c: (_c[1], _c[0]))
    region, frontier = {start}, [start]
    remaining.discard(start)
    while frontier:
      column, row = frontier.pop()
      for neighbor in ((column + 1, row), (column - 1, row), (column, row + 1), (column, row - 1)):
        if neighbor in remaining:
          remaining.discard(neighbor)
          region.add(neighbor)
          frontier.append(neighbor)
    regions.append(sorted(region, key=lambda _c: (_c[1], _c[0])))
  return regions


def Span(_values):
  low, high = min(_values), max(_values)
  return f"{low}" if low == high else f"{low}–{high}"


def Place(_region):
  """Where a region of face cells is, in the words the page uses: columns 1-8 from the left, metres up."""
  columns = [cell[0] for cell in _region]
  rows = [cell[1] for cell in _region]
  return f"columns {Span(columns)}, {min(rows)}–{max(rows) + 1} m up"


def LongestRun(_columns):
  best, run, last = 0, 0, None
  for column in sorted(set(_columns)):
    run = run + 1 if last is not None and column == last + 1 else 1
    best, last = max(best, run), column
  return best


def ThicknessReadout(_grid):
  """The integrity view as first written: the thinnest remaining cross-section. Read as the remaining thickness
  through the wall at each point of its solid body (rows 0-4; the crenellation is thin by design), where a hole
  that goes through counts as 0 m."""
  cells = {(column, row): Thickness(_grid, column, row)
           for column in range(1, SECTION_WIDTH + 1) for row in range(BODY_ROWS)}
  thinnest = min(cells.values())
  weak = [cell for cell, value in cells.items() if value == thinnest] if thinnest < WALL_THICKNESS else []
  regions = Regions([(column, row) for column, row in weak])
  if thinnest == 0:
    value = "0 m: a hole goes through"
  elif thinnest < WALL_THICKNESS:
    value = f"{thinnest} m of {WALL_THICKNESS} m"
  else:
    value = f"{WALL_THICKNESS} m of {WALL_THICKNESS} m, full thickness"
  if not regions:
    place = "none"
  elif len(regions) <= 2:
    place = " and ".join(Place(region) for region in regions)
  else:
    place = f"{len(regions)} places"
  rows = [
    {"key": "THINNEST", "value": value, "warn": thinnest == 0},
    {"key": "WEAK POINT", "value": place, "warn": False},
    {"key": "STANDS ON", "value": StandsOn(_grid), "warn": False},
  ]
  return {"rows": rows, "thinnestMeters": thinnest, "weakCells": [[column, row] for column, row in weak]}


def StandsOn(_grid):
  materials = collections.Counter(_grid[x][BASE_Y - 1][z]
                                  for x in range(1, SECTION_WIDTH + 1) for z in (OUTER_Z, INNER_Z))
  materials.pop(AIR, None)
  names = {EARTH: "earth", ROCK: "rock", BEDROCK: "bedrock"}
  main = names.get(materials.most_common(1)[0][0], "ground") if materials else "nothing"
  return f"{main}, 2 m deep over rock" if main == "earth" else main


def Openings(_grid):
  """Openings through the wall: 4-connected regions of face cells, rows 0-5, where both layers are gone.
  Row 6 is left out because the template's own crenels go through there."""
  cells = [(column, row) for column in range(1, SECTION_WIDTH + 1) for row in range(WALL_HEIGHT - 1)
           if Thickness(_grid, column, row) == 0]
  openings = []
  for region in Regions(cells):
    floorRow = min(row for _, row in region)
    floorColumns = sorted(column for column, row in region if row == floorRow)
    clearMeters, openAbove = 0, False
    for column in floorColumns:
      height = 0
      while BASE_Y + floorRow + height < Y_SIZE and Thickness(_grid, column, floorRow + height) == 0:
        height += 1
        if floorRow + height >= WALL_HEIGHT:
          openAbove = True
          break
      clearMeters = max(clearMeters, height)
    widthMeters = max(LongestRun([column for column, row in region if row == r]) for r in {row for _, row in region})
    openings.append({"cells": region, "floorRow": floorRow, "floorColumns": floorColumns, "clearMeters": clearMeters,
                     "openAbove": openAbove, "widthMeters": widthMeters,
                     "heightMeters": max(row for _, row in region) - floorRow + 1})
  return openings


def StoneAbove(_grid, _columns, _aboveRow):
  """Structure voxels of the section in _columns, above row _aboveRow (rows counted from the ground)."""
  return {(column, y, z) for column in _columns for y in range(BASE_Y + _aboveRow + 1, Y_SIZE)
          for z in (OUTER_Z, INNER_Z) if _grid[column][y][z] in STRUCTURE}


def SupportReadout(_grid, _firstUnsupported, _routeNow, _stoneSpan=STONE_SPAN_METERS):
  """The integrity view as revised: the widest hole against the width at which the stone above falls, the
  lowest opening against what foot soldiers can climb, and any gap under the foundation. The numbers are
  widths; the verdict on each, FALLS or holds, is the resolver's, as ADR-004, decision 5, requires."""
  fallWidth = FallWidth(_stoneSpan)
  openings = Openings(_grid)
  readings = {}

  hole = max(openings, key=lambda _o: (_o["widthMeters"], -_o["floorRow"]), default=None)
  if hole is None:
    readings["hole"] = {"widthMeters": 0, "falls": False, "byWidth": False, "cells": []}
    holeValue = f"none · the stone above a hole falls at {fallWidth} m"
  else:
    topRow = max(row for _, row in hole["cells"])
    falls = bool(StoneAbove(_grid, sorted({column for column, _ in hole["cells"]}), topRow) & _firstUnsupported)
    readings["hole"] = {"widthMeters": hole["widthMeters"], "falls": falls, "byWidth": hole["widthMeters"] >= fallWidth,
                        "cells": hole["cells"]}
    holeValue = (f"{hole['widthMeters']} m wide, {hole['heightMeters']} m high"
                 f" · the stone above it falls at {fallWidth} m" + (" · FALLS" if falls else " · holds"))

  lowest = min(openings, key=lambda _o: (_o["floorRow"], -_o["widthMeters"]), default=None)
  if lowest is None:
    readings["opening"] = {"floorMeters": None, "onStone": False, "clearMeters": 0, "routeNow": _routeNow, "cells": []}
    openingValue = "none"
  else:
    onStone = lowest["floorRow"] > 0
    clear = "open above" if lowest["openAbove"] else f"{lowest['clearMeters']} m clear"
    where = f"floor {lowest['floorRow']} m up, on stone" if onStone else "floor at ground"
    if _routeNow:
      verdict = "a route now"
    elif onStone:
      verdict = "no footing: soldiers never stand on stone"
    else:
      verdict = f"too low: soldiers need {UNIT_HEIGHT_METERS} m"
    readings["opening"] = {"floorMeters": lowest["floorRow"], "onStone": onStone, "clearMeters": lowest["clearMeters"],
                           "routeNow": _routeNow, "cells": lowest["cells"]}
    openingValue = f"{where} · {clear} · {verdict}"

  under = {column: [_grid[column][BASE_Y - 1][z] == AIR for z in (OUTER_Z, INNER_Z)]
           for column in range(1, SECTION_WIDTH + 1)}
  fullColumns = [column for column, gone in under.items() if all(gone)]
  partColumns = [column for column, gone in under.items() if any(gone) and not all(gone)]
  partFaces = sorted({face for column in partColumns for face, gone in zip(("outer", "inner"), under[column]) if gone})
  gapMeters = LongestRun(fullColumns)
  partMeters = LongestRun(partColumns)
  gapFalls = bool(StoneAbove(_grid, fullColumns, -1) & _firstUnsupported) if fullColumns else False
  readings["gap"] = {"widthMeters": gapMeters, "underOneFaceMeters": partMeters, "falls": gapFalls,
                     "byWidth": gapMeters >= fallWidth, "cells": [[column, -1] for column in fullColumns]}
  if gapMeters == 0 and partMeters == 0:
    gapValue = f"none · the wall above falls at {fallWidth} m"
  else:
    gapValue = f"{gapMeters} m across the full thickness" if gapMeters else "none across the full thickness"
    if partMeters:
      gapValue += f" ({partMeters} m under the {' and '.join(partFaces)} face only)"
    gapValue += f" · the wall above falls at {fallWidth} m" + (" · FALLS" if gapFalls else " · holds")

  weakKey, weakCells, weakPlace = WeakPoint(readings, fallWidth)
  rows = [
    {"key": "WIDEST HOLE", "value": holeValue, "warn": readings["hole"]["falls"]},
    {"key": "LOWEST OPENING", "value": openingValue, "warn": _routeNow},
    {"key": "GAP UNDER FOUNDATION", "value": gapValue, "warn": gapFalls},
    {"key": "WEAK POINT", "value": weakPlace, "warn": False},
    {"key": "STANDS ON", "value": StandsOn(_grid), "warn": False},
  ]
  for reading in readings.values():
    reading["cells"] = [list(cell) for cell in reading["cells"]]
  return {"rows": rows, "readings": readings, "weakReading": weakKey, "weakCells": weakCells,
          "fallWidthMeters": fallWidth}


def WeakPoint(_readings, _fallWidth):
  """Where the section is closest to open: the first reading past its mark, else the one nearest to it."""
  hole, opening, gap = _readings["hole"], _readings["opening"], _readings["gap"]
  candidates = [
    ("opening", opening["routeNow"], 1.0 if opening["routeNow"] else 0.0, opening["cells"]),
    ("hole", hole["falls"], hole["widthMeters"] / _fallWidth, hole["cells"]),
    ("gap", gap["falls"], gap["widthMeters"] / _fallWidth, gap["cells"]),
  ]
  if not opening["onStone"] and opening["floorMeters"] is not None:
    nearness = max(candidates[0][2], opening["clearMeters"] / UNIT_HEIGHT_METERS)
    candidates[0] = ("opening", opening["routeNow"], nearness, opening["cells"])
  past = [candidate for candidate in candidates if candidate[1]]
  key, _, ratio, cells = past[0] if past else max(candidates, key=lambda _c: _c[2])
  if not cells or (not past and ratio == 0):
    return None, [], "none"
  if key == "gap":
    place = f"columns {Span([cell[0] for cell in cells])}, under the foundation"
  else:
    place = Place(cells)
  return key, [list(cell) for cell in cells], place


# ── Views for the page ────────────────────────────────────────────────────────────────────────


def FrontView(_grid):
  """The section cut along its length and seen from outside: for each row, top first, the near (outer) and
  far (inner) layer as material codes, from the left neighbour's column to the right neighbour's."""
  near = ["".join(MATERIAL_CODES[_grid[x][y][OUTER_Z]] for x in range(X_SIZE)) for y in reversed(range(FRONT_ROWS))]
  far = ["".join(MATERIAL_CODES[_grid[x][y][INNER_Z]] for x in range(X_SIZE)) for y in reversed(range(FRONT_ROWS))]
  return {"near": near, "far": far}


def TopView(_grid):
  """Looking down, inside at the top: each column's top material and its height in metres above the ground."""
  materials, heights = [], []
  for z in reversed(range(Z_SIZE)):
    rowMaterials, rowHeights = "", []
    for x in range(X_SIZE):
      y = max((y for y in range(Y_SIZE) if _grid[x][y][z] != AIR), default=-1)
      rowMaterials += MATERIAL_CODES[_grid[x][y][z]] if y >= 0 else "."
      rowHeights.append(y + 1 - BASE_Y)
    materials.append(rowMaterials)
    heights.append(rowHeights)
  return {"material": materials, "height": heights}


def Highest(_grid, _x, _z):
  return max((y for y in range(Y_SIZE) if _grid[_x][y][_z] != AIR), default=-1)


# ── The states ────────────────────────────────────────────────────────────────────────────────


def Hole(_first, _last, _bottomRow, _topRow, _faces="both"):
  return {"kind": "hole", "columns": [_first, _last], "rows": [_bottomRow, _topRow], "faces": _faces}


def Undercut(_first, _last, _faces="both"):
  return {"kind": "undercut", "columns": [_first, _last], "faces": _faces}


SHAVED = Hole(1, 8, 0, BODY_ROWS - 1, "outer")

# Ids are neutral and do not follow the answers. Titles describe the damage only; the page never shows them
# to a participant. The answer is computed below, never written here.
PRACTICE = {"id": "P00", "title": "Undamaged section", "damage": []}
STATES = [
  {"id": "W01", "title": "Slot at the foot, 4 m wide and 1 m high, columns 3–6", "damage": [Hole(3, 6, 0, 0)]},
  {"id": "W02", "title": "Gap under the foundation, 6 m, columns 2–7, with the sappers' trench",
   "damage": [Undercut(2, 7)]},
  {"id": "W03", "title": "Slot at the foot, 5 m wide and 1 m high, columns 2–6", "damage": [Hole(2, 6, 0, 0)]},
  {"id": "W04", "title": "Outer face shaved off the whole body: 1 m left everywhere", "damage": [SHAVED]},
  {"id": "W05", "title": "Tall slit, 1 m wide, from 1 m to 5 m up, column 5", "damage": [Hole(5, 5, 1, 4)]},
  {"id": "W06", "title": "Outer face shaved off, and a 5 m gap under the foundation, columns 2–6",
   "damage": [SHAVED, Undercut(2, 6)]},
  {"id": "W07", "title": "Hole 1 m wide and 2 m high at the foot, column 6", "damage": [Hole(6, 6, 0, 1)]},
  {"id": "W08", "title": "Window 6 m wide and 2 m high, from 2 m up, columns 2–7", "damage": [Hole(2, 7, 2, 3)]},
  {"id": "W09", "title": "Slot at the foot, 7 m wide and 1 m high, columns 1–7", "damage": [Hole(1, 7, 0, 0)]},
  {"id": "W10", "title": "Gap under the foundation, 4 m, columns 3–6, with the sappers' trench",
   "damage": [Undercut(3, 6)]},
  {"id": "W11", "title": "Two slots at the foot, each 3 m wide and 1 m high, columns 1–3 and 5–7",
   "damage": [Hole(1, 3, 0, 0), Hole(5, 7, 0, 0)]},
  {"id": "W12", "title": "Outer face shaved off, and a 5 m slot at the foot of the inner face, columns 3–7",
   "damage": [SHAVED, Hole(3, 7, 0, 0, "inner")]},
  {"id": "W13", "title": "Gap under the foundation, 5 m, columns 1–5, against the left neighbour",
   "damage": [Undercut(1, 5)]},
  {"id": "W14", "title": "Window 3 m wide and 2 m high, from 2 m up, columns 3–5", "damage": [Hole(3, 5, 2, 3)]},
  {"id": "W15", "title": "Slot at the foot, 6 m wide and 1 m high, columns 3–8, against the right neighbour",
   "damage": [Hole(3, 8, 0, 0)]},
  {"id": "W16", "title": "Gap under the outer face only, the whole 8 m, with the sappers' trench",
   "damage": [Undercut(1, 8, "outer")]},
]


def Answer(_grid):
  return "open" if Route(_grid) is not None else "shut"


def LooseAgainstStructure(_grid):
  """Rubble against a standing structure that rises within the set height of its top (GameDescription.md,
  *Materials*). The stand-in does not enforce that rule, so it reports any case where the rule would bind."""
  found = []
  for x, y, z in itertools.product(range(X_SIZE), range(Y_SIZE), range(Z_SIZE)):
    if _grid[x][y][z] not in (STONE_RUBBLE, EARTH_RUBBLE, ROCK_RUBBLE):
      continue
    for dx, dz in SIDES:
      nx, nz = x + dx, z + dz
      structureTop = Highest(_grid, nx, nz) + 1 if InBounds(nx, y, nz) else 0
      if InBounds(nx, y, nz) and _grid[nx][y][nz] in STRUCTURE and y + 1 > structureTop - LOOSE_SET_HEIGHT_METERS:
        found.append([x, y, z])
        break
  return found


def RestingOnStructure(_grid):
  return [[x, y, z] for x, y, z in itertools.product(range(X_SIZE), range(1, Y_SIZE), range(Z_SIZE))
          if _grid[x][y][z] in LOOSE and _grid[x][y - 1][z] in STRUCTURE]


def BuildItem(_state):
  grid = NewGrid()
  ApplyDamage(grid, _state["damage"])
  before = CopyGrid(grid)
  settled, history, firstUnsupported = Resolve(grid)
  if history["settledBeforeSupport"]:
    moves = history["settledBeforeSupport"]
    raise RuntimeError(f"{_state['id']}: the damage state's own earth is not at rest ({moves} moves)")
  routeNow = Route(before)
  routeAfter = Route(settled)
  answer = "open" if routeAfter is not None else "shut"
  thickness = ThicknessReadout(before)
  support = SupportReadout(before, firstUnsupported, routeNow is not None)

  sensitivity = {}
  variants = (("fallOutward", {"_fall": "outward"}), ("fallInward", {"_fall": "inward"}),
              ("fallDamage", {"_fallDamage": True}), ("stoneSpan1", {"_stoneSpan": 1}),
              ("stoneSpan3", {"_stoneSpan": 3}))
  for name, arguments in variants:
    variant, _, _ = Resolve(grid, **arguments)
    sensitivity[name] = Answer(variant)

  readings = support["readings"]
  literal = {
    "thickness": "open" if thickness["thinnestMeters"] == 0 else "shut",
    "asItStands": "open" if routeNow is not None else "shut",
    "support": "open" if (readings["opening"]["routeNow"] or readings["gap"]["falls"]
                          or (readings["hole"]["falls"] and readings["opening"]["floorMeters"] == 0)) else "shut",
  }
  checks = {
    "holeVerdictMatchesWidth": readings["hole"]["falls"] == readings["hole"]["byWidth"],
    "gapVerdictMatchesWidth": readings["gap"]["falls"] == readings["gap"]["byWidth"],
    "looseNearStructureTop": len(LooseAgainstStructure(settled)),
    "looseRestingOnStructure": len(RestingOnStructure(settled)),
    "rubbleAtGroundEdge": any(settled[x][y][z] in (STONE_RUBBLE, EARTH_RUBBLE, ROCK_RUBBLE)
                              for x in range(X_SIZE) for y in range(Y_SIZE) for z in (0, Z_SIZE - 1)),
  }
  fallen = sum(tick["voxels"] for tick in history["ticks"])
  return {
    "id": _state["id"],
    "title": _state["title"],
    "damage": _state["damage"],
    "front": FrontView(before),
    "top": TopView(before),
    "readouts": {"thickness": thickness, "support": support},
    "answer": answer,
    "resolution": {
      "ticks": history["ticks"],
      "fallenVoxels": fallen,
      "routeBefore": routeNow,
      "route": routeAfter,
      "front": FrontView(settled),
      "top": TopView(settled),
    },
    "literalReadings": literal,
    "sensitivity": sensitivity,
    "checks": checks,
  }


# ── Scoring arithmetic ────────────────────────────────────────────────────────────────────────


def ScoreDistribution(_items, _accuracy):
  return [math.comb(_items, k) * _accuracy**k * (1 - _accuracy) ** (_items - k) for k in range(_items + 1)]


def MedianPassProbability(_participants, _items, _accuracy, _passMark):
  """The chance that the median participant's accuracy reaches _passMark when every participant answers each
  item correctly with probability _accuracy, independently."""
  distribution = ScoreDistribution(_items, _accuracy)
  total = 0.0
  for scores in itertools.combinations_with_replacement(range(_items + 1), _participants):
    middle = _participants // 2
    median = scores[middle] if _participants % 2 else (scores[middle - 1] + scores[middle]) / 2
    if median / _items < _passMark - 1e-12:
      continue
    ways = math.factorial(_participants)
    for count in collections.Counter(scores).values():
      ways //= math.factorial(count)
    total += ways * math.prod(distribution[score] for score in scores)
  return total


def Scoring():
  passMark = 0.65
  needed = math.ceil(passMark * ITEM_COUNT - 1e-9)
  power = {}
  for participants in (3, 6):
    power[str(participants)] = {
      f"{accuracy:.2f}": round(MedianPassProbability(participants, ITEM_COUNT, accuracy, passMark), 4)
      for accuracy in (0.5, 0.6, 0.7, 0.75, 0.8, 0.9)
    }
  return {
    "passMarkMedianAccuracy": passMark,
    "items": ITEM_COUNT,
    "correctNeededForOneParticipant": needed,
    "medianOfSixPassesWhenThirdAndFourthSumTo": math.ceil(passMark * ITEM_COUNT * 2 - 1e-9),
    "chanceOneGuesserReachesPassMark": round(sum(ScoreDistribution(ITEM_COUNT, 0.5)[needed:]), 4),
    "chanceMedianPasses": power,
    "note": ("chanceMedianPasses[participants][perItemAccuracy]: each participant answers each item right with that "
             "probability, independently."),
  }


# ── Output ────────────────────────────────────────────────────────────────────────────────────


def Dumps(_value, _indent=0):
  """JSON with two-space indentation, and lists of numbers or short strings kept on one line."""
  pad, inner = "  " * _indent, "  " * (_indent + 1)
  if isinstance(_value, dict):
    if not _value:
      return "{}"
    items = [f"{inner}{json.dumps(key, ensure_ascii=False)}: {Dumps(value, _indent + 1)}"
             for key, value in _value.items()]
    return "{\n" + ",\n".join(items) + "\n" + pad + "}"
  if isinstance(_value, list):
    flat = [isinstance(item, list) and all(not isinstance(cell, (dict, list)) for cell in item) for item in _value]
    scalars = all(not isinstance(item, (dict, list)) for item in _value)
    if scalars or (all(flat) and all(len(item) <= 3 for item in _value)):
      return json.dumps(_value, ensure_ascii=False, separators=(", ", ": "))
    if all(flat):
      rows = (inner + json.dumps(item, ensure_ascii=False, separators=(",", ":")) for item in _value)
      return "[\n" + ",\n".join(rows) + "\n" + pad + "]"
    return "[\n" + ",\n".join(inner + Dumps(item, _indent + 1) for item in _value) + "\n" + pad + "]"
  return json.dumps(_value, ensure_ascii=False)


def Legend():
  palette = json.loads(PALETTE_PATH.read_text(encoding="utf-8"))
  hexes = {entry["index"]: (entry["name"], entry["hex"]) for entry in palette["materials"]}
  return {code: {"index": index, "name": hexes[index][0], "hex": hexes[index][1]}
          for index, code in MATERIAL_CODES.items() if index != AIR}


def TemplateMatches():
  if not WALL_TEMPLATE_PATH.exists():
    return None
  return json.loads(WALL_TEMPLATE_PATH.read_text(encoding="utf-8"))["layers"] == T3_LAYERS


def Build():
  items = [BuildItem(state) for state in STATES]
  practice = BuildItem(PRACTICE)
  counts = collections.Counter(item["answer"] for item in items)
  agreement = {name: sum(1 for item in items if item["literalReadings"][name] == item["answer"])
               for name in ("thickness", "asItStands", "support")}
  disagree = [item["id"] for item in items
              if item["literalReadings"]["thickness"] != item["literalReadings"]["support"]]
  robust = {name: [item["id"] for item in items if item["sensitivity"][name] != item["answer"]]
            for name in items[0]["sensitivity"]}
  body = {
    "instrument": "Breach prediction, paper test (design review F-04, F-47; cheapest test 3)",
    "writeUp": "Design/Review/PaperTests/BreachPrediction.md",
    "generator": "Tools/PaperTests/BreachPrediction.py",
    "status": "Proposed values; no participant has taken it.",
    "model": {
      "voxelMeters": VOXEL_METERS,
      "wall": ("T3, the standard stone section: 8 m long, 2 m thick, 7 m high (5 m solid and 2 m of crenellation), "
               "96 voxels of masonry"),
      "templateMatchesGameData": TemplateMatches(),
      "neighbors": ("one column of each neighbouring section, intact, so support runs into them as it would along "
                    "a wall line"),
      "ground": "y 0 bedrock, y 1 common rock, y 2-3 earth; the wall's lowest row stands on the earth at y 4",
      "aprons": f"{APRON} m of ground on each side of the wall",
      "gridVoxels": {"x": X_SIZE, "y": Y_SIZE, "z": Z_SIZE},
      "axes": "x along the wall, left to right as seen from outside; y up; z from outside to inside",
      "stoneSpanMeters": STONE_SPAN_METERS,
      "rockSpanMeters": ROCK_SPAN_METERS,
      "fallWidthMeters": FallWidth(STONE_SPAN_METERS),
      "anchorDepthMeters": ANCHOR_DEPTH_METERS,
      "climbMeters": CLIMB_METERS,
      "unitHeightMeters": UNIT_HEIGHT_METERS,
      "looseSetHeightMeters": LOOSE_SET_HEIGHT_METERS,
      "shotsAheadN": 0,
    },
    "views": {
      "front": ("rows top first, y 10 down to y 0; columns x 0-9, where 0 and 9 are the neighbouring sections; "
                "near is the outer layer, far the inner"),
      "top": "rows inside first, z 15 down to z 0; heights are metres above the ground's surface",
      "groundRows": BASE_Y,
      "legend": Legend(),
    },
    "summary": {
      "open": counts["open"],
      "shut": counts["shut"],
      "literalReadingsCorrect": agreement,
      "thicknessAndSupportDisagreeOn": disagree,
      "answerChangesUnder": robust,
    },
    "scoring": Scoring(),
    "practice": practice,
    "items": items,
  }
  digest = hashlib.sha256(Dumps(body).encode("utf-8")).hexdigest()[:12]
  return {"version": digest, **body}


def Verify(_data):
  errors = []
  items = _data["items"]
  if len(items) != ITEM_COUNT:
    errors.append(f"{len(items)} items, not {ITEM_COUNT}")
  if _data["summary"]["open"] != ITEM_COUNT // 2 or _data["summary"]["shut"] != ITEM_COUNT // 2:
    errors.append(f"the items split {_data['summary']['open']} open and {_data['summary']['shut']} shut, not even (H2)")
  if _data["practice"]["answer"] != "shut":
    errors.append("the practice item is not shut")
  for item in items + [_data["practice"]]:
    checks = item["checks"]
    if not checks["holeVerdictMatchesWidth"] or not checks["gapVerdictMatchesWidth"]:
      errors.append(f"{item['id']}: a reading's verdict disagrees with its width against the fall width")
    if checks["looseRestingOnStructure"]:
      errors.append(f"{item['id']}: loose material rests on a structure")
    if checks["looseNearStructureTop"]:
      errors.append(f"{item['id']}: a heap rises within the set height of a structure's top, "
                    "which the stand-in does not model")
    if checks["rubbleAtGroundEdge"]:
      errors.append(f"{item['id']}: rubble reached the edge of the modelled ground")
  return errors


def PageData(_data):
  return json.dumps(_data, ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")


def UpdatedPage(_page, _data):
  pattern = re.compile(re.escape(HTML_BEGIN) + r".*?" + re.escape(HTML_END), re.S)
  if not pattern.search(_page):
    raise RuntimeError(f"{HTML_PATH.name} has no {HTML_BEGIN} block")
  return pattern.sub(lambda _m: HTML_BEGIN + PageData(_data) + HTML_END, _page, count=1)


def ReadText(_path):
  raw = _path.read_bytes().decode("utf-8")
  return raw, ("\r\n" if "\r\n" in raw else "\n")


def Report(_data):
  print(f"{'id':4} {'answer':6} {'thick':5} {'now':5} {'supp':5} {'fell':>4}  title")
  for item in _data["items"]:
    literal = item["literalReadings"]
    print(f"{item['id']:4} {item['answer']:6} {literal['thickness']:5} {literal['asItStands']:5} "
          f"{literal['support']:5} "
          f"{item['resolution']['fallenVoxels']:>4}  {item['title']}")
  summary = _data["summary"]
  print(f"{summary['open']} open, {summary['shut']} shut")
  print("literal readings correct:", summary["literalReadingsCorrect"])
  print("thickness and support disagree on:", ", ".join(summary["thicknessAndSupportDisagreeOn"]))
  for name, changed in summary["answerChangesUnder"].items():
    print(f"answer changes under {name}: {', '.join(changed) if changed else 'none'}")
  scoring = _data["scoring"]
  needed, chance = scoring["correctNeededForOneParticipant"], scoring["chanceOneGuesserReachesPassMark"]
  print(f"one guesser reaches {needed}/{ITEM_COUNT}: {chance}")
  print("median passes:", scoring["chanceMedianPasses"])


def Main(_arguments):
  parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
  parser.add_argument("--check", action="store_true", help="exit 1 if the JSON or the page's items are stale")
  arguments = parser.parse_args(_arguments)

  data = Build()
  errors = Verify(data)
  for error in errors:
    print(f"error: {error}", file=sys.stderr)
  if errors:
    return 1
  if data["model"]["templateMatchesGameData"] is False:
    print(f"note: {WALL_TEMPLATE_PATH.name} no longer matches the copy here; the instrument keeps its copy",
          file=sys.stderr)

  jsonText = (Dumps(data) + "\n").replace("\n", "\r\n")  # .gitattributes checks JSON out with CRLF
  page, newline = ReadText(HTML_PATH)
  pageText = UpdatedPage(page, data)
  if arguments.check:
    stale = []
    if not JSON_PATH.exists() or JSON_PATH.read_bytes().decode("utf-8") != jsonText:
      stale.append(JSON_PATH.name)
    if pageText != page:
      stale.append(HTML_PATH.name)
    for name in stale:
      print(f"stale: {name}; run the script without --check", file=sys.stderr)
    return 1 if stale else 0

  JSON_PATH.write_bytes(jsonText.encode("utf-8"))
  HTML_PATH.write_bytes(pageText.encode("utf-8"))
  Report(data)
  jsonName, pageName = JSON_PATH.relative_to(ROOT).as_posix(), HTML_PATH.relative_to(ROOT).as_posix()
  print(f"wrote {jsonName} and the items in {pageName} (version {data['version']})")
  return 0


if __name__ == "__main__":
  sys.exit(Main(sys.argv[1:]))
