"""Bakes GameData/ into what the build compiles in or ships (ADR-005, ADR-008, ADR-010 decision 7, ADR-016).

The client half writes C++ headers into the build's intermediate folder, where the Content library compiles them
in: the palette's look (colours, jitter and shade sets), the client view of every template in
GameData/Server/Templates/, every model in GameData/Client/Models/, every line model in GameData/Client/Lines/ (ADR-018)
and the shared hash. The server half writes
<outputdir>\\ServerData\\ServerData.bin, which the simulation reads (see *The server's file* below).

A template's decorations reach the world as their host (GameDesign.md, M2's values, item 7). Each decoration voxel,
resolved to its host's behaviour by ADR-005 decision 6, is written as the palette entry that `decorates` that
decoration with that behaviour: an opening in timber, an opening in stone, iron in timber. Both halves write these
indices, so a template's world voxels and the client's preview agree, and the shared hash covers them. A decoration
whose behaviour no entry decorates it with is a problem.

The client view of a template (ADR-008, decision 2) is its id and display name, its size in voxels after trimming
the empty padding, so that the occupied voxels' minimum corner is its origin (ADR-005, decision 2), its voxel edge in
millimetres, one palette index per voxel (x fastest, then z, then y; 0 is empty) with x mirrored from the layer file,
so that the shape looks as drawn in the world's frame (ADR-017), and its counted cost in m3 of stone and of timber
(ADR-005, decision 7). The cost is counted by Tools/CheckAssets.py's own code, so the bake and the asset check cannot
disagree on it. Nothing server-only goes in, and check 9 of the asset check
searches what this writes for it. A model's view is the same without the cost (ADR-005, decision 8). Both tables are
in ascending id, compared byte by byte.

The shared hash (ADR-008, decision 5) is FNV-1a 64, with Core::Fnv1a64's offset basis and prime (Core/Hash.h), over
this canonical encoding of the client view of the shared sources, the palette and the templates. Every integer is
little-endian, nothing is padded, and the fields come in this order:
  1. for each palette index from 0 to 255: u8 1 if the palette has an entry there and 0 if not, then u32 the
     entry's sRGB colour as 0xRRGGBB, 0 where there is none;
  2. u32 the number of templates;
  3. for each template, in table order: u32 the byte length of its id, then the id's UTF-8 bytes; u32 the byte
     length of its name, then the name's UTF-8 bytes; u32 its width, height and depth in voxels and its voxel
     edge in millimetres; u32 its stone and its timber in m3; then its width x height x depth u8 palette indices,
     in table order.
Models and line models are client content, not shared, so the hash does not cover them. A line model's table carries its
drawn length and dot rule and, for each segment, its span in millimetres, its sRGB colour or that it takes the side's
faction colour, its width in pixels and whether it tapers (ADR-018), read by Tools/CheckAssets.py's own code (its check 12). Content/SharedHash.h states the same
encoding, and ContentTests recomputes the hash from the compiled tables, so the two halves cannot drift.

Generated headers are build output: never committed, never edited, and not linted (they sit in a Generated/ folder,
outside .clang-tidy's header filter). Each is rewritten only when its text changes.

The server's file (ADR-016) is a header and a body, little-endian and unpadded throughout:
  the header: the four bytes ERSD; u32 the format version, 1; u64 Core::Fnv1a64 over every byte of the body;
  the body: u64 the shared hash, as above; u32 the number of sections; for each section a directory entry of its
    four-character tag, u32 its version, u32 its offset from the start of the body and u32 its length; then the
    sections, in the directory's order: MATL, TMPL and RULE, each at version 1.
  MATL: for each palette index from 0 to 255, six u8: 1 if the palette has the index and 0 if not; its behaviour,
    numbered as Simulation::Behavior declares them (BEHAVIORS below); its support, numbered as Simulation::Support
    (SUPPORT below); its span in voxels, from RULE's value for its behaviour, 0 for a behaviour that is not judged by
    span; flags, bit 0 for walkable ground (Palette.json's `ground`) and bit 1 for liquid (water); and the index its
    rubble takes (Palette.json's `rubble`), 0 when it leaves none. A decoration, which never reaches the world, and
    an index only models use have behaviour 0.
  TMPL: u32 the number of templates; then each in ascending id: u32 the byte length of its id, then its UTF-8 bytes;
    the same for its name and for its critical part's name, empty when it names none; u32 its width, height and
    depth in voxels, its voxel edge in millimetres, its stone and its timber in m3; then its width x height x depth
    u8 palette indices, and as many u8 parts (0 none, 1 critical, 2 foundation, 3 stock), both in the client view's
    order: x fastest, then z, then y, mirrored in x from the layer file (ADR-017).
  RULE: u32 the number of values; then each in ascending key order: u32 the byte length of its key, its UTF-8 bytes
    and its value as an i64. The keys and values are GameData/Server/Rules.json's, converted by Tools/CheckAssets.py
    (its check 10) into the simulation's units, so a key names the converted unit, as warningBaseTicks does.
Nothing under GameData/Client/ reaches it, which check 11 of the asset check proves. It is rewritten only when its
bytes change.

Usage:
  python Tools/BakeContent.py --client OUT_DIR
  python Tools/BakeContent.py --server OUT_DIR
"""

