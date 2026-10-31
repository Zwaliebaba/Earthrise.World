"""The Siege castle's route table, range circles and repair model: an M0 paper test.

It answers findings F-01, F-02 and F-03 of Design/Review/GameDescription-2026-10-31.md ("Cheapest
tests", item 4). The write-up is Design/Review/PaperTests/SiegeRouteTable.md, which marks every value
as Decided or Proposed and gives its reason. The templates' sizes and voxels are read from
GameData/Server/Templates and their materials from GameData/Server/Palette.json, so the castle is drawn
from the real T1-T5.

Revised on the owner's decisions of 2026-10-31. Stone spans 2 m, so T2's floors and roof are now timber.
Masonry, palette index 6, is what the package calls dressed stone, and it takes the ordinary stone rules.
Repair's pass mark is for one engine, and the ten-second lock stays. The script then checks four
Proposed fixes, (a) to (d), for the Decided rules the first run broke.

The model, in one paragraph. The castle sits on a 1 m grid, north up, with the attacker's camp to the
south. A garrison reaches a ground target if the horizontal distance from its building's outer face
is within the archers' range plus one band per elevation step, and, where line of sight is asked
for, if the straight line from its platform to the target clears every other structure. Engines lob,
so they need range and nothing else. A wall section opens when the trebuchet has removed a fixed net
volume; repair restores volume at a fixed rate per section whenever the section has not been hit for
ten seconds. Collapses use ADR-004's support rule on the T3 voxels, and earth slumps to 45 degrees.

Standard library only.

Usage:
  python Tools/PaperTests/SiegeRouteTable.py
"""

import heapq
import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TEMPLATES = ROOT / "GameData" / "Server" / "Templates"
PALETTE = ROOT / "GameData" / "Server" / "Palette.json"

# ── Decided values, and the placeholders the documents give ─────────────────────────────────────
SECTION_METRES = 8  # T3, MvpAssets.md
STONE_SPAN = 2  # Decided by the owner, 2026-10-31: stone spans 2 m, two voxels (ADR-004 decision 2)
TREBUCHET_MIN_METRES = 12.0  # Hud.md placeholder
TREBUCHET_MAX_METRES = 70.0  # Hud.md placeholder
SHOT_STONE_M3 = 6.0  # Hud.md placeholder
SHOT_PRICE_TARGET = 0.5  # GameDesign.md: a breach costs the attacker about half the defender's rebuild
BREACH_TARGET_SECONDS = 180.0  # GameDescription.md, balance targets: one trebuchet, a standard section
GATE_TARGET_SECONDS = 240.0  # GameDescription.md, balance targets: a ram on a gatehouse's gate, under fire
REPAIR_LOCK_SECONDS = 10.0  # GameDescription.md, Templates; kept by the owner, 2026-10-31
STEP_METRES = 3.0  # GameDescription.md, Combat model
MAX_STEPS = 2
SQUAD_ON_FOOT = 6
SAPPER_FACTOR = 2.0  # sappers dig at twice a laborer's speed
RUBBLE_STONE_SHARE = 0.5  # rubble of building stone clears for stone at half its volume
REPAIR_BAND = (1.3, 2.0)  # the pass mark for one engine, as the owner restated it on 2026-10-31
SWEEP_RELOADS = (6, 8, 10, 12, 15, 20, 25, 30)  # the plausible reloads F-03 names, 6-30 s
RELOAD_RANGE = range(6, 31)

# ── Proposed by this test (each reason is in the write-up) ──────────────────────────────────────
ARCHER_RANGE_METRES = 30.0  # on the level: six bands of 5 m
BAND_METRES = 5.0  # one band of range
BOW_METRES = 1.0  # a garrison's shot leaves 1 m above the floor it stands on
TARGET_METRES = 1.0  # a man on the ground is hit 1 m up
SALLY_LEASH_METRES = 70.0  # the defender sallies out to this distance from its curtain
RELOAD_SECONDS = 20.0  # the trebuchet's reload: nine shots in the three-minute target
OPEN_VOLUME_M3 = 20.0  # net volume removed before a section opens: 5 m x 2 m x 2 m at its base
OPEN_HOLE = (2, 7, 2)  # that hole, in a section's own columns: x from 2 to 6, the bottom 2 m
REPAIR_MULTIPLIER = 1.5  # the repair rate is set so that one engine at RELOAD_SECONDS takes this long
RAM_BLOW_SECONDS = 5.0  # a blow every 5 s, inside the lock, as F-02 assumed
RAM_TIMBER_CRATER = 4  # voxels: the ram's "large" crater in timber
GATE_KEEPS = 0.5  # a gate works while at least half its voxels stand
LABORER_EARTH_M3_PER_SECOND = 0.1
FOUNDATION_METRES = 1  # a standard foundation: it reaches rock under thin soil and rests on earth over deep earth
WARNING_SECONDS_PER_METRE = 1.0  # a collapse warns for 1 s per metre of the falling piece's height
SIDE_SECTIONS = 6  # sections on the east, west and north curtains
CAMP_SOUTH_METRES = 100.0  # the attacker's camp, south of the castle
SCAN_MARGIN_METRES = 130  # how far out the trebuchet's stands are searched
ABOUT = 0.25  # "about" a target time means within a quarter of it
SHOT_PRICE_BAND = (0.4, 0.6)  # "about half"
REPAIR_RATES = (0.06, 0.08, 0.10)  # the repair rates a fix must hold at, around the Proposed 0.08 m³/s
LOB_DEGREES = (15, 20, 25, 30, 35, 40, 45, 50, 60, 70)  # the descents swept for the gate

# ── The Proposed fixes (a) to (d) ───────────────────────────────────────────────────────────────
FIX_TREBUCHET_MAX_METRES = 50.0  # (a) the trebuchet's longest range
DESCENT_DEGREES = 45.0  # (b) a trebuchet's shot descends at 45 degrees or steeper
FIX_RAM_BLOW_SECONDS = 15.0  # (c) a ram's blow every 15 s
FIX_DEEP_EARTH_FOUNDATION_METRES = 4  # (c) a standard foundation reaches 4 m down on deep earth
FIX_SHOT_STONE_M3 = 1.5  # (d) a shot's stone

ERA_ORDER = {"Hall": 0, "Keep": 1, "Citadel": 2}
KIND_BY_BEHAVIOR = {"buildingStone": "stone", "timber": "timber"}  # palette behaviors; any other is a decoration
DECORATION = "decoration"


# ── Templates ───────────────────────────────────────────────────────────────────────────────────


def ReadPalette():
  palette = json.loads(PALETTE.read_text(encoding="utf-8"))
  return {material["index"]: material for material in palette["materials"]}


def ReadTemplate(_stem):
  """A template's JSON, its voxels as {(x, y, z): (kind, part)} with stock left out, and its palette materials
  counted. The kind is stone or timber, from the palette's behavior. Masonry (index 6, the package's "dressed
  stone") has the behavior of building stone, so it is ordinary stone. A decoration, such as an opening, behaves
  and costs as the material around it (ADR-005), here the template's main kind."""
  palette = ReadPalette()
  data = json.loads((TEMPLATES / f"earthrise-{_stem}-1m.json").read_text(encoding="utf-8"))
  kinds = {}
  names = {}
  for char, entry in data["legend"].items():
    material = palette[entry["material"]]
    kinds[char] = KIND_BY_BEHAVIOR.get(material["behavior"], DECORATION)
    names[char] = f"{material['name']} ({material['index']})"
  parts = data.get("parts")
  raw = {}
  counts = {}
  for y, layer in enumerate(data["layers"]):
    for z, row in enumerate(layer):
      for x, char in enumerate(row):
        if char == ".":
          continue
        part = parts[y][z][x] if parts else "."
        if part == "3":
          continue
        raw[(x, y, z)] = (kinds[char], part)
        counts[names[char]] = counts.get(names[char], 0) + 1
  plain = [kind for kind, _ in raw.values() if kind != DECORATION]
  main = "timber" if 2 * plain.count("timber") > len(plain) else "stone"
  voxels = {voxel: (main if kind == DECORATION else kind, part) for voxel, (kind, part) in raw.items()}
  return data, voxels, counts


def TemplateFacts(_stem):
  data, voxels, counts = ReadTemplate(_stem)
  width = len(data["layers"][0][0])
  depth = len(data["layers"][0])
  height = max(y for _, y, _ in voxels) + 1
  kinds = [kind for kind, _ in voxels.values()]
  return {
    "id": data["id"],
    "name": data["name"],
    "size": (width, height, depth),
    "stone": kinds.count("stone"),
    "timber": kinds.count("timber"),
    "critical": sum(1 for _, part in voxels.values() if part == "1"),
    "criticalName": data.get("criticalPart"),
    "materials": counts,
    "voxels": voxels,
  }


# ── Geometry ────────────────────────────────────────────────────────────────────────────────────


def RectDistance(_rect, _x, _y):
  x0, y0, x1, y1 = _rect
  dx = max(x0 - _x, 0.0, _x - x1)
  dy = max(y0 - _y, 0.0, _y - y1)
  return math.hypot(dx, dy)


def RectFarthest(_rect, _x, _y):
  x0, y0, x1, y1 = _rect
  return max(math.hypot(cx - _x, cy - _y) for cx in (x0, x1) for cy in (y0, y1))


def NearestPoint(_rect, _x, _y):
  x0, y0, x1, y1 = _rect
  return min(max(_x, x0), x1), min(max(_y, y0), y1)


def Cells(_rect):
  x0, y0, x1, y1 = _rect
  return ((x, y) for x in range(int(x0), int(x1)) for y in range(int(y0), int(y1)))


# ── The castle ──────────────────────────────────────────────────────────────────────────────────


