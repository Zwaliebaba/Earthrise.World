"""Hold rule test: does where the player quarries move where the Hold waves breach?

The paper test for design review finding F-45, item 2 of "Cheapest tests" in
Design/Review/GameDescription-2026-10-31.md, run in M0 (Design/MvpPlan.md). Pass: under the revised wave rule,
the breach moves with quarry placement in at least two of three layouts.

The script builds three Hold layouts as plans of 1 m columns and applies to each the digs a player might choose.
It then scores every stretch of curtain on the four reasons GameDesign.md (Stages, Hold) gives a wave:
- how close the section is to opening;
- whether it stands on earth with no ditch in front;
- how far it is from the towers;
- whether it is a gate.
Finally it picks among the best few with a seeded draw.

Every value the design leaves open is a constant or a Rules field below. Design/Review/PaperTests/HoldRuleTest.md
marks each one Proposed and gives its reason.

Standard library only. Scores are Fractions and distances are integers, so a run is exact and repeats on every
machine.

Usage:
  python Tools/PaperTests/HoldRuleTest.py              plans at 2 m per character
  python Tools/PaperTests/HoldRuleTest.py --scale 1    plans at 1 m per character
"""

import argparse
import sys
from collections import deque
from dataclasses import dataclass, field, replace
from fractions import Fraction
from math import isqrt

# The curtain's prices, from Design/MvpAssets.md (Decided).
SECTION_STONE_M3 = 96
TOWER_STONE_M3 = 479
GATEHOUSE_STONE_M3 = 585

# Ground and digs. All Proposed except the ditch profile.
OUTCROP_FRINGE_METERS = 4  # building stone dips 1 m per metre under the soil this far beyond an outcrop's core
OUTCROP_STONE_DEPTH_METERS = 10  # how far down an outcrop's building stone goes
QUARRY_DEPTH_METERS = 8  # how deep every quarry in the test is dug
DITCH_DEPTHS_METERS = (1, 2, 2, 1)  # Decided: the standard ditch, 2 m deep with 1:1 sides, stepped on the 1 m grid
RAMPART_HEIGHTS_METERS = (1, 2, 2, 1)  # the ditch's own spoil, 6 m^3 per metre, on the side towards the keep
RAMPART_BERM_METERS = 1  # level ground between the rampart and the ditch, so that neither slumps
MAX_BERM_METERS = 8  # the widest berm the near quarries try
SEEDS = tuple(range(1, 9))  # the wave seeds printed for each placement

SIDES = {"N": (0, 1), "S": (0, -1), "E": (1, 0), "W": (-1, 0)}
NEIGHBORS = ((1, 0), (-1, 0), (0, 1), (0, -1))
UNREACHED = 10**6


@dataclass(frozen=True)
class Rules:
  """The wave rule's open values. The defaults are the Proposed set; the sensitivity runs change one at a time."""

  label: str = "Proposed"
  weightOpen: Fraction = Fraction(1)
  weightEarth: Fraction = Fraction(1)
  weightTowers: Fraction = Fraction(1)
  weightGate: Fraction = Fraction(1)
  topFew: int = 3  # N, the size of "the best few"
  stoneSpanMeters: int = 4  # the smallest span at which T2's 8 m stone floors stand
  ditchBandMeters: int = 10  # how far in front of the face a cut counts as "a ditch in front"
  ditchDepthMeters: int = 2  # how deep a cut must be to count, the standard ditch's depth
  ditchShare: Fraction = Fraction(1, 2)  # how much of the section's length the cut must cover
  quarryIsDitch: bool = True  # any cut in the band counts, not only a ditch designation
  digsSlump: bool = True  # a dig's earth faces slump at 45 degrees into it and past its rim
  towerScale: str = "max"  # "max": distance / farthest; "range": (distance - nearest) / (farthest - nearest)
  moveShare: Fraction = Fraction(1, 3)  # the share of waves that must land elsewhere for the breach to have moved


PROPOSED = Rules()


@dataclass(frozen=True)
class Rect:
  """A rectangle of 1 m columns, both corners inclusive. x runs east, y runs north."""

  x0: int
  y0: int
  x1: int
  y1: int

  def Cells(self):
    return {(x, y) for x in range(self.x0, self.x1 + 1) for y in range(self.y0, self.y1 + 1)}

  def Grown(self, _meters):
    return Rect(self.x0 - _meters, self.y0 - _meters, self.x1 + _meters, self.y1 + _meters)

  def CenterDoubled(self):
    """The centre in half metres, so that it stays an integer."""
    return (self.x0 + self.x1 + 1, self.y0 + self.y1 + 1)

  def Chebyshev(self, _x, _y):
    return max(self.x0 - _x, _x - self.x1, self.y0 - _y, _y - self.y1, 0)


@dataclass
class Piece:
  name: str
  kind: str  # "section", "gate", "tower", "keep" or "hall"
  rect: Rect
  cells: frozenset
  outward: str = ""  # the side a stretch of curtain faces: "N", "S", "E" or "W"
  center: tuple = (0, 0)  # in half metres; for the keep, its great tower