import argparse
import json
import re
import struct
import sys
from pathlib import Path

import CheckAssets

ROOT = Path(__file__).resolve().parent.parent
PALETTE = ROOT / "GameData" / "Server" / "Palette.json"
TEMPLATES = ROOT / "GameData" / "Server" / "Templates"
MODELS = ROOT / "GameData" / "Client" / "Models"
LINES = ROOT / "GameData" / "Client" / "Lines"
PALETTE_ENTRIES = 256

# Core::FNV_OFFSET_BASIS and Core::FNV_PRIME (Core/Hash.h). ContentTests fails if these ever differ.
FNV_OFFSET_BASIS = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
MASK_64 = (1 << 64) - 1

# An id names a C++ array in the generated header, so it is an identifier: T1, U5, S3. A pose's id ends in a lower-case
# letter, S3c, which the array's name spells in upper case, so no two ids may differ only in case (CheckIdsDiffer).
ID = CheckAssets.ASSET_ID


class View:
  """The client view of one layer file: a template's when it has a cost, a model's when it has none. A template's also
  carries what only the server's file holds: its parts and its critical part's name."""

  def __init__(self, _source, _id, _name, _size, _edgeMillimeters, _indices, _cost, _parts=None, _criticalPart=""):
    self.source = _source  # the layer file's name, for the generated comment
    self.id = _id
    self.name = _name
    self.width, self.height, self.depth = _size
    self.edgeMillimeters = _edgeMillimeters
    self.indices = _indices  # bytes, x fastest, then z, then y, mirrored in x from the layer file (ADR-017)
    self.cost = _cost  # (stone m3, timber m3), or None for a model
    self.parts = _parts  # bytes in the same order, 0 none, 1 critical, 2 foundation, 3 stock; None for a model
    self.criticalPart = _criticalPart  # the critical part's name, empty when it names none


# ── Reading ─────────────────────────────────────────────────────────────────────────────────────


CORNER_SHADE_SETS = ("structure", "terrain")  # in the order Content::CornerShadeSet numbers them
MAX_JITTER = 0.25


class PaletteLook:
  """What the client draws each palette index with (RendererLook.md, sections 3 to 5), 256 of each."""

  def __init__(self):
    self.colors = [0] * PALETTE_ENTRIES      # sRGB
    self.present = [False] * PALETTE_ENTRIES  # whether the palette has the index
    self.jitter = [0.0] * PALETTE_ENTRIES     # the HSL lightness range: offsets fall within +-jitter/2
    self.shadeSets = [0] * PALETTE_ENTRIES    # an index into CORNER_SHADE_SETS
    self.cornerShades = []                    # each set's four shade values, ao level 0 to 3


