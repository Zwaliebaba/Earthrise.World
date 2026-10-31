"""Checks the voxel content under GameData/ (ADR-005, ADR-006, ADR-008).

It prints one line per problem, naming the file and the reason, then a summary line, and exits 1 if it found any.
A layer file is any .json under GameData/ with `layers` and a `legend`; the font's glyph file is not one.
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
   names, such as behaviours as bare numbers, is not caught.
   Still vacuous: there is no server bake until M2, so nothing yet checks that it reads nothing under
   GameData/Client/. And the client bake writes only the palette today, so the search covers one table. When a
   client table rightly names a material whose name is also a behaviour, such as water for the shallow-water
   rule, the vocabulary here changes in the same commit.

Usage:
  python3 Tools/CheckAssets.py              the repository this script sits in
  python3 Tools/CheckAssets.py --root DIR   another tree, such as a mutated copy of it
"""

import argparse
import json
import re
import subprocess
import sys
import tempfile
from collections import Counter
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


def CheckManifest(_report, _root, _manifest, _assets):
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
  return listed


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
    for path in outputs:
      words = Words(path.read_text(encoding="utf-8", errors="replace"))
      for category, phrases in vocabulary.items():
        found = [" ".join(phrase) for phrase in phrases if Contains(words, phrase)]
        if found:
          _report.Problem(bake, f"the client bake wrote {path.name}, which carries server-only data: the {category} "
                                f"word(s) {', '.join(found)} (ADR-008, decision 2)")
    return len(outputs)


# ── Running it ──────────────────────────────────────────────────────────────────────────────────


def Check(_root):
  """Runs every check over the tree at _root. Returns (report, assets, how many files the client bake wrote)."""
  report = Report(_root)
  palette, behaviors = ReadPalette(report, _root)
  assets = ReadAssets(report, _root, palette)
  manifestPath = _root / "GameData" / "Manifest.json"
  manifest = ReadJson(report, manifestPath) or {}
  entries = CheckManifest(report, _root, manifest, assets)
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
  return report, assets, outputs


def Main(_arguments):
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument("--root", type=Path, default=ROOT, help="the repository root (default: this script's)")
  arguments = parser.parse_args(_arguments)
  report, assets, outputs = Check(arguments.root.resolve())
  for problem in report.problems:
    print(problem)
  if report.problems:
    print(f"CheckAssets: {Plural(len(report.problems), 'problem')}")
    return 1
  templates = sum(1 for relative in assets if IsTemplate(relative))
  print(f"CheckAssets: OK, {len(assets)} layer files, {templates} of them templates, "
        f"and {Plural(outputs, 'file')} from the client bake")
  return 0


if __name__ == "__main__":
  sys.exit(Main(sys.argv[1:]))