def BuildCastle():
  """The castle every layout shares, in plan coordinates with the SW tower's outer corner at the origin."""
  pieces = []

  def Add(_name, _kind, _rect, _outward="", _cells=None, _centerRect=None):
    cells = frozenset(_cells if _cells is not None else _rect.Cells())
    pieces.append(Piece(_name, _kind, _rect, cells, _outward, (_centerRect or _rect).CenterDoubled()))

  Add("SW", "tower", Rect(0, 0, 6, 6))
  Add("SE", "tower", Rect(32, 0, 38, 6))
  Add("NW", "tower", Rect(0, 31, 6, 37))
  Add("NE", "tower", Rect(31, 31, 37, 37))
  Add("S1", "section", Rect(7, 0, 14, 1), "S")
  gate = Rect(15, 0, 23, 6)
  Add("G", "gate", gate, "S", gate.Cells() - Rect(18, 1, 20, 6).Cells())  # T5: two piers, the gate across the front
  Add("S2", "section", Rect(24, 0, 31, 1), "S")
  for index, x0 in enumerate((7, 15, 23)):
    Add(f"N{index + 1}", "section", Rect(x0, 36, x0 + 7, 37), "N")
  for index, y0 in enumerate((7, 15, 23)):
    Add(f"W{index + 1}", "section", Rect(0, y0, 1, y0 + 7), "W")
    Add(f"E{index + 1}", "section", Rect(36, y0, 37, y0 + 7), "E")
  greatTower = Rect(8, 17, 19, 28)
  Add("Keep", "keep", Rect(8, 14, 19, 28), "", greatTower.Cells() | Rect(12, 14, 15, 16).Cells(), greatTower)
  Add("Hall", "hall", Rect(4, 7, 14, 13))
  return pieces


CASTLE = BuildCastle()
STRETCHES = [piece for piece in CASTLE if piece.kind in ("section", "gate")]
CURTAIN_KINDS = ("section", "gate", "tower")
TOWERS = [piece for piece in CASTLE if piece.kind in ("tower", "keep")]
STRETCH_ORDER = ["N1", "N2", "N3", "E1", "E2", "E3", "S2", "G", "S1", "W1", "W2", "W3"]


def CurtainStoneM3():
  sections = sum(1 for piece in CASTLE if piece.kind == "section")
  towers = sum(1 for piece in CASTLE if piece.kind == "tower")
  return sections * SECTION_STONE_M3 + towers * TOWER_STONE_M3 + GATEHOUSE_STONE_M3


def OuterFace(_piece):
  """The outer column of a stretch, and the run of positions along it."""
  side = _piece.outward
  rect = _piece.rect
  if side == "N":
    return [(x, rect.y1) for x in range(rect.x0, rect.x1 + 1)]
  if side == "S":
    return [(x, rect.y0) for x in range(rect.x0, rect.x1 + 1)]
  if side == "E":
    return [(rect.x1, y) for y in range(rect.y0, rect.y1 + 1)]
  return [(rect.x0, y) for y in range(rect.y0, rect.y1 + 1)]


@dataclass(frozen=True)
class Ground:
  """A region whose generated ground differs from the layout's default: earth depth, then rock down to bedrock."""

  rect: Rect
  earthMeters: int
  bedrockMeters: int


@dataclass(frozen=True)
class Pit:
  """A quarry: its footprint is cut straight down to its depth, or to bedrock.

  A pit with a towardFace is the near quarry. Its side facing the wall is drawn up to a berm from the wall's
  nearest column, faceCoordinate, and the berm is chosen when the placement is evaluated.
  """

  outcrop: str
  rect: Rect
  towardFace: str = ""
  faceCoordinate: int = 0
  depthMeters: int = QUARRY_DEPTH_METERS

  def Footprint(self, _bermMeters):
    rect = self.rect
    if self.towardFace == "E":
      return Rect(rect.x0, rect.y0, self.faceCoordinate - 1 - _bermMeters, rect.y1)
    if self.towardFace == "W":
      return Rect(self.faceCoordinate + 1 + _bermMeters, rect.y0, rect.x1, rect.y1)
    if self.towardFace == "N":
      return Rect(rect.x0, rect.y0, rect.x1, self.faceCoordinate - 1 - _bermMeters)
    if self.towardFace == "S":
      return Rect(rect.x0, self.faceCoordinate + 1 + _bermMeters, rect.x1, rect.y1)
    return rect


@dataclass(frozen=True)
class Ditch:
  """A standard ditch along one side of the curtain, with its rampart against the wall."""

  side: str
  start: int
  end: int


@dataclass(frozen=True)
class Placement:
  key: str
  title: str
  pits: tuple = ()
  ditches: tuple = ()

  def Chases(self):
    return any(pit.towardFace for pit in self.pits)


@dataclass
class Layout:
  key: str
  title: str
  story: tuple
  earthMeters: int
  bedrockMeters: int
  regions: tuple
  outcrops: dict
  placements: tuple
  quarryPair: tuple  # the two placements that dig the same stone in different places