def ReadPaletteLook():
  data = json.loads(PALETTE.read_text(encoding="utf-8"))
  entries = data.get("entries") or data.get("materials")
  look = PaletteLook()
  for entry in entries:
    index = entry["index"]
    if not 1 <= index < PALETTE_ENTRIES:
      raise ValueError(f"palette index {index} is outside 1-255")
    if look.present[index]:
      raise ValueError(f"palette index {index} appears twice")
    jitter = entry.get("jitter")
    if not isinstance(jitter, (int, float)) or not 0 <= jitter <= MAX_JITTER:
      raise ValueError(f"palette index {index} has a jitter of {jitter!r}, not a number from 0 to {MAX_JITTER}")
    shadeSet = entry.get("aoSet")
    if shadeSet not in CORNER_SHADE_SETS:
      raise ValueError(f"palette index {index} has an aoSet of {shadeSet!r}, not one of {', '.join(CORNER_SHADE_SETS)}")
    look.colors[index] = int(entry["hex"].lstrip("#"), 16)
    look.present[index] = True
    look.jitter[index] = float(jitter)
    look.shadeSets[index] = CORNER_SHADE_SETS.index(shadeSet)
  for name in CORNER_SHADE_SETS:
    shades = data.get("ao", {}).get(name)
    if not isinstance(shades, list) or len(shades) != 4 or not all(isinstance(value, (int, float)) and 0 < value <= 1 for value in shades):
      raise ValueError(f"the palette's ao.{name} is {shades!r}, not four shades in (0, 1]")
    look.cornerShades.append([float(value) for value in shades])
  return look


def WholeNumber(_value):
  return isinstance(_value, (int, float)) and float(_value).is_integer() and _value >= 0


def DecorationHosts(_palette):
  """{(decoration index, host behaviour): the entry that decorates it so}, from Palette.json's `decorates`."""
  hosts = {}
  for index, entry in sorted(_palette.items(), key=lambda item: item[0] if isinstance(item[0], int) else -1):
    decorated = entry.get("decorates")
    if decorated is not None:
      hosts.setdefault((decorated, entry.get("behavior")), index)
  return hosts


PART_NUMBERS = {CheckAssets.NO_PART: 0, CheckAssets.CRITICAL: 1, CheckAssets.FOUNDATION: 2, CheckAssets.STOCK: 3}


