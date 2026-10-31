"""Bakes GameData/ into what the build compiles in or ships (ADR-005, ADR-008, ADR-010 decision 7).

The client half writes C++ headers into the build's intermediate folder, where the Content library compiles them
in: the palette's colours, the client view of every template in GameData/Server/Templates/, every model in
GameData/Client/Models/, and the shared hash. The server half, writing <outputdir>\\ServerData, follows in M2.

The client view of a template (ADR-008, decision 2) is its id and display name, its size in voxels after trimming
the empty padding, so that the occupied voxels' minimum corner is its origin (ADR-005, decision 2), its voxel edge in
millimetres, one palette index per voxel in layer-file order (x fastest, then z, then y; 0 is empty), and its counted
cost in m3 of stone and of timber (ADR-005, decision 7). The cost is counted by Tools/CheckAssets.py's own code, so
the bake and the asset check cannot disagree on it. Nothing server-only goes in, and check 9 of the asset check
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
Models are client content, not shared, so the hash does not cover them. Content/SharedHash.h states the same
encoding, and ContentTests recomputes the hash from the compiled tables, so the two halves cannot drift.

Generated headers are build output: never committed, never edited, and not linted (they sit in a Generated/ folder,
outside .clang-tidy's header filter). Each is rewritten only when its text changes.

Usage:
  python Tools/BakeContent.py --client OUT_DIR
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
PALETTE_ENTRIES = 256

# Core::FNV_OFFSET_BASIS and Core::FNV_PRIME (Core/Hash.h). ContentTests fails if these ever differ.
FNV_OFFSET_BASIS = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
MASK_64 = (1 << 64) - 1

# An id names a C++ array in the generated header, so it is an identifier: T1, U5, S3.
ID = re.compile(r"[A-Z][A-Z0-9]*")


class View:
  """The client view of one layer file: a template's when it has a cost, a model's when it has none."""

  def __init__(self, _source, _id, _name, _size, _edgeMillimeters, _indices, _cost):
    self.source = _source  # the layer file's name, for the generated comment
    self.id = _id
    self.name = _name
    self.width, self.height, self.depth = _size
    self.edgeMillimeters = _edgeMillimeters
    self.indices = _indices  # bytes, x fastest, then z, then y
    self.cost = _cost  # (stone m3, timber m3), or None for a model


# ── Reading ─────────────────────────────────────────────────────────────────────────────────────


def ReadPaletteColors():
  """([sRGB colour for each index], [whether the palette has that index]), 256 of each."""
  data = json.loads(PALETTE.read_text(encoding="utf-8"))
  entries = data.get("entries") or data.get("materials")
  colors = [0] * PALETTE_ENTRIES
  present = [False] * PALETTE_ENTRIES
  for entry in entries:
    index = entry["index"]
    if not 1 <= index < PALETTE_ENTRIES:
      raise ValueError(f"palette index {index} is outside 1-255")
    if present[index]:
      raise ValueError(f"palette index {index} appears twice")
    colors[index] = int(entry["hex"].lstrip("#"), 16)
    present[index] = True
  return colors, present


def WholeNumber(_value):
  return isinstance(_value, (int, float)) and float(_value).is_integer() and _value >= 0


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
  indices = bytearray(width * height * depth)
  for (x, y, z), index in asset.voxels.items():
    indices[(x - low[0]) + width * ((z - low[2]) + depth * (y - low[1]))] = index

  cost = None
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
  return View(_path.name, assetId, name, size, int(edge), bytes(indices), cost)


def ReadViews(_report, _folder, _palette, _isTemplate):
  """Every layer file's view in _folder, in ascending id."""
  views = [ReadView(_report, path, _palette, _isTemplate) for path in sorted(_folder.glob("*.json"))]
  views = sorted((view for view in views if view is not None), key=lambda view: view.id)
  for first, second in zip(views, views[1:]):
    if first.id == second.id:
      _report.Problem(_folder / second.source, f"has the id {second.id}, as {first.source} does")
  return views


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


def PaletteHeader(_colors, _present):
  rows = ",\n".join("  " + ", ".join(f"0x{_colors[i + j]:06x}u" for j in range(8))
                    for i in range(0, PALETTE_ENTRIES, 8))
  flags = ",\n".join("  " + ", ".join("true" if _present[i + j] else "false" for j in range(16))
                     for i in range(0, PALETTE_ENTRIES, 16))
  return Header("GameData/Server/Palette.json", ["array", "cstdint"],
                f"inline constexpr std::array<std::uint32_t, {PALETTE_ENTRIES}> PALETTE_SRGB = {{\n{rows}}};\n\n"
                f"inline constexpr std::array<bool, {PALETTE_ENTRIES}> PALETTE_PRESENT = {{\n{flags}}};\n\n")


def IndicesArray(_name, _view):
  """The view's indices as a constexpr array, one row of x per line, as the layer file holds them."""
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
    name = f"MODEL_{view.id}_INDICES"
    body += IndicesArray(name, view)
    entries.append(f"  {{.id = {CppString(view.id)}, .name = {CppString(view.name)}, {Grid(name, view)}}},")
  body += f"inline constexpr std::array<ModelView, {len(_models)}> MODELS = {{{{\n" + "\n".join(entries) + "\n}};\n\n"
  return Header("GameData/Client/Models/", ["array", "cstdint"], body)


def SharedHashHeader(_hash):
  return Header("GameData/Server/Palette.json and GameData/Server/Templates/", ["cstdint"],
                "// FNV-1a 64 over the canonical encoding of the client view of the palette and the templates\n"
                "// (ADR-008, decision 5). Content/SharedHash.h defines the encoding.\n"
                f"inline constexpr std::uint64_t SHARED_HASH = 0x{_hash:016x}ULL;\n\n")


def WriteIfChanged(_path, _text):
  """Leaves an unchanged header's timestamp alone, so a rebuild does not recompile its users."""
  if _path.exists() and _path.read_text(encoding="utf-8") == _text:
    return False
  _path.write_text(_text, encoding="utf-8")
  return True


def Main(_arguments):
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument("--client", type=Path, required=True, help="the folder the client's headers go to")
  arguments = parser.parse_args(_arguments)
  if sys.version_info < (3, 12):
    print("BakeContent: Python 3.12 or later is needed (ADR-010)")
    return 1

  colors, present = ReadPaletteColors()
  report = CheckAssets.Report(ROOT)
  palette, _ = CheckAssets.ReadPalette(report, ROOT)
  templates = ReadViews(report, TEMPLATES, palette, True)
  models = ReadViews(report, MODELS, palette, False)
  if report.problems:
    for problem in report.problems:
      print(f"BakeContent: {problem}")
    print(f"BakeContent: {len(report.problems)} problem(s), so nothing was written")
    return 1

  headers = {
    "PaletteTable.h": PaletteHeader(colors, present),
    "TemplateTable.h": TemplateHeader(templates),
    "ModelTable.h": ModelHeader(models),
    "SharedHashValue.h": SharedHashHeader(Fnv1a64(SharedEncoding(colors, present, templates))),
  }
  arguments.client.mkdir(parents=True, exist_ok=True)
  for name, text in headers.items():
    changed = WriteIfChanged(arguments.client / name, text)
    print(f"BakeContent: {'wrote' if changed else 'kept'} {name}")
  return 0


if __name__ == "__main__":
  sys.exit(Main(sys.argv[1:]))