def BuildLayouts():
  shelfWest = Ground(Rect(-100, -100, 6, 100), 0, 8)
  layoutA = Layout(
    "A",
    "an outcrop inside the east curtain",
    ("East and south stand on 4 m of earth over rock, with bedrock at 12 m. North and west stand on a rock",
     "shelf, with bedrock at 8 m. One outcrop lies inside the curtain, between the keep and E1-E3. The other",
     "lies 19 m west of the west wall, on the shelf."),
    4,
    12,
    (shelfWest, Ground(Rect(-100, 31, 100, 100), 0, 8)),
    {"inner": Rect(23, 13, 28, 25), "west": Rect(-30, 8, -19, 27)},
    (Placement("A1", "near: the inner outcrop, chased east towards E1-E3",
               (Pit("inner", Rect(22, 12, 32, 26), "E", 36),)),
     Placement("A2", "far: the west outcrop", (Pit("west", Rect(-31, 7, -18, 28)),)),
     Placement("A3", "far, plus a ditch and rampart in front of E1-E3",
               (Pit("west", Rect(-31, 7, -18, 28)),), (Ditch("E", 7, 30),))),
    ("A1", "A2"),
  )
  layoutB = Layout(
    "B",
    "an outcrop in front of an earth wall",
    ("The curtain stands on 3 m of earth, with bedrock at 10 m, except the west side, which stands on a rock",
     "shelf. One outcrop lies 8 m north of N1-N3; the other lies 15 m east of E1-E3."),
    3,
    10,
    (shelfWest,),
    {"north": Rect(12, 45, 25, 56), "east": Rect(52, 8, 63, 27)},
    (Placement("B1", "near: the north outcrop, chased south towards N1-N3",
               (Pit("north", Rect(11, 40, 26, 57), "S", 37),)),
     Placement("B2", "far: the east outcrop", (Pit("east", Rect(51, 7, 64, 28)),)),
     Placement("B3", "far, plus a ditch and rampart in front of N1-N3",
               (Pit("east", Rect(51, 7, 64, 28)),), (Ditch("N", 7, 30),))),
    ("B1", "B2"),
  )
  layoutC = Layout(
    "C",
    "outcrops by an earth wall and by a rock wall",
    ("East and north stand on 4 m of earth, with bedrock at 10 m. West and south, the gate side, stand on a rock",
     "shelf, with bedrock at 8 m. One outcrop lies 8 m east of E1-E3, in the earth. The other lies 5 m west",
     "of W1-W3, on the shelf."),
    4,
    10,
    (shelfWest, Ground(Rect(-100, -100, 100, 6), 0, 8)),
    {"east": Rect(45, 11, 56, 26), "west": Rect(-14, 11, -5, 26)},
    (Placement("C1", "near the earth wall: the east outcrop, chased west towards E1-E3",
               (Pit("east", Rect(41, 10, 57, 27), "W", 37),)),
     Placement("C2", "near the rock wall: the west outcrop", (Pit("west", Rect(-15, 10, -4, 27)),)),
     Placement("C3", "the west outcrop, plus a ditch and rampart in front of E1-E3",
               (Pit("west", Rect(-15, 10, -4, 27)),), (Ditch("E", 7, 30),))),
    ("C1", "C2"),
  )
  return [layoutA, layoutB, layoutC]


def DitchColumns(_ditch):
  """Each column of the ditch and its rampart, with the change of level it makes."""
  dx, dy = SIDES[_ditch.side]
  face = {"N": 37, "S": 0, "E": 37, "W": 0}[_ditch.side]
  profile = list(RAMPART_HEIGHTS_METERS) + [0] * RAMPART_BERM_METERS + [-depth for depth in DITCH_DEPTHS_METERS]
  for along in range(_ditch.start, _ditch.end + 1):
    for offset, change in enumerate(profile, start=1):
      if _ditch.side in ("N", "S"):
        yield (along, face + dy * offset), change
      else:
        yield (face + dx * offset, along), change


def PlanBounds(_layout):
  rects = [Rect(0, 0, 38, 37).Grown(12)]
  rects += [core.Grown(OUTCROP_FRINGE_METERS + 2) for core in _layout.outcrops.values()]
  for placement in _layout.placements:
    rects += [pit.Footprint(0).Grown(3) for pit in placement.pits]
  return Rect(min(r.x0 for r in rects), min(r.y0 for r in rects), max(r.x1 for r in rects), max(r.y1 for r in rects))