def ReadView(_report, _path, _palette, _isTemplate):
  """One layer file's client view, or None after reporting why it has none."""
  data = CheckAssets.ReadJson(_report, _path)
  if data is None:
    return None
  if not CheckAssets.IsLayerFile(data):
    _report.Problem(_path, "is not a layer file: it has no layers or no legend")
    return None
  asset = CheckAssets.ReadAsset(_report, _path, data, _palette)
  if asset is None:
    return None
  assetId, name = data.get("id"), data.get("name")
  if not isinstance(assetId, str) or not ID.fullmatch(assetId):
    _report.Problem(_path, f"its id {assetId!r} is not an identifier such as T1")
    return None
  if not isinstance(name, str) or not name:
    _report.Problem(_path, "has no name")
    return None
  edge = asset.voxelSize * 1000 if isinstance(asset.voxelSize, (int, float)) else None
  if edge is None or not WholeNumber(edge) or edge == 0:
    _report.Problem(_path, f"its voxelSize {asset.voxelSize!r} is not a whole number of millimetres")
    return None
  if not asset.voxels:
    _report.Problem(_path, "holds no voxels")
    return None

  low = [min(position[axis] for position in asset.voxels) for axis in range(3)]
  size = [max(position[axis] for position in asset.voxels) - low[axis] + 1 for axis in range(3)]
  width, height, depth = size

  cost, parts, criticalPart, written = None, None, "", dict(asset.voxels)
  if _isTemplate:
    behaviors, unresolved = CheckAssets.ResolveBehaviors(asset, _palette)
    if unresolved:
      _report.Problem(_path, f"{len(unresolved)} decoration(s) do not resolve, so its cost cannot be counted "
                             "(ADR-005, decision 6)")
      return None
    counted = CheckAssets.CountCost(asset, behaviors)
    if not WholeNumber(counted["stone"]) or not WholeNumber(counted["timber"]):
      _report.Problem(_path, f"its cost of {counted['stone']} m3 of stone and {counted['timber']} m3 of timber is "
                             "not a whole number of m3")
      return None
    cost = (int(counted["stone"]), int(counted["timber"]))
    # Each decoration is written as the entry that decorates it with its host's behaviour (M2's values, item 7).
    hosts = DecorationHosts(_palette)
    for position, index in asset.voxels.items():
      if _palette.get(index, {}).get("behavior") == CheckAssets.DECORATION:
        host = hosts.get((index, behaviors[position]))
        if host is None:
          _report.Problem(_path, f"index {index} decorates {behaviors[position]} at {CheckAssets.At(position)}, and no "
                                 "palette entry decorates it so (GameDesign.md, M2's values, item 7)")
          return None
        written[position] = host
    named = data.get("criticalPart")
    if isinstance(named, str) and named.strip() and named.split()[0] != "None":
      criticalPart = named
    parts = bytearray(width * height * depth)

  # Mirrored in x: a layer file is drawn in the prototype's right-handed frame and the world is left-handed, so
  # the mirror keeps its front +z and makes it look as drawn (ADR-017).
  indices = bytearray(width * height * depth)
  for (x, y, z), index in written.items():
    at = (width - 1 - (x - low[0])) + width * ((z - low[2]) + depth * (y - low[1]))
    indices[at] = index
    if parts is not None:
      parts[at] = PART_NUMBERS.get(asset.Part((x, y, z)), 0)
  return View(_path.name, assetId, name, size, int(edge), bytes(indices), cost,
              bytes(parts) if parts is not None else None, criticalPart)


def ReadViews(_report, _folder, _palette, _isTemplate):
  """Every layer file's view in _folder, in ascending id."""
  views = [ReadView(_report, path, _palette, _isTemplate) for path in sorted(_folder.glob("*.json"))]
  views = sorted((view for view in views if view is not None), key=lambda view: view.id)
  CheckIdsDiffer(_report, _folder, views)
  return views


def CheckIdsDiffer(_report, _folder, _items):
  """Reports two of _items, each with an id and a source, whose ids are the same or differ only in case."""
  seen = {}
  for item in _items:
    first = seen.setdefault(item.id.upper(), item)
    if first is not item:
      alike = "the id" if first.id == item.id else "an id differing only in case from"
      _report.Problem(_folder / item.source, f"has {alike} {first.id}, as {first.source} does")


def ReadLineModels(_report, _folder):
  """Every line model in _folder, in ascending id (ADR-018), read and checked by the asset check's own code, so the bake
  and the check cannot disagree on what one is."""
  models = []
  for path in sorted(_folder.glob("*.json")):
    data = CheckAssets.ReadJson(_report, path)
    if data is None:
      continue
    if not CheckAssets.IsLineModel(data):
      _report.Problem(path, "is not a line model: it has no segments or no drawnLengthMetres (ADR-018)")
      continue
    model = CheckAssets.ReadLineModel(_report, path, data)
    if model is not None:
      models.append(model)
  models.sort(key=lambda model: model.id)
  CheckIdsDiffer(_report, _folder, models)
  return models


# ── The shared hash ─────────────────────────────────────────────────────────────────────────────


def Fnv1a64(_bytes, _seed=FNV_OFFSET_BASIS):
  result = _seed
  for value in _bytes:
    result = ((result ^ value) * FNV_PRIME) & MASK_64
  return result


