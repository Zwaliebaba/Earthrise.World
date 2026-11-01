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
   unit says how it converts: meters to voxels and seconds to ticks, which must come out whole, and speeds and
   accelerations to Core::Fixed per tick, floored once. A key with no unit the bake knows, a value that is not a
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
13. The font's glyph file, GameData/Client/Fonts/EarthriseMason-glyphs.json (Hud.md §9; ADR-019, decision 1), holds 127
   glyphs, no code point twice. Each glyph's rows are made of `.` and `#`, each as wide as its `w` says, at most 16 so
   that a row is one 16-bit mask, and they fit the cell below their `top`. The 15 symbols and the 25 icons of Hud.md §9
   are present, the icons marked as icons. The bake reads the font with this check's code.
14. Manifest.json's `debrisByRubble` (MvpAssets.md, *Poses, projectiles, gates and debris*): each row names a palette
   index whose behaviour is a rubble, once; models, by id, that are in GameData/Client/Models/; and a tint, if any,
   that is exactly one palette entry's colour, so that it bakes as that index. The bake reads the table with this
   check's code.

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
RULE_CONVERSIONS = (
  ("MetersPerSecondSquared", "MetersPerTickSquared", Fraction(FIXED_ONE_RAW, TICKS_PER_SECOND * TICKS_PER_SECOND), False),
  ("MetersPerSecond", "MetersPerTick", Fraction(FIXED_ONE_RAW, TICKS_PER_SECOND), False),
  ("Seconds", "Ticks", Fraction(TICKS_PER_SECOND), True),
  ("Meters", "Voxels", Fraction(VOXELS_PER_METER), True),
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
FONT_GLYPH_COUNT = 127
FONT_CELL_ROWS = 10    # Hud.md §9: one row above the caps, two below the baseline
FONT_BASELINE_ROW = 7
FONT_ROW_BITS = 16
# Hud.md §9: the 15 symbols, and the 25 command icons at U+E000 to U+E018.
FONT_SYMBOLS = "·×²³°←→↑↓↺↻…–—’"
FONT_ICONS = range(0xE000, 0xE019)


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
  ReadRules(report, _root)
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
