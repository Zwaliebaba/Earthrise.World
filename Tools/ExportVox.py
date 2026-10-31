"""Write a MagicaVoxel .vox beside each voxel layer file (ADR-008).

The .json layer file is the source and is committed. The .vox is a working copy for editing in
MagicaVoxel, and .gitignore keeps it out of the repository. After editing, run ImportVox.py to
write the change back into the .json.

Usage:
  python Tools/ExportVox.py                 every layer file under GameData/
  python Tools/ExportVox.py <path> ...      the given layer files, or the layer files under folders
"""

import json
import sys
from pathlib import Path

from VoxFile import EncodeVox, Grid, IsLayerFile, LayerVoxels, ReadLayerFile, ReadPalette

ROOT = Path(__file__).resolve().parent.parent
GAME_DATA = ROOT / "GameData"
PALETTE = GAME_DATA / "Server" / "Palette.json"


def LayerFiles(_paths):
  for path in _paths:
    path = Path(path)
    candidates = sorted(path.rglob("*.json")) if path.is_dir() else [path]
    for candidate in candidates:
      try:
        data, _ = ReadLayerFile(candidate)
      except (OSError, json.JSONDecodeError):
        continue
      if IsLayerFile(data):
        yield candidate, data


def Main(_arguments):
  palette = ReadPalette(PALETTE)
  written = 0
  for jsonPath, data in LayerFiles(_arguments or [GAME_DATA]):
    width, height, depth = Grid(data)
    voxPath = jsonPath.with_suffix(".vox")
    voxPath.write_bytes(EncodeVox(width, height, depth, LayerVoxels(data), palette))
    written += 1
    print(f"wrote {voxPath.relative_to(ROOT) if voxPath.is_relative_to(ROOT) else voxPath}")
  print(f"{written} file(s) exported")
  return 0


if __name__ == "__main__":
  sys.exit(Main(sys.argv[1:]))