def SharedEncoding(_colors, _present, _templates):
  """The canonical encoding of the client view of the shared sources, as this file's docstring defines it."""
  out = bytearray()
  for index in range(PALETTE_ENTRIES):
    out += struct.pack("<BI", 1 if _present[index] else 0, _colors[index])
  out += struct.pack("<I", len(_templates))
  for view in _templates:
    for text in (view.id, view.name):
      raw = text.encode("utf-8")
      out += struct.pack("<I", len(raw)) + raw
    out += struct.pack("<IIII", view.width, view.height, view.depth, view.edgeMillimeters)
    out += struct.pack("<II", *view.cost)
    out += view.indices
  return bytes(out)


# ── Writing ─────────────────────────────────────────────────────────────────────────────────────


def Header(_sources, _includes, _body):
  """A generated header: what it was baked from, the standard headers it includes, and _body in Content::Generated."""
  includes = "".join(f"#include <{name}>\n" for name in _includes)
  return (f"// Generated by Tools/BakeContent.py from {_sources}. Do not edit.\n"
          "#pragma once\n\n"
          f"{includes}\n"
          "namespace Content::Generated\n{\n\n"
          f"{_body}"
          "} // namespace Content::Generated\n")


def CppString(_text):
  """A C++ narrow string literal of _text's UTF-8 bytes, whatever the compiler takes the source's encoding to be."""
  out = []
  for value in _text.encode("utf-8"):
    out.append(chr(value) if 0x20 <= value < 0x7F and chr(value) not in '"\\' else f"\\{value:03o}")
  return '"' + "".join(out) + '"'


def FloatLiteral(_value):
  return f"{_value!r}F" if "." in repr(_value) or "e" in repr(_value) else f"{_value!r}.0F"


def PaletteHeader(_look):
  rows = ",\n".join("  " + ", ".join(f"0x{_look.colors[i + j]:06x}u" for j in range(8))
                    for i in range(0, PALETTE_ENTRIES, 8))
  flags = ",\n".join("  " + ", ".join("true" if _look.present[i + j] else "false" for j in range(16))
                     for i in range(0, PALETTE_ENTRIES, 16))
  jitter = ",\n".join("  " + ", ".join(FloatLiteral(_look.jitter[i + j]) for j in range(16))
                      for i in range(0, PALETTE_ENTRIES, 16))
  sets = ",\n".join("  " + ", ".join(str(_look.shadeSets[i + j]) for j in range(32))
                    for i in range(0, PALETTE_ENTRIES, 32))
  shades = ",\n".join("  {" + ", ".join(FloatLiteral(value) for value in row) + "}" for row in _look.cornerShades)
  return Header("GameData/Server/Palette.json", ["array", "cstdint"],
                f"inline constexpr std::array<std::uint32_t, {PALETTE_ENTRIES}> PALETTE_SRGB = {{\n{rows}}};\n\n"
                f"inline constexpr std::array<bool, {PALETTE_ENTRIES}> PALETTE_PRESENT = {{\n{flags}}};\n\n"
                f"inline constexpr std::array<float, {PALETTE_ENTRIES}> PALETTE_JITTER = {{\n{jitter}}};\n\n"
                f"// By index, the corner-shade set: {', '.join(f'{i} {name}' for i, name in enumerate(CORNER_SHADE_SETS))}.\n"
                f"inline constexpr std::array<std::uint8_t, {PALETTE_ENTRIES}> PALETTE_SHADE_SET = {{\n{sets}}};\n\n"
                f"inline constexpr std::array<std::array<float, 4>, {len(CORNER_SHADE_SETS)}> CORNER_SHADES = {{{{\n{shades}}}}};\n\n")


def IndicesArray(_name, _view):
  """The view's indices as a constexpr array, one row of x per line, each the layer file's row reversed."""
  lines = []
  for y in range(_view.height):
    lines.append(f"  // y = {y}")
    for z in range(_view.depth):
      start = _view.width * (z + _view.depth * y)
      lines.append("  " + ", ".join(str(value) for value in _view.indices[start : start + _view.width]) + ",")
  return (f"// {_view.id}, {_view.name}, from {_view.source}: {_view.width} x {_view.height} x {_view.depth} voxels "
          f"of {_view.edgeMillimeters} mm.\n"
          f"inline constexpr std::array<std::uint8_t, {len(_view.indices)}> {_name} = {{\n"
          + "\n".join(lines) + "\n};\n\n")