class World:
  """A height field of 1 m columns over a layout's plan.

  Level 0 is the untouched surface, and a column's level is the top of its ground. Earth lies from the level
  down to the rock top, rock below that down to bedrock. A column under a structure cannot rise above 0, where
  the structure's bottom row rests. There are no tunnels, so every solid column is continuous to its anchor.
  """

  def __init__(self, _layout):
    self.bounds = PlanBounds(_layout)
    self.level = {}
    self.rockTop = {}
    self.bedrock = {}
    self.earthOriginal = {}
    self.stoneTopDepth = {}  # depth of building stone's top, where an outcrop's stone lies
    self.cap = {}
    self.owner = {}
    self.footprint = set()
    self.pitCells = set()
    self.ditchCut = set()
    self.rampart = set()
    self.dugEarthM3 = 0
    self.dugRockM3 = 0
    self.stoneYieldM3 = 0
    for piece in CASTLE:
      for cell in piece.cells:
        self.owner[cell] = piece
    for x in range(self.bounds.x0, self.bounds.x1 + 1):
      for y in range(self.bounds.y0, self.bounds.y1 + 1):
        earth, bedrock = _layout.earthMeters, _layout.bedrockMeters
        for region in _layout.regions:
          if region.rect.Chebyshev(x, y) == 0:
            earth, bedrock = region.earthMeters, region.bedrockMeters
        distance = min(core.Chebyshev(x, y) for core in _layout.outcrops.values())
        earth = min(earth, distance)
        if distance <= OUTCROP_FRINGE_METERS:
          self.stoneTopDepth[(x, y)] = earth
        self.level[(x, y)] = 0
        self.rockTop[(x, y)] = -earth
        self.bedrock[(x, y)] = -bedrock
        self.earthOriginal[(x, y)] = earth
        self.cap[(x, y)] = 0 if (x, y) in self.owner else None

  def Dig(self, _rect, _depthMeters):
    for cell in sorted(_rect.Cells()):
      if cell not in self.level or cell in self.owner:
        continue
      self.footprint.add(cell)
      self.pitCells.add(cell)
      top = self.level[cell]
      target = max(-_depthMeters, self.bedrock[cell])
      if target >= top:
        continue
      rock = self.rockTop[cell]
      self.dugEarthM3 += max(0, top - max(target, rock))
      self.dugRockM3 += max(0, min(top, rock) - target)
      if cell in self.stoneTopDepth:
        stoneHigh = -self.stoneTopDepth[cell]  # one above the top voxel of stone
        stoneLow = max(-OUTCROP_STONE_DEPTH_METERS, self.bedrock[cell])
        self.stoneYieldM3 += max(0, min(top, stoneHigh) - max(target, stoneLow))
      self.level[cell] = target
      self.rockTop[cell] = min(rock, target)

  def DigDitch(self, _ditch):
    """The ditch cuts its profile through whatever it meets, as a designation digs any diggable material."""
    for cell, change in DitchColumns(_ditch):
      self.footprint.add(cell)
      if change < 0:
        self.ditchCut.add(cell)
      elif change > 0:
        self.rampart.add(cell)
      self.level[cell] += change
      self.rockTop[cell] = min(self.rockTop[cell], self.level[cell])

  def Slump(self, _digsSlump):
    """Earth slumps to 45 degrees: no earth column stands more than one voxel above its neighbor.

    Earth moves one voxel at a time to the lower neighbor, so it is conserved, and the order is fixed. Rock never
    moves. Where digs do not slump, only columns inside a dig's footprint may give earth.
    """
    level, rockTop, cap = self.level, self.rockTop, self.cap
    start = set()
    for cell in self.footprint:
      start.add(cell)
      start.update((cell[0] + dx, cell[1] + dy) for dx, dy in NEIGHBORS)
    queue = deque(sorted(cell for cell in start if cell in level))
    queued = set(queue)
    while queue:
      cell = queue.popleft()
      queued.discard(cell)
      if not _digsSlump and cell not in self.footprint:
        continue
      for dx, dy in NEIGHBORS:
        other = (cell[0] + dx, cell[1] + dy)
        if other not in level:
          continue
        moved = False
        while (level[cell] - level[other] >= 2 and level[cell] > rockTop[cell] and
               (cap[other] is None or level[other] < cap[other])):
          level[cell] -= 1
          level[other] += 1
          moved = True
        if moved:
          for touched in (cell, other):
            for ex, ey in ((0, 0),) + NEIGHBORS:
              near = (touched[0] + ex, touched[1] + ey)
              if near in level and near not in queued:
                queue.append(near)
                queued.add(near)

  def SupportDistances(self):
    """ADR-004's support distance, as it applies to a structure standing on a height field.

    A structure voxel goes down through its own column, or sideways through solid structure, until it reaches
    ground. A column whose ground still touches the structure's base is grounded, with distance 0. Every other
    footprint column counts the fewest sideways steps through the footprint to a grounded column. Structures
    that touch are one body, so the curtain's sections, towers and gatehouse support one another.
    """
    distance = {}
    queue = deque()
    for cell in sorted(self.owner):
      if self.level[cell] == 0:
        distance[cell] = 0
        queue.append(cell)
    while queue:
      cell = queue.popleft()
      for dx, dy in NEIGHBORS:
        other = (cell[0] + dx, cell[1] + dy)
        if other in self.owner and other not in distance:
          distance[other] = distance[cell] + 1
          queue.append(other)
    return {cell: distance.get(cell, UNREACHED) for cell in self.owner}


@dataclass
class StretchReading:
  name: str
  gapCells: int
  supportMeters: int
  fallen: bool
  openTerm: Fraction
  onEarth: bool
  ditchCover: Fraction
  ditchInFront: bool
  earthTerm: int
  towerTenths: int
  towerTerm: Fraction = Fraction(0)
  gateTerm: int = 0
  score: Fraction = Fraction(0)


@dataclass
class Evaluation:
  placement: Placement
  bermMeters: int
  world: World
  readings: dict
  anyFallen: bool
  probabilities: dict = field(default_factory=dict)
  sapperProbabilities: dict = field(default_factory=dict)


def TowerTenths(_piece):
  """Distance from the stretch's centre to the nearest tower's centre, in tenths of a metre, rounded down."""
  cx, cy = _piece.center
  best = min((cx - tower.center[0]) ** 2 + (cy - tower.center[1]) ** 2 for tower in TOWERS)
  return isqrt(25 * best)  # half metres squared to tenths of a metre: 10 * sqrt(best) / 2


