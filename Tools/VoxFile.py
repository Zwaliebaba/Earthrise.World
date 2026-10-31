"""Reading and writing voxel layer files and MagicaVoxel .vox files (ADR-005, ADR-008).

Shared by ExportVox.py and ImportVox.py. Standard library only.

A layer file is JSON: `layers[y][z]` is a string over x, `.` is empty, and `legend` maps each
character to a palette index. A template adds `parts[y][z]` on the same grid. y is up and +z is the
front. A .vox stores voxel (x, y, z) at (x, depth - 1 - z, y), with the colour index equal to the
palette index, in MagicaVoxel's version 150.
"""

import json
import struct
from pathlib import Path

VOX_VERSION = 150
PALETTE_ENTRIES = 256


def IsLayerFile(_data):
  return isinstance(_data, dict) and "layers" in _data and "legend" in _data


def ReadLayerFile(_path):
  """Returns (data, style), where style records the file's line ending and final newline so that
  WriteLayerFile can keep them: .gitattributes checks JSON out with CRLF."""
  raw = Path(_path).read_bytes().decode("utf-8")
  newline = "\r\n" if "\r\n" in raw else "\n"
  return json.loads(raw), (newline, raw.endswith("\n"))


def WriteLayerFile(_path, _data, _style):
  newline, finalNewline = _style
  text = json.dumps(_data, indent=1, ensure_ascii=False)
  if finalNewline:
    text += "\n"
  Path(_path).write_bytes(text.replace("\n", newline).encode("utf-8"))


def LegendIndex(_entry):
  """A legend entry is either a palette index or {"material": index, ...}."""
  return _entry["material"] if isinstance(_entry, dict) else int(_entry)


def ReadPalette(_path):
  """Palette index -> (name, '#rrggbb')."""
  data = json.loads(Path(_path).read_text(encoding="utf-8"))
  entries = data.get("entries") or data.get("materials")
  return {entry["index"]: (entry["name"], entry["hex"]) for entry in entries}


def Grid(_data):
  """(width, height, depth) of a layer file, from its layers."""
  layers = _data["layers"]
  height = len(layers)
  depth = len(layers[0]) if height else 0
  width = len(layers[0][0]) if depth else 0
  return width, height, depth


def LayerVoxels(_data):
  """Voxels as (x, y, z, paletteIndex), in layer order: y, then z, then x."""
  legend = {char: LegendIndex(entry) for char, entry in _data["legend"].items()}
  voxels = []
  for y, layer in enumerate(_data["layers"]):
    for z, row in enumerate(layer):
      for x, char in enumerate(row):
        if char != ".":
          voxels.append((x, y, z, legend[char]))
  return voxels


def _Chunk(_id, _content, _children=b""):
  return _id + struct.pack("<II", len(_content), len(_children)) + _content + _children


def EncodeVox(_width, _height, _depth, _voxels, _palette):
  """_voxels: (x, y, z, index) in layer coordinates. _palette: index -> (name, hex)."""
  size = _Chunk(b"SIZE", struct.pack("<III", _width, _depth, _height))
  body = bytearray(struct.pack("<I", len(_voxels)))
  for x, y, z, index in _voxels:
    if not 1 <= index <= 255:
      raise ValueError(f"palette index {index} is outside 1-255")
    body += bytes((x, _depth - 1 - z, y, index))
  xyzi = _Chunk(b"XYZI", bytes(body))
  # MagicaVoxel's RGBA entry k is colour index k + 1. Unused entries are opaque black.
  rgba = bytearray()
  for k in range(PALETTE_ENTRIES):
    hexColor = _palette.get(k + 1, (None, "#000000"))[1]
    rgba += bytes.fromhex(hexColor[1:]) + b"\xff"
  main = _Chunk(b"MAIN", b"", size + xyzi + _Chunk(b"RGBA", bytes(rgba)))
  return b"VOX " + struct.pack("<I", VOX_VERSION) + main


def DecodeVox(_bytes):
  """Returns (width, height, depth, voxels as (x, y, z, index) in layer coordinates)."""
  if _bytes[:4] != b"VOX ":
    raise ValueError("not a MagicaVoxel file")
  sizes, models = [], []

  def Walk(_start, _end):
    offset = _start
    while offset < _end:
      chunkId = _bytes[offset : offset + 4]
      contentBytes, childBytes = struct.unpack("<II", _bytes[offset + 4 : offset + 12])
      content = _bytes[offset + 12 : offset + 12 + contentBytes]
      if chunkId == b"SIZE":
        sizes.append(struct.unpack("<III", content[:12]))
      elif chunkId == b"XYZI":
        count = struct.unpack("<I", content[:4])[0]
        models.append([tuple(content[4 + 4 * i : 8 + 4 * i]) for i in range(count)])
      if childBytes:
        Walk(offset + 12 + contentBytes, offset + 12 + contentBytes + childBytes)
      offset += 12 + contentBytes + childBytes

  Walk(8, len(_bytes))
  if len(sizes) != 1 or len(models) != 1:
    raise ValueError(f"expected one model, found {len(models)}; merge the model's objects in MagicaVoxel first")
  width, depth, height = sizes[0]
  voxels = [(vx, vz, depth - 1 - vy, index) for vx, vy, vz, index in models[0]]
  return width, height, depth, voxels