def Grid(_name, _view):
  return (f".grid = {{.widthVoxels = {_view.width}, .heightVoxels = {_view.height}, .depthVoxels = {_view.depth}, "
          f".voxelEdgeMillimeters = {_view.edgeMillimeters}, .indices = {_name}}}")


def TemplateHeader(_templates):
  body = ("// The client view of each template, in ascending id (ADR-008, decision 2). Content/Catalog.cpp includes\n"
          "// this after Catalog.h, which declares TemplateView.\n\n")
  entries = []
  for view in _templates:
    name = f"TEMPLATE_{view.id}_INDICES"
    body += IndicesArray(name, view)
    stone, timber = view.cost
    entries.append(f"  {{.id = {CppString(view.id)}, .name = {CppString(view.name)}, {Grid(name, view)}, "
                   f".stoneM3 = {stone}, .timberM3 = {timber}}},")
  body += (f"inline constexpr std::array<TemplateView, {len(_templates)}> TEMPLATES = {{{{\n"
           + "\n".join(entries) + "\n}};\n\n")
  return Header("GameData/Server/Templates/", ["array", "cstdint"], body)


def ModelHeader(_models):
  body = ("// Each model, in ascending id (ADR-005, decision 8). Content/Catalog.cpp includes this after Catalog.h,\n"
          "// which declares ModelView.\n\n")
  entries = []
  for view in _models:
    name = f"MODEL_{view.id.upper()}_INDICES"
    body += IndicesArray(name, view)
    entries.append(f"  {{.id = {CppString(view.id)}, .name = {CppString(view.name)}, {Grid(name, view)}}},")
  body += f"inline constexpr std::array<ModelView, {len(_models)}> MODELS = {{{{\n" + "\n".join(entries) + "\n}};\n\n"
  return Header("GameData/Client/Models/", ["array", "cstdint"], body)


def LineModelHeader(_models):
  body = ("// Each line model, in ascending id (ADR-018). Content/Catalog.cpp includes this after Catalog.h, which declares\n"
          "// LineModelView and LineSegmentView.\n\n")
  entries = []
  for model in _models:
    name = f"LINE_MODEL_{model.id.upper()}_SEGMENTS"
    rows = []
    for start, end, srgb, width, tapers in model.segments:
      rows.append(f"  {{.fromMillimeters = {start}, .toMillimeters = {end}, .colorSrgb = 0x{srgb or 0:06x}u, "
                  f".factionColor = {'true' if srgb is None else 'false'}, .widthPixels = {width}, "
                  f".tapers = {'true' if tapers else 'false'}}},")
    body += (f"// {model.id}, {model.name}, from {model.source}: {CheckAssets.Plural(len(model.segments), 'segment')} over "
             f"{model.drawnLengthMillimeters} mm.\n"
             f"inline constexpr std::array<LineSegmentView, {len(model.segments)}> {name} = {{{{\n" + "\n".join(rows) + "\n}};\n\n")
    entries.append(f"  {{.id = {CppString(model.id)}, .name = {CppString(model.name)}, "
                   f".drawnLengthMillimeters = {model.drawnLengthMillimeters}, .dotBelowPixels = {model.dotBelowPixels}, "
                   f".dotWidthPixels = {model.dotWidthPixels}, .segments = {{{name}}}}},")
  body += f"inline constexpr std::array<LineModelView, {len(_models)}> LINE_MODELS = {{{{\n" + "\n".join(entries) + "\n}};\n\n"
  return Header("GameData/Client/Lines/", ["array", "cstdint"], body)



def SharedHashHeader(_hash):
  return Header("GameData/Server/Palette.json and GameData/Server/Templates/", ["cstdint"],
                "// FNV-1a 64 over the canonical encoding of the client view of the palette and the templates\n"
                "// (ADR-008, decision 5). Content/SharedHash.h defines the encoding.\n"
                f"inline constexpr std::uint64_t SHARED_HASH = 0x{_hash:016x}ULL;\n\n")