g_worldCache = {}


def SlumpedWorld(_layout, _placement, _bermMeters, _digsSlump):
  key = (_layout.key, _placement.key, _bermMeters, _digsSlump)
  if key not in g_worldCache:
    world = World(_layout)
    for pit in _placement.pits:
      world.Dig(pit.Footprint(_bermMeters), pit.depthMeters)
    for ditch in _placement.ditches:
      world.DigDitch(ditch)
    world.Slump(_digsSlump)
    g_worldCache[key] = (world, world.SupportDistances())
  return g_worldCache[key]


def Read(_layout, _placement, _rules, _bermMeters):
  world, distances = SlumpedWorld(_layout, _placement, _bermMeters, _rules.digsSlump)
  span = _rules.stoneSpanMeters
  anyFallen = any(value > span for value in distances.values())
  readings = {}
  for piece in STRETCHES:
    support = max(distances[cell] for cell in piece.cells)
    fallen = support > span
    openTerm = Fraction(1) if fallen else Fraction(support, span + 1)
    onEarth = any(world.earthOriginal[cell] >= 1 for cell in piece.cells)
    face = OuterFace(piece)
    dx, dy = SIDES[piece.outward]
    covered = 0
    for fx, fy in face:
      band = [(fx + dx * step, fy + dy * step) for step in range(1, _rules.ditchBandMeters + 1)]
      if _rules.quarryIsDitch:
        cut = any(world.level.get(cell, 0) <= -_rules.ditchDepthMeters for cell in band)
      else:
        cut = any(cell in world.ditchCut and world.level[cell] <= -_rules.ditchDepthMeters for cell in band)
      covered += 1 if cut else 0
    cover = Fraction(covered, len(face))
    ditchInFront = cover >= _rules.ditchShare
    readings[piece.name] = StretchReading(
      piece.name,
      sum(1 for cell in piece.cells if world.level[cell] < 0),
      support if support < UNREACHED else -1,
      fallen,
      openTerm,
      onEarth,
      cover,
      ditchInFront,
      1 if onEarth and not ditchInFront else 0,
      TowerTenths(piece),
      gateTerm=1 if piece.kind == "gate" else 0,
    )
  tenths = [reading.towerTenths for reading in readings.values()]
  nearest, farthest = min(tenths), max(tenths)
  for reading in readings.values():
    if _rules.towerScale == "range":
      reading.towerTerm = Fraction(reading.towerTenths - nearest, max(1, farthest - nearest))
    else:
      reading.towerTerm = Fraction(reading.towerTenths, farthest)
    reading.score = (_rules.weightOpen * reading.openTerm + _rules.weightEarth * reading.earthTerm +
                     _rules.weightTowers * reading.towerTerm + _rules.weightGate * reading.gateTerm)
  return Evaluation(_placement, _bermMeters, world, readings, anyFallen)


def Probabilities(_scores, _topFew):
  """The exact chance of each stretch being picked: a uniform draw over the best few, ties at the cut split evenly."""
  if not _scores:
    return {}
  ordered = sorted(_scores.values(), reverse=True)
  if len(ordered) <= _topFew:
    return {name: Fraction(1, len(ordered)) for name in _scores}
  cut = ordered[_topFew - 1]
  above = [name for name, score in _scores.items() if score > cut]
  tied = [name for name, score in _scores.items() if score == cut]
  slots = _topFew - len(above)
  chances = {name: Fraction(1, _topFew) for name in above}
  for name in tied:
    chances[name] = Fraction(slots, len(tied) * _topFew)
  return chances


