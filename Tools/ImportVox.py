"""Write a .vox edited in MagicaVoxel back into its layer file (ADR-008).

The .json beside the .vox is rewritten: its layers come from the .vox, and everything else is kept.
A template's parts are kept for every voxel that is still there. A voxel the edit added has no part,
so set its part in the .json by hand if it belongs to a critical part, the foundation or stock. A
colour that the legend does not have yet gets a new legend character, named from the palette.

Usage:
    python Tools/ImportVox.py <file.vox> ...
"""

import string
import sys
from pathlib import Path

from VoxFile import DecodeVox, LegendIndex, ReadLayerFile, ReadPalette, WriteLayerFile

ROOT = Path(__file__).resolve().parent.parent
PALETTE = ROOT / "GameData" / "Server" / "Palette.json"
LEGEND_CHARACTERS = string.ascii_lowercase + string.ascii_uppercase + string.digits


def Import(_voxPath, _palette):
    jsonPath = _voxPath.with_suffix(".json")
    data, style = ReadLayerFile(jsonPath)
    width, height, depth, voxels = DecodeVox(_voxPath.read_bytes())

    legend = data["legend"]
    charByIndex = {LegendIndex(entry): char for char, entry in legend.items()}
    for index in sorted({voxel[3] for voxel in voxels} - set(charByIndex)):
        if index not in _palette:
            raise ValueError(f"{_voxPath.name}: colour index {index} is not in Palette.json")
        char = next(c for c in LEGEND_CHARACTERS if c not in legend)
        name, hexColor = _palette[index]
        legend[char] = {"material": index, "name": name, "hex": hexColor}
        charByIndex[index] = char
        print(f"  new legend '{char}': {index} {name}")

    grid = [[["."] * width for _ in range(depth)] for _ in range(height)]
    for x, y, z, index in voxels:
        grid[y][z][x] = charByIndex[index]
    oldLayers = data["layers"]
    data["layers"] = [["".join(row) for row in layer] for layer in grid]

    added = 0
    if "parts" in data:
        oldParts = data["parts"]
        parts = [[["."] * width for _ in range(depth)] for _ in range(height)]
        for x, y, z, _ in voxels:
            inOld = y < len(oldLayers) and z < len(oldLayers[y]) and x < len(oldLayers[y][z])
            if inOld and oldLayers[y][z][x] != ".":
                parts[y][z][x] = oldParts[y][z][x]
            else:
                added += 1
        data["parts"] = [["".join(row) for row in layer] for layer in parts]

    if "size" in data:
        data["size"] = {"x": width, "y": height, "z": depth}
    WriteLayerFile(jsonPath, data, style)
    print(f"updated {jsonPath.name}: {len(voxels)} voxels" + (f", {added} without a part" if added else ""))


def Main(_arguments):
    if not _arguments:
        print(__doc__)
        return 2
    palette = ReadPalette(PALETTE)
    for argument in _arguments:
        Import(Path(argument), palette)
    return 0


if __name__ == "__main__":
    sys.exit(Main(sys.argv[1:]))