def BuildCastle(_sides=SIDE_SECTIONS):
  """The Siege castle on the 1 m grid. x runs east and y north; a rect is (x0, y0, x1, y1), half-open.

  Towers (T4, 7 x 7) stand at the corners and in the south curtain. Walls (T3, 2 m thick) run between
  them with their outer face 3 m inside the towers' outer faces. The gatehouse (T5, 9 x 7) is in the
  south curtain, facing the camp. The keep (T2, 12 x 15) stands in the middle of the bailey with its
  forebuilding towards the gate, and the hall (T1, 11 x 7) in the south-west of the bailey.
  """
  size = SECTION_METRES * _sides + 14
  templates = {
    "T1": TemplateFacts("t1-hall"),
    "T2": TemplateFacts("t2-keep"),
    "T3": TemplateFacts("t3-stone-wall-standard"),
    "T4": TemplateFacts("t4-tower"),
    "T5": TemplateFacts("t5-gatehouse"),
  }
  structures = {}
  sections = []

  def Structure(_id, _kind, _rect, _top, _stand=None):
    structures[_id] = {"id": _id, "kind": _kind, "rect": _rect, "top": _top, "stand": _stand}

  platform = templates["T4"]["size"][1] - 1  # the floor of the merlon layer
  corners = {"SW": (0, 0), "SE": (size - 7, 0), "NW": (0, size - 7), "NE": (size - 7, size - 7)}
  for name, (x, y) in corners.items():
    Structure(name, "tower", (x, y, x + 7, y + 7), platform, platform)

  # The south curtain: sections, the gatehouse, one section, the south tower, the rest.
  southSections = _sides - 2
  before = southSections // 2
  after = southSections - before - 1
  x = 7
  count = 0
  gateX = None
  for item in ["S"] * before + ["GH", "S", "ST"] + ["S"] * after:
    if item == "S":
      count += 1
      sections.append({"id": f"S{count}", "side": "S", "rect": (x, 3, x + 8, 5)})
      x += 8
    elif item == "GH":
      gateX = x
      top = templates["T5"]["size"][1] - 1
      Structure("GH", "gatehouse", (x, 0, x + 9, 7), top, top)
      x += 9
    else:
      Structure("ST", "tower", (x, 0, x + 7, 7), platform, platform)
      x += 7
  assert x == size - 7, "the south curtain does not close"
  for side in ("E", "N", "W"):
    for index in range(_sides):
      along = 7 + 8 * index
      rect = {
        "E": (size - 5, along, size - 3, along + 8),
        "N": (along, size - 5, along + 8, size - 3),
        "W": (3, along, 5, along + 8),
      }[side]
      sections.append({"id": f"{side}{index + 1}", "side": side, "rect": rect})
  for section in sections:
    Structure(section["id"], "wall", section["rect"], 6)  # the solid parapet; merlons rise 1 m more

  middle = size / 2
  keepX0 = int(middle) - 6
  keepY0 = int(middle) - 7
  greatTower = (keepX0, keepY0 + 3, keepX0 + 12, keepY0 + 15)
  Structure("KEEP", "keep", greatTower, 16, 16)  # the roof floor; the parapet rises to 19 m
  Structure("KEEP-F", "forebuilding", (keepX0 + 4, keepY0, keepX0 + 8, keepY0 + 3), 8)
  hall = templates["T1"]["size"]
  Structure("HALL", "hall", (9, 9, 9 + hall[0], 9 + hall[2]), hall[1])

  weak = next(s for s in sections if s["id"] == f"E{(_sides + 1) // 2}")
  for section in sections:
    x0, y0, x1, y1 = section["rect"]
    xm, ym = (x0 + x1) / 2, (y0 + y1) / 2
    face, foot, inner = {
      "S": ((xm, y0), (xm, y0 - 0.5), (xm, y1 + 0.5)),
      "N": ((xm, y1), (xm, y1 + 0.5), (xm, y0 - 0.5)),
      "W": ((x0, ym), (x0 - 0.5, ym), (x1 + 0.5, ym)),
      "E": ((x1, ym), (x1 + 0.5, ym), (x0 - 0.5, ym)),
    }[section["side"]]
    section.update(face=face, foot=foot, inner=inner, earth=section is weak, ditch=section is not weak)

  castle = {
    "size": size,
    "sides": _sides,
    "structures": structures,
    "sections": sections,
    "weak": weak,
    "gateX": gateX,
    "passage": (gateX + 3, 0, gateX + 6, 7),
    "gate": (gateX + 4.5, 0.0),
    "gateInner": (gateX + 4.5, 7.5),
    "greatTower": greatTower,
    "keepRect": (keepX0, keepY0, keepX0 + 12, keepY0 + 15),
    "camp": (middle, -CAMP_SOUTH_METRES),
    "templates": templates,
  }
  heights = {}
  for structure in structures.values():
    for cell in Cells(structure["rect"]):
      heights[cell] = (structure["top"], structure["id"])
  castle["heights"] = heights
  castle["garrisons"] = [s for s in structures.values() if s["stand"] is not None]
  for garrison in castle["garrisons"]:
    garrison["steps"] = min(MAX_STEPS, int(garrison["stand"] // STEP_METRES))
    garrison["reach"] = ARCHER_RANGE_METRES + BAND_METRES * garrison["steps"]
  return castle


def RingDistance(_castle, _cx, _cy):
  """How many cells a cell lies outside the castle's outer rectangle (0 inside), as a square ring."""
  size = _castle["size"]
  dx = -_cx if _cx < 0 else (_cx - size + 1 if _cx >= size else 0)
  dy = -_cy if _cy < 0 else (_cy - size + 1 if _cy >= size else 0)
  return max(dx, dy)


def InGap(_castle, _cx, _cy):
  """The causeway in front of the gate, and the unditched stretch in front of the weak section."""
  weak = _castle["weak"]["rect"]
  if _cy < 0 and _castle["gateX"] + 2 <= _cx < _castle["gateX"] + 7:
    return True
  return _cx >= _castle["size"] and weak[1] <= _cy < weak[3]


def Earthwork(_castle, _cx, _cy):
  """'rampart', 'ditch' or None. The standard ditch is 5 m wide; its rampart lies towards the keep."""
  ring = RingDistance(_castle, _cx, _cy)
  if ring == 0 or InGap(_castle, _cx, _cy):
    return None
  if 2 <= ring <= 4:
    return "rampart"
  if 5 <= ring <= 9:
    return "ditch"
  return None


def DeepEarth(_castle, _cx, _cy):
  weak = _castle["weak"]["rect"]
  return _castle["size"] - 6 <= _cx < _castle["size"] + 30 and weak[1] <= _cy < weak[3]


# ── Reach and line of sight ─────────────────────────────────────────────────────────────────────


def LineClear(_castle, _garrison, _x, _y):
  """Whether a flat shot from the garrison's outer face, at its platform, reaches (_x, _y) 1 m up."""
  sx, sy = NearestPoint(_garrison["rect"], _x, _y)
  h0 = _garrison["stand"] + BOW_METRES
  steps = max(1, int(math.hypot(_x - sx, _y - sy) / 0.25))
  for index in range(1, steps):
    t = index / steps
    entry = _castle["heights"].get((math.floor(sx + (_x - sx) * t), math.floor(sy + (_y - sy) * t)))
    if entry is None or entry[1] == _garrison["id"]:
      continue
    if h0 + (TARGET_METRES - h0) * t < entry[0]:
      return False
  return True


def Covering(_castle, _x, _y, _sight=True):
  """The garrisons that can shoot a man standing at (_x, _y)."""
  found = []
  for garrison in _castle["garrisons"]:
    if RectDistance(garrison["rect"], _x, _y) > garrison["reach"]:
      continue
    if _sight and not LineClear(_castle, garrison, _x, _y):
      continue
    found.append(garrison["id"])
  return found


def ScanStands(_castle, _margin=SCAN_MARGIN_METRES):
  """Every 1 m cell where an engine can stand outside the castle and its earthworks, as
  (x, y, clearance, nearest, farthest): the distance beyond the nearest garrison's reach (positive
  means out of every reach, by distance alone), and the great tower's nearest and farthest points."""
  size = _castle["size"]
  garrisons = [(g["rect"], g["reach"]) for g in _castle["garrisons"]]
  greatTower = _castle["greatTower"]
  stands = []
  for cx in range(-_margin, size + _margin):
    for cy in range(-_margin, size + _margin):
      if RingDistance(_castle, cx, cy) <= 9:
        continue
      x, y = cx + 0.5, cy + 0.5
      clearance = min(RectDistance(rect, x, y) - reach for rect, reach in garrisons)
      stands.append((x, y, clearance, RectDistance(greatTower, x, y), RectFarthest(greatTower, x, y)))
  return stands


def CoversGreatTower(_stand, _maxRange=TREBUCHET_MAX_METRES):
  _, _, _, nearest, farthest = _stand
  return nearest <= _maxRange and farthest >= TREBUCHET_MIN_METRES


def KeepToCurtain(_castle):
  """The shortest distance from the great tower to the curtain's outer face, and to its inner face."""
  greatTower = _castle["greatTower"]
  sections = _castle["sections"]
  outer = min(RectDistance(greatTower, *section["face"]) for section in sections)
  inner = min(RectDistance(greatTower, *NearestPoint(section["rect"], *section["inner"])) for section in sections)
  return outer, inner


def CuspBound(_castle):
  """A closed form for fix (a): the distance from the great tower to the nearest point where the reaches of two
  neighboring garrisons on one side of the castle meet outside it. Their gap is measured between their facing
  sides along the line of their outer faces. Where two reaches do not meet, the point is on that line."""
  size = _castle["size"]
  best = math.inf
  for side in "SNWE":
    along = []
    for garrison in _castle["garrisons"]:
      x0, y0, x1, y1 = garrison["rect"]
      if side in "SN" and (y0 == 0 if side == "S" else y1 == size):
        along.append((x0, x1, garrison["reach"]))
      elif side in "WE" and (x0 == 0 if side == "W" else x1 == size):
        along.append((y0, y1, garrison["reach"]))
    along.sort()
    for (_, first, firstReach), (second, _, secondReach) in zip(along, along[1:]):
      reach = min(firstReach, secondReach)
      out = math.sqrt(max(0.0, reach * reach - ((second - first) / 2) ** 2))
      mid = (first + second) / 2
      point = {"S": (mid, -out), "N": (mid, size + out), "W": (-out, mid), "E": (size + out, mid)}[side]
      best = min(best, RectDistance(_castle["greatTower"], *point))
  return best


# ── Collapses on the T3 voxels (ADR-004 decision 2) ─────────────────────────────────────────────


def WallStrip(_castle, _foundation=0):
  """Three T3 sections end to end; the middle one is the section under test. With _foundation, the middle
  section's foundation goes that far down, and its neighbors' FOUNDATION_METRES, to the rock under them.
  Returns ({(x, y, z)}, {x: the bottom voxel's y}); the foundation matters only where the ground goes."""
  voxels = _castle["templates"]["T3"]["voxels"]
  strip = {(x + SECTION_METRES * index, y, z) for index in range(3) for (x, y, z) in voxels}
  bottoms = {}
  if _foundation:
    for x in range(3 * SECTION_METRES):
      depth = _foundation if SECTION_METRES <= x < 2 * SECTION_METRES else FOUNDATION_METRES
      bottoms[x] = -depth
      strip |= {(x, y, z) for y in range(-depth, 0) for z in (0, 1)}
  return strip, bottoms


def SupportDistances(_solid, _groundless=frozenset(), _bottoms=None):
  """Each voxel's support distance (ADR-004 decision 2): the fewest sideways steps on a path that moves only down
  or sideways through solid voxels to the ground. A column's bottom voxel, at y = 0 or at _bottoms[x], rests on
  the ground unless its column is in _groundless."""
  distance = {}
  queue = []
  for voxel in _solid:
    bottom = 0 if _bottoms is None else _bottoms.get(voxel[0], 0)
    if voxel[1] == bottom and voxel[0] not in _groundless:
      distance[voxel] = 0
      queue.append((0, voxel))
  heapq.heapify(queue)
  while queue:
    cost, voxel = heapq.heappop(queue)
    if cost > distance.get(voxel, math.inf):
      continue
    x, y, z = voxel
    # The reverse of a move: what sits above can step down onto this voxel for free, and a sideways
    # neighbor can step across onto it for one.
    moves = (((x, y + 1, z), 0), ((x + 1, y, z), 1), ((x - 1, y, z), 1), ((x, y, z + 1), 1), ((x, y, z - 1), 1))
    for neighbor, step in moves:
      if neighbor in _solid and cost + step < distance.get(neighbor, math.inf):
        distance[neighbor] = cost + step
        heapq.heappush(queue, (cost + step, neighbor))
  return distance


def Unsupported(_solid, _groundless, _span=STONE_SPAN, _bottoms=None):
  """The voxels whose support distance exceeds the span. The strips are all stone, so one span serves."""
  distance = SupportDistances(_solid, _groundless, _bottoms)
  return {voxel for voxel in _solid if distance.get(voxel, math.inf) > _span}


def WidestSupport(_template):
  """The widest support distance among each kind's voxels, with the template whole on flat ground."""
  voxels = _template["voxels"]
  distance = SupportDistances(set(voxels))
  widest = {}
  for voxel, (kind, _) in voxels.items():
    widest[kind] = max(widest.get(kind, 0), distance.get(voxel, math.inf))
  return widest


def PieceHeight(_piece):
  return max(y for _, y, _ in _piece) - min(y for _, y, _ in _piece) + 1 if _piece else 0


def Settle(_solid, _groundless, _span=STONE_SPAN, _bottoms=None):
  """Drops every unsupported piece, and whatever its loss leaves unsupported in turn (a cascade).
  Returns the voxels that fell and the height of the tallest piece, which sets the warning."""
  fallen = set()
  tallest = 0
  while True:
    piece = Unsupported(_solid, _groundless, _span, _bottoms)
    if not piece:
      return fallen, tallest
    _solid -= piece
    fallen |= piece
    tallest = max(tallest, PieceHeight(piece))


def Route(_solid, _columns):
  """Whether foot soldiers can pass: a column empty through the wall's whole thickness for two
  metres, from the ground or from one metre up (the step onto a rubble heap they can climb)."""
  for x in _columns:
    for floor in (0, 1):
      if all((x, y, z) not in _solid for y in (floor, floor + 1) for z in (0, 1)):
        return True
  return False


def TrenchDig(_foundation):
  """The earth sappers dig, per metre of wall, before the ground has gone from under a foundation _foundation
  metres deep across the wall's 2 m (GameDescription.md, Undermining). Earth slumps to 45 degrees: no column
  of earth stands more than one voxel above its neighbor, and one that does gives a voxel to it. The sappers
  dig at the wall's foot and carry out whatever slumps in. Columns 0 and 1 are under the wall, so earth can
  fill them only up to the foundation. The bailey's first column is deep earth, as is the ground outside;
  the rest of the bailey is 1 m of soil over rock, which does not slump. Returns the voxels dug."""
  heights = {}
  floors = {}
  caps = {}
  for z in range(-12, 24):
    heights[z] = -_foundation if z in (0, 1) else 0
    caps[z] = heights[z]
    floors[z] = -1 if z < -1 else -math.inf
  columns = sorted(heights)

  def Slump():
    moved = True
    while moved:
      moved = False
      for z in columns:
        for neighbor in (z - 1, z + 1):
          if neighbor not in heights:
            continue
          if heights[z] - heights[neighbor] > 1 and heights[z] > floors[z] and heights[neighbor] < caps[neighbor]:
            heights[z] -= 1
            heights[neighbor] += 1
            moved = True

  dug = 0
  while heights[0] > -_foundation - 1 or heights[1] > -_foundation - 1:
    heights[2] -= 1
    dug += 1
    Slump()
  return dug


def Undermine(_castle, _foundation, _span=STONE_SPAN):
  """The sappers at the weak section, whose foundation goes _foundation metres down into deep earth. The trench
  runs the section's length; its neighbors stand on rock, which holds a vertical face at the trench's ends."""
  strip, bottoms = WallStrip(_castle, _foundation)
  columns = range(SECTION_METRES, 2 * SECTION_METRES)
  solid = set(strip)
  fallen, tallest = Settle(solid, set(columns), _span, bottoms)
  dig = TrenchDig(_foundation) * SECTION_METRES
  squadRate = SQUAD_ON_FOOT * LABORER_EARTH_M3_PER_SECOND * SAPPER_FACTOR
  route = Route(solid, columns)
  warning = WARNING_SECONDS_PER_METRE * tallest
  return {
    "foundation": _foundation,
    "dig": dig,
    "squadRate": squadRate,
    "digSeconds": dig / squadRate,
    "warning": warning,
    "seconds": dig / squadRate + warning if route else math.inf,
    "fallen": len(fallen),
    "foundationStone": 2 * SECTION_METRES * _foundation,
    "width": len({v[0] for v in fallen if v[1] >= 0}),
    "route": route,
  }


def BreachMechanics(_castle, _span=STONE_SPAN):
  """What each breaker removes and what falls, on the section under test (the strip's middle one).
  The ram may carry its slot on into the next section."""
  strip, _ = WallStrip(_castle)
  base = SECTION_METRES
  columns = range(base, base + SECTION_METRES)
  every = range(0, 3 * SECTION_METRES)
  results = {"span": _span}

  # The trebuchet's hole: OPEN_VOLUME_M3 net, through the wall at its base.
  x0, x1, height = OPEN_HOLE
  hole = {v for v in strip if base + x0 <= v[0] < base + x1 and v[1] < height}
  solid = strip - hole
  fallen, _ = Settle(solid, set(), _span)
  results["trebuchet"] = {"removed": len(hole), "fallen": len(fallen), "width": x1 - x0, "route": Route(solid, columns)}

  # The ram: one voxel a blow, the bottom layer only, the front face first, column by column from
  # the hole's left edge, until foot soldiers have a route.
  solid = set(strip)
  blows = 0
  fallen = set()
  warning = 0
  for column in range(base + x0, 3 * SECTION_METRES):
    for z in (1, 0):
      if (column, 0, z) not in solid or Route(solid, every):
        continue
      solid.discard((column, 0, z))
      blows += 1
      dropped, tallest = Settle(solid, set(), _span)
      fallen |= dropped
      if dropped:
        warning = tallest
  gap = [x for x in every if all((x, y, z) not in solid for y in (1, 2) for z in (0, 1))]
  results["ram"] = {
    "blows": blows,
    "fallen": len(fallen),
    "warning": WARNING_SECONDS_PER_METRE * warning,
    "width": len(gap),
    "route": Route(solid, every),
  }
  results["sappers"] = Undermine(_castle, FOUNDATION_METRES, _span)
  return results


def SpanSensitivity(_castle, _span):
  """The three breakers again at another stone span. The ram may carry its slot on into the next
  section, and the undercut is centred on the section under test and lengthened until a route opens."""
  strip, _ = WallStrip(_castle)
  base = SECTION_METRES
  every = range(0, 3 * SECTION_METRES)
  x0, x1, height = OPEN_HOLE
  solid = {v for v in strip if not (base + x0 <= v[0] < base + x1 and v[1] < height)}
  trebuchetFallen, _ = Settle(solid, set(), _span)
  solid = set(strip)
  blows = 0
  warning = 0
  for column in range(base + x0, 3 * SECTION_METRES):
    for z in (1, 0):
      if Route(solid, every):
        break
      solid.discard((column, 0, z))
      blows += 1
      dropped, tallest = Settle(solid, set(), _span)
      warning = tallest if dropped else warning
  ramRoute = Route(solid, every)
  undercut = None
  for length in range(1, 3 * SECTION_METRES + 1):
    start = base + (SECTION_METRES - length) // 2
    solid = set(strip)
    fallen, _ = Settle(solid, set(range(start, start + length)), _span)
    if Route(solid, every):
      undercut = (length, len(fallen))
      break
  return {
    "trebuchetFallen": len(trebuchetFallen),
    "ramBlows": blows if ramRoute else None,
    "ramSeconds": blows * RAM_BLOW_SECONDS + WARNING_SECONDS_PER_METRE * warning if ramRoute else None,
    "undercut": undercut,
  }


def RamOnStone(_ram, _cadence, _rate):
  """The ram at a section's base, a blow every _cadence seconds, each taking one voxel: (seconds unrepaired,
  seconds repaired, blows repaired, m³ repaired). Repair restores _rate m³ a second in what the lock leaves
  between blows; the expected count is worked as for the trebuchet (Bombard)."""
  blows = _ram["blows"]
  repair = _rate * max(0.0, _cadence - REPAIR_LOCK_SECONDS)
  unrepaired = blows * _cadence + _ram["warning"]
  if repair >= 1.0:
    return unrepaired, math.inf, math.inf, math.inf
  expected = (blows - repair) / (1.0 - repair)
  return unrepaired, expected * _cadence + _ram["warning"], expected, (expected - 1) * repair


# ── The gate ────────────────────────────────────────────────────────────────────────────────────


def RecessedGate(_voxels):
  """Fix (b), in memory only: T5 with its 12 gate voxels moved from the front of its passage to the inner end."""
  gate = [voxel for voxel, (_, part) in _voxels.items() if part == "1"]
  back = min(z for _, _, z in _voxels)
  moved = {voxel: entry for voxel, entry in _voxels.items() if voxel not in gate}
  for x, y, _ in gate:
    assert (x, y, back) not in moved, "the passage's inner end is not open"
    moved[(x, y, back)] = ("timber", "1")
  return moved


def LobPath(_aim, _degrees):
  """The cells a lobbed shot crosses in its last 20 m, coming along the passage from the front (+z), aimed at the
  middle of voxel _aim's front face and descending at _degrees. It ends at _aim, or at None on the ground."""
  x, y, z = _aim
  slope = math.tan(math.radians(_degrees))
  cells = []
  for step in range(2000, -6, -1):
    ahead = step / 100
    height = y + 0.5 + ahead * slope
    if height < 0:
      cells.append(None)
      return cells
    cell = (x, math.floor(height), math.floor(z + 1 + ahead))
    if not cells or cells[-1] != cell:
      cells.append(cell)
    if cell == _aim:
      return cells
  return cells


def Cross(_voxel):
  x, y, z = _voxel
  return ((x, y, z), (x + 1, y, z), (x - 1, y, z), (x, y + 1, z), (x, y - 1, z), (x, y, z + 1), (x, y, z - 1))


def GateByLob(_voxels, _degrees, _rate, _notch=False):
  """A trebuchet against T5's gate, its shots descending at _degrees along the passage, with repair at the
  per-building rate between them. A shot that first meets the gate takes a very large crater of timber: the
  voxel struck and its six neighbors, timber only, because the stone beside the gate takes the trebuchet's
  medium crater, which reaches no sideways neighbor. With _notch, the trebuchet first cuts away the stone in
  the path of its cheapest next shot. Returns None if no shot reaches the gate at all."""
  solid = dict(_voxels)
  gate = sorted(voxel for voxel, (_, part) in _voxels.items() if part == "1")
  keeps = math.ceil(GATE_KEEPS * len(gate))
  shots = notch = timber = 0
  repaired = 0.0
  while shots < 60:
    best = None
    for aim in gate:
      if aim not in solid:
        continue
      path = LobPath(aim, _degrees)
      if path[-1] != aim:
        continue
      blockers = [cell for cell in path[:-1] if cell in solid]
      if any(solid[cell][0] != "stone" for cell in blockers) or (blockers and not _notch):
        continue
      crater = [cell for cell in Cross(aim) if cell in solid and solid[cell][0] == "timber"]
      key = (len(blockers), -len(crater))
      if best is None or key < best[0]:
        best = (key, blockers, crater)
    if best is None:
      break
    _, blockers, crater = best
    for cell in blockers + crater:
      del solid[cell]
    notch += len(blockers)
    timber += len(crater)
    shots += 1
    if sum(1 for voxel in gate if voxel in solid) + repaired < keeps:
      return {"broken": True, "shots": shots, "notch": notch, "timber": timber, "gate": len(gate), "keeps": keeps}
    repaired += min(_rate * max(0.0, RELOAD_SECONDS - REPAIR_LOCK_SECONDS), timber - repaired)
  if shots == 0:
    return None
  return {"broken": False, "shots": shots, "notch": notch, "timber": timber, "gate": len(gate), "keeps": keeps}


def SteepestReach(_voxels):
  """The steepest descent at which any shot along the passage reaches the intact gate, by bisection."""

  def Reaches(_degrees):
    for aim, (_, part) in _voxels.items():
      if part != "1":
        continue
      path = LobPath(aim, _degrees)
      if path[-1] == aim and not any(cell in _voxels for cell in path[:-1]):
        return True
    return False

  low, high = 1.0, 89.0
  for _ in range(40):
    middle = (low + high) / 2
    if Reaches(middle):
      low = middle
    else:
      high = middle
  return low


def StoneShots(_volume, _rate):
  """Expected shots for the trebuchet to cut _volume m³ of stone at the section rate, unrepaired and repaired."""
  hit = OPEN_VOLUME_M3 / ShotsToOpen(RELOAD_SECONDS)
  repair = _rate * max(0.0, RELOAD_SECONDS - REPAIR_LOCK_SECONDS)
  if _volume <= 0:
    return 0.0, 0.0
  return _volume / hit, max(_volume / hit, (_volume - repair) / (hit - repair))


def GateByRam(_castle, _craterVoxels, _blowSeconds, _rate):
  """A consistency check on the four-minute gate target: blows that each remove _craterVoxels of
  the gate, with repair at the per-building rate whenever the lock allows. Returns (blows, seconds),
  or None if repair keeps up."""
  gate = sum(1 for _, part in _castle["templates"]["T5"]["voxels"].values() if part == "1")
  keeps = math.ceil(GATE_KEEPS * gate)
  standing = float(gate)
  for blow in range(1, 1000):
    standing -= _craterVoxels
    if standing < keeps:
      return blow, blow * _blowSeconds
    standing = min(float(gate), standing + _rate * max(0.0, _blowSeconds - REPAIR_LOCK_SECONDS))
  return None


# ── The repair model (F-03) ─────────────────────────────────────────────────────────────────────

SCHEDULES = {
  # name: (shots that land together, seconds between landings as a share of the reload)
  "one": (1, 1.0),
  "step": (2, 1.0),
  "offset": (1, 0.5),
}


def ShotsToOpen(_reload):
  """Shots one engine needs, unrepaired: the three-minute target holds at every reload."""
  return max(1, round(BREACH_TARGET_SECONDS / _reload))


def Bombard(_reload, _schedule, _rate):
  """The expected breach: (seconds unrepaired, seconds repaired, shots repaired, m³ repaired).

  Landings come every gap seconds and each removes `together` craters of OPEN_VOLUME_M3 / shots.
  After each landing, repair is locked for ten seconds and then restores _rate m³ a second until
  the next one. The section opens on the landing that brings the net removed volume to OPEN_VOLUME_M3.
  This is the review's working for F-03, c / (c - r), with the lock and a per-section rate. It counts
  landings as a real number, which averages over where in its last shot a section opens; one siege
  moves in whole shots."""
  shots = ShotsToOpen(_reload)
  together, spacing = SCHEDULES[_schedule]
  gap = _reload * spacing
  hit = together * OPEN_VOLUME_M3 / shots
  repair = _rate * max(0.0, gap - REPAIR_LOCK_SECONDS)
  landings0 = OPEN_VOLUME_M3 / hit
  if repair >= hit:
    return landings0 * gap, math.inf, math.inf, math.inf
  landings = (OPEN_VOLUME_M3 - repair) / (hit - repair)
  return landings0 * gap, landings * gap, landings * together, (landings - 1) * repair


def Multiplier(_reload, _schedule, _rate):
  unrepaired, repaired, _, _ = Bombard(_reload, _schedule, _rate)
  return repaired / unrepaired


def WholeShots(_reload, _rate):
  """One engine's multiplier when the section opens on a whole shot: the expected count rounded up."""
  _, _, landings, _ = Bombard(_reload, "one", _rate)
  if landings == math.inf:
    return math.inf
  return math.ceil(landings - 1e-9) / ShotsToOpen(_reload)


def RateFor(_multiplier, _reload, _schedule="one"):
  """The repair rate that gives _multiplier, by bisection."""
  a, b = 0.0, 20.0
  for _ in range(60):
    mid = (a + b) / 2
    if Multiplier(_reload, _schedule, mid) < _multiplier:
      a = mid
    else:
      b = mid
  return b


def RateWindow(_reload, _schedule="one"):
  """The repair rates (m³/s per section) that give a multiplier inside REPAIR_BAND, or None."""
  low, high = REPAIR_BAND
  top = 20.0
  if Multiplier(_reload, _schedule, top) < low:
    return None
  a, b = 0.0, top
  for _ in range(50):
    mid = (a + b) / 2
    if Multiplier(_reload, _schedule, mid) >= low:
      b = mid
    else:
      a = mid
  rateLow = b
  a, b = 0.0, top
  for _ in range(50):
    mid = (a + b) / 2
    if Multiplier(_reload, _schedule, mid) <= high:
      a = mid
    else:
      b = mid
  rateHigh = a
  return (rateLow, rateHigh) if rateLow <= rateHigh else None


def Intersect(_windows):
  if any(window is None for window in _windows):
    return None
  low = max(window[0] for window in _windows)
  high = min(window[1] for window in _windows)
  return (low, high) if low <= high else None


def WorksFrom():
  """The shortest reload R for which one rate gives one engine 1.3-2x at every reload from R to 30 s, with that
  rate window; None if not even 30 s alone works."""
  windows = {reload: RateWindow(reload) for reload in RELOAD_RANGE}
  best = None
  for start in reversed(RELOAD_RANGE):
    joint = Intersect([windows[r] for r in RELOAD_RANGE if r >= start])
    if joint is None:
      break
    best = (start, joint)
  return best


# ── Printing ────────────────────────────────────────────────────────────────────────────────────


def Table(_header, _rows):
  lines = ["| " + " | ".join(_header) + " |", "|" + "|".join("---" for _ in _header) + "|"]
  lines += ["| " + " | ".join(str(cell) for cell in row) + " |" for row in _rows]
  return "\n".join(lines)


def Plan(_castle):
  """The castle at one character per metre, north up."""
  size = _castle["size"]
  margin = 11
  structures = _castle["structures"]
  owner = {}
  for structure in structures.values():
    for cell in Cells(structure["rect"]):
      owner[cell] = structure
  labels = {}

  def Label(_text, _x, _y):
    for offset, char in enumerate(_text):
      labels[(_x + offset, _y)] = char

  for section in _castle["sections"]:
    x0, y0, x1, y1 = section["rect"]
    if section["side"] in "SN":
      Label(section["id"], int((x0 + x1) / 2) - 1, int(y0) if section["side"] == "S" else int(y1) - 1)
    else:
      Label(section["id"], int(x0), int((y0 + y1) / 2))
  for name in ("SW", "SE", "NW", "NE", "ST"):
    x0, y0, _, _ = structures[name]["rect"]
    Label(f"<{name}>", x0 + 1, y0 + 1 if name in ("SW", "SE", "ST") else y0 + 5)
  x0, y0, _, _ = structures["GH"]["rect"]
  Label("<GH>", x0 + 2, y0 + 5)
  x0, y0, _, _ = _castle["greatTower"]
  Label("<KEEP>", x0 + 3, y0 + 6)
  x0, y0, _, _ = structures["HALL"]["rect"]
  Label("<HALL>", x0 + 2, y0 + 3)
  passage = _castle["passage"]
  marks = {"tower": "T", "gatehouse": "G", "wall": "#", "keep": "K", "forebuilding": "k", "hall": "H"}

  def Char(_cx, _cy):
    if (_cx, _cy) in labels:
      return labels[(_cx, _cy)]
    structure = owner.get((_cx, _cy))
    if structure is not None:
      if structure["kind"] == "gatehouse" and passage[0] <= _cx < passage[2]:
        return "=" if _cy == 0 else "_"
      return marks[structure["kind"]]
    earthwork = Earthwork(_castle, _cx, _cy)
    if earthwork == "ditch":
      return "~"
    if earthwork == "rampart":
      return "^"
    return "," if DeepEarth(_castle, _cx, _cy) else "."

  width = size + 2 * margin
  ruler = "".join("|" if x % 10 == 0 else " " for x in range(-margin, size + margin))
  numbers = [" "] * width
  for x in range(-margin, size + margin):
    if x % 10 == 0:
      for offset, char in enumerate(str(x)):
        if x + margin + offset < width:
          numbers[x + margin + offset] = char
  lines = ["      " + "".join(numbers), "      " + ruler]
  for cy in range(size + margin - 1, -margin - 1, -1):
    prefix = f"{cy:4d}  " if cy % 10 == 0 else "      "
    lines.append(prefix + "".join(Char(cx, cy) for cx in range(-margin, size + margin)))
  lines.append("      " + ruler)
  return "\n".join(lines)


def RangeMap(_castle, _stands):
  """The approach at 3 m by 6 m a character, so that it looks roughly square."""
  size = _castle["size"]
  lookup = {(math.floor(s[0]), math.floor(s[1])): s for s in _stands}
  weakFace = _castle["weak"]["face"]
  camp = _castle["camp"]
  span = 90
  lines = []
  for row in range((size + span) // 6, -(span // 6) - 2, -1):
    chars = []
    for column in range(-span // 3, (size + span) // 3 + 1):
      x, y = column * 3 + 1.5, row * 6 + 3.0
      cx, cy = math.floor(x), math.floor(y)
      stand = lookup.get((cx, cy))
      if abs(x - camp[0]) <= 1.5 and abs(y - camp[1]) <= 3.0:
        chars.append("C")
      elif RingDistance(_castle, cx, cy) == 0:
        chars.append("K" if RectDistance(_castle["greatTower"], x, y) == 0 else "#")
      elif stand is None:
        chars.append("~")
      elif stand[2] <= 0:
        chars.append("o")
      elif CoversGreatTower(stand):
        chars.append("*")
      elif TREBUCHET_MIN_METRES <= math.hypot(x - weakFace[0], y - weakFace[1]) <= TREBUCHET_MAX_METRES:
        chars.append("+")
      else:
        chars.append(".")
    lines.append("".join(chars))
  return "\n".join(lines)


def Seconds(_value):
  return "never" if _value is None or _value == math.inf else f"{_value:.0f}"


def Window(_window):
  if _window is None:
    return "none"
  if _window[1] >= 19.9:
    return f"{_window[0]:.3f} or more"
  return f"{_window[0]:.3f}–{_window[1]:.3f}"


def About(_seconds, _target=BREACH_TARGET_SECONDS):
  return abs(_seconds - _target) <= ABOUT * _target


def Mark(_passed):
  return "PASS" if _passed else "FAIL"


# ── The test ────────────────────────────────────────────────────────────────────────────────────


def PrintInputs(_castle, _rate):
  print("== Templates read from GameData/Server/Templates, materials from GameData/Server/Palette.json ==")
  rows = []
  for key, facts in _castle["templates"].items():
    width, height, depth = facts["size"]
    critical = f"{facts['criticalName']}: {facts['critical']}"
    materials = ", ".join(f"{name} {count}" for name, count in sorted(facts["materials"].items()))
    widest = WidestSupport(facts)
    support = ", ".join(f"{kind} {widest[kind]}" for kind in ("stone", "timber") if kind in widest)
    rows.append([key, facts["name"], f"{width} x {height} x {depth}", facts["stone"], facts["timber"], critical,
                 materials, support])
  print(Table(["Id", "Template", "W x H x D (m)", "Stone m³", "Timber m³", "Critical part: voxels",
               "Palette materials (index): voxels", "Widest support distance (m)"], rows))
  widest = {key: WidestSupport(facts) for key, facts in _castle["templates"].items()}
  stoneWidest = max(support.get("stone", 0) for support in widest.values())
  timber = ", ".join(f"{key} {support['timber']} m" for key, support in widest.items() if support.get("timber"))
  print(f"Every template's stone stands at the Decided stone span of {STONE_SPAN} m: "
        f"{'yes' if stoneWidest <= STONE_SPAN else 'no'} (the widest is {stoneWidest} m).")
  print("Masonry is stone like any other: craters from the stone column, rubble that yields stone.")
  print(f"Timber spans furthest. The timber span each template needs to stand whole: {timber}.")
  print()
  print("== Values Decided by the owner on 2026-10-31 ==")
  decided = [
    ("Stone span", f"{STONE_SPAN} m; T2's floors and roof are timber"),
    ("Masonry (palette 6)", "the package's \"dressed stone\": stone laid by a template, under the ordinary "
                            "stone rules"),
    ("Repair's pass mark", "one engine: repair adds 1.3–2x to the time to open a section, at the Proposed reload"),
    ("Repair lock", f"{REPAIR_LOCK_SECONDS:.0f} s after each hit; two engines alternating keep a section locked"),
  ]
  print(Table(["Value", "Decided"], decided))
  print()
  print("== Values this test chose (Proposed) ==")
  platform = _castle["templates"]["T4"]["size"][1] - 1
  squadRate = SQUAD_ON_FOOT * LABORER_EARTH_M3_PER_SECOND * SAPPER_FACTOR
  shots = ShotsToOpen(RELOAD_SECONDS)
  proposed = [
    ("Archers' range on the level", f"{ARCHER_RANGE_METRES:.0f} m, six bands of {BAND_METRES:.0f} m"),
    ("Elevation", f"towers and gatehouse fire from {platform} m, the keep from 16 m: two steps each"),
    ("Garrisons", f"a squad of {SQUAD_ON_FOOT} archers in each tower, the gatehouse and the keep; "
                  "GH adds its passage attack"),
    ("Sally", f"men-at-arms and outriders, out to {SALLY_LEASH_METRES:.0f} m from the curtain"),
    ("Trebuchet reload", f"{RELOAD_SECONDS:.0f} s, so {shots} shots open a standard section in "
                         f"{BREACH_TARGET_SECONDS:.0f} s"),
    ("Volume to open a section", f"{OPEN_VOLUME_M3:.0f} m³ net: 5 m x 2 m through the wall at its base"),
    ("Repair rate", f"{_rate} m³/s a section ({_rate * 60:.1f} m³ a minute): one engine at {RELOAD_SECONDS:.0f} s "
                    f"then takes {REPAIR_MULTIPLIER}x as long"),
    ("Ram", f"a blow every {RAM_BLOW_SECONDS:.0f} s; 1 voxel of stone a blow, bottom metre only; "
            f"{RAM_TIMBER_CRATER} voxels of timber"),
    ("Very large crater (trebuchet, timber)", "a voxel and its six neighbors, of timber only"),
    ("Gate threshold", f"the gate works while at least {GATE_KEEPS:.0%} of its voxels stand"),
    ("Sappers", f"a laborer digs earth at {LABORER_EARTH_M3_PER_SECOND} m³/s, so six sappers dig {squadRate:.1f} m³/s"),
    ("Standard foundation", f"{FOUNDATION_METRES} m deep: on rock under thin soil, on earth over deep earth"),
    ("Collapse warning", f"{WARNING_SECONDS_PER_METRE:.0f} s per metre of the falling piece's height"),
    ("Ground", "1 m of soil over common rock, except deep earth under the weak section"),
    ("Camp", f"{CAMP_SOUTH_METRES:.0f} m south of the castle"),
    ("\"About\" a target time", f"within {ABOUT:.0%} of it; \"about half\" is "
                                 f"{SHOT_PRICE_BAND[0]}–{SHOT_PRICE_BAND[1]}"),
  ]
  print(Table(["Value", "Proposed"], proposed))
  print()
  print("== Values the Proposed fixes change ==")
  fixes = [
    ("(a) Trebuchet range", f"{TREBUCHET_MIN_METRES:.0f}–{TREBUCHET_MAX_METRES:.0f} m (Hud.md placeholder)",
     f"{TREBUCHET_MIN_METRES:.0f}–{FIX_TREBUCHET_MAX_METRES:.0f} m"),
    ("(a) Range guard", "engine range < tower reach + keep-to-curtain distance",
     "no stand outside every garrison's reach is in engine range of the great tower"),
    ("(b) T5's gate", "the front row of its passage, flush with the outer face",
     "the inner end of its passage (a template change, proposed only)"),
    ("(b) Trebuchet's descent", "not stated", f"{DESCENT_DEGREES:.0f} degrees or steeper"),
    ("(c) Targets", "none for undermining or a ram at a wall's base",
     f"each opens a standard section in about {BREACH_TARGET_SECONDS:.0f} s, unopposed"),
    ("(c) Ram", f"a blow every {RAM_BLOW_SECONDS:.0f} s", f"a blow every {FIX_RAM_BLOW_SECONDS:.0f} s"),
    ("(c) Standard foundation on deep earth", f"{FOUNDATION_METRES} m", f"{FIX_DEEP_EARTH_FOUNDATION_METRES} m"),
    ("(d) A shot's stone", f"{SHOT_STONE_M3:.0f} m³ (Hud.md placeholder)", f"{FIX_SHOT_STONE_M3} m³"),
  ]
  print(Table(["Value", "As the documents stand", "Proposed fix"], fixes))
  print()


def PrintPlan(_castle):
  size = _castle["size"]
  weak = _castle["weak"]
  print(f"== The Siege castle: {size} m x {size} m over its towers, 1 character = 1 m, north up ==")
  print("T tower  G gatehouse  = timber gate  _ gate passage  # wall section (labelled)  K great tower")
  print("k forebuilding  H hall  ^ rampart  ~ ditch  . thin soil over rock  , deep earth")
  print("The attacker's camp is 100 m south of the castle. The ditch stops at the causeway in front of the gate")
  print(f"and in front of {weak['id']}, the one section that stands on deep earth.")
  print()
  print(Plan(_castle))
  print()
  outer, inner = KeepToCurtain(_castle)
  print(f"Bailey {size - 10} m x {size - 10} m inside the curtain. Great tower to the curtain, on the shortest side:")
  count = len(_castle["sections"])
  print(f"{inner:.0f} m to its inner face, {outer:.0f} m to its outer face. Curtain: {count} sections of T3;")
  print("towers SW, SE, NW, NE and ST (T4); gatehouse GH (T5); keep (T2); hall (T1).")
  print()


def CoveringArea(_stands, _maxRange):
  return sum(1 for s in _stands if s[2] > 0 and CoversGreatTower(s, _maxRange))


def PrintRanges(_castle, _stands, _variants):
  """F-01: can a trebuchet outside every garrison's reach hit the great tower?"""
  size = _castle["size"]
  weak = _castle["weak"]
  print("== Range circles ==")
  rows = []
  for garrison in _castle["garrisons"]:
    rows.append([garrison["id"], garrison["kind"], garrison["stand"], garrison["steps"], f"{garrison['reach']:.0f}"])
  print(Table(["Garrison", "Building", "Stands at (m)", "Steps", "Reach from its outer face (m)"], rows))
  print(f"Archers on the level: {ARCHER_RANGE_METRES:.0f} m. Trebuchet: {TREBUCHET_MIN_METRES:.0f}–"
        f"{TREBUCHET_MAX_METRES:.0f} m. Engines lob, so they need no line of sight.")
  print()
  safe = [s for s in _stands if s[2] > 0]
  danger = [s for s in safe if CoversGreatTower(s)]
  print(f"Engine stands searched: every 1 m cell out to {SCAN_MARGIN_METRES} m beyond the castle, outside the ditch")
  print(f"ring. Stands outside every garrison's reach, by distance alone (the smallest the safe area can be): "
        f"{len(safe)} m².")
  print(f"Of those, stands from which the trebuchet reaches the great tower: {len(danger)} m².")
  if danger:
    closest = min(danger, key=lambda s: s[3])
    safest = max(danger, key=lambda s: s[2])
    print(f"  The closest is {closest[3]:.1f} m from the great tower, at ({closest[0]:.1f}, {closest[1]:.1f}),")
    print(f"  {closest[2]:.1f} m beyond the nearest garrison's reach.")
    print(f"  The safest is {safest[2]:.1f} m beyond every reach and {safest[3]:.1f} m from the great tower.")
    sides = {"north": 0, "east": 0, "south": 0, "west": 0}
    for x, y, *_ in danger:
      angle = math.degrees(math.atan2(y - size / 2, x - size / 2))
      if -45 <= angle < 45:
        sides["east"] += 1
      elif 45 <= angle < 135:
        sides["north"] += 1
      elif -135 <= angle < -45:
        sides["south"] += 1
      else:
        sides["west"] += 1
    print("  By side (m²): " + ", ".join(f"{side} {area}" for side, area in sides.items()))
  reach = ARCHER_RANGE_METRES + BAND_METRES * MAX_STEPS
  outer, _ = KeepToCurtain(_castle)
  bound = reach + outer
  verdict = "holds" if TREBUCHET_MAX_METRES < bound else "VIOLATED"
  print("The Decided guard (GameDesign.md, the design review, decision 2): engine range < tower reach")
  print("  + keep-to-curtain distance;")
  print(f"  here {TREBUCHET_MAX_METRES:.0f} < {reach:.0f} + {outer:.0f} = {bound:.0f}: {verdict}.")
  trueBound = min(s[3] for s in safe)
  print(f"  The range below which no stand outside reach reaches the great tower is {trueBound:.1f} m on this castle,")
  print(f"  {bound - trueBound:.1f} m less than the guard's formula allows: between towers, their reach ends closer")
  print("  to the curtain than tower reach measured from the curtain line.")
  print()
  print("Sensitivity on this castle (distance only):")
  rows = []
  for archer in (30, 35, 40, 45, 50, 55, 60):
    other = archer + BAND_METRES * MAX_STEPS
    area = sum(1 for s in _stands if s[2] + (reach - other) > 0 and CoversGreatTower(s))
    rows.append([archer, f"{other:.0f}", area])
  print(Table(["Archer range (m)", "Garrison reach (m)", "Stands covering the great tower (m²)"], rows))
  print()
  rows = []
  for sides, (other, otherStands) in _variants.items():
    otherOuter, _ = KeepToCurtain(other)
    guard = "holds" if TREBUCHET_MAX_METRES < reach + otherOuter else "violated"
    area = CoveringArea(otherStands, TREBUCHET_MAX_METRES)
    rows.append([sides, f"{other['size']} m", f"{otherOuter:.0f}", guard, area, len(other["sections"]) * 96])
  print(Table(["Sections a side", "Castle", "Keep to curtain (m)", "Guard", "Stands covering the great tower (m²)",
               "Curtain walls (m³)"], rows))
  print("The larger castles keep towers only at the corners and beside the gate, as this one does.")
  print()
  print("Every garrison can itself be shelled from outside every garrison's reach:")
  rows = []
  for garrison in _castle["garrisons"]:
    rect = garrison["rect"]
    cells = []
    for maxRange in (TREBUCHET_MAX_METRES, FIX_TREBUCHET_MAX_METRES):
      candidates = [s for s in safe if RectFarthest(rect, s[0], s[1]) >= TREBUCHET_MIN_METRES
                    and RectDistance(rect, s[0], s[1]) <= maxRange]
      cells.append(f"{len(candidates)} m²")
    best = min((RectDistance(rect, s[0], s[1]) for s in safe), default=None)
    rows.append([garrison["id"]] + cells + ["—" if best is None else f"{best:.1f}"])
  print(Table(["Target", f"Safe stands in range at {TREBUCHET_MAX_METRES:.0f} m",
               f"At {FIX_TREBUCHET_MAX_METRES:.0f} m (fix a)", "Closest (m)"], rows))
  print()
  print("Range map, 3 m x 6 m a character, north up: # castle, K great tower, ~ earthworks, C camp,")
  print("o inside a garrison's reach, * outside every reach with the great tower in trebuchet range,")
  print(f"+ outside every reach with {weak['id']} in range but not the great tower, . outside every reach.")
  print()
  print(RangeMap(_castle, _stands))
  print()
  return danger


def PrintSections(_castle, _stands):
  print("== Every wall section ==")
  camp = _castle["camp"]
  weak = _castle["weak"]
  safe = [s for s in _stands if s[2] > 0]
  sections = _castle["sections"]
  distances = {s["id"]: math.hypot(s["face"][0] - camp[0], s["face"][1] - camp[1]) for s in sections}
  nearest = min(sections, key=lambda s: distances[s["id"]])
  rows = []
  facts = {}
  for section in sections:
    foot = Covering(_castle, *section["foot"])
    mouth = Covering(_castle, *section["inner"])
    gap = min(RectDistance(g["rect"], *section["foot"]) for g in _castle["garrisons"])
    fx, fy = section["face"]
    standoffs = [math.hypot(s[0] - fx, s[1] - fy) for s in safe]
    standoff = min((d for d in standoffs if TREBUCHET_MIN_METRES <= d <= TREBUCHET_MAX_METRES), default=None)
    toKeep = RectDistance(_castle["keepRect"], *section["inner"])
    facts[section["id"]] = {"foot": foot, "mouth": mouth, "toKeep": toKeep, "standoff": standoff}
    mark = " (nearest)" if section is nearest else " (weak)" if section is weak else ""
    rows.append([
      section["id"] + mark,
      "deep earth" if section["earth"] else "rock",
      "yes" if section["ditch"] else "no",
      f"{distances[section['id']]:.0f}",
      f"{gap:.0f}",
      ", ".join(foot) or "none",
      "—" if standoff is None else f"{standoff:.0f}",
      len(mouth),
      f"{toKeep:.0f}",
    ])
  print(Table(["Section", "Ground", "Ditch", "From camp (m)", "Nearest garrison (m)", "Garrisons on its foot",
               "Closest safe trebuchet stand (m)", "Garrisons on its inner mouth", "Inner mouth to keep (m)"], rows))
  print()
  print(f"Nearest section to the camp: {nearest['id']}. Weak section: {weak['id']} (deep earth, no ditch, "
        "among the farthest from towers).")
  gx, gy = _castle["gate"]
  gateStand = min(d for d in (math.hypot(s[0] - gx, s[1] - gy) for s in safe) if d >= TREBUCHET_MIN_METRES)
  print(f"Closest safe trebuchet stand to the gate: {gateStand:.0f} m.")
  size = _castle["size"]
  covering = [s for s in safe if CoversGreatTower(s)]
  farthest = max((RectDistance((0, 0, size, size), s[0], s[1]) for s in covering), default=0)
  print(f"Every stand that covers the great tower lies within {farthest:.0f} m of the castle's outer faces, inside the")
  print(f"sally's Proposed leash of {SALLY_LEASH_METRES:.0f} m; so does every closest stand in the table above.")
  print()
  return nearest, facts, gateStand


def PrintMechanics(_castle, _rate):
  mechanics = BreachMechanics(_castle)
  treb, ram, sappers = mechanics["trebuchet"], mechanics["ram"], mechanics["sappers"]
  print(f"== Breach mechanics on T3, from ADR-004's support rule with the Decided stone span of {STONE_SPAN} m ==")
  print("T3 is masonry (palette 6), which takes the ordinary stone rules: what falls is stone rubble, which clears")
  print(f"for stone at {RUBBLE_STONE_SHARE:.0%} of its volume.")
  print(f"Trebuchet: a hole of {treb['removed']} m³ ({OPEN_HOLE[1] - OPEN_HOLE[0]} m x {OPEN_HOLE[2]} m through 2 m) "
        f"at the base is a route ({'yes' if treb['route'] else 'no'}); {treb['fallen']} m³ above it then falls.")
  print(f"Ram: {ram['blows']} blows cut a slot along the bottom metre; the stone above it falls in pieces, "
        f"{ram['fallen']} m³ in all,")
  print(f"  leaving a gap {ram['width']} m wide over the rubble, the last piece after a {ram['warning']:.0f} s "
        f"warning. Route: {'yes' if ram['route'] else 'no'}.")
  print(f"Sappers: under the slump rule, a squad digs {sappers['dig']} m³ ({sappers['dig'] // SECTION_METRES} m³ a "
        f"metre) before the ground has gone from under")
  print(f"  {_castle['weak']['id']}'s {sappers['foundation']} m foundation along its {SECTION_METRES} m; at "
        f"{sappers['squadRate']:.1f} m³/s that takes {sappers['digSeconds']:.0f} s. {sappers['fallen']} m³ falls, "
        f"{sappers['width']} m of the section,")
  print(f"  after a warning of {sappers['warning']:.0f} s. The ends within {STONE_SPAN} m of sound sections stay up. "
        f"Route: {'yes' if sappers['route'] else 'no'}.")
  gate = GateByLob(_castle["templates"]["T5"]["voxels"], DESCENT_DEGREES, _rate)
  print(f"Gate by trebuchet: {gate['shots']} very large craters take {gate['timber']} of the gate's {gate['gate']} "
        "timber voxels, and no stone;")
  print(f"  the gate works while {gate['keeps']} stand, so it is broken after {gate['shots'] * RELOAD_SECONDS:.0f} s, "
        "repair between the shots included.")
  fast = GateByRam(_castle, RAM_TIMBER_CRATER, RAM_BLOW_SECONDS, _rate)
  print(f"Gate by ram, checked against the craters: a large crater of {RAM_TIMBER_CRATER} voxels a blow, every "
        f"{RAM_BLOW_SECONDS:.0f} s, breaks the gate")
  print(f"  in {fast[0]} blows, {fast[1]:.0f} s, against the Decided target of {GATE_TARGET_SECONDS:.0f} s.")
  for cadence in (FIX_RAM_BLOW_SECONDS, 30.0, 60.0, 120.0):
    slow = GateByRam(_castle, RAM_TIMBER_CRATER, cadence, _rate)
    result = "repair keeps up, the gate never breaks." if slow is None else f"{slow[0]} blows, {slow[1]:.0f} s."
    print(f"  A blow every {cadence:.0f} s: {result}")
  print(f"  No cadence gives about {GATE_TARGET_SECONDS:.0f} s at {RAM_TIMBER_CRATER} voxels a blow: the target holds "
        "only if the passage attack")
  print("  keeps the ram off the gate most of the time.")
  print()
  print("At each span, on T3 sections end to end (the Decided span, and 4 m as a sensitivity only):")
  rows = []
  for span in (STONE_SPAN, 4):
    result = SpanSensitivity(_castle, span)
    ram = "no route" if result["ramBlows"] is None else f"{result['ramBlows']} blows, {result['ramSeconds']:.0f} s"
    length, fell = result["undercut"] if result["undercut"] else (None, 0)
    undercut = "never" if length is None else f"{length} m ({fell} m³ falls)"
    within = "—" if length is None else ("yes" if length <= SECTION_METRES else "no: it reaches into the neighbours")
    label = f"{span} (Decided)" if span == STONE_SPAN else f"{span} (sensitivity)"
    rows.append([label, result["trebuchetFallen"], ram, undercut, within])
  print(Table(["Span (voxels)", "Falls above the trebuchet's 5 m hole (m³)", "Ram at the base, to a route",
               "Shortest undercut that opens a route", "Within one 8 m section?"], rows))
  print()
  return mechanics, gate


def BuildRoutes(_castle, _rate, _mechanics, _nearest, _facts, _fixed):
  """Every way in: as the documents stand, or with the Proposed fixes (a) to (d)."""
  weak = _castle["weak"]
  treb, ram = _mechanics["trebuchet"], _mechanics["ram"]
  price = FIX_SHOT_STONE_M3 if _fixed else SHOT_STONE_M3
  routes = []

  def AddRoute(_name, _open, _openRepaired, _stone, _stoneRepaired, _defender, _note, _hurt, _era, _reasons):
    routes.append({"name": _name, "open": _open, "openRepaired": _openRepaired, "stone": _stone,
                   "stoneRepaired": _stoneRepaired, "defender": _defender, "note": _note, "hurt": _hurt, "era": _era,
                   "reasons": _reasons})

  AddRoute("Gate · ram", GATE_TARGET_SECONDS, GATE_TARGET_SECONDS, 0, 0, 0, "+7 timber", ["GH (passage)", "sally"],
           "Hall", "it is the gate; no stone; the ram's roof stops tower arrows")
  t5 = _castle["templates"]["T5"]["voxels"]
  if not _fixed:
    gate = GateByLob(t5, DESCENT_DEGREES, _rate)
    seconds = gate["shots"] * RELOAD_SECONDS
    AddRoute("Gate · trebuchet", seconds, seconds, gate["shots"] * price, gate["shots"] * price, 0,
             f"+{gate['timber']} timber", ["sally"], "Citadel",
             "it is the gate; two shots from outside every reach; breaking the gate puts out the gatehouse's garrison")
  else:
    gate = GateByLob(RecessedGate(t5), DESCENT_DEGREES, _rate, _notch=True)
    plain, repaired = StoneShots(gate["notch"], _rate)
    AddRoute("Gate · trebuchet (notch, then gate)", (plain + gate["shots"]) * RELOAD_SECONDS,
             (repaired + gate["shots"]) * RELOAD_SECONDS, (plain + gate["shots"]) * price,
             (repaired + gate["shots"]) * price, gate["notch"], f"+{gate['timber']} timber", ["sally"], "Citadel",
             "it is the gate, but the arch over the passage must be cut back first")
  time0, timeR, shotsR, repairedR = Bombard(RELOAD_SECONDS, "one", _rate)
  shots0 = ShotsToOpen(RELOAD_SECONDS)
  rebuild = treb["removed"] + treb["fallen"]
  note = f"{repairedR:.0f} repaired + {rebuild} rebuilt"
  trebStone = (shots0 * price, shotsR * price)
  AddRoute(f"{_nearest['id']} · trebuchet (nearest)", time0, timeR, *trebStone, rebuild + repairedR, note, ["sally"],
           "Citadel", "nearest the camp; stand-off outside every reach")
  AddRoute(f"{weak['id']} · trebuchet (weak)", time0, timeR, *trebStone, rebuild + repairedR, note, ["sally"],
           "Citadel", "on earth, no ditch, far from towers; stand-off outside every reach")
  AddRoute("Other sections · trebuchet", time0, timeR, *trebStone, rebuild + repairedR, note, ["sally"], "Citadel",
           "the same in every scored column as the two above; the storm leg differs")
  sappers = Undermine(_castle, FIX_DEEP_EARTH_FOUNDATION_METRES if _fixed else FOUNDATION_METRES)
  why = "the only section on earth; no ditch to cross; no stone; the most for the defender to rebuild"
  AddRoute(f"{weak['id']} · sappers (undermining)", sappers["seconds"], sappers["seconds"], 0, 0, sappers["fallen"],
           "+ the ground under it", _facts[weak["id"]]["foot"] + ["sally"], "Keep", why)
  cadence = FIX_RAM_BLOW_SECONDS if _fixed else RAM_BLOW_SECONDS
  plain, repaired, _, restored = RamOnStone(ram, cadence, _rate)
  locked = "blows keep repair locked" if cadence <= REPAIR_LOCK_SECONDS else "repair works between its blows"
  AddRoute(f"{weak['id']} · ram at the base", plain, repaired, 0, 0, ram["blows"] + ram["fallen"] + restored, "",
           ["sally"], "Hall", f"no ditch, so the ram can reach the foot; arrow-proof; {locked}")
  return routes, sappers


def PrintRoutes(_title, _routes, _rate, _fixed):
  print(f"== {_title} ==")

  def Score(_route):
    return {
      "Open": _route["open"],
      "Open, repaired": _route["openRepaired"],
      "Attacker stone": _route["stoneRepaired"],
      "Defender stone": -_route["defender"],
      "Breakers hurt by": len(_route["hurt"]),
      "Era": ERA_ORDER[_route["era"]],
    }

  scores = [Score(route) for route in _routes]
  columns = list(scores[0].keys())
  best = {column: min(score[column] for score in scores) for column in columns}
  for route, score in zip(_routes, scores):
    route["wins"] = [column for column in columns if score[column] == best[column]]
    route["dominatedBy"] = [
      other["name"] for other, otherScore in zip(_routes, scores)
      if otherScore is not score
      and all(otherScore[c] <= score[c] for c in columns) and any(otherScore[c] < score[c] for c in columns)
    ]
  rows = []
  for route in _routes:
    defender = f"{route['defender']:.0f}" + (f" ({route['note']})" if route["note"] else "")
    won = f"{len(route['wins'])}/{len(columns)}" + (" DOMINANT" if len(route["wins"]) == len(columns) else "")
    rows.append([
      route["name"],
      Seconds(route["open"]),
      Seconds(route["openRepaired"]),
      f"{route['stone']:.0f} / {route['stoneRepaired']:.0f}",
      defender,
      ", ".join(route["hurt"]) + f" ({len(route['hurt'])})",
      route["era"],
      won,
      "yes" if route["dominatedBy"] else "no",
      route["reasons"],
    ])
  print(Table(["Route", "Open (s)", "Open with repair (s)", "Attacker stone m³ (unrepaired / repaired)",
               "Defender stone m³", "Defenders who can hurt the breakers", "Era (breaker)", "Columns won", "Dominated",
               "Why pick it"], rows))
  print()
  dominant = [route["name"] for route in _routes if len(route["wins"]) == len(columns)]
  print("Dominant route: " + (", ".join(dominant) if dominant else "none"))
  for route in _routes:
    print(f"  {route['name']}: wins {', '.join(route['wins']) or 'nothing'}")
  print("Not dominated by any other route: " + ", ".join(r["name"] for r in _routes if not r["dominatedBy"]))
  for route in _routes:
    if route["dominatedBy"]:
      print(f"Dominated: {route['name']}, by {', '.join(route['dominatedBy'])}")
  print("Times and stone with repair are expected values (see the repair model); one siege moves in whole shots.")
  price = FIX_SHOT_STONE_M3 if _fixed else SHOT_STONE_M3
  cadence = FIX_RAM_BLOW_SECONDS if _fixed else RAM_BLOW_SECONDS
  print(f"Repair at {_rate} m³/s a section; one trebuchet at a {RELOAD_SECONDS:.0f} s reload; {price} m³ a shot; "
        f"a ram blow every {cadence:.0f} s.")
  print()
  return dominant


def PrintStorm(_castle, _mechanics, _nearest, _facts):
  print("== Storming: from each opening to the keep's foot ==")
  weak = _castle["weak"]
  keepRect = _castle["keepRect"]
  treb, ram, sappers = _mechanics["trebuchet"], _mechanics["ram"], _mechanics["sappers"]
  gateMouth = [g for g in Covering(_castle, *_castle["gateInner"]) if g != "GH"]
  rows = [["Gate passage", "3 m, 7 m long", ", ".join(gateMouth) + " (GH's own is put out when its gate breaks)",
           f"{RectDistance(keepRect, *_castle['gateInner']):.0f}"]]
  for section in (_nearest, weak):
    if section is weak:
      width = f"{treb['width']} m (trebuchet), {ram['width']} m (ram), {sappers['width']} m (sappers)"
    else:
      width = f"{treb['width']} m (trebuchet)"
    facts = _facts[section["id"]]
    rows.append([section["id"], width, ", ".join(facts["mouth"]), f"{facts['toKeep']:.0f}"])
  print(Table(["Opening", "Width", "Garrisons that can shoot its inner mouth", "To the keep's foot (m)"], rows))
  print()
  x0, y0, x1, y1 = keepRect
  feet = {
    "north": ((x0 + x1) / 2, y1 + 0.5),
    "east": (x1 + 0.5, (y0 + y1) / 2),
    "south": ((x0 + x1) / 2, y0 - 0.5),
    "west": (x0 - 0.5, (y0 + y1) / 2),
  }
  rows = []
  for side, point in feet.items():
    covering = [g for g in Covering(_castle, *point) if g != "KEEP"]
    rows.append([side, ", ".join(covering), len(covering)])
  print(Table(["Keep's foot", "Garrisons that can shoot it (the keep's own is put out once it stops working)", "Count"],
              rows))
  print()


def PrintRepair(_rate):
  """F-03, restated by the owner: with one engine, repair adds 1.3-2x to the time to open a section."""
  print("== Repair model (F-03) ==")
  print(f"A section opens at {OPEN_VOLUME_M3:.0f} m³ net. The three-minute target holds at every reload, so one engine")
  print(f"needs round(180 / reload) shots and each removes {OPEN_VOLUME_M3:.0f} m³ / shots. Repair is locked for "
        f"{REPAIR_LOCK_SECONDS:.0f} s after every hit.")
  print("A multiplier is the breach time with repair over the time without, for the same engines. The pass mark")
  print("is for one engine (owner, 2026-10-31); two engines are shown as the behaviour the lock intends.")
  print(f"Repair: {_rate} m³/s.")
  print()
  rows = []
  for reload in SWEEP_RELOADS:
    shots = ShotsToOpen(reload)
    one = Multiplier(reload, "one", _rate)
    whole = WholeShots(reload, _rate)
    inBand = REPAIR_BAND[0] <= one <= REPAIR_BAND[1]
    cells = [reload, shots, f"{OPEN_VOLUME_M3 / shots:.2f}", f"{Bombard(reload, 'one', 0.0)[0]:.0f}", f"{one:.2f}",
             f"{whole:.2f}", "yes" if inBand else "no", Window(RateWindow(reload))]
    for schedule in ("step", "offset"):
      m = Multiplier(reload, schedule, _rate)
      cells.append("never" if m == math.inf else f"{m:.2f}")
    rows.append(cells)
  print(Table(["Reload (s)", "Shots", "m³ a shot", "One engine, unrepaired (s)", "x one engine",
               "x one engine, whole shots", "In 1.3–2x?", "Rates for 1.3–2x, one engine (m³/s)",
               "x two in step (intended)", "x two offset (intended)"], rows))
  print()
  holds = [reload for reload in RELOAD_RANGE if REPAIR_BAND[0] <= Multiplier(reload, "one", _rate) <= REPAIR_BAND[1]]
  dead = [reload for reload in RELOAD_RANGE if Multiplier(reload, "one", _rate) == 1.0]
  locked = [reload for reload in RELOAD_RANGE if Multiplier(reload, "offset", _rate) == 1.0]
  span = f"{holds[0]}–{holds[-1]} s" if holds and holds == list(range(holds[0], holds[-1] + 1)) else str(holds)
  print(f"At {_rate} m³/s, one engine's multiplier is inside 1.3–2x at reloads of {span}.")
  print(f"Repair does nothing for one engine at reloads of {dead[-1]} s or less: no landing leaves the section "
        "unlocked.")
  print(f"Two engines firing alternately keep repair locked out (x1.00) at every reload up to {locked[-1]} s.")
  tail = WorksFrom()
  print(f"One rate serves one engine at every reload from {tail[0]} s to 30 s: {Window(tail[1])} m³/s.")
  print()
  return (Multiplier(RELOAD_SECONDS, "one", _rate), WholeShots(RELOAD_SECONDS, _rate),
          Multiplier(RELOAD_SECONDS, "step", _rate), Multiplier(RELOAD_SECONDS, "offset", _rate))


def PrintFixRange(_variants, _facts):
  """Fix (a): the trebuchet's range, and the guard restated as what it measures."""
  print("== Proposed fix (a): the engine range guard ==")
  print(f"The trebuchet's longest range becomes {FIX_TREBUCHET_MAX_METRES:.0f} m, and the guard reads: no stand "
        "outside every garrison's reach")
  print("is within an engine's range of the great tower, measured on the castle's plan.")
  reach = ARCHER_RANGE_METRES + BAND_METRES * MAX_STEPS
  rows = []
  results = {}
  for sides, (castle, stands) in _variants.items():
    outer, _ = KeepToCurtain(castle)
    formula = reach + outer
    measured = min(s[3] for s in stands if s[2] > 0)
    cusp = CuspBound(castle)
    now = CoveringArea(stands, TREBUCHET_MAX_METRES)
    fixed = CoveringArea(stands, FIX_TREBUCHET_MAX_METRES)
    results[sides] = (formula, measured, cusp, now, fixed)
    rows.append([sides, f"{castle['size']} m", f"{formula:.0f}", f"{cusp:.1f}", f"{measured:.1f}", now,
                 "holds" if TREBUCHET_MAX_METRES < formula else "violated", fixed,
                 "holds" if FIX_TREBUCHET_MAX_METRES < formula else "violated", "holds" if fixed == 0 else "violated"])
  print(Table(["Sections a side", "Castle", "Formula: reach + keep to curtain (m)",
               "Closed form: nearest meeting of two reaches (m)", "Measured: nearest stand outside reach (m)",
               f"Covering m² at {TREBUCHET_MAX_METRES:.0f} m", f"Guard as written at {TREBUCHET_MAX_METRES:.0f} m",
               f"Covering m² at {FIX_TREBUCHET_MAX_METRES:.0f} m",
               f"Guard as written at {FIX_TREBUCHET_MAX_METRES:.0f} m",
               f"Measured guard at {FIX_TREBUCHET_MAX_METRES:.0f} m"], rows))
  siege = results[SIDE_SECTIONS]
  castle, stands = _variants[SIDE_SECTIONS]
  print(f"The Siege castle ({SIDE_SECTIONS} a side): no stand covers the great tower at "
        f"{FIX_TREBUCHET_MAX_METRES:.0f} m, {siege[1] - FIX_TREBUCHET_MAX_METRES:.1f} m inside the measured bound.")
  insufficient = [(r, CoveringArea(stands, r)) for r in range(math.ceil(siege[1]), math.ceil(siege[0]))]
  insufficient = [(r, area) for r, area in insufficient if area > 0]
  if insufficient:
    (first, firstArea), (last, lastArea) = insufficient[0], insufficient[-1]
    print(f"The formula is not sufficient: on the Siege castle it holds at {first}–{last} m, where "
          f"{firstArea}–{lastArea} m² of stands cover the great tower.")
  large = [sides for sides, r in results.items() if TREBUCHET_MAX_METRES < r[0] and r[3] > 0]
  print("At 70 m it holds on castles of " + ", ".join(str(s) for s in large) + " sections a side, which leave stands "
        "in range.")
  worst = [(sides, r[4]) for sides, r in results.items() if r[4] > 0]
  if worst:
    print("At the fixed range, the measured guard still fails on castles of "
          + ", ".join(f"{s} a side ({a} m²)" for s, a in worst) + ": a castle the Skirmish AI builds")
    print("must add a tower to a long side, or keep its keep further in.")
  sufficient = all(r[2] <= r[1] + 0.5 for r in results.values())
  print(f"The closed form is no larger than the measured bound, within the 1 m grid, on every castle here: "
        f"{'yes' if sufficient else 'no'}.")
  farthest = max(fact["standoff"] for fact in _facts.values())
  print(f"Every section still has a safe stand within {farthest:.0f} m of its face, inside the fixed range.")
  print()
  return siege[4] == 0 and siege[0] > FIX_TREBUCHET_MAX_METRES


def PrintFixGate(_castle, _rate):
  """Fix (b): the gate at the inner end of T5's passage."""
  t5 = _castle["templates"]["T5"]["voxels"]
  recessed = RecessedGate(t5)
  gate = [voxel for voxel, (_, part) in t5.items() if part == "1"]
  front = max(z for _, _, z in gate)
  print("== Proposed fix (b): T5's gate at the inner end of its passage ==")
  print(f"T5's gate is row z = {front} of 0–{front}, its front face, so nothing stands between it and a lobbed shot. "
        "The fix moves its")
  print(f"{len(gate)} timber voxels to row z = 0 and leaves the 3 m x 4 m passage open in front of it, under 5 m of "
        "the gatehouse's stone.")
  print(f"The template keeps {_castle['templates']['T5']['stone']} m³ of stone and "
        f"{_castle['templates']['T5']['timber']} m³ of timber. Shots come along the passage, the attacker's best line.")
  rows = []
  for degrees in LOB_DEGREES:
    cells = []
    for voxels in (t5, recessed):
      result = GateByLob(voxels, degrees, _rate)
      if result is None:
        cells.append("no shot reaches it")
      elif result["broken"]:
        cells.append(f"broken in {result['shots']} shots, {result['shots'] * RELOAD_SECONDS:.0f} s")
      else:
        shots = f"{result['shots']} shots take" if result["shots"] > 1 else "1 shot takes"
        cells.append(f"stands: {shots} {result['timber']} voxels, and no later shot reaches it")
    rows.append([degrees] + cells)
  print(Table(["Descent (degrees)", "T5 as drawn", "T5 with the gate at the inner end"], rows))
  smaller = []
  for crater in (RAM_TIMBER_CRATER, 3, 2):
    result = GateByRam(_castle, crater, RELOAD_SECONDS, _rate)
    smaller.append(f"{crater} voxels, {result[0]} shots ({result[1]:.0f} s)" if result else f"{crater} voxels, never")
  print("Tuning the crater alone does not do it. If the trebuchet's very large crater in timber were no larger than")
  print("the ram's large one, each shot taking that many gate voxels, the gate as drawn would break at:")
  print(f"  {'; '.join(smaller)}.")
  steepest = SteepestReach(recessed)
  print(f"The steepest descent at which any shot reaches the recessed gate: {steepest:.1f} degrees.")
  notched = GateByLob(recessed, DESCENT_DEGREES, _rate, _notch=True)
  plain, repaired = StoneShots(notched["notch"], _rate)
  print(f"At the Proposed {DESCENT_DEGREES:.0f} degrees the trebuchet must first cut back the arch: "
        f"{notched['notch']} m³ of stone, about {plain:.1f} shots at the section rate")
  print(f"({repaired:.1f} with repair); then {notched['shots']} shots break the gate. Expected: "
        f"{(plain + notched['shots']) * RELOAD_SECONDS:.0f} s unrepaired and "
        f"{(repaired + notched['shots']) * RELOAD_SECONDS:.0f} s with repair,")
  section0, sectionR, _, _ = Bombard(RELOAD_SECONDS, "one", _rate)
  print(f"against {section0:.0f} s and {sectionR:.0f} s for a section. A ram must now enter the passage to reach "
        "the gate, where the gatehouse's")
  print("passage attack reaches it, as GameDescription.md (Buildings) describes.")
  print()
  return (steepest < DESCENT_DEGREES, notched, (plain + notched["shots"]) * RELOAD_SECONDS,
          (repaired + notched["shots"]) * RELOAD_SECONDS)


def PrintFixTargets(_castle, _mechanics):
  """Fix (c): balance targets for undermining and for a ram at a wall's base, and the tuning that meets them."""
  ram = _mechanics["ram"]
  low, high = BREACH_TARGET_SECONDS * (1 - ABOUT), BREACH_TARGET_SECONDS * (1 + ABOUT)
  print("== Proposed fix (c): targets for undermining and for a ram at a wall's base ==")
  print(f"Target: each opens a standard section in about {BREACH_TARGET_SECONDS:.0f} s ({low:.0f}–{high:.0f} s), "
        "unopposed and without repair, like one")
  print(f"trebuchet; repair at most doubles that, at every repair rate from {REPAIR_RATES[0]:.2f} to "
        f"{REPAIR_RATES[-1]:.2f} m³/s.")
  print(f"The ram takes {ram['blows']} voxels from the bottom metre of {_castle['weak']['id']} before the stone above "
        f"falls and a route opens, after a {ram['warning']:.0f} s warning.")
  rows = []
  ramPass = {}
  ramOpen = {}
  for cadence in (5, 8, 10, 12, 13, 14, 15, 16, 17, 20):
    plain = RamOnStone(ram, cadence, REPAIR_RATES[0])[0]
    repaired = [RamOnStone(ram, cadence, rate)[1] for rate in REPAIR_RATES]
    worst = max(repaired) / plain
    ramPass[cadence] = About(plain) and worst <= REPAIR_BAND[1]
    ramOpen[cadence] = plain
    rows.append([cadence, f"{plain:.0f}"] + [Seconds(t) for t in repaired]
                + ["never opens" if worst == math.inf else f"{worst:.2f}", "yes" if ramPass[cadence] else "no"])
  repairColumns = [f"With repair at {rate:.2f} m³/s (s)" for rate in REPAIR_RATES]
  print(Table(["Ram blow every (s)", "Open, unrepaired (s)"] + repairColumns + ["Worst x", "Meets the target"], rows))
  rows = []
  sapPass = {}
  sapOpen = {}
  for depth in range(1, 7):
    result = Undermine(_castle, depth)
    sapPass[depth] = About(result["seconds"])
    sapOpen[depth] = result["seconds"]
    rows.append([depth, result["dig"], f"{result['digSeconds']:.0f}", f"{result['warning']:.0f}",
                 Seconds(result["seconds"]), result["fallen"], result["foundationStone"],
                 "yes" if sapPass[depth] else "no"])
  print(Table(["Foundation on deep earth (m)", "Earth the sappers dig (m³)", "Digging (s)", "Warning (s)", "Open (s)",
               "Stone that falls (m³)", "Foundation stone a section (m³)", "Meets the target"], rows))
  base = Undermine(_castle, FOUNDATION_METRES)
  rate = base["dig"] / (SQUAD_ON_FOOT * SAPPER_FACTOR * (BREACH_TARGET_SECONDS - base["warning"]))
  print(f"Keeping the {FOUNDATION_METRES} m foundation instead, a laborer would have to dig earth at {rate:.3f} m³/s, "
        f"{rate / LABORER_EARTH_M3_PER_SECOND:.0%} of the")
  print("Proposed 0.1 m³/s that the tempo's quarrying already needs, against \"Earth: dug quickly\"")
  print("(GameDescription.md, Materials).")
  cadences = [c for c, ok in ramPass.items() if ok]
  depths = [d for d, ok in sapPass.items() if ok]
  print(f"Ram cadences that meet the target: {', '.join(f'{c} s' for c in cadences) or 'none'}. Foundations that meet "
        f"it: {', '.join(f'{d} m' for d in depths) or 'none'}.")
  nearestCadence = min(cadences, key=lambda c: abs(ramOpen[c] - BREACH_TARGET_SECONDS), default=None)
  nearestDepth = min(depths, key=lambda d: abs(sapOpen[d] - BREACH_TARGET_SECONDS), default=None)
  print(f"The passing values nearest {BREACH_TARGET_SECONDS:.0f} s: a blow every {nearestCadence} s and a "
        f"{nearestDepth} m foundation. The smallest: {cadences[0]} s and {depths[0]} m.")
  fixedRam = RamOnStone(ram, FIX_RAM_BLOW_SECONDS, REPAIR_RATES[1])
  fixedSappers = Undermine(_castle, FIX_DEEP_EARTH_FOUNDATION_METRES)
  print(f"Proposed: a blow every {FIX_RAM_BLOW_SECONDS:.0f} s ({fixedRam[0]:.0f} s, {fixedRam[1]:.0f} s with repair) "
        f"and a {FIX_DEEP_EARTH_FOUNDATION_METRES} m foundation on deep earth ({fixedSappers['seconds']:.0f} s).")
  print()
  passed = ramPass.get(int(FIX_RAM_BLOW_SECONDS), False) and sapPass.get(FIX_DEEP_EARTH_FOUNDATION_METRES, False)
  return passed, fixedRam, fixedSappers


def PrintFixPrice(_mechanics, _rate):
  """Fix (d): the shot price that makes a breach cost the attacker about half the defender's rebuild."""
  treb = _mechanics["trebuchet"]
  rebuild = treb["removed"] + treb["fallen"]
  rubble = RUBBLE_STONE_SHARE * treb["fallen"]
  _, _, shotsR, repairedR = Bombard(RELOAD_SECONDS, "one", _rate)
  shots0 = ShotsToOpen(RELOAD_SECONDS)
  print("== Proposed fix (d): the shot price ==")
  print(f"A breach is {shots0} shots unrepaired and {shotsR:.1f} with repair. The defender rebuilds {rebuild} m³ "
        f"({treb['removed']} m³ of crater and {treb['fallen']} m³ that fell),")
  print(f"and repairs {repairedR:.1f} m³ under fire. The {treb['fallen']} m³ that fell is stone rubble, which clears "
        f"for {rubble:.1f} m³ of stone (net columns).")
  rows = []
  passed = {}
  for price in (SHOT_STONE_M3, 3.0, 2.0, FIX_SHOT_STONE_M3, 1.0):
    ratios = [
      shots0 * price / rebuild,
      shotsR * price / (rebuild + repairedR),
      shots0 * price / (rebuild - rubble),
      shotsR * price / (rebuild + repairedR - rubble),
    ]
    passed[price] = all(SHOT_PRICE_BAND[0] <= r <= SHOT_PRICE_BAND[1] for r in ratios[:2])
    rows.append([price, f"{shots0 * price:.1f}", f"{shotsR * price:.1f}"] + [f"{r:.2f}" for r in ratios]
                + ["yes" if passed[price] else "no"])
  print(Table(["Shot (m³)", "Attacker, unrepaired (m³)", "Attacker, repaired (m³)", "Ratio", "Ratio, repaired",
               "Ratio, net of rubble", "Ratio, net and repaired", f"About {SHOT_PRICE_TARGET} (first two)?"], rows))
  exact = SHOT_PRICE_TARGET * rebuild / shots0
  print(f"Exactly {SHOT_PRICE_TARGET} unrepaired needs {exact:.2f} m³ a shot; exactly {SHOT_PRICE_TARGET} with repair "
        f"needs {SHOT_PRICE_TARGET * (rebuild + repairedR) / shotsR:.2f} m³.")
  print()
  return passed[FIX_SHOT_STONE_M3]


def Main():
  castle = BuildCastle()
  rate = round(RateFor(REPAIR_MULTIPLIER, RELOAD_SECONDS), 3)
  print("SIEGE CASTLE ROUTE TABLE · M0 paper test for review findings F-01, F-02, F-03")
  print("Revised on the owner's decisions of 2026-10-31, with Proposed fixes (a) to (d)")
  print()
  PrintInputs(castle, rate)
  PrintPlan(castle)
  stands = ScanStands(castle)
  variants = {SIDE_SECTIONS: (castle, stands)}
  for sides in range(SIDE_SECTIONS + 1, 13):
    other = BuildCastle(sides)
    variants[sides] = (other, ScanStands(other))
  danger = PrintRanges(castle, stands, variants)
  nearest, facts, _ = PrintSections(castle, stands)
  mechanics, _ = PrintMechanics(castle, rate)
  routes, _ = BuildRoutes(castle, rate, mechanics, nearest, facts, False)
  dominant = PrintRoutes("Route table, as the documents stand", routes, rate, False)
  PrintStorm(castle, mechanics, nearest, facts)
  one, whole, step, offset = PrintRepair(rate)
  fixA = PrintFixRange(variants, facts)
  fixB, _, gateOpen, gateRepaired = PrintFixGate(castle, rate)
  fixC, fixedRam, fixedSappers = PrintFixTargets(castle, mechanics)
  fixD = PrintFixPrice(mechanics, rate)
  fixedRoutes, _ = BuildRoutes(castle, rate, mechanics, nearest, facts, True)
  fixedDominant = PrintRoutes("Route table with the Proposed fixes (a) to (d)", fixedRoutes, rate, True)

  print("== Verdict ==")
  print("As the documents stand, with the owner's decisions of 2026-10-31:")
  verdict = "PASS" if not dominant else "FAIL (" + ", ".join(dominant) + ")"
  print(f"1. No breach route wins every column: {verdict}.")
  fastest = sorted(routes, key=lambda r: r["openRepaired"])[:3]
  print("   Fastest: " + ", ".join(f"{r['name']} {Seconds(r['openRepaired'])} s" for r in fastest) + ".")
  print(f"   Dominated: {', '.join(r['name'] for r in routes if r['dominatedBy']) or 'none'}.")
  if danger:
    print(f"2. Engine circles from outside tower range over the great tower: {len(danger)} m² of stands cover it.")
    print("   The win rule needs the attacker's soldiers to hold the keep's foot for two minutes, so shelling")
    print("   the great tower cannot win alone and a breach or the gate is still needed: harmless under the")
    print("   storming rule, PASS on the second clause. The placeholder 70 m range violates the Decided guard.")
  else:
    print("2. No engine circle from outside tower range covers the great tower: PASS.")
  passed = REPAIR_BAND[0] <= one <= REPAIR_BAND[1] and REPAIR_BAND[0] <= whole <= REPAIR_BAND[1]
  print(f"3. With one engine at the Proposed {RELOAD_SECONDS:.0f} s reload, repair adds {one:.2f}x "
        f"({whole:.2f}x in whole shots): {Mark(passed)}.")
  print(f"   Two engines at {RELOAD_SECONDS:.0f} s: x{step:.2f} in step, x{offset:.2f} offset; the lock intends this.")
  print("The Proposed fixes:")
  print(f"(a) Trebuchet range {FIX_TREBUCHET_MAX_METRES:.0f} m and the guard measured: {Mark(fixA)} on the Siege "
        "castle.")
  print(f"(b) T5's gate at the inner end of its passage: {Mark(fixB)}; the gate takes {gateOpen:.0f} s by trebuchet "
        f"({gateRepaired:.0f} s with repair).")
  print(f"(c) A ram blow every {FIX_RAM_BLOW_SECONDS:.0f} s, a {FIX_DEEP_EARTH_FOUNDATION_METRES} m foundation on deep "
        f"earth: {Mark(fixC)}; {fixedRam[0]:.0f} s and {fixedSappers['seconds']:.0f} s.")
  print(f"(d) A shot at {FIX_SHOT_STONE_M3} m³: {Mark(fixD)}.")
  verdict = "PASS" if not fixedDominant else "FAIL (" + ", ".join(fixedDominant) + ")"
  print(f"With all four: 1. no route wins every column: {verdict}. 2. no stand outside every reach covers the great")
  print(f"tower: {Mark(fixA)}. 3. is unchanged by them.")
  return 0


if __name__ == "__main__":
  sys.exit(Main())