# ── The server's file (ADR-016) ─────────────────────────────────────────────────────────────────

SERVER_FILE = "ServerData.bin"
SERVER_MAGIC = b"ERSD"
SERVER_FORMAT_VERSION = 1
SECTION_VERSIONS = {"MATL": 1, "TMPL": 1, "RULE": 1}  # in the file's order
# Simulation::Behavior, in its declared order, by Palette.json's behaviour names; 0 is none.
BEHAVIORS = ("none", "earth", "commonRock", "buildingStone", "bedrock", "timber", "leaves", "water", "stoneRubble",
             "wasteRubble")
# Simulation::Support: 0 none, 1 carries (and is never judged by span), 2 spans, 3 anchors (ADR-004, decision 2).
SUPPORT = {"earth": 1, "commonRock": 2, "buildingStone": 2, "bedrock": 3, "timber": 2, "leaves": 2, "water": 0,
           "stoneRubble": 1, "wasteRubble": 1}
GROUND_FLAG = 1 << 0
LIQUID_FLAG = 1 << 1
MAX_SPAN_VOXELS = 254  # the support search counts distances in a u8, with 255 for unreached


def Materials(_report, _palette, _rules):
  """MATL: each palette index resolved to one rule, six u8 apiece."""
  path = PALETTE
  out = bytearray()
  for index in range(PALETTE_ENTRIES):
    entry = _palette.get(index)
    if entry is None:
      out += bytes(6)
      continue
    behavior = entry.get("behavior")
    if behavior in (CheckAssets.DECORATION, CheckAssets.MODEL_ONLY):
      out += bytes([1, 0, 0, 0, 0, 0])
      continue
    if behavior not in SUPPORT:
      _report.Problem(path, f"index {index} has the behaviour {behavior!r}, which the server's file does not number")
      continue
    ground, rubble = entry.get("ground"), entry.get("rubble")
    if not isinstance(ground, bool):
      _report.Problem(path, f"index {index} gives no server-side ground flag (GameDesign.md, M2's values, item 14)")
      continue
    if not isinstance(rubble, int) or (rubble != 0 and rubble not in _palette):
      _report.Problem(path, f"index {index} gives no rubble index that the palette has (M2's values, item 7)")
      continue
    span = 0
    if SUPPORT[behavior] == 2:
      key = f"{behavior}SpanVoxels"
      span = _rules.get(key)
      if not isinstance(span, int) or not 0 < span <= MAX_SPAN_VOXELS:
        _report.Problem(path, f"index {index} spans, and Rules.json gives no {key} from 1 to {MAX_SPAN_VOXELS}")
        continue
    flags = (GROUND_FLAG if ground else 0) | (LIQUID_FLAG if behavior == "water" else 0)
    out += bytes([1, BEHAVIORS.index(behavior), SUPPORT[behavior], span, flags, rubble])
  return bytes(out)


def LengthPrefixed(_text):
  raw = _text.encode("utf-8")
  return struct.pack("<I", len(raw)) + raw


def ServerTemplates(_templates):
  """TMPL: the client view of each template, with its parts and its critical part's name."""
  out = bytearray(struct.pack("<I", len(_templates)))
  for view in _templates:
    out += LengthPrefixed(view.id) + LengthPrefixed(view.name) + LengthPrefixed(view.criticalPart)
    out += struct.pack("<IIII", view.width, view.height, view.depth, view.edgeMillimeters)
    out += struct.pack("<II", *view.cost)
    out += view.indices + view.parts
  return bytes(out)


def ServerRules(_rules):
  """RULE: each converted value, in ascending key order."""
  out = bytearray(struct.pack("<I", len(_rules)))
  for key in sorted(_rules):
    out += LengthPrefixed(key) + struct.pack("<q", _rules[key])
  return bytes(out)


