"""The Siege castle's route table, range circles and repair model: an M0 paper test.

It answers findings F-01, F-02 and F-03 of Design/Review/GameDescription-2026-10-31.md ("Cheapest
tests", item 4). The write-up is Design/Review/PaperTests/SiegeRouteTable.md, which marks every value
chosen here as Proposed and gives its reason. The templates' sizes, volumes and voxels are read from
GameData/Server/Templates, so the castle is drawn from the real T1-T5.

The model, in one paragraph. The castle sits on a 1 m grid, north up, with the attacker's camp to the
south. A garrison reaches a ground target if the horizontal distance from its building's outer face
is within the archers' range plus one band per elevation step, and, where line of sight is asked
for, if the straight line from its platform to the target clears every other structure. Engines lob,
so they need range and nothing else. A wall section opens when the trebuchet has removed a fixed net
volume; repair restores volume at a fixed rate per section whenever the section has not been hit for
ten seconds. Collapses use ADR-004's support rule on the T3 voxels, with a Proposed stone span.

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

# ── Decided values, and the placeholders the documents give ─────────────────────────────────────
SECTION_METRES = 8  # T3, MvpAssets.md
TREBUCHET_MIN_METRES = 12.0  # Hud.md placeholder
TREBUCHET_MAX_METRES = 70.0  # Hud.md placeholder
SHOT_STONE_M3 = 6.0  # Hud.md placeholder
BREACH_TARGET_SECONDS = 180.0  # GameDescription.md, balance targets: one trebuchet, a standard section
GATE_TARGET_SECONDS = 240.0  # GameDescription.md, balance targets: a ram on a gatehouse's gate, under fire
REPAIR_LOCK_SECONDS = 10.0  # GameDescription.md, Templates
STEP_METRES = 3.0  # GameDescription.md, Combat model
MAX_STEPS = 2
SQUAD_ON_FOOT = 6
REPAIR_BAND = (1.3, 2.0)  # the review's pass mark for F-03, which MvpPlan.md M0 takes up
RELOAD_RANGE = range(6, 31)  # the plausible reloads F-03 names, in whole seconds

# ── Proposed by this test (each reason is in the write-up) ──────────────────────────────────────
ARCHER_RANGE_METRES = 30.0  # on the level: six bands of 5 m
BAND_METRES = 5.0  # one band of range
BOW_METRES = 1.0  # a garrison's shot leaves 1 m above the floor it stands on
TARGET_METRES = 1.0  # a man on the ground is hit 1 m up
SALLY_LEASH_METRES = 70.0  # the defender sallies out to this distance from its curtain
RELOAD_SECONDS = 20.0  # the trebuchet's reload: nine shots in the three-minute target
OPEN_VOLUME_M3 = 20.0  # net volume removed before a section opens: 5 m x 2 m x 2 m at its base
OPEN_HOLE = (2, 7, 2)  # that hole, in a section's own columns: x from 2 to 6, the bottom 2 m
STONE_SPAN = 2  # voxels of unsupported horizontal run that stone holds (ADR-004 decision 2)
REPAIR_MULTIPLIER = 1.5  # the repair rate is set so that one engine at RELOAD_SECONDS takes this long
RAM_BLOW_SECONDS = 5.0  # a blow every 5 s, inside the lock, as F-02 assumed
RAM_TIMBER_CRATER = 4  # voxels: the ram's "large" crater in timber
GATE_KEEPS = 0.5  # a gate works while at least half its voxels stand
LABORER_EARTH_M3_PER_SECOND = 0.1
SAPPER_FACTOR = 2.0  # Decided: sappers dig at twice a laborer's speed
TRENCH_M = (8, 2, 3)  # length along the section, width, depth
WARNING_SECONDS_PER_METRE = 1.0  # a collapse warns for 1 s per metre of the falling piece's height
SIDE_SECTIONS = 6  # sections on the east, west and north curtains
CAMP_SOUTH_METRES = 100.0  # the attacker's camp, south of the castle
SCAN_MARGIN_METRES = 130  # how far out the trebuchet's stands are searched

ERA_ORDER = {"Hall": 0, "Keep": 1, "Citadel": 2}
DECORATIONS = ("opening", "faction")
TIMBERS = ("timber", "roof")


# ── Templates ───────────────────────────────────────────────────────────────────────────────────


def ReadTemplate(_stem):
  """A template's JSON and its voxels as {(x, y, z): (material, part)}, stock left out."""
  data = json.loads((TEMPLATES / f"earthrise-{_stem}-1m.json").read_text(encoding="utf-8"))
  names = {}
  for char, entry in data["legend"].items():
    names[char] = entry["name"] if isinstance(entry, dict) else str(entry)
  parts = data.get("parts")
  voxels = {}
  for y, layer in enumerate(data["layers"]):
    for z, row in enumerate(layer):
      for x, char in enumerate(row):
        if char == ".":
          continue
        part = parts[y][z][x] if parts else "."
        if part == "3":
          continue
        voxels[(x, y, z)] = (names[char], part)
  return data, voxels


