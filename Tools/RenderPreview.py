"""Renders a layer file's preview, a .png for Design/Previews/ (MvpAssets.md, *Previews*), or the font's atlases.

The preview is the layer file as drawn: y up, +z the front, +x to the right, seen from the front right and above in an
isometric projection, each voxel a cube of its legend's colour with its top, front and right faces shaded apart. Only
faces open to air are drawn, back to front. Nothing reads the previews; they are for the owner to judge an asset by.
It needs Pillow, which a build-time tool may use (AGENTS.md, R14): nothing here ships or links.

Given the font's glyph file, GameData/Client/Fonts/EarthriseMason-glyphs.json, it writes EarthriseMason-atlas-1x.png,
-2x.png and -3x.png instead (Hud.md §9): each glyph's rows in white on transparent at its cell's x and y, below its top,
16 cells of 10 texels across, scaled by nearest neighbour. The atlas the client builds, Client::GlyphAtlas, lays the
cells out the same way, in code-point order.

Usage:
  python Tools/RenderPreview.py GameData/Server/Templates/earthrise-t5-gatehouse-1m.json [more files]
                                [--out Design/Previews] [--pixels 12]
"""

import argparse
import json
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
SHADES = {"top": 1.0, "front": 0.82, "right": 0.66}
BACKGROUND = (238, 234, 226, 255)
MARGIN_PIXELS = 16


def Voxels(_data):
  """{(x, y, z): '#rrggbb'} from a layer file's layers[y][z][x] and its legend."""
  voxels = {}
  for y, layer in enumerate(_data["layers"]):
    for z, row in enumerate(layer):
      for x, symbol in enumerate(row):
        if symbol != "." and symbol in _data["legend"]:
          voxels[(x, y, z)] = _data["legend"][symbol]["hex"]
  return voxels


def Shade(_hex, _factor):
  value = int(_hex.lstrip("#"), 16)
  return tuple(min(255, round(((value >> shift) & 0xFF) * _factor)) for shift in (16, 8, 0)) + (255,)


def Render(_path, _out, _pixels):
  data = json.loads(Path(_path).read_text(encoding="utf-8"))
  voxels = Voxels(data)
  if not voxels:
    raise ValueError(f"{_path} holds no voxels")
  across = _pixels * math.cos(math.radians(30))
  down = _pixels * math.sin(math.radians(30))

  def Screen(_x, _y, _z):
    return ((_x - _z) * across, (_x + _z) * down - _y * _pixels)

  corners = [Screen(x + dx, y + dy, z + dz) for (x, y, z) in voxels for dx in (0, 1) for dy in (0, 1) for dz in (0, 1)]
  left = min(point[0] for point in corners) - MARGIN_PIXELS
  top = min(point[1] for point in corners) - MARGIN_PIXELS
  width = math.ceil(max(point[0] for point in corners) - left + MARGIN_PIXELS)
  height = math.ceil(max(point[1] for point in corners) - top + MARGIN_PIXELS)
  image = Image.new("RGBA", (width, height), BACKGROUND)
  draw = ImageDraw.Draw(image)

  def Polygon(_points, _color):
    draw.polygon([(px - left, py - top) for px, py in _points], fill=_color, outline=Shade("#000000", 1)[:3] + (40,))

  # Back to front: the viewer stands at +x, +y, +z, so a cube nearer it, by x + y + z, is drawn later.
  for (x, y, z) in sorted(voxels, key=lambda position: (sum(position), position[1])):
    color = voxels[(x, y, z)]
    if (x, y + 1, z) not in voxels:
      Polygon([Screen(x, y + 1, z), Screen(x + 1, y + 1, z), Screen(x + 1, y + 1, z + 1), Screen(x, y + 1, z + 1)],
              Shade(color, SHADES["top"]))
    if (x, y, z + 1) not in voxels:
      Polygon([Screen(x, y, z + 1), Screen(x + 1, y, z + 1), Screen(x + 1, y + 1, z + 1), Screen(x, y + 1, z + 1)],
              Shade(color, SHADES["front"]))
    if (x + 1, y, z) not in voxels:
      Polygon([Screen(x + 1, y, z), Screen(x + 1, y, z + 1), Screen(x + 1, y + 1, z + 1), Screen(x + 1, y + 1, z)],
              Shade(color, SHADES["right"]))
  target = Path(_out) / (Path(_path).stem + ".png")
  image.save(target)
  return target


def RenderFontAtlas(_path, _out):
  """The font's three atlas previews."""
  data = json.loads(Path(_path).read_text(encoding="utf-8"))
  cell = data["cell"]
  rows = max(glyph["y"] for glyph in data["glyphs"]) // cell + 1
  atlas = Image.new("RGBA", (16 * cell, rows * cell), (0, 0, 0, 0))
  for glyph in data["glyphs"]:
    for row, text in enumerate(glyph["rows"]):
      for column, pixel in enumerate(text):
        if pixel == "#":
          atlas.putpixel((glyph["x"] + column, glyph["y"] + glyph["top"] + row), (255, 255, 255, 255))
  written = []
  for scale in (1, 2, 3):
    target = Path(_out) / f"EarthriseMason-atlas-{scale}x.png"
    atlas.resize((atlas.width * scale, atlas.height * scale), Image.NEAREST).save(target)
    written.append(target)
  return written


def Main(_arguments):
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument("files", nargs="+", type=Path)
  parser.add_argument("--out", type=Path, default=ROOT / "Design" / "Previews")
  parser.add_argument("--pixels", type=int, default=12, help="a voxel's edge on screen")
  arguments = parser.parse_args(_arguments)
  for path in arguments.files:
    if path.name.endswith("-glyphs.json"):
      for target in RenderFontAtlas(path, arguments.out):
        print(f"RenderPreview: wrote {target}")
    else:
      print(f"RenderPreview: wrote {Render(path, arguments.out, arguments.pixels)}")
  return 0


if __name__ == "__main__":
  sys.exit(Main(sys.argv[1:]))
