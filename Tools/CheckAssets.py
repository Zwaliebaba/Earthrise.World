"""Checks the voxel content under GameData/ (ADR-005, ADR-006, ADR-008).

It prints one line per problem, naming the file and the reason, then a summary line, and exits 1 if it found any.
A layer file is any .json under GameData/ with `layers` and a `legend`; the font's glyph file is not one. A line
model is any .json under GameData/Client/Lines/ (ADR-018).
Positions are (x, y, z) in the layer file's grid: y up, +z the front.

What it checks, and how each rule is read against the files:
1. Every legend entry is a palette index, and every character in the layers is in the legend.
2. A file's name ends -1m (a world asset, `voxelSize` 1) or -05m (a model, `voxelSize` 0.5), and its `voxelSize`
   agrees. A world asset's voxels use only `world` and `both` indices, and a model's only `models` and `both`
   (GameData/README.md, and the `grids` of Palette.json). A legend entry that no voxel uses is not a use: T2's
   legend still maps `j` to the faction colour, with no voxel left in it.
3. In a template, every decoration (behaviour `inherit`) resolves by ADR-005 decision 6, and no voxel is
   `modelOnly`. A voxel is stock if its part is 3 and structure otherwise, foundation included: decision 6 names
   two kinds. A decoration takes the most common behaviour among its face neighbours of its own kind that have
   one. A tie goes to the behaviour carried by the lowest palette index among those neighbours. The passes are
   simultaneous: a decoration resolved in one pass votes in the next with its behaviour and the index that
   decided it, so the result does not depend on the order the voxels are visited in.
4. A template's counted cost matches `costM3` in Manifest.json (ADR-005, decision 7). Stone is the volume of its
   building-stone voxels and timber of its timber voxels, decorations as their host, stock and foundation left
   out. `foundation` is the volume of the part-2 voxels, which is how the manifest reads: 0 for every template,
   and 48 for E2. A volume is voxels times voxelSize cubed. T3 comes to exactly 96 m3 (ADR-006, decision 3).
5. A template that names a critical part has voxels of part 1. Its `criticalPart` names none when it is absent
   or begins with "None", as T3's "None (sections open by the breach rule)" and T6's "None" do.
6. Every structure voxel of a template reaches the template's bottom layer through structure voxels, face to
   face (ADR-005). The bottom layer is the lowest occupied one, stock included, because the baker trims empty
   padding; so structure that starts on top of its stock does not reach it.
7. Each unit figure in GameData/Client/Models is identical to every figure in its squad illustration (ADR-005,
   decision 8). A squad's figures are its face-connected groups of voxels, which is how GameData/README.md says
   they were cut, and there are `squadOf` of them. Voxels are compared as positions and palette indices after
   translating each figure to its minimum corner, so legend characters may differ.
8. Every file Manifest.json lists exists, every layer file under GameData/ is listed, and each listed layer
   file's `sizeVoxels` and `voxelCount` match it. `sizeVoxels` is the occupied bounding box, not the file's grid:
   T7, T8, T9, S3 and every squad carry empty padding, which the baker trims.
9. Client and server stay apart (ADR-008). The client bake, `Tools/BakeContent.py --client`, runs into a
   temporary folder, and what it writes must carry no behaviour, no part or critical-part data and no threshold.
   Its text is split into words, at underscores and changes of case, and searched for that vocabulary:
   Palette.json's behaviour names, "behavior", "part", "critical" and "threshold". Data leaked under other
   names, such as behaviours as bare numbers, is not caught. The search covers every table the bake writes: the
   palette, the templates' client view, the models and the shared hash.
   One exception is made, because the client rightly holds a template's counted cost (ADR-008, decision 2), and
   timber is both a cost and a behaviour's name: the cost's two fields, the identifiers stoneM3 and timberM3, are
   removed before the search, and nothing else is. Timber under any other name is still caught.
   When a client table rightly names another material whose name is also a behaviour, such as water for the
   shallow-water rule, the exception here grows by that name in the same commit.
10. GameData/Server/Rules.json converts exactly (ADR-016, decision 4). Each value is a decimal string, and its key's
   unit says how it converts: meters to voxels, seconds to ticks and cubic metres to hundredths of one, which must come
   out whole, and speeds, accelerations and rates in cubic metres a second to Core::Fixed per tick, floored once.
   Counts, damage, percentages, centimetres and steps pass as they are, and must be whole. A key with no unit the bake knows, a value that is not a
   decimal, and a value that does not come out whole are each a problem. A span that is not a whole number of
   voxels is the case ADR-016 names.
11. Nothing under GameData/Client/ reaches the server's bake (ADR-008, decision 2). The server bake,
   `Tools/BakeContent.py --server`, runs twice: in this tree, and in a temporary copy holding only Tools/ and
   GameData/Server/. The two files must be the same bytes, so the server's data cannot have read a client file.
12. A line model (ADR-018) has an id that is an identifier, a name, a positive `drawnLengthMetres` in whole millimetres,
   its dot rule, `dotBelowPixels` and `dotWidthPixels`, each a whole number of pixels from 1, and `segments` that run
   contiguously from 0 to the drawn length, each with a `color` that is `#rrggbb` or `faction` and a `widthPixels` of at
   least 1. The bake reads line models with this check's code, so a problem here stops the bake too. Check 8 covers
   their manifest entries, and a layer file in Client/Lines/ is a problem: voxel models go in Client/Models/.
13. The font's glyph file, GameData/Client/Fonts/EarthriseMason-glyphs.json (Hud.md §9; ADR-019, decision 1), holds 131
   glyphs, no code point twice. Each glyph's rows are made of `.` and `#`, each as wide as its `w` says, at most 16 so
   that a row is one 16-bit mask, and they fit the cell below their `top`. The 15 symbols and the 29 icons of Hud.md §9
   are present, the icons marked as icons. The bake reads the font with this check's code.
14. Manifest.json's `debrisByRubble` (MvpAssets.md, *Poses, projectiles, gates and debris*): each row names a palette
   index whose behaviour is a rubble, once; models, by id, that are in GameData/Client/Models/; and a tint, if any,
   that is exactly one palette entry's colour, so that it bakes as that index. The bake reads the table with this
   check's code.

15. GameData/Server/Units.json and Structures.json (ADR-023, decision 1; ADR-024, decision 7) hold every unit kind once
   and every structure's template once, each a template GameData/Server/Templates/ holds. Every bonus names a kind, every
   navigation class and attack type is one the simulation numbers, and every value converts exactly as check 10's do,
   a value without a unit being a whole number; hit points, figures and vision are at least 1. A template that names a
   critical part gives a criticalThresholdPercent from 1 to 100, and one that names none gives none. The bake reads both
   with this check's code.
16. Every scenario in GameData/Server/Scenarios/ (ADR-026, decision 2) has an identifier for its id, three 64-bit seeds, a
   site on the map whose voxels below its level its ground boxes cover, and ground boxes and earthworks within the site.
   Its structures name templates the server holds, have names of their own and do not overlap; its players have ids
   of their own, a side and a commander ADR-027 names; every squad has its kind's figures, and every squad's cell and
   every machine's footprint is walkable on the levelled site, outside every structure, held by no other unit; every
   garrison names a structure it places with room for it (check 15's capacities); and its trial's keep is a T2, its
   nearest and weak sections T3s, and its attacker and defender its players on those sides. Each value with a unit
   converts as check 10's do. A scenario that gives mirrorOf (MvpPlan.md, M4, input 28) takes its seeds and players
   from its own source and the rest from the scenario it names, mirrored east to west about the site's centre line:
   the structures it mirrors must stand symmetric about that line, its rename table renames each structure once to a
   name of its own, each section's side letter matches the side it faces out to and each corner tower's its side of the
   keep, and its trial names T3 sections nearest and weak. The bake reads scenarios with this check's code, and writes a
   mirror whole.
Usage:
  python3 Tools/CheckAssets.py              the repository this script sits in
  python3 Tools/CheckAssets.py --root DIR   another tree, such as a mutated copy of it
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from collections import Counter
from fractions import Fraction
from pathlib import Path

from VoxFile import IsLayerFile, LegendIndex

ROOT = Path(__file__).resolve().parent.parent
FACES = ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1))

# A file-name suffix: the voxel size it means, what such an asset is called, and the palette grids it may use.
SIZES = {"-1m": (1, "world asset", ("world", "both")), "-05m": (0.5, "model", ("models", "both"))}

NO_PART = "."
CRITICAL = "1"
FOUNDATION = "2"
STOCK = "3"

# An id names a C++ array in the generated header, so it is an identifier: T1, U5, S3, X2. A pose's id ends in a
# lower-case letter, S3c, which the array's name spells in upper case, so no two ids may differ only in case.
ASSET_ID = re.compile(r"[A-Z][A-Za-z0-9]*")

DECORATION = "inherit"
MODEL_ONLY = "modelOnly"
COSTED = {"buildingStone": "stone", "timber": "timber"}
COST_KEYS = ("stone", "timber", "foundation")
T3_COST_M3 = 96

# What the client's tables must not carry (ADR-008, decision 2), as word sequences. Check 9 adds the behaviour
# names Palette.json defines.
SERVER_ONLY_WORDS = {
  "behaviour": [["behavior"], ["behaviors"], ["behaviour"], ["behaviours"]],
  "part data": [["part"], ["parts"], ["critical"]],
  "threshold": [["threshold"], ["thresholds"]],
}

# What the client's tables rightly carry under a name that shares a word with that vocabulary: a template's counted
# cost (ADR-008, decision 2), whose timber is also a behaviour. Check 9 removes these whole identifiers before it
# searches, and nothing else.
CLIENT_COST_FIELDS = ("stoneM3", "timberM3")


# Check 10: how a Rules.json key's unit converts into the simulation's (ADR-016, decision 4). The longest suffix that
# matches wins. Each is (source suffix, converted suffix, factor, whole): a whole conversion must come out a whole
# number of the converted unit, and the others are Core::Fixed raw values, floored once (ADR-013, decision 3).
TICKS_PER_SECOND = 20     # ADR-013, decision 1
VOXELS_PER_METER = 1      # the 1 m world (ADR-006)
FIXED_ONE_RAW = 1 << 16   # Core::Fixed::ONE_RAW
HUNDREDTHS = 100          # a stockpile and a price in hundredths of a cubic metre (MvpPlan-M4.md, Contracts)
RULE_CONVERSIONS = (
  ("CubicMetersPerSecond", "CubicMetersPerTick", Fraction(FIXED_ONE_RAW, TICKS_PER_SECOND), False),
  ("MetersPerSecondSquared", "MetersPerTickSquared", Fraction(FIXED_ONE_RAW, TICKS_PER_SECOND * TICKS_PER_SECOND), False),
  ("MetersPerSecond", "MetersPerTick", Fraction(FIXED_ONE_RAW, TICKS_PER_SECOND), False),
  ("SecondsPerHitPoint", "TicksPerHitPoint", Fraction(TICKS_PER_SECOND), True),
  ("Seconds", "Ticks", Fraction(TICKS_PER_SECOND), True),
  ("CubicMeters", "HundredthsCubicMeters", Fraction(HUNDREDTHS), True),
  ("Centimeters", "Centimeters", Fraction(1), True),
  ("DamagePerCubicMeter", "DamagePerCubicMeter", Fraction(1), True),
  ("DamagePerMeter", "DamagePerVoxel", Fraction(1, VOXELS_PER_METER), True),
  ("Meters", "Voxels", Fraction(VOXELS_PER_METER), True),
  ("Damage", "Damage", Fraction(1), True),
  ("Percent", "Percent", Fraction(1), True),
  ("Steps", "Steps", Fraction(1), True),
  ("Voxels", "Voxels", Fraction(1), True),
  ("Cells", "Cells", Fraction(1), True),
  ("Factor", "Factor", Fraction(1), True),
  ("PerTick", "PerTick", Fraction(1), True),
)
DECIMAL = re.compile(r"-?\d+(\.\d+)?")
INT64 = (-(1 << 63), (1 << 63) - 1)


class Report:
  def __init__(self, _root):
    self.root = _root
    self.problems = []

  def Problem(self, _path, _message):
    where = _path.relative_to(self.root).as_posix() if isinstance(_path, Path) else str(_path)
    self.problems.append(f"{where}: {_message}")


class Asset:
  """A layer file, read: its voxels as {(x, y, z): palette index}, and its parts as {(x, y, z): part}."""

  def __init__(self, _path, _data, _voxels, _parts):
    self.path = _path
    self.data = _data
    self.voxels = _voxels
    self.parts = _parts
    self.voxelSize = _data.get("voxelSize")

  def Part(self, _position):
    return self.parts.get(_position, NO_PART)

  def Kind(self, _position):
    return "stock" if self.Part(_position) == STOCK else "structure"

  def BoundingBox(self):
    if not self.voxels:
      return {"x": 0, "y": 0, "z": 0}
    return {axis: max(p[i] for p in self.voxels) - min(p[i] for p in self.voxels) + 1 for i, axis in enumerate("xyz")}


def Plural(_count, _noun):
  return f"{_count} {_noun}" + ("" if _count == 1 else "s")


def DoNot(_count):
  return "does not" if _count == 1 else "do not"


def At(_position):
  return "x={} y={} z={}".format(*_position)


def Neighbors(_position):
  x, y, z = _position
  return [(x + dx, y + dy, z + dz) for dx, dy, dz in FACES]


def Components(_positions):
  """The face-connected groups of _positions, each a set, in the order of their lowest position."""
  left = set(_positions)
  groups = []
  while left:
    start = min(left)
    left.discard(start)
    group, stack = {start}, [start]
    while stack:
      for neighbor in Neighbors(stack.pop()):
        if neighbor in left:
          left.discard(neighbor)
          group.add(neighbor)
          stack.append(neighbor)
    groups.append(group)
  return groups


def Normalized(_voxels):
  """_voxels as a set of (x, y, z, index), translated so that its minimum corner is the origin."""
  if not _voxels:
    return frozenset()
  low = [min(p[i] for p in _voxels) for i in range(3)]
  return frozenset((x - low[0], y - low[1], z - low[2], index) for (x, y, z), index in _voxels.items())


# ── Reading, and check 1 ────────────────────────────────────────────────────────────────────────


def ReadJson(_report, _path):
  try:
    return json.loads(_path.read_text(encoding="utf-8"))
  except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
    _report.Problem(_path, f"cannot be read as JSON: {error}")
    return None


def ReadPalette(_report, _root):
  """Returns ({palette index: entry}, Palette.json's behaviors). Indices are frozen, so each appears once."""
  path = _root / "GameData" / "Server" / "Palette.json"
  data = ReadJson(_report, path) or {}
  palette = {}
  for entry in data.get("materials", []):
    index = entry.get("index")
    if index in palette:
      _report.Problem(path, f"index {index} appears twice (ADR-005, decision 4)")
    palette[index] = entry
  return palette, data.get("behaviors", {})


def ReadAsset(_report, _path, _data, _palette):
  """Check 1 and the grid's shape. Returns the Asset, or None if its grid cannot be read."""
  legend = {}
  for char, entry in _data["legend"].items():
    try:
      legend[char] = LegendIndex(entry)
    except (KeyError, TypeError, ValueError):
      _report.Problem(_path, f"legend '{char}' gives no palette index")
      continue
    if legend[char] not in _palette:
      _report.Problem(_path, f"legend '{char}' is index {legend[char]}, which is not in the palette "
                             "(ADR-005, decision 4)")

  layers = _data["layers"]
  depth = len(layers[0]) if layers else 0
  width = len(layers[0][0]) if depth else 0
  if any(len(layer) != depth or any(len(row) != width for row in layer) for layer in layers):
    _report.Problem(_path, "its layers are not a rectangular grid")
    return None
  parts = _data.get("parts")
  if parts is not None and [[len(row) for row in layer] for layer in parts] != [[width] * depth] * len(layers):
    _report.Problem(_path, "its parts are not on the same grid as its layers")
    return None

  voxels, partOf, unknown = {}, {}, Counter()
  for y, layer in enumerate(layers):
    for z, row in enumerate(layer):
      for x, char in enumerate(row):
        if char == ".":
          continue
        if char not in legend:
          unknown[char] += 1
          continue
        voxels[(x, y, z)] = legend[char]
        if parts is not None:
          partOf[(x, y, z)] = parts[y][z][x]
  for char, count in sorted(unknown.items()):
    _report.Problem(_path, f"{Plural(count, 'voxel')} of '{char}', which is not in the legend")
  return Asset(_path, _data, voxels, partOf)


def ReadAssets(_report, _root, _palette):
  """{path relative to GameData/: Asset} for every layer file under GameData/."""
  assets = {}
  for path in sorted((_root / "GameData").rglob("*.json")):
    data = ReadJson(_report, path)
    if data is not None and IsLayerFile(data):
      asset = ReadAsset(_report, path, data, _palette)
      if asset is not None:
        assets[path.relative_to(_root / "GameData").as_posix()] = asset
  return assets


def IsTemplate(_relative):
  return _relative.startswith("Server/Templates/")


# ── Checks 2 to 6, one file at a time ───────────────────────────────────────────────────────────


def CheckGrid(_report, _asset, _palette):
  """Check 2: the name's suffix, the voxelSize, and the palette grids the voxels use."""
  suffix = next((s for s in SIZES if _asset.path.stem.endswith(s)), None)
  if suffix is None:
    _report.Problem(_asset.path, "its name ends neither -1m nor -05m, so it states no voxel size (GameData/README.md)")
    return
  voxelSize, what, grids = SIZES[suffix]
  if _asset.voxelSize != voxelSize:
    _report.Problem(_asset.path, f"voxelSize is {_asset.voxelSize}, but the name ends {suffix} (GameData/README.md)")
  used = Counter(_asset.voxels.values())
  for index in sorted(i for i in used if i in _palette):
    grid = _palette[index].get("grid")
    if grid not in grids:
      _report.Problem(_asset.path, f"{Plural(used[index], 'voxel')} of index {index} ({_palette[index].get('name')}), "
                                   f"whose grid is {grid}; a {what} uses only {' and '.join(grids)} indices "
                                   "(GameData/README.md)")


def ResolveBehaviors(_asset, _palette):
  """Every voxel's behaviour, with decorations resolved by ADR-005 decision 6.

  Returns ({position: behaviour}, [decorations that never resolve]). A voxel whose index is not in the palette, or
  is modelOnly, has no behaviour and is no decoration's host."""
  decided, pending = {}, []
  for position, index in _asset.voxels.items():
    behavior = _palette.get(index, {}).get("behavior")
    if behavior == DECORATION:
      pending.append(position)
    elif behavior is not None and behavior != MODEL_ONLY:
      decided[position] = (behavior, index)
  while pending:
    found = {}
    for position in pending:
      votes, lowest = Counter(), {}
      for neighbor in Neighbors(position):
        if neighbor in decided and _asset.Kind(neighbor) == _asset.Kind(position):
          behavior, index = decided[neighbor]
          votes[behavior] += 1
          lowest[behavior] = min(lowest.get(behavior, index), index)
      if votes:
        most = max(votes.values())
        winner = min((b for b in votes if votes[b] == most), key=lambda b: lowest[b])
        found[position] = (winner, lowest[winner])
    if not found:
      break
    decided.update(found)
    pending = [position for position in pending if position not in found]
  return {position: behavior for position, (behavior, _) in decided.items()}, sorted(pending)


def CheckDecorations(_report, _asset, _palette):
  """Check 3. Returns every voxel's behaviour, for check 4."""
  modelOnly = Counter(i for i in _asset.voxels.values() if _palette.get(i, {}).get("behavior") == MODEL_ONLY)
  for index in sorted(modelOnly):
    name = _palette[index].get("name")
    _report.Problem(_asset.path, f"{Plural(modelOnly[index], 'voxel')} of index {index} ({name}), which is modelOnly; "
                                 "no template holds one (ADR-005, decision 6)")
  behaviors, unresolved = ResolveBehaviors(_asset, _palette)
  if unresolved:
    _report.Problem(_asset.path, f"{Plural(len(unresolved), 'decoration')} {DoNot(len(unresolved))} resolve to a host "
                                 f"of their own kind, first at {At(unresolved[0])} (ADR-005, decision 6)")
  return behaviors


def CountCost(_asset, _behaviors):
  """Check 4's count, in m3: {stone, timber, foundation}."""
  volumeM3 = _asset.voxelSize ** 3 if isinstance(_asset.voxelSize, (int, float)) else 0
  counts = Counter()
  for position in _asset.voxels:
    part = _asset.Part(position)
    if part == FOUNDATION:
      counts["foundation"] += 1
    elif part != STOCK and _behaviors.get(position) in COSTED:
      counts[COSTED[_behaviors[position]]] += 1
  return {key: counts[key] * volumeM3 for key in COST_KEYS}


def CheckCost(_report, _asset, _behaviors, _entry):
  """Check 4: the counted cost against Manifest.json, and T3 against ADR-006."""
  counted = CountCost(_asset, _behaviors)
  expected = _entry.get("costM3")
  if not isinstance(expected, dict):
    _report.Problem(_asset.path, f"Manifest.json gives {_entry.get('id')} no costM3 (ADR-005, decision 7)")
  elif any(expected.get(key) != counted[key] for key in COST_KEYS):
    count = ", ".join(f"{key} {counted[key]:g}" for key in COST_KEYS)
    said = ", ".join(f"{key} {expected.get(key)}" for key in COST_KEYS)
    _report.Problem(_asset.path, f"counts {count} m3, but Manifest.json says {said} (ADR-005, decision 7)")
  total = counted["stone"] + counted["timber"]
  if _entry.get("id") == "T3" and total != T3_COST_M3:
    _report.Problem(_asset.path, f"T3 counts {total:g} m3, not the {T3_COST_M3} m3 of an 8 x 2 x 6 m wall section "
                                 "(ADR-006, decision 3)")


def CheckCriticalPart(_report, _asset):
  """Check 5."""
  name = _asset.data.get("criticalPart")
  if not isinstance(name, str) or not name.strip() or name.split()[0] == "None":
    return
  if CRITICAL not in _asset.parts.values():
    _report.Problem(_asset.path, f"names the critical part '{name}', but no voxel is part {CRITICAL} "
                                 "(ADR-005, decision 2)")


def CheckConnected(_report, _asset):
  """Check 6: all structure reaches the bottom layer through structure, face to face."""
  if not _asset.voxels:
    return
  bottom = min(y for _, y, _ in _asset.voxels)
  structure = {p for p in _asset.voxels if _asset.Kind(p) == "structure"}
  reached = {p for p in structure if p[1] == bottom}
  stack = list(reached)
  while stack:
    for neighbor in Neighbors(stack.pop()):
      if neighbor in structure and neighbor not in reached:
        reached.add(neighbor)
        stack.append(neighbor)
  cut = sorted(structure - reached)
  if cut:
    _report.Problem(_asset.path, f"{Plural(len(cut), 'structure voxel')} {DoNot(len(cut))} reach the bottom layer "
                                 f"through structure, first at {At(cut[0])}: nothing may float or stand only on stock "
                                 "(ADR-005)")


# ── Checks 7 and 8, against the manifest ────────────────────────────────────────────────────────


def CheckFigure(_report, _root, _entry, _assets):
  """Check 7: a unit figure against every figure in its squad."""
  figure = _entry["figure"]
  squad = _assets.get(_entry.get("files", {}).get("json"))
  model = _assets.get(figure.get("json"))
  if squad is None or model is None:
    return  # check 8 reports the missing file
  groups = Components(squad.voxels)
  if len(groups) != figure.get("squadOf"):
    _report.Problem(squad.path, f"holds {Plural(len(groups), 'figure')}, but Manifest.json gives {_entry.get('id')} "
                                f"squadOf {figure.get('squadOf')} (GameData/README.md)")
  shape = Normalized(model.voxels)
  differences = {}
  for number, group in enumerate(groups, 1):
    other = Normalized({p: squad.voxels[p] for p in group})
    if other != shape:
      differences[number] = len(other ^ shape)
  if differences:
    numbers = ("figure " if len(differences) == 1 else "figures ") + ", ".join(str(n) for n in differences)
    first = next(iter(differences))
    squadName = squad.path.relative_to(_root).as_posix()
    _report.Problem(model.path, f"differs from {numbers} of {len(groups)} in {squadName}, by "
                                f"{Plural(differences[first], 'voxel')} from figure {first}; each figure is identical "
                                "to the figures in its squad (ADR-005, decision 8)")


def CheckListedShape(_report, _manifestPath, _id, _whose, _listed, _asset):
  if _listed.get("voxelCount") != len(_asset.voxels):
    _report.Problem(_manifestPath, f"{_id} gives {_whose} voxelCount {_listed.get('voxelCount')}, "
                                   f"but {_asset.path.name} holds {len(_asset.voxels)}")
  box = _asset.BoundingBox()
  if _listed.get("sizeVoxels") != box:
    _report.Problem(_manifestPath, f"{_id} gives {_whose} sizeVoxels {_listed.get('sizeVoxels')}, "
                                   f"but {_asset.path.name} occupies {box}")


def CheckManifest(_report, _root, _manifest, _assets, _lineModels):
  """Check 8. Returns {listed path: its manifest entry}, for checks 4 and 7."""
  manifestPath = _root / "GameData" / "Manifest.json"
  listed = {}
  for entry in _manifest.get("assets", []):
    assetId = entry.get("id")
    files = dict(entry.get("files", {}))
    figure = entry.get("figure")
    if isinstance(figure, dict):
      files["figure"] = figure.get("json")
    for relative in files.values():
      if not isinstance(relative, str) or not (_root / "GameData" / relative).is_file():
        _report.Problem(manifestPath, f"{assetId} lists {relative}, which does not exist")
        continue
      listed.setdefault(relative, entry)
    if files.get("json") in _assets:
      CheckListedShape(_report, manifestPath, assetId, "its", entry, _assets[files["json"]])
    if isinstance(figure, dict) and figure.get("json") in _assets:
      CheckListedShape(_report, manifestPath, assetId, "its figure's", figure, _assets[figure["json"]])
  for relative, asset in _assets.items():
    if relative not in listed:
      _report.Problem(asset.path, "is a layer file that Manifest.json does not list")
  for relative, model in _lineModels.items():
    if relative not in listed:
      _report.Problem(model.path, "is a line model that Manifest.json does not list")
  return listed


# ── Check 12, line models ───────────────────────────────────────────────────────────────────────

LINES_FOLDER = "Client/Lines"
HEX_COLOR = re.compile(r"#[0-9a-fA-F]{6}")
FACTION_COLOR = "faction"
DOT_FIELDS = ("dotBelowPixels", "dotWidthPixels")


class LineModel:
  """A line model, read (ADR-018): what the bake writes of it. Its segments are (from mm, to mm, sRGB or None for the
  side's faction colour, width in pixels, tapers), from its tail along its axis."""

  def __init__(self, _path, _data, _id, _name, _drawnLengthMillimeters, _dotBelowPixels, _dotWidthPixels, _segments):
    self.path = _path
    self.source = _path.name
    self.data = _data
    self.id = _id
    self.name = _name
    self.drawnLengthMillimeters = _drawnLengthMillimeters
    self.dotBelowPixels = _dotBelowPixels
    self.dotWidthPixels = _dotWidthPixels
    self.segments = _segments


def IsLineModel(_data):
  return isinstance(_data, dict) and "segments" in _data and "drawnLengthMetres" in _data


def Millimeters(_value):
  """A length in metres as a whole number of millimetres from 0, or None when it is not one."""
  if isinstance(_value, bool) or not isinstance(_value, (int, float)):
    return None
  exact = Fraction(str(_value)) * 1000
  return int(exact) if exact.denominator == 1 and exact >= 0 else None


def WholePixels(_value):
  return isinstance(_value, int) and not isinstance(_value, bool) and _value >= 1


def ReadLineModel(_report, _path, _data):
  """Check 12 over one file. Returns the LineModel, or None after reporting everything wrong with it."""
  before = len(_report.problems)
  assetId, name = _data.get("id"), _data.get("name")
  if not isinstance(assetId, str) or not ASSET_ID.fullmatch(assetId):
    _report.Problem(_path, f"its id {assetId!r} is not an identifier such as X2")
  if not isinstance(name, str) or not name:
    _report.Problem(_path, "has no name")
  drawn = Millimeters(_data.get("drawnLengthMetres"))
  if not drawn:
    _report.Problem(_path, f"its drawnLengthMetres {_data.get('drawnLengthMetres')!r} is not a positive whole number of "
                           "millimetres (ADR-018)")
  dots = []
  for key in DOT_FIELDS:
    value = _data.get(key)
    if not WholePixels(value):
      _report.Problem(_path, f"its {key} {value!r} is not a whole number of pixels from 1 (ADR-018)")
    dots.append(value)
  segments = _data.get("segments")
  if not isinstance(segments, list) or not segments or not all(isinstance(segment, dict) for segment in segments):
    _report.Problem(_path, "has no segments, or one that is not an object (ADR-018)")
    segments = []
  read, reached = [], 0
  for number, segment in enumerate(segments, 1):
    label = f"segment {number} ({segment.get('name')})"
    if not isinstance(segment.get("name"), str) or not segment.get("name"):
      _report.Problem(_path, f"segment {number} has no name")
    start, end = Millimeters(segment.get("fromMetres")), Millimeters(segment.get("toMetres"))
    if start is None or end is None or start >= end:
      _report.Problem(_path, f"{label} runs from {segment.get('fromMetres')!r} to {segment.get('toMetres')!r} m, which is not "
                             "a span of whole millimetres that ends after it starts (ADR-018)")
    elif start != reached:
      where = "its tail at 0" if number == 1 else f"segment {number - 1}'s end at {reached} mm"
      _report.Problem(_path, f"its segments are not contiguous from 0 to drawnLengthMetres: {label} starts at {start} mm, "
                             f"not at {where} (ADR-018)")
    if end is not None:
      reached = end
    color = segment.get("color")
    if color == FACTION_COLOR:
      srgb = None
    elif isinstance(color, str) and HEX_COLOR.fullmatch(color):
      srgb = int(color[1:], 16)
    else:
      _report.Problem(_path, f"{label} has the colour {color!r}, which is neither #rrggbb nor {FACTION_COLOR} (ADR-018)")
      srgb = 0
    width = segment.get("widthPixels")
    if not WholePixels(width):
      _report.Problem(_path, f"{label} has a widthPixels of {width!r}, which is not a whole number of pixels from 1 (ADR-018)")
    tapers = segment.get("tapers", False)
    if not isinstance(tapers, bool):
      _report.Problem(_path, f"{label} has a tapers of {tapers!r}, not true or false (ADR-018)")
    read.append((start, end, srgb, width, tapers))
  if segments and drawn and reached != drawn:
    _report.Problem(_path, f"its segments end at {reached} mm, not at its drawn length of {drawn} mm (ADR-018)")
  if len(_report.problems) > before:
    return None
  return LineModel(_path, _data, assetId, name, drawn, dots[0], dots[1], read)


def ReadLineModels(_report, _root):
  """{path relative to GameData/: LineModel} for every line model under GameData/Client/Lines/ that reads clean."""
  models = {}
  for path in sorted((_root / "GameData" / LINES_FOLDER).glob("*.json")):
    data = ReadJson(_report, path)
    if data is None:
      continue
    if IsLayerFile(data) or not IsLineModel(data):
      _report.Problem(path, f"is not a line model: {LINES_FOLDER}/ holds line models, each with segments and a "
                            "drawnLengthMetres, and voxel models go in Client/Models/ (ADR-018)")
      continue
    model = ReadLineModel(_report, path, data)
    if model is not None:
      models[path.relative_to(_root / "GameData").as_posix()] = model
  return models


# ── Check 13, the font ──────────────────────────────────────────────────────────────────────────

FONT_FILE = "Client/Fonts/EarthriseMason-glyphs.json"
FONT_GLYPH_COUNT = 131
FONT_CELL_ROWS = 10    # Hud.md §9: one row above the caps, two below the baseline
FONT_BASELINE_ROW = 7
FONT_ROW_BITS = 16
# Hud.md §9: the 15 symbols, and the 29 command icons at U+E000 to U+E01C (M4-B5 added SPLIT, MERGE, DEPLOY and WALL).
FONT_SYMBOLS = "·×²³°←→↑↓↺↻…–—’"
FONT_ICONS = range(0xE000, 0xE01D)


class Glyph:
  """A glyph, read (ADR-019, decision 1): what the bake writes of it. Its masks are its rows from its top, bit x set
  for a pixel at column x."""

  def __init__(self, _code, _width, _top, _advance, _icon, _masks):
    self.code = _code
    self.width = _width
    self.top = _top
    self.advance = _advance
    self.icon = _icon
    self.masks = _masks


def ReadFont(_report, _root):
  """Check 13. Returns the glyphs in ascending code point, or None after reporting everything wrong with the file."""
  path = _root / "GameData" / FONT_FILE
  if not path.is_file():
    _report.Problem(path, "is missing: it is the font the HUD draws with (Hud.md §9)")
    return None
  data = ReadJson(_report, path)
  if data is None:
    return None
  before = len(_report.problems)
  cell, baseline = data.get("cell"), data.get("baselineRow")
  if cell != FONT_CELL_ROWS or baseline != FONT_BASELINE_ROW:
    _report.Problem(path, f"its cell of {cell!r} rows with the baseline at {baseline!r} is not Hud.md §9's {FONT_CELL_ROWS} "
                          f"with the baseline at {FONT_BASELINE_ROW}")
    cell = FONT_CELL_ROWS
  glyphs = data.get("glyphs")
  if not isinstance(glyphs, list) or not all(isinstance(glyph, dict) for glyph in glyphs):
    _report.Problem(path, "has no list of glyphs")
    return None
  if len(glyphs) != FONT_GLYPH_COUNT:
    _report.Problem(path, f"holds {Plural(len(glyphs), 'glyph')}, not the {FONT_GLYPH_COUNT} of Hud.md §9")
  read, seen = {}, set()
  for number, glyph in enumerate(glyphs, 1):
    code = glyph.get("code")
    label = f"glyph {number} (U+{code:04X})" if isinstance(code, int) else f"glyph {number}"
    if not isinstance(code, int) or isinstance(code, bool) or not 0 <= code <= 0x10FFFF:
      _report.Problem(path, f"{label} has the code {code!r}, which is not a code point")
      continue
    if code in seen:
      _report.Problem(path, f"{label} is the second glyph for U+{code:04X}")
      continue
    seen.add(code)
    width, top, advance, rows = glyph.get("w"), glyph.get("top"), glyph.get("advance"), glyph.get("rows")
    if not WholePixels(width) or width > FONT_ROW_BITS:
      _report.Problem(path, f"{label} is {width!r} wide, not from 1 to {FONT_ROW_BITS} pixels, the widest a row's mask holds")
      continue
    if not isinstance(rows, list) or not rows or not all(isinstance(row, str) for row in rows):
      _report.Problem(path, f"{label} has no rows")
      continue
    for row in rows:
      if len(row) != width:
        _report.Problem(path, f"{label} has the row {row!r}, which is not {width} wide, as its w says")
      if set(row) - {".", "#"}:
        _report.Problem(path, f"{label} has the row {row!r}, which is not made of . and #")
    if not isinstance(top, int) or isinstance(top, bool) or top < 0 or top + len(rows) > cell:
      _report.Problem(path, f"{label}'s {Plural(len(rows), 'row')} from its top {top!r} do not fit the {cell}-row cell")
    if not isinstance(advance, int) or isinstance(advance, bool) or advance < 0:
      _report.Problem(path, f"{label} has the advance {advance!r}, not a whole number of pixels")
    icon = glyph.get("icon")
    if not isinstance(icon, bool):
      _report.Problem(path, f"{label} has an icon of {icon!r}, not true or false")
    masks = [sum(1 << x for x, char in enumerate(row) if char == "#") for row in rows]
    read[code] = Glyph(code, width, top, advance, icon is True, masks)
  missing = [symbol for symbol in FONT_SYMBOLS if ord(symbol) not in read]
  if missing:
    _report.Problem(path, f"lacks the symbol(s) {' '.join(missing)} of Hud.md §9")
  icons = [f"U+{code:04X}" for code in FONT_ICONS if code not in read or not read[code].icon]
  if icons:
    _report.Problem(path, f"lacks the command icon(s) {', '.join(icons)} of Hud.md §9, or does not mark them as icons")
  if len(_report.problems) > before:
    return None
  return [read[code] for code in sorted(read)]


# ── Check 14, the debris table ──────────────────────────────────────────────────────────────────


class DebrisChoice:
  """A row of debrisByRubble, read: the rubble's palette index, its models' ids, and its tint as a palette index, or 0."""

  def __init__(self, _rubbleIndex, _models, _tintIndex):
    self.rubbleIndex = _rubbleIndex
    self.models = _models
    self.tintIndex = _tintIndex


def ReadDebris(_report, _root, _palette, _modelIds):
  """Check 14 over Manifest.json's debrisByRubble, given the ids of the models in GameData/Client/Models/. Returns the
  rows in ascending rubble index, or None after reporting everything wrong with them."""
  path = _root / "GameData" / "Manifest.json"
  data = ReadJson(_report, path) or {}
  table = (data.get("debrisByRubble") or {}).get("table")
  if not isinstance(table, list) or not table or not all(isinstance(row, dict) for row in table):
    _report.Problem(path, "has no debrisByRubble table, which says what debris lies on each rubble")
    return None
  before = len(_report.problems)
  rubbles = {index for index, entry in _palette.items() if str(entry.get("behavior", "")).endswith("Rubble")}
  colors = Counter(str(entry.get("hex", "")).lower() for entry in _palette.values())
  read = {}
  for number, row in enumerate(table, 1):
    index = row.get("rubbleIndex")
    label = f"debrisByRubble row {number}"
    if index not in rubbles:
      _report.Problem(path, f"{label} names index {index!r}, which is not a palette index whose behaviour is a rubble")
      continue
    if index in read:
      _report.Problem(path, f"{label} names index {index} a second time")
      continue
    models = row.get("debris")
    if not isinstance(models, list) or not models or not all(isinstance(model, str) for model in models):
      _report.Problem(path, f"{label} names no models")
      continue
    for model in models:
      if model not in _modelIds:
        _report.Problem(path, f"{label} names the model {model!r}, which is not in GameData/Client/Models/")
    tint, tintIndex = row.get("tint"), 0
    if tint is not None:
      matching = [entry for entry, value in sorted(_palette.items()) if str(value.get("hex", "")).lower() == str(tint).lower()]
      if not isinstance(tint, str) or not HEX_COLOR.fullmatch(tint) or colors[tint.lower()] != 1:
        _report.Problem(path, f"{label} has the tint {tint!r}, which is not exactly one palette entry's colour")
      else:
        tintIndex = matching[0]
    read[index] = DebrisChoice(index, models, tintIndex)
  if len(_report.problems) > before:
    return None
  return [read[index] for index in sorted(read)]


# ── Check 9, the client bake ────────────────────────────────────────────────────────────────────


def Words(_text):
  """The words of _text's identifiers, in lower case: PALETTE_BEHAVIOR and paletteBehavior are both palette,
  behavior. A number, hexadecimal included, has none."""
  words = []
  for identifier in re.findall(r"\b[A-Za-z_]\w*", _text):
    for piece in identifier.split("_"):
      words += [word.lower() for word in re.findall(r"[A-Z]?[a-z]+|[A-Z]+(?![a-z])|\d+", piece)]
  return words


def Contains(_words, _phrase):
  return any(_words[i : i + len(_phrase)] == _phrase for i in range(len(_words) - len(_phrase) + 1))


def CheckClientBake(_report, _root, _behaviors):
  """Check 9: what the client bake writes carries nothing server-only. Returns how many files it wrote."""
  bake = _root / "Tools" / "BakeContent.py"
  if not bake.is_file():
    _report.Problem(bake, "the client bake is missing, so nothing shows its tables are clean (ADR-008)")
    return 0
  vocabulary = dict(SERVER_ONLY_WORDS)
  vocabulary["behaviour"] = vocabulary["behaviour"] + [Words(name) for name in sorted(_behaviors)]
  with tempfile.TemporaryDirectory() as folder:
    result = subprocess.run([sys.executable, str(bake), "--client", folder], capture_output=True, text=True)
    if result.returncode != 0:
      last = ((result.stdout + result.stderr).strip().splitlines() or ["no output"])[-1]
      _report.Problem(bake, f"the client bake failed with exit code {result.returncode}: {last}")
      return 0
    outputs = sorted(path for path in Path(folder).rglob("*") if path.is_file())
    costFields = re.compile(r"\b(?:" + "|".join(CLIENT_COST_FIELDS) + r")\b")
    for path in outputs:
      words = Words(costFields.sub(" ", path.read_text(encoding="utf-8", errors="replace")))
      for category, phrases in vocabulary.items():
        found = [" ".join(phrase) for phrase in phrases if Contains(words, phrase)]
        if found:
          _report.Problem(bake, f"the client bake wrote {path.name}, which carries server-only data: the {category} "
                                f"word(s) {', '.join(found)} (ADR-008, decision 2)")
    return len(outputs)


# ── Checks 10 and 11, the server's data ─────────────────────────────────────────────────────────


def ConvertRule(_key, _value):
  """A Rules.json entry in the simulation's unit: (converted key, integer), or (None, the reason it does not convert)."""
  if not isinstance(_value, str) or not DECIMAL.fullmatch(_value):
    return None, f"{_value!r} is not a decimal string"
  for suffix, converted, factor, whole in RULE_CONVERSIONS:
    if _key.endswith(suffix) and len(_key) > len(suffix):
      exact = Fraction(_value) * factor
      if whole and exact.denominator != 1:
        return None, f"{_value} comes to {float(exact):g} {converted[0].lower() + converted[1:]}, not a whole number"
      number = exact.numerator // exact.denominator  # floored once, toward negative infinity
      if not INT64[0] <= number <= INT64[1]:
        return None, f"{_value} does not fit in 64 bits"
      return _key[: -len(suffix)] + converted, number
  return None, "its key names no unit the bake converts (" + ", ".join(entry[0] for entry in RULE_CONVERSIONS) + ")"


def ReadRules(_report, _root):
  """Check 10: Rules.json, converted. Returns {converted key: integer}, in ascending key order."""
  path = _root / "GameData" / "Server" / "Rules.json"
  data = ReadJson(_report, path)
  if data is None:
    return {}
  values = data.get("values")
  if not isinstance(values, dict) or not values:
    _report.Problem(path, "has no values (ADR-016, decision 4)")
    return {}
  rules = {}
  for key, value in values.items():
    converted, number = ConvertRule(key, value)
    if converted is None:
      _report.Problem(path, f"{key}: {number} (ADR-016, decision 4)")
    elif converted in rules:
      _report.Problem(path, f"{key}: converts to {converted}, which another key already gives")
    else:
      rules[converted] = number
  return dict(sorted(rules.items()))


# ── Check 15, units and structures ───────────────────────────────────────────────────────────────

# Protocol::UnitKind, NavigationClass and DamageType, in their declared order, by the names Units.json gives them.
UNIT_KINDS = ("laborer", "spearman", "archer", "outrider", "manAtArms", "sapper", "ram", "trebuchet")
NAVIGATION_CLASSES = ("foot", "cavalry", "machines")
DAMAGE_TYPES = ("melee", "pierce", "crush")
# A unit's fields: (source key, converted key, whether it may be 0). Those with a unit convert as Rules.json's do (check
# 10); the rest are whole numbers.
UNIT_FIELDS = (
  ("squadFigures", "squadFigures", False),
  ("hitPoints", "hitPoints", False),
  ("armorMelee", "armorMelee", True),
  ("armorPierce", "armorPierce", True),
  ("armorCrush", "armorCrush", True),
  ("attack", "attack", True),
  ("reloadSeconds", "reloadTicks", True),
  ("rangeMeters", "rangeVoxels", True),
  ("visionMeters", "visionVoxels", False),
  ("speedMetersPerSecond", "speedMetersPerTick", True),
  ("digCubicMetersPerSecond", "digCubicMetersPerTick", True),
)
STRUCTURE_FIELDS = (
  ("garrisonCapacity", "garrisonCapacity", True),
  ("standHeightMeters", "standHeightVoxels", True),
  ("visionMeters", "visionVoxels", False),
  ("garrisonedVisionMeters", "garrisonedVisionVoxels", False),
  ("controlHitPoints", "controlHitPoints", False),
  ("controlHitPointsPerCubicMeter", "controlHitPointsPerCubicMeter", False),
)
MAX_FIELD = (1 << 31) - 1
MAX_SMALL = 255  # figures a squad, a garrison's capacity: a u8 in the server's file


def ConvertField(_key, _value):
  """A unit's or a structure's value: (integer, None), or (None, the reason it does not convert)."""
  if any(_key.endswith(entry[0]) and len(_key) > len(entry[0]) for entry in RULE_CONVERSIONS):
    converted, number = ConvertRule(_key, _value)
    return (number, None) if converted is not None else (None, number)
  if not isinstance(_value, str) or not re.fullmatch(r"\d+", _value):
    return None, f"{_value!r} is not a whole number as a decimal string"
  return int(_value), None


def ReadFields(_report, _path, _what, _entry, _fields):
  """_entry's _fields, converted: {converted key: integer}, or None after reporting each problem."""
  values, good = {}, True
  for key, converted, zero in _fields:
    number, reason = ConvertField(key, _entry.get(key))
    most = MAX_SMALL if key in ("squadFigures", "garrisonCapacity") else MAX_FIELD
    if number is None:
      _report.Problem(_path, f"{_what}'s {key}: {reason}")
      good = False
    elif not 0 <= number <= most or (number == 0 and not zero):
      _report.Problem(_path, f"{_what}'s {key} is {_entry.get(key)}, outside {0 if zero else 1} to {most}")
      good = False
    else:
      values[converted] = number
  return values if good else None


def ReadUnits(_report, _root):
  """Check 15: GameData/Server/Units.json (ADR-023, decision 1). Returns its records in Protocol::UnitKind's order, each
  the converted fields with kind, navigationClass and attackType as their numbers and bonus as eight integers in kind
  order."""
  path = _root / "GameData" / "Server" / "Units.json"
  data = ReadJson(_report, path)
  if data is None:
    return []
  entries = data.get("units")
  if not isinstance(entries, list):
    _report.Problem(path, "has no units (ADR-023, decision 1)")
    return []
  units, seen = {}, set()
  for entry in entries:
    kind = entry.get("kind") if isinstance(entry, dict) else None
    if kind not in UNIT_KINDS:
      _report.Problem(path, f"names the kind {kind!r}, which is not one of {', '.join(UNIT_KINDS)}")
      continue
    if kind in seen:
      _report.Problem(path, f"gives the kind {kind} twice")
      continue
    seen.add(kind)
    record = ReadFields(_report, path, kind, entry, UNIT_FIELDS)
    navigation, attackType, bonus = entry.get("navigationClass"), entry.get("attackType"), entry.get("bonus")
    if navigation not in NAVIGATION_CLASSES:
      _report.Problem(path, f"{kind}'s navigationClass {navigation!r} is not one of {', '.join(NAVIGATION_CLASSES)}")
      record = None
    if attackType not in DAMAGE_TYPES:
      _report.Problem(path, f"{kind}'s attackType {attackType!r} is not one of {', '.join(DAMAGE_TYPES)}")
      record = None
    bonuses = [0] * len(UNIT_KINDS)
    if not isinstance(bonus, dict):
      _report.Problem(path, f"{kind} has no bonus table")
      record = None
    for against, amount in (bonus.items() if isinstance(bonus, dict) else []):
      number, reason = ConvertField("bonus", amount)
      if against not in UNIT_KINDS:
        _report.Problem(path, f"{kind}'s bonus names the kind {against!r}, which is not one of {', '.join(UNIT_KINDS)}")
        record = None
      elif number is None or number > MAX_FIELD:
        _report.Problem(path, f"{kind}'s bonus against {against}: {reason or 'too large'}")
        record = None
      else:
        bonuses[UNIT_KINDS.index(against)] = number
    if record is not None:
      record.update(kind=UNIT_KINDS.index(kind), navigationClass=NAVIGATION_CLASSES.index(navigation),
                    attackType=DAMAGE_TYPES.index(attackType), bonus=bonuses)
      units[kind] = record
  for kind in UNIT_KINDS:
    if kind not in seen:
      _report.Problem(path, f"gives no {kind}: every kind has one record (ADR-023, decision 1)")
  return [units[kind] for kind in UNIT_KINDS if kind in units]


def ReadStructures(_report, _root, _templates):
  """Check 15: GameData/Server/Structures.json (ADR-024, decision 7), against _templates ({id: its critical part's
  name, empty for none}). Returns its records in ascending template id, each the converted fields with template and
  criticalThresholdPercent, 0 for a template that names no critical part."""
  path = _root / "GameData" / "Server" / "Structures.json"
  data = ReadJson(_report, path)
  if data is None:
    return []
  entries = data.get("structures")
  if not isinstance(entries, list):
    _report.Problem(path, "has no structures (ADR-024, decision 7)")
    return []
  structures, seen = {}, set()
  for entry in entries:
    template = entry.get("template") if isinstance(entry, dict) else None
    if template not in _templates:
      _report.Problem(path, f"names the template {template!r}, which GameData/Server/Templates/ does not hold")
      continue
    if template in seen:
      _report.Problem(path, f"gives the template {template} twice")
      continue
    seen.add(template)
    record = ReadFields(_report, path, template, entry, STRUCTURE_FIELDS)
    threshold, number = entry.get("criticalThresholdPercent"), 0
    if _templates[template]:
      number, _ = ConvertField("criticalThresholdPercent", threshold)
      if number is None or not 1 <= number <= 100:
        _report.Problem(path, f"{template} names the critical part {_templates[template]}, and its criticalThresholdPercent "
                              f"{threshold!r} is not a whole number from 1 to 100")
        record = None
    elif threshold is not None:
      _report.Problem(path, f"{template} names no critical part, so it gives no criticalThresholdPercent")
      record = None
    if record is not None:
      record.update(template=template, criticalThresholdPercent=number)
      structures[template] = record
  return [structures[template] for template in sorted(structures)]


def TemplateCriticalParts(_report, _root):
  """{template id: its critical part's name, empty for none}, from the layer files in GameData/Server/Templates/."""
  parts = {}
  for path in sorted((_root / "GameData" / "Server" / "Templates").glob("*.json")):
    data = ReadJson(_report, path)
    if isinstance(data, dict) and isinstance(data.get("id"), str):
      named = data.get("criticalPart")
      parts[data["id"]] = named if isinstance(named, str) and named.strip() and named.split()[0] != "None" else ""
  return parts


# ── Check 16, scenarios ──────────────────────────────────────────────────────────────────────────

# Protocol::EngineKind, EngineState, Side and TrialVersion, in their declared order, by the names a scenario gives them.
ENGINE_KINDS = ("trebuchet", "ram")
ENGINE_STATES = ("packed", "deploying", "deployed", "packing")
SIDES = ("attacker", "defender")
TRIAL_VERSIONS = ("voxel", "control")
COMMANDERS = ("human", "siegeDefender", "siegeAttacker")  # ADR-027, decision 1
MAP_VOXELS = (512, 64, 512)  # the MVP map, Simulation::Generator's MAP_WIDTH_VOXELS, MAP_HEIGHT_VOXELS and MAP_DEPTH_VOXELS
MAX_SEED = (1 << 64) - 1


def TemplateSizes(_report, _root):
  """{template id: (width, height, depth) in voxels, its occupied box}, from the layer files in GameData/Server/Templates/."""
  sizes = {}
  for path in sorted((_root / "GameData" / "Server" / "Templates").glob("*.json")):
    data = ReadJson(_report, path)
    if not isinstance(data, dict) or not isinstance(data.get("id"), str) or not IsLayerFile(data):
      continue
    cells = [(x, y, z) for y, layer in enumerate(data["layers"]) for z, row in enumerate(layer) for x, char in enumerate(row)
             if char != "."]
    if cells:
      sizes[data["id"]] = tuple(max(cell[axis] for cell in cells) - min(cell[axis] for cell in cells) + 1 for axis in range(3))
  return sizes


def IsPosition(_value):
  return isinstance(_value, list) and len(_value) == 3 and all(isinstance(v, int) and not isinstance(v, bool) for v in _value)


def Within(_point, _low, _high):
  return all(_low[axis] <= _point[axis] <= _high[axis] for axis in range(len(_low)))


class ScenarioReader:
  """Reads one scenario source for check 16, reporting each problem against its file, and gives it converted."""

  def __init__(self, _report, _path, _data, _templates, _structureRules, _units, _rules):
    self.report, self.path, self.data = _report, _path, _data
    self.templates, self.structureRules, self.rules = _templates, _structureRules, _rules
    self.figures = {UNIT_KINDS[unit["kind"]]: unit["squadFigures"] for unit in _units}

  def Problem(self, _message):
    self.report.Problem(self.path, _message)

  def Number(self, _key, _value, _what, _least=0):
    """A decimal string with a unit in its key, converted as check 10's values are."""
    number, reason = ConvertField(_key, _value)
    if number is None:
      self.Problem(f"{_what}'s {_key}: {reason}")
    elif number < _least:
      self.Problem(f"{_what}'s {_key} is {_value}, below {_least}")
      number = None
    return number

  def Box(self, _entry, _what):
    low, high = _entry.get("min") if isinstance(_entry, dict) else None, _entry.get("max") if isinstance(_entry, dict) else None
    material = _entry.get("material") if isinstance(_entry, dict) else None
    if not IsPosition(low) or not IsPosition(high) or any(low[axis] > high[axis] for axis in range(3)):
      self.Problem(f"{_what} is not a box of two corners, min no greater than max")
      return None
    if not isinstance(material, int) or isinstance(material, bool) or not 0 <= material <= 255:
      self.Problem(f"{_what}'s material {material!r} is not a palette index or 0")
      return None
    return (material, tuple(low), tuple(high))

  def Read(self):
    data, scenario = self.data, {}
    scenario["id"] = data.get("id")
    if not isinstance(scenario["id"], str) or not ASSET_ID.fullmatch(scenario["id"]):
      self.Problem(f"its id {scenario['id']!r} is not an identifier such as Siege")
      return None
    seeds = data.get("seeds") if isinstance(data.get("seeds"), dict) else {}
    scenario["seeds"] = []
    for name in ("world", "resource", "match"):
      value = seeds.get(name)
      if not isinstance(value, str) or not re.fullmatch(r"\d+", value) or int(value) > MAX_SEED:
        self.Problem(f"its {name} seed {value!r} is not a whole number of 64 bits as a decimal string")
        return None
      scenario["seeds"].append(int(value))
    site = data.get("site") if isinstance(data.get("site"), dict) else {}
    low, high = site.get("min"), site.get("max")
    level = self.Number("groundMeters", site.get("groundMeters"), "its site", 1)
    if not (isinstance(low, list) and isinstance(high, list) and len(low) == len(high) == 2 and all(isinstance(v, int) for v in low + high)
            and 0 <= low[0] <= high[0] < MAP_VOXELS[0] and 0 <= low[1] <= high[1] < MAP_VOXELS[2]) or level is None:
      self.Problem("its site is not a box of x and z on the map with its groundMeters")
      return None
    scenario["site"] = (low[0], low[1], high[0], high[1], level)
    siteLow, siteHigh = (low[0], 0, low[1]), (high[0], MAP_VOXELS[1] - 1, high[1])
    scenario["ground"] = [self.Box(entry, f"ground box {number + 1}") for number, entry in enumerate(data.get("ground") or [])]
    scenario["earthworks"] = [self.Box(entry, f"earthwork {number + 1}") for number, entry in enumerate(data.get("earthworks") or [])]
    if None in scenario["ground"] or None in scenario["earthworks"]:
      return None
    for kind in ("ground", "earthworks"):
      for number, (_, boxLow, boxHigh) in enumerate(scenario[kind]):
        if not Within(boxLow, siteLow, siteHigh) or not Within(boxHigh, siteLow, siteHigh):
          self.Problem(f"its {kind} box {number + 1} leaves its site")
    if not self.GroundCovers(scenario):
      return None
    if not self.ReadStructures(scenario) or not self.ReadPlayers(scenario) or not self.ReadUnits(scenario) or not self.ReadTrial(scenario):
      return None
    return scenario

  def GroundCovers(self, _scenario):
    """Every voxel of the site below its level takes a material from a ground box, so the site is the scenario's, not the
    generator's (ADR-026, decision 1)."""
    x0, z0, x1, z1, level = _scenario["site"]
    layers = [[] for _ in range(level)]
    for material, low, high in _scenario["ground"]:
      for y in range(max(low[1], 0), min(high[1], level - 1) + 1):
        layers[y].append((low[0], low[2], high[0], high[2]))
    for y, boxes in enumerate(layers):
      # A layer is covered when the boxes' union is the site's rectangle: count the cells each covers once.
      covered = set()
      for bx0, bz0, bx1, bz1 in boxes:
        for z in range(max(bz0, z0), min(bz1, z1) + 1):
          covered.add((z, max(bx0, x0), min(bx1, x1)))
      rows = {}
      for z, a, b in covered:
        rows.setdefault(z, []).append((a, b))
      for z in range(z0, z1 + 1):
        reach = x0
        for a, b in sorted(rows.get(z, [])):
          if a > reach:
            break
          reach = max(reach, b + 1)
        if reach <= x1:
          self.Problem(f"its ground boxes leave the site's voxel ({reach}, {y}, {z}) below its level without a material")
          return False
    return True

  def ReadStructures(self, _scenario):
    structures, names = [], set()
    for number, entry in enumerate(self.data.get("structures") or []):
      what = f"structure {number + 1}"
      template, name, owner = entry.get("template"), entry.get("name"), entry.get("owner")
      origin, turns = entry.get("origin"), entry.get("quarterTurns")
      depth = self.Number("foundationDepthMeters", entry.get("foundationDepthMeters"), what)
      if template not in self.templates:
        self.Problem(f"{what} names the template {template!r}, which GameData/Server/Templates/ does not hold")
        return False
      if not isinstance(name, str) or not name or name in names:
        self.Problem(f"{what} has the name {name!r}, which is empty or another structure's")
        return False
      if not isinstance(owner, int) or not 1 <= owner <= 255 or not IsPosition(origin) or turns not in (0, 1, 2, 3) or depth is None:
        self.Problem(f"{what}, {name}, needs an owner from 1 to 255, an origin, a quarter turn from 0 to 3 and its foundationDepthMeters")
        return False
      names.add(name)
      width, height, depthVoxels = self.templates[template]
      extent = (depthVoxels, height, width) if turns % 2 == 1 else (width, height, depthVoxels)
      structures.append({"template": template, "name": name, "owner": owner, "origin": tuple(origin), "quarterTurns": turns,
                         "foundationDepthVoxels": depth, "extent": extent})
    for index, first in enumerate(structures):
      for second in structures[index + 1 :]:
        if all(first["origin"][axis] < second["origin"][axis] + second["extent"][axis] and
               second["origin"][axis] < first["origin"][axis] + first["extent"][axis] for axis in range(3)):
          self.Problem(f"the structures {first['name']} and {second['name']} overlap")
          return False
    _scenario["structures"] = structures
    return True

  def ReadPlayers(self, _scenario):
    players, ids = [], set()
    for entry in self.data.get("players") or []:
      playerId, side, commander = entry.get("id"), entry.get("side"), entry.get("commander")
      stone = self.Number("stoneCubicMeters", entry.get("stoneCubicMeters"), f"player {playerId}")
      timber = self.Number("timberCubicMeters", entry.get("timberCubicMeters"), f"player {playerId}")
      if not isinstance(playerId, int) or not 1 <= playerId <= 255 or playerId in ids or side not in SIDES or commander not in COMMANDERS \
         or not isinstance(entry.get("revealed"), bool) or stone is None or timber is None:
        self.Problem(f"the player {playerId!r} needs an id of its own from 1 to 255, a side of {', '.join(SIDES)}, a commander of "
                     f"{', '.join(COMMANDERS)}, whether its map is revealed, and its stockpiles")
        return False
      ids.add(playerId)
      players.append({"id": playerId, "side": SIDES.index(side), "stone": stone, "timber": timber, "commander": commander,
                      "revealed": entry["revealed"]})
    _scenario["players"] = players
    return bool(players)

  def Top(self, _scenario, _x, _z):
    """The first empty voxel above the levelled site's ground at (_x, _z), after the earthworks, and the material under it."""
    level = _scenario["site"][4]
    column = {}
    for material, low, high in _scenario["ground"]:
      if low[0] <= _x <= high[0] and low[2] <= _z <= high[2]:
        for y in range(low[1], min(high[1], level - 1) + 1):
          column[y] = material
    for material, low, high in _scenario["earthworks"]:
      if low[0] <= _x <= high[0] and low[2] <= _z <= high[2]:
        for y in range(low[1], high[1] + 1):
          column[y] = material
    solid = [y for y, material in column.items() if material != 0]
    top = max(solid) + 1 if solid else 0
    return top, column.get(top - 1, 0)

  def Walkable(self, _scenario, _cell, _palette):
    x, y, z = _cell
    x0, z0, x1, z1, _ = _scenario["site"]
    if not (x0 <= x <= x1 and z0 <= z <= z1):
      return False
    for structure in _scenario["structures"]:
      origin, extent = structure["origin"], structure["extent"]
      if origin[0] <= x < origin[0] + extent[0] and origin[2] <= z < origin[2] + extent[2]:
        return False
    top, under = self.Top(_scenario, x, z)
    return top == y and bool(_palette.get(under, {}).get("ground"))

  def ReadUnits(self, _scenario):
    palette = self.palette
    taken, squads, engines, garrisons = set(), [], [], []
    for number, entry in enumerate(self.data.get("squads") or []):
      kind, owner, cells = entry.get("kind"), entry.get("owner"), entry.get("cells")
      if kind not in UNIT_KINDS or kind in ("ram", "trebuchet") or not isinstance(cells, list) or not all(IsPosition(c) for c in cells):
        self.Problem(f"squad {number + 1} needs a kind of soldier or laborer and its cells")
        return False
      if len(cells) != self.figures.get(kind):
        self.Problem(f"squad {number + 1}, of {kind}, has {len(cells)} cells, and a squad of {kind} has {self.figures.get(kind)} figures")
        return False
      for cell in cells:
        if not self.Walkable(_scenario, cell, palette) or tuple(cell) in taken:
          self.Problem(f"squad {number + 1}'s cell {tuple(cell)} is not a walkable cell of the levelled site that no other unit holds")
          return False
        taken.add(tuple(cell))
      squads.append({"kind": UNIT_KINDS.index(kind), "owner": owner, "cells": [tuple(cell) for cell in cells]})
    width = self.rules.get("machinesWidthVoxels", 1)
    for number, entry in enumerate(self.data.get("engines") or []):
      kind, cell, turns, state = entry.get("kind"), entry.get("cell"), entry.get("quarterTurns"), entry.get("state")
      if kind not in ENGINE_KINDS or not IsPosition(cell) or turns not in (0, 1, 2, 3) or state not in ENGINE_STATES:
        self.Problem(f"engine {number + 1} needs a kind of {', '.join(ENGINE_KINDS)}, a cell, a quarter turn and a state")
        return False
      for dx in range(width):
        for dz in range(width):
          footprint = (cell[0] + dx, cell[1], cell[2] + dz)
          if not self.Walkable(_scenario, footprint, palette) or footprint in taken:
            self.Problem(f"engine {number + 1}'s footprint cell {footprint} is not a walkable cell of the levelled site that no other unit holds")
            return False
          taken.add(footprint)
      engines.append({"kind": ENGINE_KINDS.index(kind), "owner": entry.get("owner"), "cell": tuple(cell), "quarterTurns": turns,
                      "state": ENGINE_STATES.index(state)})
    held = {}
    byName = {structure["name"]: structure for structure in _scenario["structures"]}
    for number, entry in enumerate(self.data.get("garrisons") or []):
      kind, name = entry.get("kind"), entry.get("structure")
      structure = byName.get(name)
      rules = self.structureRules.get(structure["template"]) if structure else None
      if kind not in UNIT_KINDS or kind in ("ram", "trebuchet") or structure is None:
        self.Problem(f"garrison {number + 1} needs a kind of soldier and a structure the scenario places, and names {name!r}")
        return False
      held[name] = held.get(name, 0) + self.figures[kind]
      if rules is None or held[name] > rules["garrisonCapacity"]:
        self.Problem(f"garrison {number + 1} puts {held[name]} figures in {name}, which holds "
                     f"{rules['garrisonCapacity'] if rules else 0} (ADR-024, decision 5)")
        return False
      garrisons.append({"kind": UNIT_KINDS.index(kind), "owner": entry.get("owner"), "structure": name})
    owners = {player["id"] for player in _scenario["players"]}
    for thing in squads + engines + garrisons:
      if thing["owner"] not in owners:
        self.Problem(f"a squad, an engine or a garrison belongs to {thing['owner']!r}, which is not one of its players")
        return False
    _scenario.update(squads=squads, engines=engines, garrisons=garrisons)
    return True

  def ReadTrial(self, _scenario):
    trial = self.data.get("trial")
    if not isinstance(trial, dict):
      self.Problem("has no trial (ADR-026, decision 4)")
      return False
    byName = {structure["name"]: structure for structure in _scenario["structures"]}
    keep, nearest, weak = byName.get(trial.get("keep")), byName.get(trial.get("nearest")), byName.get(trial.get("weak"))
    if keep is None or keep["template"] != "T2":
      self.Problem(f"its trial's keep {trial.get('keep')!r} is not a T2 it places (ADR-026, decision 2)")
      return False
    if nearest is None or weak is None or nearest["template"] != "T3" or weak["template"] != "T3":
      self.Problem("its trial's nearest and weak sections must be T3 sections it places")
      return False
    sides = {player["id"]: player["side"] for player in _scenario["players"]}
    attacker, defender = trial.get("attacker"), trial.get("defender")
    if sides.get(attacker) != SIDES.index("attacker") or sides.get(defender) != SIDES.index("defender"):
      self.Problem("its trial's attacker and defender must be its players on those sides")
      return False
    limit = self.Number("timeLimitSeconds", trial.get("timeLimitSeconds"), "its trial", 1)
    hold = self.Number("holdSeconds", trial.get("holdSeconds"), "its trial", 1)
    ring = self.Number("ringMeters", trial.get("ringMeters"), "its trial", 1)
    low, high = trial.get("campMin"), trial.get("campMax")
    version = trial.get("version")
    if None in (limit, hold, ring) or version not in TRIAL_VERSIONS or not IsPosition(low) or not IsPosition(high):
      self.Problem(f"its trial needs a version of {', '.join(TRIAL_VERSIONS)}, its limit, hold and ring, and the camp's box")
      return False
    _scenario["trial"] = {"version": TRIAL_VERSIONS.index(version), "attacker": attacker, "defender": defender, "keep": keep["name"],
                          "timeLimitTicks": limit, "holdTicks": hold, "ringCells": ring, "campMin": tuple(low), "campMax": tuple(high),
                          "nearest": nearest["name"], "weak": weak["name"]}
    return True


# Which way each kind faces: a template's front, and the ram's, is its +z; the trebuchet throws towards its +x (MvpAssets.md).
# A turn takes +z to +x, so mirroring east to west maps a +z facing's turns 1 and 3 onto each other, and a +x facing's 0 and 2.
FACES_X = ("trebuchet",)
SECTION_SIDES = {2: "S", 1: "E", 0: "N", 3: "W"}  # the side a section faces out to, by its quarter turns


def MirrorTurns(_turns, _facesX):
  return (2 - _turns) % 4 if _facesX else (4 - _turns) % 4


def ReadMirror(_report, _path, _data, _original, _machinesWidth):
  """A scenario that gives mirrorOf (ADR-026, decision 1; MvpPlan.md, M4, input 28): _original mirrored east to west about
  its site's centre line, each structure renamed by its rename table, with its own id, seeds and players and its trial's
  nearest and weak sections. Returns it whole, as the bake writes it, or None after reporting each problem."""

  def Problem(_message):
    _report.Problem(_path, _message)

  x0, z0, x1, z1, level = _original["site"]
  span = x0 + x1  # x maps to span - x
  rename = _data.get("rename")
  names = [structure["name"] for structure in _original["structures"]]
  if not isinstance(rename, dict) or sorted(rename) != sorted(names) or len(set(rename.values())) != len(names) or \
     not all(isinstance(value, str) and value for value in rename.values()):
    Problem(f"its rename table must rename each of {_original['id']}'s structures once, to names of their own")
    return None
  # The castle stands on its site's centre line, or its mirror would not stand where it does.
  low = min(structure["origin"][0] for structure in _original["structures"])
  high = max(structure["origin"][0] + structure["extent"][0] - 1 for structure in _original["structures"])
  if low + high != span:
    Problem(f"{_original['id']}'s structures span x {low} to {high}, which is not symmetric about its site's centre line "
            f"at {span / 2}")
    return None

  def Box(_box):
    material, boxLow, boxHigh = _box
    return (material, (span - boxHigh[0], boxLow[1], boxLow[2]), (span - boxLow[0], boxHigh[1], boxHigh[2]))

  def Cell(_cell):
    return (span - _cell[0], _cell[1], _cell[2])

  scenario = {"id": _data.get("id"), "site": _original["site"], "players": None}
  scenario["ground"] = [Box(box) for box in _original["ground"]]
  scenario["earthworks"] = [Box(box) for box in _original["earthworks"]]
  scenario["structures"] = []
  for structure in _original["structures"]:
    origin, extent = structure["origin"], structure["extent"]
    scenario["structures"].append({**structure, "name": rename[structure["name"]],
                                   "origin": (span - origin[0] - extent[0] + 1, origin[1], origin[2]),
                                   "quarterTurns": MirrorTurns(structure["quarterTurns"], False)})
  scenario["squads"] = [{**squad, "cells": [Cell(cell) for cell in squad["cells"]]} for squad in _original["squads"]]
  scenario["engines"] = []
  for engine in _original["engines"]:
    cell = engine["cell"]
    scenario["engines"].append({**engine, "cell": (span - cell[0] - _machinesWidth + 1, cell[1], cell[2]),
                                "quarterTurns": MirrorTurns(engine["quarterTurns"], ENGINE_KINDS[engine["kind"]] in FACES_X)})
  scenario["garrisons"] = [{**garrison, "structure": rename[garrison["structure"]]} for garrison in _original["garrisons"]]
  trial = dict(_original["trial"])
  roles = _data.get("trial") if isinstance(_data.get("trial"), dict) else {}
  campLow, campHigh = trial["campMin"], trial["campMax"]
  trial.update(keep=rename[trial["keep"]], campMin=(span - campHigh[0], campLow[1], campLow[2]),
               campMax=(span - campLow[0], campHigh[1], campHigh[2]), nearest=roles.get("nearest"), weak=roles.get("weak"))
  byName = {structure["name"]: structure for structure in scenario["structures"]}
  if byName.get(trial["nearest"], {}).get("template") != "T3" or byName.get(trial["weak"], {}).get("template") != "T3":
    Problem("its trial's nearest and weak sections must be T3 sections its mirror places")
    return None
  scenario["trial"] = trial
  # Each name's side letter matches the side it stands on: a section's the side it faces out to, and a corner tower's
  # its side of the keep's middle, north or south and then east or west.
  keep = byName[trial["keep"]]
  keepX = keep["origin"][0] + keep["extent"][0] / 2
  keepZ = keep["origin"][2] + keep["extent"][2] / 2
  for structure in scenario["structures"]:
    name = structure["name"]
    middleX = structure["origin"][0] + structure["extent"][0] / 2
    middleZ = structure["origin"][2] + structure["extent"][2] / 2
    if structure["template"] == "T3" and re.fullmatch(r"[SENW]\d+", name) and name[0] != SECTION_SIDES[structure["quarterTurns"]]:
      Problem(f"its section {name} faces {SECTION_SIDES[structure['quarterTurns']]}, so its name's side letter is wrong")
      return None
    if structure["template"] == "T4" and re.fullmatch(r"[SN][EWT]", name):
      south, east = middleZ < keepZ, middleX > keepX
      if (name[0] == "S") != south or (name[1] != "T" and (name[1] == "E") != east):
        Problem(f"its tower {name} stands {'south' if south else 'north'}-{'east' if east else 'west'} of the keep")
        return None
  return scenario


def ReadScenarios(_report, _root, _palette, _units, _structures, _rules):
  """Check 16: every scenario in GameData/Server/Scenarios/ (ADR-026, decision 2), read and converted, in ascending id.
  _units and _structures are check 15's, and _rules check 10's."""
  folder = _root / "GameData" / "Server" / "Scenarios"
  templates = TemplateSizes(_report, _root)
  structureRules = {structure["template"]: structure for structure in _structures}
  scenarios, mirrors = {}, []
  for path in sorted(folder.glob("*.json")) if folder.is_dir() else []:
    data = ReadJson(_report, path)
    if not isinstance(data, dict):
      continue
    if "mirrorOf" in data:
      mirrors.append((path, data))
      continue
    reader = ScenarioReader(_report, path, data, templates, structureRules, _units, _rules)
    reader.palette = _palette
    scenario = reader.Read()
    if scenario is None:
      continue
    if scenario["id"] in scenarios:
      _report.Problem(path, f"has the id {scenario['id']}, as another scenario does")
      continue
    scenarios[scenario["id"]] = scenario
  # A mirror takes its seeds and players from its own source and everything else from the scenario it mirrors, which the
  # bake resolves, so the simulation never sees mirrorOf (ADR-026, decision 2).
  for path, data in mirrors:
    original = scenarios.get(data.get("mirrorOf"))
    if original is None:
      _report.Problem(path, f"mirrors {data.get('mirrorOf')!r}, which is no scenario that does not mirror another")
      continue
    reader = ScenarioReader(_report, path, data, templates, structureRules, _units, _rules)
    reader.palette = _palette
    scenario = ReadMirror(_report, path, data, original, _rules.get("machinesWidthVoxels", 1))
    if scenario is None:
      continue
    seeds = data.get("seeds") if isinstance(data.get("seeds"), dict) else {}
    if not isinstance(scenario["id"], str) or not ASSET_ID.fullmatch(scenario["id"]) or scenario["id"] in scenarios:
      _report.Problem(path, f"its id {scenario['id']!r} is not an identifier of its own")
      continue
    if not all(isinstance(seeds.get(name), str) and re.fullmatch(r"\d+", seeds.get(name)) and int(seeds[name]) <= MAX_SEED
               for name in ("world", "resource", "match")):
      _report.Problem(path, "its seeds are not three whole numbers of 64 bits as decimal strings")
      continue
    scenario["seeds"] = [int(seeds[name]) for name in ("world", "resource", "match")]
    if not reader.ReadPlayers(scenario):
      continue
    scenarios[scenario["id"]] = scenario
  return [scenarios[key] for key in sorted(scenarios)]


SERVER_ONLY_TREE = ("Tools", "GameData/Server")


def CheckServerBake(_report, _root):
  """Check 11: the server bake gives the same bytes from a tree that holds nothing under GameData/Client/."""
  bake = _root / "Tools" / "BakeContent.py"
  if not bake.is_file():
    _report.Problem(bake, "the bake is missing, so nothing shows the server's data is clean (ADR-008)")
    return
  with tempfile.TemporaryDirectory() as folder:
    here, alone = Path(folder) / "here", Path(folder) / "alone"
    for relative in SERVER_ONLY_TREE:
      shutil.copytree(_root / relative, alone / relative, ignore=shutil.ignore_patterns("__pycache__"))
    files = []
    for tree, out in ((_root, here), (alone, alone / "out")):
      result = subprocess.run([sys.executable, str(tree / "Tools" / "BakeContent.py"), "--server", str(out)], capture_output=True,
                              text=True)
      if result.returncode != 0:
        last = ((result.stdout + result.stderr).strip().splitlines() or ["no output"])[-1]
        where = "" if tree == _root else " from a tree with no GameData/Client/"
        _report.Problem(bake, f"the server bake failed{where} with exit code {result.returncode}: {last}")
        return
      files.append((out / "ServerData.bin").read_bytes())
    if files[0] != files[1]:
      _report.Problem(bake, "the server bake writes different bytes when GameData/Client/ is absent, so client content reaches "
                            "the server's data (ADR-008, decision 2)")


# ── Running it ──────────────────────────────────────────────────────────────────────────────────


def Check(_root):
  """Runs every check over the tree at _root. Returns (report, assets, how many files the client bake wrote, line models)."""
  report = Report(_root)
  palette, behaviors = ReadPalette(report, _root)
  assets = ReadAssets(report, _root, palette)
  lineModels = ReadLineModels(report, _root)
  ReadFont(report, _root)
  modelIds = {asset.data.get("id") for relative, asset in assets.items() if relative.startswith("Client/Models/")}
  ReadDebris(report, _root, palette, modelIds)
  manifestPath = _root / "GameData" / "Manifest.json"
  manifest = ReadJson(report, manifestPath) or {}
  entries = CheckManifest(report, _root, manifest, assets, lineModels)
  for relative, asset in assets.items():
    CheckGrid(report, asset, palette)
    if not IsTemplate(relative):
      continue
    if "parts" not in asset.data:
      report.Problem(asset.path, "is a template with no parts (ADR-005, decision 2)")
    resolved = CheckDecorations(report, asset, palette)
    if relative in entries:
      CheckCost(report, asset, resolved, entries[relative])
    CheckCriticalPart(report, asset)
    CheckConnected(report, asset)
  if not any(entry.get("id") == "T3" for entry in manifest.get("assets", [])):
    report.Problem(manifestPath, "has no T3, the standard wall section whose cost ADR-006 fixes")
  for entry in manifest.get("assets", []):
    if isinstance(entry.get("figure"), dict):
      CheckFigure(report, _root, entry, assets)
  outputs = CheckClientBake(report, _root, behaviors)
  rules = ReadRules(report, _root)
  units = ReadUnits(report, _root)
  structures = ReadStructures(report, _root, TemplateCriticalParts(report, _root))
  ReadScenarios(report, _root, palette, units, structures, rules)
  CheckServerBake(report, _root)
  return report, assets, outputs, lineModels


def Main(_arguments):
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument("--root", type=Path, default=ROOT, help="the repository root (default: this script's)")
  arguments = parser.parse_args(_arguments)
  report, assets, outputs, lineModels = Check(arguments.root.resolve())
  for problem in report.problems:
    print(problem)
  if report.problems:
    print(f"CheckAssets: {Plural(len(report.problems), 'problem')}")
    return 1
  templates = sum(1 for relative in assets if IsTemplate(relative))
  print(f"CheckAssets: OK, {len(assets)} layer files, {templates} of them templates, {Plural(len(lineModels), 'line model')}, "
        f"and {Plural(outputs, 'file')} from the client bake")
  return 0


if __name__ == "__main__":
  sys.exit(Main(sys.argv[1:]))