def TemplateFacts(_stem):
  data, voxels = ReadTemplate(_stem)
  width = len(data["layers"][0][0])
  depth = len(data["layers"][0])
  height = max(y for _, y, _ in voxels) + 1
  # Openings and faction colours are painted on solid voxels and cost as the material around them
  # (ADR-005), so they count as the template's main material.
  materials = [material for material, _ in voxels.values()]
  plain = [material for material in materials if material not in DECORATIONS]
  timberMain = 2 * sum(1 for material in plain if material in TIMBERS) > len(plain)
  timber = sum(1 for material in materials if material in TIMBERS or (timberMain and material in DECORATIONS))
  return {
    "id": data["id"],
    "name": data["name"],
    "size": (width, height, depth),
    "stone": len(voxels) - timber,
    "timber": timber,
    "critical": sum(1 for _, part in voxels.values() if part == "1"),
    "criticalName": data.get("criticalPart"),
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


# ── Collapses on the T3 voxels (ADR-004 decision 2) ─────────────────────────────────────────────


def WallStrip(_castle):
  """Three T3 sections end to end; the middle one is the section under test. {(x, y, z)}"""
  voxels = _castle["templates"]["T3"]["voxels"]
  return {(x + SECTION_METRES * index, y, z) for index in range(3) for (x, y, z) in voxels}


def Unsupported(_solid, _groundless, _span=STONE_SPAN):
  """The voxels whose support distance exceeds the span. A bottom voxel rests on the ground unless
  its column is in _groundless; a path moves only down or sideways, and only sideways steps count."""
  distance = {}
  queue = []
  for voxel in _solid:
    if voxel[1] == 0 and voxel[0] not in _groundless:
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
  return {voxel for voxel in _solid if distance.get(voxel, math.inf) > _span}


def PieceHeight(_piece):
  return max(y for _, y, _ in _piece) - min(y for _, y, _ in _piece) + 1 if _piece else 0


def Settle(_solid, _groundless, _span=STONE_SPAN):
  """Drops every unsupported piece, and whatever its loss leaves unsupported in turn (a cascade).
  Returns the voxels that fell and the height of the tallest piece, which sets the warning."""
  fallen = set()
  tallest = 0
  while True:
    piece = Unsupported(_solid, _groundless, _span)
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


def BreachMechanics(_castle, _span=STONE_SPAN):
  """What each breaker removes and what falls, on the section under test (the strip's middle one).
  The ram may carry its slot on into the next section; the sappers' trench is the section's length,
  because the castle's other sections stand on rock."""
  strip = WallStrip(_castle)
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

  # The sappers: the trench takes the ground from under the whole section.
  solid = set(strip)
  fallen, tallest = Settle(solid, set(columns), _span)
  length, width, depth = TRENCH_M
  trench = length * width * depth
  squadRate = SQUAD_ON_FOOT * LABORER_EARTH_M3_PER_SECOND * SAPPER_FACTOR
  results["sappers"] = {
    "fallen": len(fallen),
    "trench": trench,
    "squadRate": squadRate,
    "dig": trench / squadRate,
    "warning": WARNING_SECONDS_PER_METRE * tallest,
    "width": len({v[0] for v in fallen}),
    "route": Route(solid, columns),
  }
  return results


def SpanToStand(_template):
  """The smallest stone span at which a template stands whole on flat ground."""
  solid = set(_template["voxels"])
  return next(span for span in range(0, 13) if not Unsupported(solid, set(), span))


def SpanSensitivity(_castle, _span):
  """The three breakers again at another stone span. The ram may carry its slot on into the next
  section, and the undercut is centred on the section under test and lengthened until a route opens."""
  strip = WallStrip(_castle)
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


def GateByTrebuchet(_castle, _rate):
  """Two hits of a very large crater (a voxel and its six neighbors) on T5's gate, aimed at the
  middle of its lower and its upper half, with repair at the per-building rate between them."""
  voxels = _castle["templates"]["T5"]["voxels"]
  solid = dict(voxels)
  gate = [v for v, (_, part) in voxels.items() if part == "1"]
  keeps = math.ceil(GATE_KEEPS * len(gate))
  front = max(z for _, _, z in gate)
  middle = sorted({x for x, _, _ in gate})[1]
  timber = stone = 0
  repaired = 0.0
  for shot, (x, y, z) in enumerate([(middle, 1, front), (middle, 3, front)], 1):
    crater = ((x, y, z), (x + 1, y, z), (x - 1, y, z), (x, y + 1, z), (x, y - 1, z), (x, y, z + 1), (x, y, z - 1))
    for neighbor in crater:
      if neighbor in solid:
        material, _ = solid.pop(neighbor)
        if material == "timber":
          timber += 1
        else:
          stone += 1
    if sum(1 for v in gate if v in solid) + repaired < keeps:
      return {"shots": shot, "seconds": shot * RELOAD_SECONDS, "timber": timber, "stone": stone, "gate": len(gate),
              "keeps": keeps}
    repaired += min(_rate * max(0.0, RELOAD_SECONDS - REPAIR_LOCK_SECONDS), timber - repaired)
  return None


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


def ShotsToOpen(_reload, _fixed=None):
  """Shots one engine needs, unrepaired: the three-minute target at every reload, or a fixed count."""
  return _fixed if _fixed is not None else max(1, round(BREACH_TARGET_SECONDS / _reload))


def Bombard(_reload, _schedule, _rate, _lock=REPAIR_LOCK_SECONDS, _share=None, _fixed=None):
  """The expected breach: (seconds unrepaired, seconds repaired, shots repaired, m³ repaired).

  Landings come every gap seconds and each removes `together` craters of OPEN_VOLUME_M3 / shots.
  After each landing, repair is locked for _lock seconds and then restores _rate m³ a second until
  the next one; with _share it never restores more than that share of each landing. The section
  opens on the landing that brings the net removed volume to OPEN_VOLUME_M3. This is the review's
  working for F-03, c / (c - r), with the lock and a per-section rate. It counts landings as a real
  number, which averages over where in its last shot a section opens; one siege moves in whole
  shots."""
  shots = ShotsToOpen(_reload, _fixed)
  together, spacing = SCHEDULES[_schedule]
  gap = _reload * spacing
  hit = together * OPEN_VOLUME_M3 / shots
  repair = _rate * max(0.0, gap - _lock)
  if _share is not None:
    repair = min(repair, _share * hit)
  landings0 = OPEN_VOLUME_M3 / hit
  if repair >= hit:
    return landings0 * gap, math.inf, math.inf, math.inf
  landings = (OPEN_VOLUME_M3 - repair) / (hit - repair)
  return landings0 * gap, landings * gap, landings * together, (landings - 1) * repair


def Multiplier(_reload, _schedule, _rate, _lock=REPAIR_LOCK_SECONDS, _share=None, _fixed=None):
  unrepaired, repaired, _, _ = Bombard(_reload, _schedule, _rate, _lock, _share, _fixed)
  return repaired / unrepaired


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


def RateWindow(_reload, _schedule, _lock=REPAIR_LOCK_SECONDS, _share=None, _fixed=None):
  """The repair rates (m³/s per section) that give a multiplier inside REPAIR_BAND, or None."""
  low, high = REPAIR_BAND
  top = 20.0

  def M(_rate):
    return Multiplier(_reload, _schedule, _rate, _lock, _share, _fixed)

  if M(top) < low:
    return None
  a, b = 0.0, top
  for _ in range(50):
    mid = (a + b) / 2
    if M(mid) >= low:
      b = mid
    else:
      a = mid
  rateLow = b
  a, b = 0.0, top
  for _ in range(50):
    mid = (a + b) / 2
    if M(mid) <= high:
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


def WorksFrom(_schedules, _lock=REPAIR_LOCK_SECONDS, _share=None, _fixed=None):
  """The shortest reload R for which one rate serves every reload from R to 30 s, for all the given
  schedules at once, with that rate window; None if not even 30 s alone works."""
  windows = {}
  for reload in RELOAD_RANGE:
    for schedule in _schedules:
      windows[(reload, schedule)] = RateWindow(reload, schedule, _lock, _share, _fixed)
  best = None
  for start in reversed(RELOAD_RANGE):
    joint = Intersect([windows[(r, s)] for r in RELOAD_RANGE if r >= start for s in _schedules])
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


# ── The test ────────────────────────────────────────────────────────────────────────────────────


def PrintInputs(_castle, _rate):
  print("== Templates read from GameData/Server/Templates ==")
  rows = []
  for key, facts in _castle["templates"].items():
    width, height, depth = facts["size"]
    critical = f"{facts['criticalName']}: {facts['critical']}"
    rows.append([key, facts["name"], f"{width} x {height} x {depth}", facts["stone"], facts["timber"], critical])
  print(Table(["Id", "Template", "W x H x D (m)", "Stone m³", "Timber m³", "Critical part: voxels"], rows))
  print()
  print("== Values this test chose (Proposed) ==")
  platform = _castle["templates"]["T4"]["size"][1] - 1
  squadRate = SQUAD_ON_FOOT * LABORER_EARTH_M3_PER_SECOND * SAPPER_FACTOR
  shots = ShotsToOpen(RELOAD_SECONDS)
  length, width, depth = TRENCH_M
  proposed = [
    ("Archers' range on the level", f"{ARCHER_RANGE_METRES:.0f} m, six bands of {BAND_METRES:.0f} m"),
    ("Elevation", f"towers and gatehouse fire from {platform} m, the keep from 16 m: two steps each"),
    ("Garrisons", f"a squad of {SQUAD_ON_FOOT} archers in each tower, the gatehouse and the keep; "
                  "GH adds its passage attack"),
    ("Sally", f"men-at-arms and outriders, out to {SALLY_LEASH_METRES:.0f} m from the curtain"),
    ("Trebuchet reload", f"{RELOAD_SECONDS:.0f} s, so {shots} shots open a standard section in "
                         f"{BREACH_TARGET_SECONDS:.0f} s"),
    ("Volume to open a section", f"{OPEN_VOLUME_M3:.0f} m³ net: 5 m x 2 m through the wall at its base"),
    ("Stone span", f"{STONE_SPAN} voxels"),
    ("Repair rate", f"{_rate} m³/s a section ({_rate * 60:.1f} m³ a minute): one engine at {RELOAD_SECONDS:.0f} s "
                    f"then takes {REPAIR_MULTIPLIER}x as long"),
    ("Ram", f"a blow every {RAM_BLOW_SECONDS:.0f} s; 1 voxel of stone a blow, bottom metre only; "
            f"{RAM_TIMBER_CRATER} voxels of timber"),
    ("Very large crater (trebuchet, timber)", "a voxel and its six neighbors"),
    ("Gate threshold", f"the gate works while at least {GATE_KEEPS:.0%} of its voxels stand"),
    ("Sappers", f"a laborer digs earth at {LABORER_EARTH_M3_PER_SECOND} m³/s, so six sappers dig {squadRate:.1f} m³/s"),
    ("Trench to undermine", f"{length} m x {width} m x {depth} m along the section's foot"),
    ("Collapse warning", f"{WARNING_SECONDS_PER_METRE:.0f} s per metre of the falling piece's height"),
    ("Ground", "1 m of soil over common rock, except deep earth under the weak section"),
    ("Camp", f"{CAMP_SOUTH_METRES:.0f} m south of the castle"),
  ]
  print(Table(["Value", "Proposed"], proposed))
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


def PrintRanges(_castle, _stands):
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
  for sides in (6, 7, 8, 9, 10, 11, 12):
    other = BuildCastle(sides)
    otherStands = ScanStands(other)
    area = sum(1 for s in otherStands if s[2] > 0 and CoversGreatTower(s))
    otherOuter, _ = KeepToCurtain(other)
    guard = "holds" if TREBUCHET_MAX_METRES < reach + otherOuter else "violated"
    rows.append([sides, f"{other['size']} m", f"{otherOuter:.0f}", guard, area, len(other["sections"]) * 96])
  print(Table(["Sections a side", "Castle", "Keep to curtain (m)", "Guard", "Stands covering the great tower (m²)",
               "Curtain walls (m³)"], rows))
  print("The larger castles keep towers only at the corners and beside the gate, as this one does.")
  print()
  print("Every garrison can itself be shelled from outside every garrison's reach:")
  rows = []
  for garrison in _castle["garrisons"]:
    rect = garrison["rect"]
    candidates = [s for s in safe if RectFarthest(rect, s[0], s[1]) >= TREBUCHET_MIN_METRES
                  and RectDistance(rect, s[0], s[1]) <= TREBUCHET_MAX_METRES]
    best = min((RectDistance(rect, s[0], s[1]) for s in candidates), default=None)
    rows.append([garrison["id"], f"{len(candidates)} m²", "—" if best is None else f"{best:.1f}"])
  print(Table(["Target", "Safe stands in range", "Closest (m)"], rows))
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
    facts[section["id"]] = {"foot": foot, "mouth": mouth, "toKeep": toKeep}
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
  return nearest, facts


def PrintMechanics(_castle, _rate):
  mechanics = BreachMechanics(_castle)
  treb, ram, sappers = mechanics["trebuchet"], mechanics["ram"], mechanics["sappers"]
  print(f"== Breach mechanics on T3, from ADR-004's support rule with a stone span of {STONE_SPAN} voxels ==")
  print(f"Trebuchet: a hole of {treb['removed']} m³ ({OPEN_HOLE[1] - OPEN_HOLE[0]} m x {OPEN_HOLE[2]} m through 2 m) "
        f"at the base is a route ({'yes' if treb['route'] else 'no'}); {treb['fallen']} m³ above it then falls.")
  print(f"Ram: {ram['blows']} blows cut a slot along the bottom metre; the stone above it falls in pieces, "
        f"{ram['fallen']} m³ in all,")
  print(f"  leaving a gap {ram['width']} m wide over the rubble, the last piece after a {ram['warning']:.0f} s "
        f"warning. Route: {'yes' if ram['route'] else 'no'}.")
  print(f"Sappers: a trench of {sappers['trench']} m³ at {sappers['squadRate']:.1f} m³/s for a squad takes "
        f"{sappers['dig']:.0f} s; {sappers['fallen']} m³ falls,")
  print(f"  {sappers['width']} m of the section, after a {sappers['warning']:.0f} s warning. The ends within "
        f"{STONE_SPAN} m of sound sections stay up. Route: {'yes' if sappers['route'] else 'no'}.")
  gate = GateByTrebuchet(_castle, _rate)
  print(f"Gate by trebuchet: {gate['shots']} very large craters take {gate['timber']} of the gate's {gate['gate']} "
        f"timber voxels and {gate['stone']} m³ of stone;")
  print(f"  the gate works while {gate['keeps']} stand, so it is broken after {gate['seconds']:.0f} s, "
        "repair between the shots included.")
  fast = GateByRam(_castle, RAM_TIMBER_CRATER, RAM_BLOW_SECONDS, _rate)
  print(f"Gate by ram, checked against the craters: a large crater of {RAM_TIMBER_CRATER} voxels a blow, every "
        f"{RAM_BLOW_SECONDS:.0f} s, breaks the gate")
  print(f"  in {fast[0]} blows, {fast[1]:.0f} s, against the Decided target of {GATE_TARGET_SECONDS:.0f} s.")
  for cadence in (30.0, 60.0, 120.0):
    slow = GateByRam(_castle, RAM_TIMBER_CRATER, cadence, _rate)
    result = "repair keeps up, the gate never breaks." if slow is None else f"{slow[0]} blows, {slow[1]:.0f} s."
    print(f"  A blow every {cadence:.0f} s: {result}")
  print(f"  No cadence gives about {GATE_TARGET_SECONDS:.0f} s at {RAM_TIMBER_CRATER} voxels a blow: the target holds "
        "only if the passage attack")
  print("  keeps the ram off the gate most of the time.")
  print()
  needs = ", ".join(f"{key} {SpanToStand(_castle['templates'][key])}" for key in ("T2", "T3", "T4", "T5"))
  print(f"The stone span decides what falls. The span each template needs to stand whole: {needs}.")
  print("At each span, on T3 sections end to end:")
  rows = []
  for span in (2, 3, 4):
    result = SpanSensitivity(_castle, span)
    ram = "no route" if result["ramBlows"] is None else f"{result['ramBlows']} blows, {result['ramSeconds']:.0f} s"
    length, fell = result["undercut"] if result["undercut"] else (None, 0)
    undercut = "never" if length is None else f"{length} m ({fell} m³ falls)"
    within = "—" if length is None else ("yes" if length <= SECTION_METRES else "no: it reaches into the neighbours")
    rows.append([span, result["trebuchetFallen"], ram, undercut, within])
  print(Table(["Span (voxels)", "Falls above the trebuchet's 5 m hole (m³)", "Ram at the base, to a route",
               "Shortest undercut that opens a route", "Within one 8 m section?"], rows))
  print()
  return mechanics, gate


def PrintRoutes(_castle, _rate, _mechanics, _gate, _nearest, _facts, _title="Route table"):
  print(f"== {_title} ==")
  weak = _castle["weak"]
  treb, ram, sappers = _mechanics["trebuchet"], _mechanics["ram"], _mechanics["sappers"]
  time0, timeR, shotsR, repairedR = Bombard(RELOAD_SECONDS, "one", _rate)
  shots0 = ShotsToOpen(RELOAD_SECONDS)
  rebuild = treb["removed"] + treb["fallen"]
  routes = []

  def AddRoute(_name, _open, _openRepaired, _stone, _stoneRepaired, _defender, _note, _hurt, _era, _reasons):
    routes.append({"name": _name, "open": _open, "openRepaired": _openRepaired, "stone": _stone,
                   "stoneRepaired": _stoneRepaired, "defender": _defender, "note": _note, "hurt": _hurt, "era": _era,
                   "reasons": _reasons})

  AddRoute("Gate · ram", GATE_TARGET_SECONDS, GATE_TARGET_SECONDS, 0, 0, 0, "+7 timber", ["GH (passage)", "sally"],
           "Hall", "it is the gate; no stone; the ram's roof stops tower arrows; blows keep repair locked")
  gateStone = _gate["shots"] * SHOT_STONE_M3
  AddRoute("Gate · trebuchet", _gate["seconds"], _gate["seconds"], gateStone, gateStone, _gate["stone"],
           f"+{_gate['timber']} timber", ["sally"], "Citadel",
           "it is the gate; two shots from outside every reach; breaking the gate puts out the gatehouse's garrison")
  note = f"{repairedR:.0f} repaired + {rebuild} rebuilt"
  trebStone = (shots0 * SHOT_STONE_M3, shotsR * SHOT_STONE_M3)
  AddRoute(f"{_nearest['id']} · trebuchet (nearest)", time0, timeR, *trebStone, rebuild + repairedR, note, ["sally"],
           "Citadel", "nearest the camp; stand-off outside every reach")
  AddRoute(f"{weak['id']} · trebuchet (weak)", time0, timeR, *trebStone, rebuild + repairedR, note, ["sally"],
           "Citadel", "on earth, no ditch, far from towers; stand-off outside every reach")
  AddRoute("Other sections · trebuchet", time0, timeR, *trebStone, rebuild + repairedR, note, ["sally"], "Citadel",
           "the same in every scored column as the two above; the storm leg differs")
  undermine = sappers["dig"] + sappers["warning"] if sappers["route"] else math.inf
  if sappers["route"]:
    why = "the only section on earth; no ditch to cross; no stone; the most for the defender to rebuild"
  else:
    why = f"never opens: every voxel of an {SECTION_METRES} m undercut is within the span of a sound neighbor"
  AddRoute(f"{weak['id']} · sappers (undermining)", undermine, undermine, 0, 0, sappers["fallen"],
           "+ the ground under it", _facts[weak["id"]]["foot"] + ["sally"], "Keep", why)
  ramTime = ram["blows"] * RAM_BLOW_SECONDS + ram["warning"] if ram["route"] else math.inf
  AddRoute(f"{weak['id']} · ram at the base", ramTime, ramTime, 0, 0, ram["blows"] + ram["fallen"], "", ["sally"],
           "Hall", "no ditch, so the ram can reach the foot; arrow-proof; blows keep repair locked")

  def Score(_route):
    return {
      "Open": _route["open"],
      "Open, repaired": _route["openRepaired"],
      "Attacker stone": _route["stoneRepaired"],
      "Defender stone": -_route["defender"],
      "Breakers hurt by": len(_route["hurt"]),
      "Era": ERA_ORDER[_route["era"]],
    }

  scores = [Score(route) for route in routes]
  columns = list(scores[0].keys())
  best = {column: min(score[column] for score in scores) for column in columns}
  for route, score in zip(routes, scores):
    route["wins"] = [column for column in columns if score[column] == best[column]]
    route["dominatedBy"] = [
      other["name"] for other, otherScore in zip(routes, scores)
      if otherScore is not score
      and all(otherScore[c] <= score[c] for c in columns) and any(otherScore[c] < score[c] for c in columns)
    ]
  rows = []
  for route in routes:
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
  dominant = [route["name"] for route in routes if len(route["wins"]) == len(columns)]
  print("Dominant route: " + (", ".join(dominant) if dominant else "none"))
  for route in routes:
    print(f"  {route['name']}: wins {', '.join(route['wins']) or 'nothing'}")
  print("Not dominated by any other route: " + ", ".join(r["name"] for r in routes if not r["dominatedBy"]))
  for route in routes:
    if route["dominatedBy"]:
      print(f"Dominated: {route['name']}, by {', '.join(route['dominatedBy'])}")
  print("Times and stone with repair are expected values (see the repair model); one siege moves in whole shots.")
  print(f"Repair at {_rate} m³/s a section; one trebuchet at a {RELOAD_SECONDS:.0f} s reload; "
        f"{SHOT_STONE_M3:.0f} m³ a shot.")
  print(f"Shot price check: a trebuchet breach costs the attacker {shots0 * SHOT_STONE_M3:.0f} m³ against {rebuild} m³ "
        "to rebuild, a ratio of")
  print(f"  {shots0 * SHOT_STONE_M3 / rebuild:.2f}; the Decided target is about 0.5, which at {shots0} shots means "
        f"{0.5 * rebuild / shots0:.1f} m³ a shot.")
  print()
  return routes, dominant


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


def Summary(_label, _schedules, _lock=REPAIR_LOCK_SECONDS, _share=None, _fixed=None):
  whole = Intersect([RateWindow(r, s, _lock, _share, _fixed) for r in RELOAD_RANGE for s in _schedules])
  if whole is not None:
    return [_label, "yes", Window(whole), "6 s"]
  tail = WorksFrom(_schedules, _lock, _share, _fixed)
  if tail is None:
    return [_label, "no", "none", "no reload"]
  return [_label, "no", Window(tail[1]), f"{tail[0]} s"]


def PrintRepair(_rate):
  """F-03: does any per-section repair rate give 1.3-2x across the plausible reloads?"""
  print("== Repair model (F-03) ==")
  print(f"A section opens at {OPEN_VOLUME_M3:.0f} m³ net. The three-minute target holds at every reload, so one engine")
  print(f"needs round(180 / reload) shots and each removes {OPEN_VOLUME_M3:.0f} m³ / shots. Repair is locked for "
        f"{REPAIR_LOCK_SECONDS:.0f} s after every hit.")
  print("A multiplier is the breach time with repair over the time without, for the same engines.")
  print(f"Repair: {_rate} m³/s.")
  print()
  reloads = (6, 8, 10, 12, 15, 20, 25, 30)
  rows = []
  for reload in reloads:
    shots = ShotsToOpen(reload)
    cells = [reload, shots, f"{OPEN_VOLUME_M3 / shots:.2f}", f"{Bombard(reload, 'one', 0.0)[0]:.0f}"]
    for schedule in SCHEDULES:
      m = Multiplier(reload, schedule, _rate)
      cells.append("never" if m == math.inf else f"{m:.2f}")
    rows.append(cells)
  print(Table(["Reload (s)", "Shots", "m³ a shot", "One engine, unrepaired (s)", "x one engine", "x two in step",
               "x two offset"], rows))
  print()
  print("Repair rates (m³/s per section) that give 1.3–2x:")
  rows = []
  for reload in reloads:
    windows = [RateWindow(reload, schedule) for schedule in SCHEDULES]
    rows.append([reload] + [Window(w) for w in windows] + [Window(Intersect(windows[:2])), Window(Intersect(windows))])
  print(Table(["Reload (s)", "One engine", "Two in step", "Two offset", "One and two in step", "All three"], rows))
  print()
  header = ["Engines", "One rate for 6–30 s?", "Rate window (m³/s)", "Shortest reload a single rate serves, up to 30 s"]
  print("As decided (ten-second lock, a fixed rate per section):")
  decided = [
    Summary("One engine", ["one"]),
    Summary("Two in step", ["step"]),
    Summary("Two offset", ["offset"]),
    Summary("One, and two in step", ["one", "step"]),
    Summary("One, and two either way", ["one", "step", "offset"]),
  ]
  print(Table(header, decided))
  print()
  fixed = Summary("One engine, nine shots at every reload", ["one"], _fixed=9)
  print("Variant with the crater fixed instead (nine shots at every reload, so the unrepaired time is 9 x reload):")
  print(f"  one rate for 6–30 s: {fixed[1]}; a single rate serves from {fixed[3]}.")
  print()
  options = [
    ("Option A: no lock", 0.0, None),
    ("Option A': a 3 s lock", 3.0, None),
    ("Option B: no lock, and repair restores at most a third of each hit", 0.0, 1 / 3),
  ]
  for label, lock, share in options:
    print(f"{label}:")
    print(Table(header, [
      Summary("One engine", ["one"], lock, share),
      Summary("Two in step", ["step"], lock, share),
      Summary("Two offset", ["offset"], lock, share),
      Summary("One, and two either way", ["one", "step", "offset"], lock, share),
    ]))
    print()
  print("Option B at a repair rate of 0.5 m³/s, multipliers:")
  rows = [[reload] + [f"{Multiplier(reload, s, 0.5, 0.0, 1 / 3):.2f}" for s in SCHEDULES] for reload in (6, 10, 20, 30)]
  print(Table(["Reload (s)", "x one engine", "x two in step", "x two offset"], rows))
  print()
  return decided


def Main():
  castle = BuildCastle()
  rate = round(RateFor(REPAIR_MULTIPLIER, RELOAD_SECONDS), 3)
  print("SIEGE CASTLE ROUTE TABLE · M0 paper test for review findings F-01, F-02, F-03")
  print()
  PrintInputs(castle, rate)
  PrintPlan(castle)
  stands = ScanStands(castle)
  danger = PrintRanges(castle, stands)
  nearest, facts = PrintSections(castle, stands)
  mechanics, gate = PrintMechanics(castle, rate)
  routes, dominant = PrintRoutes(castle, rate, mechanics, gate, nearest, facts)
  wide = BreachMechanics(castle, 4)
  wideRoutes, wideDominant = PrintRoutes(castle, rate, wide, gate, nearest, facts,
                                         "Route table again with a stone span of 4, at which T2 stands")
  PrintStorm(castle, mechanics, nearest, facts)
  decided = PrintRepair(rate)

  print("== Verdict ==")
  for label, these, top in ((f"span {STONE_SPAN}", routes, dominant), ("span 4", wideRoutes, wideDominant)):
    verdict = "PASS" if not top else "FAIL (" + ", ".join(top) + ")"
    print(f"1. No breach route wins every column, at a stone {label}: {verdict}.")
    fastest = sorted(these, key=lambda r: r["openRepaired"])[:3]
    print("   Fastest: " + ", ".join(f"{r['name']} {Seconds(r['openRepaired'])} s" for r in fastest) + ".")
    print(f"   Dominated: {', '.join(r['name'] for r in these if r['dominatedBy']) or 'none'}.")
  if danger:
    print(f"2. Engine circles from outside tower range over the great tower: {len(danger)} m² of stands cover it.")
    print("   The win rule needs the attacker's soldiers to hold the keep's foot for two minutes, so shelling")
    print("   the great tower cannot win alone and a breach or the gate is still needed: harmless under the")
    print("   storming rule, PASS on the second clause. The placeholder 70 m range violates the Decided guard.")
  else:
    print("2. No engine circle from outside tower range covers the great tower: PASS.")
  passed = decided[-1][1] == "yes"
  print("3. One repair rate giving 1.3–2x for one and two engines over reloads of 6–30 s: "
        f"{'PASS' if passed else 'FAIL'}.")
  return 0


if __name__ == "__main__":
  sys.exit(Main())