def ServerFile(_sharedHash, _sections):
  """The whole file: the header, then the body of the shared hash, the directory and _sections ({tag: bytes})."""
  directorySize = 8 + 4 + 16 * len(_sections)
  directory, payload = bytearray(), bytearray()
  for tag, section in _sections.items():
    directory += tag.encode("ascii") + struct.pack("<III", SECTION_VERSIONS[tag], directorySize + len(payload), len(section))
    payload += section
  body = struct.pack("<QI", _sharedHash, len(_sections)) + bytes(directory) + bytes(payload)
  return SERVER_MAGIC + struct.pack("<IQ", SERVER_FORMAT_VERSION, Fnv1a64(body)) + body


def BakeServer(_report, _palette, _templates, _sharedHash):
  """The server's file, or None when a problem stops it."""
  rules = CheckAssets.ReadRules(_report, ROOT)
  materials = Materials(_report, _palette, rules)
  if _report.problems:
    return None
  return ServerFile(_sharedHash, {"MATL": materials, "TMPL": ServerTemplates(_templates), "RULE": ServerRules(rules)})


def WriteBytesIfChanged(_path, _bytes):
  """Leaves an unchanged file's timestamp alone, so the build's incremental check sees nothing to do."""
  if _path.exists() and _path.read_bytes() == _bytes:
    return False
  _path.write_bytes(_bytes)
  return True


def WriteIfChanged(_path, _text):
  """Leaves an unchanged header's timestamp alone, so a rebuild does not recompile its users."""
  if _path.exists() and _path.read_text(encoding="utf-8") == _text:
    return False
  _path.write_text(_text, encoding="utf-8")
  return True


def Main(_arguments):
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument("--client", type=Path, help="the folder the client's headers go to")
  parser.add_argument("--server", type=Path, help="the folder the server's file goes to: <outputdir>\\ServerData")
  arguments = parser.parse_args(_arguments)
  if arguments.client is None and arguments.server is None:
    parser.error("give --client, --server or both")
  if sys.version_info < (3, 12):
    print("BakeContent: Python 3.12 or later is needed (ADR-010)")
    return 1

  look = ReadPaletteLook()
  colors, present = look.colors, look.present
  report = CheckAssets.Report(ROOT)
  palette, _ = CheckAssets.ReadPalette(report, ROOT)
  templates = ReadViews(report, TEMPLATES, palette, True)
  # Only the client half reads GameData/Client/ (ADR-008, decision 2; the asset check's check 11).
  models = ReadViews(report, MODELS, palette, False) if arguments.client is not None else []
  lineModels = ReadLineModels(report, LINES) if arguments.client is not None else []
  sharedHash = Fnv1a64(SharedEncoding(colors, present, templates))
  serverFile = BakeServer(report, palette, templates, sharedHash) if arguments.server is not None else None
  if report.problems:
    for problem in report.problems:
      print(f"BakeContent: {problem}")
    print(f"BakeContent: {len(report.problems)} problem(s), so nothing was written")
    return 1

  if arguments.client is not None:
    headers = {
      "PaletteTable.h": PaletteHeader(look),
      "TemplateTable.h": TemplateHeader(templates),
      "ModelTable.h": ModelHeader(models),
      "LineModelTable.h": LineModelHeader(lineModels),
      "SharedHashValue.h": SharedHashHeader(sharedHash),
    }
    arguments.client.mkdir(parents=True, exist_ok=True)
    for name, text in headers.items():
      changed = WriteIfChanged(arguments.client / name, text)
      print(f"BakeContent: {'wrote' if changed else 'kept'} {name}")
  if arguments.server is not None:
    arguments.server.mkdir(parents=True, exist_ok=True)
    changed = WriteBytesIfChanged(arguments.server / SERVER_FILE, serverFile)
    print(f"BakeContent: {'wrote' if changed else 'kept'} {SERVER_FILE}")
  return 0


if __name__ == "__main__":
  sys.exit(Main(sys.argv[1:]))