class SplitMix64:
  """The pinned PRNG for the draw (AGENTS.md R16: a recorded seed, never a platform generator)."""

  MASK = (1 << 64) - 1

  def __init__(self, _seed):
    self.m_state = _seed & self.MASK

  def Next(self):
    self.m_state = (self.m_state + 0x9E3779B97F4A7C15) & self.MASK
    value = self.m_state
    value = ((value ^ (value >> 30)) * 0xBF58476D1CE4E5B9) & self.MASK
    value = ((value ^ (value >> 27)) * 0x94D049BB133111EB) & self.MASK
    return value ^ (value >> 31)

  def Below(self, _bound):
    limit = ((self.MASK + 1) // _bound) * _bound
    while True:
      value = self.Next()
      if value < limit:
        return value % _bound


def Draw(_scores, _topFew, _seed):
  """One wave's pick: shuffle by seed, order by score (stable, so ties keep the shuffle), keep the best few, draw."""
  rng = SplitMix64(_seed)
  names = sorted(_scores, key=STRETCH_ORDER.index)
  for index in range(len(names) - 1, 0, -1):
    other = rng.Below(index + 1)
    names[index], names[other] = names[other], names[index]
  names.sort(key=lambda name: -_scores[name])
  pool = names[:_topFew]
  return pool[rng.Below(len(pool))] if pool else "-"


def Evaluate(_layout, _placement, _rules):
  """Score a placement. A near quarry is drawn as close to its wall as it can be without the slump dropping a piece."""
  if _placement.Chases():
    for berm in range(1, MAX_BERM_METERS + 1):
      evaluation = Read(_layout, _placement, _rules, berm)
      if not evaluation.anyFallen:
        break
  else:
    evaluation = Read(_layout, _placement, _rules, 0)
  Finish(evaluation, _rules)
  return evaluation


def Scores(_evaluation):
  return {name: reading.score for name, reading in _evaluation.readings.items()}


def SapperScores(_evaluation):
  """The sapper wave trenches beside a section standing on earth (GameDesign.md, Stages), so not the gatehouse."""
  return {name: reading.score for name, reading in _evaluation.readings.items() if reading.onEarth and name != "G"}


def Finish(_evaluation, _rules):
  _evaluation.probabilities = Probabilities(Scores(_evaluation), _rules.topFew)
  _evaluation.sapperProbabilities = Probabilities(SapperScores(_evaluation), _rules.topFew)


def Elsewhere(_first, _second):
  """The share of waves that breach somewhere else: the total variation distance between two pick distributions."""
  names = set(_first) | set(_second)
  return 1 - sum(min(_first.get(name, 0), _second.get(name, 0)) for name in names)


def Decimal(_value, _places=2):
  text = f"{float(_value):.{_places}f}"
  return text[1:] if text.startswith("0.") else text


def Share(_value):
  return "0" if _value == 0 else str(_value)


def PoolText(_probabilities, _readings):
  ordered = sorted(_probabilities, key=lambda name: (-_readings[name].score, STRETCH_ORDER.index(name)))
  return ", ".join(f"{name} {Decimal(_readings[name].score)} ({Share(_probabilities[name])})" for name in ordered)


# Plan drawing, in priority order: the first symbol found in a character's cells wins.
PLAN_PRIORITY = "X!#GTKHv^q~o:,."
PLAN_LEGEND = (
  "#  wall section    G  gatehouse    T  tower    K  keep    H  hall",
  "!  structure with a gap under it (undercut)    X  a piece that fell",
  "q  quarry pit    v  ditch    ^  rampart    ~  ground lowered by slump",
  "o  building stone at the surface    :  rock at the surface    ,  earth 1-2 m    .  earth 3 m or more",
)


def CellSymbol(_world, _cell, _distances, _span):
  piece = _world.owner.get(_cell)
  if piece is not None:
    if _distances[_cell] > _span:
      return "X"
    if _world.level[_cell] < 0:
      return "!"
    return {"section": "#", "gate": "G", "tower": "T", "keep": "K", "hall": "H"}[piece.kind]
  if _cell in _world.ditchCut:
    return "v"
  if _cell in _world.rampart:
    return "^"
  if _cell in _world.pitCells:
    return "q"
  if _world.level[_cell] < 0:
    return "~"
  if _world.earthOriginal[_cell] == 0:
    return "o" if _cell in _world.stoneTopDepth else ":"
  return "," if _world.earthOriginal[_cell] <= 2 else "."


def DrawPlan(_world, _distances, _span, _scale):
  bounds = _world.bounds
  lines = []
  for y in range(bounds.y1 - (bounds.y1 - bounds.y0 + 1) % _scale, bounds.y0 - 1, -_scale):
    row = []
    for x in range(bounds.x0, bounds.x1 + 1, _scale):
      symbols = [CellSymbol(_world, (x + i, y + j), _distances, _span)
                 for i in range(_scale) for j in range(_scale) if (x + i, y + j) in _world.level]
      row.append(min(symbols, key=PLAN_PRIORITY.index) if symbols else " ")
    lines.append("    " + "".join(row).rstrip())
  return lines


def DrawCastleKey():
  """The shared castle at 2 m per character, with each stretch's name inside the wall it labels."""
  width, height = 20, 19
  grid = [[" "] * width for _ in range(height)]
  for piece in CASTLE:
    symbol = {"section": "#", "gate": "G", "tower": "T", "keep": "K", "hall": "H"}[piece.kind]
    for x, y in piece.cells:
      grid[height - 1 - y // 2][x // 2] = symbol
  labels = {"N1": (5, 16), "N2": (9, 16), "N3": (13, 16), "S1": (5, 2), "S2": (13, 2), "W1": (1, 5), "W2": (1, 9),
            "W3": (1, 13), "E1": (16, 5), "E2": (16, 9), "E3": (16, 13)}
  for name, (column, row) in labels.items():
    grid[height - 1 - row][column:column + 2] = list(name)
  return ["    " + "".join(row).rstrip() for row in grid]


def PrintLayout(_layout, _rules, _scale):
  evaluations = {placement.key: Evaluate(_layout, placement, _rules) for placement in _layout.placements}
  print(f"=== Layout {_layout.key}: {_layout.title} ===")
  for line in _layout.story:
    print(f"  {line}")
  print()
  combined = World(_layout)
  hidden = []
  for placement in _layout.placements:
    berm = evaluations[placement.key].bermMeters
    for pit in placement.pits:
      combined.Dig(pit.Footprint(berm), pit.depthMeters)
  for placement in _layout.placements:
    for ditch in placement.ditches:
      cells = {cell for cell, _ in DitchColumns(ditch)}
      if cells & (combined.pitCells | combined.ditchCut):
        hidden.append(f"{placement.key}'s ditch is not drawn: it lies where the near quarry is. It runs along the"
                      f" {ditch.side} face, {ditch.start}-{ditch.end}: rampart 1-4 m out, berm 5 m, ditch 6-9 m.")
      else:
        combined.DigDitch(ditch)
  combined.Slump(_rules.digsSlump)
  print(f"  Plan, {_scale} m per character, north up; every placement's digs drawn at once:")
  for line in DrawPlan(combined, combined.SupportDistances(), _rules.stoneSpanMeters, _scale):
    print(line)
  for note in hidden:
    print(f"  {note}")
  print()
  generated = evaluations[_layout.placements[0].key]
  print("  Ground under each stretch (generated): earth m / bedrock m")
  cells = []
  for name in STRETCH_ORDER:
    piece = next(piece for piece in STRETCHES if piece.name == name)
    earths = sorted({generated.world.earthOriginal[cell] for cell in piece.cells})
    bedrock = -max(generated.world.bedrock[cell] for cell in piece.cells)
    kind = "earth" if generated.readings[name].onEarth else "rock"
    earthText = f"{earths[0]}" if len(earths) == 1 else f"{earths[0]}-{earths[-1]}"
    cells.append(f"{name:>3} {kind:5} {earthText:>3}/{bedrock:<2}")
  for index in range(0, len(cells), 4):
    print(("    " + "   ".join(cells[index:index + 4])).rstrip())
  print()
  for placement in _layout.placements:
    evaluation = evaluations[placement.key]
    world = evaluation.world
    print(f"  {placement.key}  {placement.title}")
    detail = (f"dug {world.dugEarthM3 + world.dugRockM3} m^3 (earth {world.dugEarthM3}, rock {world.dugRockM3}), "
              f"building stone {world.stoneYieldM3} m^3")
    if placement.Chases():
      detail = f"berm {evaluation.bermMeters} m; " + detail
    print(f"      {detail}")
  print(f"      The curtain costs {CurtainStoneM3()} m^3 of stone.")
  print()
  print("  Scores: open + earth + towers + gate = score; * marks the best few. gap: columns with a gap under them;")
  print("  sup: the widest support distance in m, against the span; ditch: a ditch in front, which zeroes earth.")
  header = "    stretch " + "".join(f"| {placement.key:<39}" for placement in _layout.placements)
  print(header.rstrip())
  for name in STRETCH_ORDER:
    row = f"    {name:<7} "
    for placement in _layout.placements:
      evaluation = evaluations[placement.key]
      reading = evaluation.readings[name]
      mark = "*" if name in evaluation.probabilities else " "
      openText = "FALL" if reading.fallen else Decimal(reading.openTerm)
      terms = f"{openText:>4} {reading.earthTerm} {Decimal(reading.towerTerm):>4} {reading.gateTerm}"
      extra = f"gap{reading.gapCells:>2} sup{max(reading.supportMeters, 0)}" if reading.gapCells else ""
      if reading.ditchInFront:
        extra = (extra + " ditch").strip()
      row += f"| {terms} = {Decimal(reading.score):>4}{mark} {extra:<17}"
    print(row.rstrip())
  print()
  print(f"  The breach: a uniform draw over the best {_rules.topFew}, chance in brackets; then the pick of waves seeded"
        f" {SEEDS[0]}-{SEEDS[-1]}")
  for placement in _layout.placements:
    evaluation = evaluations[placement.key]
    picks = " ".join(f"{Draw(Scores(evaluation), _rules.topFew, seed):<2}" for seed in SEEDS)
    sapperPicks = " ".join(f"{Draw(SapperScores(evaluation), _rules.topFew, seed):<2}" for seed in SEEDS)
    print(f"    {placement.key}  {PoolText(evaluation.probabilities, evaluation.readings)}")
    print(f"        seeds: {picks}".rstrip())
    print(f"        sapper wave (sections on earth): {PoolText(evaluation.sapperProbabilities, evaluation.readings)}")
    print(f"        sapper seeds: {sapperPicks}".rstrip())
  print()
  print("  Does the breach move? (share of waves that breach elsewhere; it moves at "
        f"{_rules.moveShare} or more)")
  keys = [placement.key for placement in _layout.placements]
  for index, first in enumerate(keys):
    for second in keys[index + 1:]:
      share = Elsewhere(evaluations[first].probabilities, evaluations[second].probabilities)
      sapperShare = Elsewhere(evaluations[first].sapperProbabilities, evaluations[second].sapperProbabilities)
      pair = " <- the quarry pair" if (first, second) == _layout.quarryPair else ""
      verdict = "moves" if share >= _rules.moveShare else "does not move"
      print(f"    {first} vs {second}: {Share(share):>4} elsewhere, {verdict:13}"
            f"  sapper wave {Share(sapperShare):>4}{pair}")
  print()
  return evaluations


def QuarryShares(_layouts, _rules):
  shares = {}
  for layout in _layouts:
    first, second = (next(p for p in layout.placements if p.key == key) for key in layout.quarryPair)
    shares[layout.key] = Elsewhere(Evaluate(layout, first, _rules).probabilities,
                                   Evaluate(layout, second, _rules).probabilities)
  return shares


def PrintBermSweep(_layouts, _rules):
  print("=== The near quarry at every berm (Proposed rules otherwise) ===")
  print("  berm m: the open term of each stretch it undercuts (FALL: the slump drops it); the best few, with ties at")
  print("  the cut; and the share of waves that breach elsewhere than under the quarry pair's other placement")
  for layout in _layouts:
    near, other = (next(p for p in layout.placements if p.key == key) for key in layout.quarryPair)
    otherEvaluation = Evaluate(layout, other, _rules)
    print(f"  {near.key}:")
    for berm in range(1, 7):
      evaluation = Read(layout, near, _rules, berm)
      Finish(evaluation, _rules)
      touched = [f"{name} {'FALL' if r.fallen else Decimal(r.openTerm)}"
                 for name, r in sorted(evaluation.readings.items(), key=lambda item: STRETCH_ORDER.index(item[0]))
                 if r.gapCells]
      share = Elsewhere(evaluation.probabilities, otherEvaluation.probabilities)
      pool = ", ".join(sorted(evaluation.probabilities, key=STRETCH_ORDER.index))
      print(f"    {berm}: {', '.join(touched) or 'no stretch undercut':<34} pool {pool:<14} elsewhere {Share(share)}")
  print()


def SensitivityRules():
  half, double = Fraction(1, 2), Fraction(2)
  return [
    PROPOSED,
    replace(PROPOSED, label="open weight x2", weightOpen=double),
    replace(PROPOSED, label="open weight x1/2", weightOpen=half),
    replace(PROPOSED, label="earth weight x2", weightEarth=double),
    replace(PROPOSED, label="earth weight x1/2", weightEarth=half),
    replace(PROPOSED, label="towers weight x2", weightTowers=double),
    replace(PROPOSED, label="towers weight x1/2", weightTowers=half),
    replace(PROPOSED, label="gate weight x2", weightGate=double),
    replace(PROPOSED, label="gate weight x1/2", weightGate=half),
    replace(PROPOSED, label="best few N = 2", topFew=2),
    replace(PROPOSED, label="best few N = 4", topFew=4),
    replace(PROPOSED, label="stone span 2 m (T2 falls)", stoneSpanMeters=2),
    replace(PROPOSED, label="stone span 6 m", stoneSpanMeters=6),
    replace(PROPOSED, label="towers term over range", towerScale="range"),
    replace(PROPOSED, label="only ditches are ditches", quarryIsDitch=False),
    replace(PROPOSED, label="digs do not slump", digsSlump=False),
    replace(PROPOSED, label="moves at 1/2 of waves", moveShare=Fraction(1, 2)),
    replace(PROPOSED, label="only ditches, open x2", quarryIsDitch=False, weightOpen=double),
  ]


def PrintSensitivity(_layouts):
  print("=== Sensitivity: one value changed at a time; share of waves elsewhere for each quarry pair ===")
  pairs = "".join(f"{layout.quarryPair[0] + ' v ' + layout.quarryPair[1]:<11}" for layout in _layouts)
  print(f"    {'rules':<28}{pairs}moved   verdict")
  for rules in SensitivityRules():
    shares = QuarryShares(_layouts, rules)
    moved = sum(1 for share in shares.values() if share >= rules.moveShare)
    verdict = "PASS" if moved >= 2 else "FAIL"
    row = "".join(f"{Share(shares[layout.key]):<11}" for layout in _layouts)
    print(f"    {rules.label:<28}{row}{moved} of 3  {verdict}")
  print()


def Main(_arguments):
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument("--scale", type=int, choices=(1, 2), default=2, help="metres per character in the plans")
  arguments = parser.parse_args(_arguments)
  layouts = BuildLayouts()
  rules = PROPOSED
  print("Hold rule test (review F-45), Proposed values:")
  print(f"  weights open {rules.weightOpen}, earth {rules.weightEarth}, towers {rules.weightTowers},"
        f" gate {rules.weightGate}; best few N = {rules.topFew}; stone span {rules.stoneSpanMeters} m")
  print(f"  a ditch in front: a cut {rules.ditchDepthMeters} m deep within {rules.ditchBandMeters} m of the face along"
        f" {rules.ditchShare} of the section; quarry pits count: {rules.quarryIsDitch}")
  print(f"  quarries {QUARRY_DEPTH_METERS} m deep; the breach moves when {rules.moveShare} of waves or more"
        " land elsewhere")
  print()
  print("  The castle every layout shares, 2 m per character (stretches are named inside their wall):")
  for line in DrawCastleKey():
    print(line)
  print()
  print("  Plan legend:")
  for line in PLAN_LEGEND:
    print(f"    {line}")
  print()
  moved = 0
  for layout in layouts:
    evaluations = PrintLayout(layout, rules, arguments.scale)
    first, second = layout.quarryPair
    if Elsewhere(evaluations[first].probabilities, evaluations[second].probabilities) >= rules.moveShare:
      moved += 1
  print(f"=== Verdict: the breach moves with quarry placement in {moved} of 3 layouts: "
        f"{'PASS' if moved >= 2 else 'FAIL'} (pass needs 2) ===")
  print()
  PrintBermSweep(layouts, rules)
  PrintSensitivity(layouts)
  return 0


if __name__ == "__main__":
  sys.exit(Main(sys.argv[1:]))
