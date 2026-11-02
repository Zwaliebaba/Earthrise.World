"""Tests for Tools/CheckAssets.py: a check that passes an untouched tree proves nothing on its own.

The repository must pass. Each broken case copies GameData/, Tools/BakeContent.py and the modules the bake imports into
a temporary folder, breaks one thing, and asserts that the check reports a problem naming that reason. A case is judged
on the problems its break adds to those of an unbroken copy, so that it shows its own reason and not something already
there. Further cases test decision 6's tie and repeated-pass rules and the cost count directly, since no real template
has a tie.

Usage:
  python3 Tools/TestCheckAssets.py         every case
  python3 Tools/TestCheckAssets.py -v      with each case's name
"""

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))

import CheckAssets  # noqa: E402

T1 = "Server/Templates/earthrise-t1-hall-1m.json"
T2 = "Server/Templates/earthrise-t2-keep-1m.json"
T3 = "Server/Templates/earthrise-t3-stone-wall-standard-1m.json"
T5 = "Server/Templates/earthrise-t5-gatehouse-1m.json"
T6 = "Server/Templates/earthrise-t6-palisade-1m.json"
T7 = "Server/Templates/earthrise-t7-palisade-gate-1m.json"
E1 = "Illustrations/earthrise-e1-ditch-rampart-1m.json"
M1_LAYER_FILE = "Illustrations/earthrise-m1-ground-strata-1m.json"
SPEARMAN = "Client/Models/earthrise-u2-spearman-05m.json"
ARCHER = "Client/Models/earthrise-u3-archer-05m.json"
ARROW = "Client/Lines/earthrise-x2-arrow.json"
FONT = "Client/Fonts/EarthriseMason-glyphs.json"
MANIFEST = "Manifest.json"
PALETTE = "Server/Palette.json"
RULES = "Server/Rules.json"
UNITS = "Server/Units.json"
STRUCTURES = "Server/Structures.json"
SIEGE = "Server/Scenarios/Siege.json"
SIEGE_MIRROR = "Server/Scenarios/SiegeMirror.json"
BAKE = "Tools/BakeContent.py"
BAKE_IMPORTS = ("Tools/CheckAssets.py", "Tools/VoxFile.py")  # the bake counts costs with the asset check's code


def Copy(_destination):
  shutil.copytree(ROOT / "GameData", _destination / "GameData")
  (_destination / "Tools").mkdir()
  for relative in (BAKE,) + BAKE_IMPORTS:
    shutil.copy(ROOT / relative, _destination / relative)


def Change(_root, _relative, _change):
  """Reads a JSON file under GameData/, lets _change edit it in place, and writes it back."""
  path = _root / "GameData" / _relative
  data = json.loads(path.read_text(encoding="utf-8"))
  _change(data)
  path.write_text(json.dumps(data, indent=1, ensure_ascii=False), encoding="utf-8")


def Entry(_manifest, _id):
  return next(entry for entry in _manifest["assets"] if entry["id"] == _id)


def Segment(_lineModel, _number):
  return _lineModel["segments"][_number]


def SetCell(_layers, _x, _y, _z, _char):
  row = _layers[_y][_z]
  _layers[_y][_z] = row[:_x] + _char + row[_x + 1 :]


def Problems(_root):
  report, _, _, _ = CheckAssets.Check(_root)
  return report.problems


# ── The breaks, one per reason ──────────────────────────────────────────────────────────────────


def UnknownCharacter(_data):
  row = _data["layers"][0][0]
  x = next(i for i, char in enumerate(row) if char != ".")
  SetCell(_data["layers"], x, 0, 0, "q")


def FloatingVoxel(_data):
  """Sets one empty cell of the hollow keep whose six neighbours are empty too."""
  layers = _data["layers"]
  for y in range(1, len(layers) - 1):
    for z in range(1, len(layers[y]) - 1):
      for x in range(1, len(layers[y][z]) - 1):
        cells = [(x, y, z)] + CheckAssets.Neighbors((x, y, z))
        if all(layers[cy][cz][cx] == "." for cx, cy, cz in cells):
          SetCell(layers, x, y, z, "f")
          return
  raise AssertionError("the keep has no empty cell away from its walls")


def FittingsAmongStock(_data):
  """Marks everything in the palisade gate as stock except its iron fittings, which stay structure."""
  _data["parts"] = [["".join(part if char in ".k" else "3" for char, part in zip(row, parts)) for row, parts in
                     zip(layer, partLayer)] for layer, partLayer in zip(_data["layers"], _data["parts"])]


def OneSoldier(_data):
  """Dresses one iron voxel of the squad's third man-at-arms in cloth, so he alone differs from the model."""
  voxels = {(x, y, z): char for y, layer in enumerate(_data["layers"]) for z, row in enumerate(layer)
            for x, char in enumerate(row) if char != "."}
  third = CheckAssets.Components(voxels)[2]
  x, y, z = next(p for p in sorted(third) if voxels[p] == "k")
  SetCell(_data["layers"], x, y, z, "p")


def T3OffItsCost(_root):
  """Takes one merlon off T3 and updates the manifest to match, so only ADR-006's 96 m3 is left to object."""
  Change(_root, T3, lambda d: SetCell(d["layers"], 6, 6, 0, "."))

  def Expect(_manifest):
    Entry(_manifest, "T3")["costM3"]["stone"] = 95
    Entry(_manifest, "T3")["voxelCount"] = 95

  Change(_root, MANIFEST, Expect)


def Unsuffixed(_root):
  (_root / "GameData" / E1).rename(_root / "GameData" / E1.replace("-1m.json", ".json"))
  Change(_root, MANIFEST, lambda m: Entry(m, "E1")["files"].update(json=E1.replace("-1m.json", ".json")))


def LeakIntoBake(_root, _cpp):
  """Makes the copied client bake append _cpp to the palette header, as a bake that leaked a server field would."""
  path = _root / BAKE
  text = path.read_text(encoding="utf-8")
  closing = '"} // namespace Content::Generated\\n")'
  assert closing in text, "the bake's closing line has moved; update this test"
  path.write_text(text.replace(closing, f"{_cpp!r} + {closing}"), encoding="utf-8")


def LeakClientIntoServer(_root):
  """Makes the copied server bake count the client's models into its RULE section, as a bake that read GameData/Client/
  for the server would."""
  path = _root / BAKE
  text = path.read_text(encoding="utf-8")
  rules = '"RULE": ServerRules(rules)'
  assert rules in text, "the server bake's RULE line has moved; update this test"
  path.write_text(text.replace(rules, '"RULE": ServerRules({**rules, "modelCount": len(list(MODELS.glob("*.json")))})'),
                  encoding="utf-8")


def DuplicateIndex(_palette):
  _palette["materials"].append(dict(next(m for m in _palette["materials"] if m["index"] == 7)))


def BadColor(_palette):
  next(m for m in _palette["materials"] if m["index"] == 1)["hex"] = "#zzzzzz"


def GlyphOf(_font, _char):
  return next(glyph for glyph in _font["glyphs"] if glyph["char"] == _char)


def DebrisRow(_manifest, _rubbleIndex):
  return next(row for row in _manifest["debrisByRubble"]["table"] if row["rubbleIndex"] == _rubbleIndex)


def UnitOf(_units, _kind):
  return next(unit for unit in _units["units"] if unit["kind"] == _kind)


def StructureOf(_structures, _template):
  return next(structure for structure in _structures["structures"] if structure["template"] == _template)


def NamedStructure(_scenario, _name):
  return next(structure for structure in _scenario["structures"] if structure["name"] == _name)


def Legend(_char, _index):
  return lambda d: d["legend"][_char].update(material=_index)


BROKEN = {
  # 1. Indices and the legend.
  "a legend index not in the palette": (lambda r: Change(r, T3, Legend("f", 17)), "which is not in the palette"),
  "a character not in the legend": (lambda r: Change(r, T6, UnknownCharacter), "which is not in the legend"),
  "a palette index twice": (lambda r: Change(r, PALETTE, DuplicateIndex), "index 7 appears twice"),
  # 2. Grids and voxel sizes.
  "a world asset uses a models index": (lambda r: Change(r, E1, Legend("c", 19)),
                                        "a world asset uses only world and both indices"),
  "a model uses a world index": (lambda r: Change(r, SPEARMAN, Legend("g", 12)),
                                 "a model uses only models and both indices"),
  "voxelSize disagrees with the name": (lambda r: Change(r, T6, lambda d: d.update(voxelSize=0.5)),
                                        "voxelSize is 0.5, but the name ends -1m"),
  "a name without a voxel size": (Unsuffixed, "ends neither -1m nor -05m"),
  # 3. Decorations.
  "a decoration with no host": (lambda r: Change(r, T3, Legend("f", 20)), "do not resolve to a host"),
  "a structure decoration among stock": (lambda r: Change(r, T7, FittingsAmongStock), "do not resolve to a host"),
  "a modelOnly index in a template": (lambda r: Change(r, T1, Legend("m", 16)), "which is modelOnly"),
  # 4. Costs.
  "a wrong cost in the manifest": (lambda r: Change(r, MANIFEST, lambda m: Entry(m, "T5")["costM3"].update(timber=13)),
                                   "but Manifest.json says stone 589, timber 13"),
  "the package's cost, decorations left out": (
    lambda r: Change(r, MANIFEST, lambda m: Entry(m, "T1").update(costM3=Entry(m, "T1")["packageCostM3"])),
    "counts stone 0, timber 188, foundation 0 m3, but Manifest.json says stone 0, timber 185"),
  "stock costed in the manifest": (lambda r: Change(r, MANIFEST, lambda m: Entry(m, "T8")["costM3"].update(stone=11)),
                                   "but Manifest.json says stone 11"),
  "T3 off 96 m3": (T3OffItsCost, "T3 counts 95 m3, not the 96 m3"),
  # 5. Critical parts.
  "an empty critical part": (lambda r: Change(r, T5, lambda d: d.update(parts=[[row.replace("1", ".") for row in layer]
                                                                               for layer in d["parts"]])),
                             "names the critical part 'Gate', but no voxel is part 1"),
  # 6. Support.
  "a floating voxel": (lambda r: Change(r, T2, FloatingVoxel), "1 structure voxel does not reach the bottom layer"),
  "a wall standing on stock": (lambda r: Change(r, T3, lambda d: d["parts"].__setitem__(0, ["33333333"] * 2)),
                               "80 structure voxels do not reach the bottom layer"),
  # 7. Figures and squads.
  "a figure differs from its squad": (lambda r: Change(r, ARCHER, Legend("t", 24)),
                                      "differs from figures 1, 2, 3, 4, 5, 6 of 6"),
  "one figure of a squad differs": (lambda r: Change(r, "Illustrations/earthrise-u5-men-at-arms-05m.json", OneSoldier),
                                    "differs from figure 3 of 6"),
  "a squad short of a figure": (lambda r: Change(r, MANIFEST, lambda m: Entry(m, "U1")["figure"].update(squadOf=5)),
                                "holds 6 figures, but Manifest.json gives U1 squadOf 5"),
  # 8. The manifest.
  "an unlisted layer file": (lambda r: shutil.copy(r / "GameData" / T3, r / "GameData" / T3.replace("t3-", "t10-")),
                             "is a layer file that Manifest.json does not list"),
  "a listed file is missing": (lambda r: (r / "GameData" / M1_LAYER_FILE).unlink(), f"lists {M1_LAYER_FILE}, which does not"),
  "a wrong voxelCount": (lambda r: Change(r, MANIFEST, lambda m: Entry(m, "T6").update(voxelCount=36)),
                         "T6 gives its voxelCount 36, but"),
  "sizeVoxels of the padded grid": (lambda r: Change(r, MANIFEST, lambda m: Entry(m, "S3").update(
                                      sizeVoxels={"x": 26, "y": 28, "z": 9})), "S3 gives its sizeVoxels"),
  "a wrong figure voxelCount": (lambda r: Change(r, MANIFEST, lambda m: Entry(m, "U4")["figure"].update(voxelCount=11)),
                                "U4 gives its figure's voxelCount 11"),
  # 9. Client and server.
  "behaviours leak into the client": (
    lambda r: LeakIntoBake(r, 'inline constexpr const char* PALETTE_BEHAVIOR[] = {"buildingStone"};\n\n'),
    "carries server-only data: the behaviour word(s) behavior, building stone"),
  "a behaviour's name leaks into the client": (
    lambda r: LeakIntoBake(r, "inline constexpr std::array<bool, 256> PALETTE_MODEL_ONLY = {};\n\n"),
    "the behaviour word(s) model only"),
  "timber leaks into the client other than as a cost": (
    lambda r: LeakIntoBake(r, "inline constexpr std::array<bool, 256> PALETTE_TIMBER = {};\n\n"),
    "the behaviour word(s) timber"),
  "critical parts leak into the client": (
    lambda r: LeakIntoBake(r, "inline constexpr std::uint8_t T5_CRITICAL_PART[] = {0, 1, 1};\n\n"),
    "the part data word(s) part, critical"),
  "a threshold leaks into the client": (
    lambda r: LeakIntoBake(r, "inline constexpr std::uint32_t WALL_COLLAPSE_THRESHOLD_PERCENT = 60;\n\n"),
    "the threshold word(s) threshold"),
  "the client bake fails": (lambda r: Change(r, PALETTE, BadColor), "the client bake failed with exit code 1"),
  "the client bake is missing": (lambda r: (r / BAKE).unlink(), "the client bake is missing"),
  # 10. Rules.json.
  "a span that is not a whole number of voxels": (lambda r: Change(r, RULES, lambda d: d["values"].update(timberSpanMeters="5.5")),
                                                  "timberSpanMeters: 5.5 comes to 5.5 voxels, not a whole number"),
  "a rule whose key names no unit": (lambda r: Change(r, RULES, lambda d: d["values"].update(slumpRate="3")),
                                     "slumpRate: its key names no unit the bake converts"),
  "a rule that is not a decimal": (lambda r: Change(r, RULES, lambda d: d["values"].update(warningBaseSeconds="1/2")),
                                   "warningBaseSeconds: '1/2' is not a decimal string"),
  # 11. The server's bake.
  "client content reaches the server's data": (LeakClientIntoServer, "client content reaches the server's data"),
  "the server bake fails": (lambda r: Change(r, PALETTE, lambda p: next(m for m in p["materials"] if m["index"] == 4).pop("ground")),
                            "the server bake failed with exit code 1"),
  # 12. Line models.
  "a line model's segments leave a gap": (lambda r: Change(r, ARROW, lambda d: Segment(d, 1).update(fromMetres=0.2)),
                                          "its segments are not contiguous from 0 to drawnLengthMetres: segment 2 (shaft) "
                                          "starts at 200 mm, not at segment 1's end at 180 mm"),
  "a line model's segments stop short": (lambda r: Change(r, ARROW, lambda d: Segment(d, 2).update(toMetres=0.9)),
                                         "its segments end at 900 mm, not at its drawn length of 1000 mm"),
  "a line model's colour is neither hex nor faction": (lambda r: Change(r, ARROW, lambda d: Segment(d, 1).update(color="red")),
                                                       "segment 2 (shaft) has the colour 'red', which is neither #rrggbb nor faction"),
  "a line model's width is under a pixel": (lambda r: Change(r, ARROW, lambda d: Segment(d, 1).update(widthPixels=0)),
                                            "segment 2 (shaft) has a widthPixels of 0, which is not a whole number of pixels from 1"),
  "a line model's dot rule is missing": (lambda r: Change(r, ARROW, lambda d: d.pop("dotBelowPixels")),
                                         "its dotBelowPixels None is not a whole number of pixels from 1"),
  "an unlisted line model": (lambda r: shutil.copy(r / "GameData" / ARROW, r / "GameData" / ARROW.replace("x2-arrow", "x3-bolt")),
                             "is a line model that Manifest.json does not list"),
  "a voxel model among the line models": (lambda r: shutil.copy(r / "GameData" / SPEARMAN, r / "GameData" / "Client" / "Lines" / "u2-05m.json"),
                                          "is not a line model: Client/Lines/ holds line models"),
  # 13. The font.
  "a glyph's row wider than the glyph": (lambda r: Change(r, FONT, lambda f: GlyphOf(f, "1")["rows"].__setitem__(0, "..#...")),
                                         "has the row '..#...', which is not 5 wide, as its w says"),
  "a glyph's row of another character": (lambda r: Change(r, FONT, lambda f: GlyphOf(f, "1")["rows"].__setitem__(0, "..X..")),
                                         "has the row '..X..', which is not made of . and #"),
  "a glyph below the cell": (lambda r: Change(r, FONT, lambda f: GlyphOf(f, "1").update(top=4)),
                             "7 rows from its top 4 do not fit the 10-row cell"),
  "a code point twice": (lambda r: Change(r, FONT, lambda f: GlyphOf(f, "2").update(code=49)), "is the second glyph for U+0031"),
  "a symbol missing": (lambda r: Change(r, FONT, lambda f: f["glyphs"].remove(GlyphOf(f, "·"))), "lacks the symbol(s) ·"),
  "an icon not marked as one": (lambda r: Change(r, FONT, lambda f: GlyphOf(f, "\ue018").update(icon=False)),
                                "lacks the command icon(s) U+E018"),
  "M4's wall icon missing": (lambda r: Change(r, FONT, lambda f: f["glyphs"].remove(GlyphOf(f, "\ue01c"))), "lacks the command icon(s) U+E01C"),
  # 14. The debris table.
  "debris naming a model that does not exist": (lambda r: Change(r, MANIFEST, lambda m: DebrisRow(m, 26).update(debris=["D9"])),
                                                "names the model 'D9', which is not in GameData/Client/Models/"),
  "debris on an index that is no rubble": (lambda r: Change(r, MANIFEST, lambda m: DebrisRow(m, 26).update(rubbleIndex=6)),
                                           "names index 6, which is not a palette index whose behaviour is a rubble"),
  "a debris tint no palette entry has": (lambda r: Change(r, MANIFEST, lambda m: DebrisRow(m, 28).update(tint="#123456")),
                                         "has the tint '#123456', which is not exactly one palette entry's colour"),
  # 15. Units and structures.
  "a unit kind missing": (lambda r: Change(r, UNITS, lambda u: u["units"].remove(UnitOf(u, "sapper"))),
                          "gives no sapper: every kind has one record"),
  "a unit kind twice": (lambda r: Change(r, UNITS, lambda u: u["units"].append(dict(UnitOf(u, "archer")))), "gives the kind archer twice"),
  "a bonus naming no kind": (lambda r: Change(r, UNITS, lambda u: UnitOf(u, "spearman")["bonus"].update(knight="12")),
                             "spearman's bonus names the kind 'knight'"),
  "a unit value that does not convert": (lambda r: Change(r, UNITS, lambda u: UnitOf(u, "archer").update(rangeMeters="30.5")),
                                         "archer's rangeMeters: 30.5 comes to 30.5 voxels, not a whole number"),
  "a unit with no hit points": (lambda r: Change(r, UNITS, lambda u: UnitOf(u, "ram").update(hitPoints="0")),
                                "ram's hitPoints is 0, outside 1 to"),
  "a unit of no class": (lambda r: Change(r, UNITS, lambda u: UnitOf(u, "outrider").update(navigationClass="flying")),
                         "outrider's navigationClass 'flying' is not one of"),
  "a structure naming no template": (lambda r: Change(r, STRUCTURES, lambda s: StructureOf(s, "T4").update(template="T10")),
                                     "names the template 'T10', which GameData/Server/Templates/ does not hold"),
  "a threshold above 100": (lambda r: Change(r, STRUCTURES, lambda s: StructureOf(s, "T4").update(criticalThresholdPercent="101")),
                            "T4 names the critical part Firing platform, and its criticalThresholdPercent '101'"),
  "a threshold for no critical part": (lambda r: Change(r, STRUCTURES, lambda s: StructureOf(s, "T3").update(criticalThresholdPercent="50")),
                                       "T3 names no critical part, so it gives no criticalThresholdPercent"),
  "a structure value that does not convert": (lambda r: Change(r, STRUCTURES, lambda s: StructureOf(s, "T5").update(standHeightMeters="11.5")),
                                              "T5's standHeightMeters: 11.5 comes to 11.5 voxels, not a whole number"),
  # 16. Scenarios.
  "a scenario's structure naming no template": (lambda r: Change(r, SIEGE, lambda s: NamedStructure(s, "S3").update(template="T10")),
                                                "structure 4 names the template 'T10'"),
  "two structures overlapping": (lambda r: Change(r, SIEGE, lambda s: NamedStructure(s, "S3").update(origin=NamedStructure(s, "S2")["origin"])),
                                 "the structures S2 and S3 overlap"),
  "two structures of one name": (lambda r: Change(r, SIEGE, lambda s: NamedStructure(s, "S3").update(name="S2")),
                                 "has the name 'S2', which is empty or another structure's"),
  "a squad on a structure": (lambda r: Change(r, SIEGE, lambda s: s["squads"][0]["cells"].__setitem__(0, list(NamedStructure(s, "S3")["origin"]))),
                             "squad 1's cell"),
  "a squad in the ditch": (lambda r: Change(r, SIEGE, lambda s: s["squads"][0]["cells"].__setitem__(0, [340, 9, 296])),
                           "squad 1's cell (340, 9, 296) is not a walkable cell"),
  "two squads in one cell": (lambda r: Change(r, SIEGE, lambda s: s["squads"][1]["cells"].__setitem__(0, s["squads"][0]["cells"][0])),
                             "squad 2's cell"),
  "a squad short of its figures": (lambda r: Change(r, SIEGE, lambda s: s["squads"][0]["cells"].pop()),
                                   "squad 1, of manAtArms, has 5 cells, and a squad of manAtArms has 6 figures"),
  "a garrison with no room": (lambda r: Change(r, SIEGE, lambda s: s["garrisons"].append(dict(s["garrisons"][0]))),
                              "puts 12 figures in SW, which holds 6"),
  "a garrison in a wall": (lambda r: Change(r, SIEGE, lambda s: s["garrisons"][0].update(structure="S1")), "puts 6 figures in S1, which holds 0"),
  "a keep that is no T2": (lambda r: Change(r, SIEGE, lambda s: s["trial"].update(keep="HALL")), "its trial's keep 'HALL' is not a T2"),
  "a site its ground does not fill": (lambda r: Change(r, SIEGE, lambda s: s["ground"][1]["max"].__setitem__(1, 6)),
                                      "its ground boxes leave the site's voxel (240, 7, 182) below its level without a material"),
  "an earthwork leaving the site": (lambda r: Change(r, SIEGE, lambda s: s["earthworks"][0]["min"].__setitem__(0, 100)),
                                    "its earthworks box 1 leaves its site"),
  "a commander no ADR names": (lambda r: Change(r, SIEGE, lambda s: s["players"][1].update(commander="cleverDefender")),
                               "the player 2 needs an id of its own"),
  "a mirror renaming two structures alike": (lambda r: Change(r, SIEGE_MIRROR, lambda m: m["rename"].update(S1="S3")),
                                             "its rename table must rename each of Siege's structures once"),
  "a mirrored section named for the wrong side": (lambda r: Change(r, SIEGE_MIRROR, lambda m: m["rename"].update(E1="E1", W1="W1")),
                                                  "its section E1 faces W, so its name's side letter is wrong"),
  "a mirror of nothing": (lambda r: Change(r, SIEGE_MIRROR, lambda m: m.update(mirrorOf="Hold")), "mirrors 'Hold'"),
  "a castle off its site's centre line": (
    lambda r: Change(r, SIEGE, lambda s: s["site"].update(min=[230, 182], max=[439, 381])) or
              Change(r, SIEGE, lambda s: [box["min"].__setitem__(0, 230) for box in s["ground"][:3]]),
    "which is not symmetric about its site's centre line"),
}


class CheckAssetsTests(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    with tempfile.TemporaryDirectory() as folder:
      root = Path(folder)
      Copy(root)
      cls.unbroken = set(Problems(root))

  def test_the_repository_passes(self):
    report, assets, outputs, lineModels = CheckAssets.Check(ROOT)
    self.assertEqual(report.problems, [])
    self.assertTrue(any(CheckAssets.IsTemplate(relative) for relative in assets))
    self.assertGreater(outputs, 0, "the client bake wrote nothing, so check 9 searched nothing")
    self.assertIn(ARROW, lineModels)

  def test_an_unbroken_copy_reads_as_the_repository(self):
    report, _, _, _ = CheckAssets.Check(ROOT)
    self.assertEqual(self.unbroken, set(report.problems))

  def test_each_broken_copy_fails_for_its_reason(self):
    for name, (breakIt, expected) in BROKEN.items():
      with self.subTest(name), tempfile.TemporaryDirectory() as folder:
        root = Path(folder)
        Copy(root)
        breakIt(root)
        added = sorted(set(Problems(root)) - self.unbroken)
        self.assertTrue(any(expected in problem for problem in added), f"{name}: expected {expected!r} in {added}")

  def test_the_command_line_reports_and_fails(self):
    with tempfile.TemporaryDirectory() as folder:
      root = Path(folder)
      Copy(root)
      Change(root, T3, Legend("f", 17))
      result = subprocess.run([sys.executable, str(TOOLS / "CheckAssets.py"), "--root", str(root)],
                              capture_output=True, text=True)
      lines = result.stdout.strip().splitlines()
      self.assertEqual(result.returncode, 1)
      self.assertIn(f"GameData/{T3}: legend 'f' is index 17, which is not in the palette", result.stdout)
      self.assertRegex(lines[-1], r"^CheckAssets: \d+ problems?$")


def Template(_voxels, _parts=None):
  """A template made of _voxels, {(x, y, z): palette index}, with _parts, {(x, y, z): part}."""
  return CheckAssets.Asset(Path("Synthetic-1m.json"), {"voxelSize": 1}, _voxels, _parts or {})


class DecisionSixTests(unittest.TestCase):
  """ADR-005 decision 6 and decision 7 on small templates: no real template has a tie or a second pass."""

  @classmethod
  def setUpClass(cls):
    cls.palette, _ = CheckAssets.ReadPalette(CheckAssets.Report(ROOT), ROOT)

  def Resolve(self, _voxels, _parts=None):
    return CheckAssets.ResolveBehaviors(Template(_voxels, _parts), self.palette)

  def test_a_tie_goes_to_the_lower_palette_index(self):
    behaviors, _ = self.Resolve({(0, 0, 0): 7, (1, 0, 0): 20, (2, 0, 0): 4})
    self.assertEqual(behaviors[(1, 0, 0)], "buildingStone")
    behaviors, _ = self.Resolve({(0, 0, 0): 7, (1, 0, 0): 20, (2, 0, 0): 2})
    self.assertEqual(behaviors[(1, 0, 0)], "earth")

  def test_the_most_common_behaviour_beats_a_lower_index(self):
    behaviors, _ = self.Resolve({(0, 0, 0): 4, (1, 0, 0): 20, (2, 0, 0): 7, (1, 1, 0): 12})
    self.assertEqual(behaviors[(1, 0, 0)], "timber")

  def test_a_fitting_set_into_an_opening_finds_its_host_in_a_later_pass(self):
    behaviors, unresolved = self.Resolve({(0, 0, 0): 6, (1, 0, 0): 20, (2, 0, 0): 18})
    self.assertEqual(behaviors[(2, 0, 0)], "buildingStone")
    self.assertEqual(unresolved, [])

  def test_structure_looks_at_structure_and_stock_at_stock(self):
    _, unresolved = self.Resolve({(0, 0, 0): 7, (1, 0, 0): 20}, {(0, 0, 0): "3", (1, 0, 0): "."})
    self.assertEqual(unresolved, [(1, 0, 0)])
    behaviors, _ = self.Resolve({(0, 0, 0): 7, (1, 0, 0): 20}, {(0, 0, 0): "3", (1, 0, 0): "3"})
    self.assertEqual(behaviors[(1, 0, 0)], "timber")

  def test_decorations_touching_only_decorations_never_resolve(self):
    _, unresolved = self.Resolve({(0, 0, 0): 20, (1, 0, 0): 18})
    self.assertEqual(unresolved, [(0, 0, 0), (1, 0, 0)])

  def test_the_cost_counts_decorations_as_their_host_and_leaves_out_stock_and_foundation(self):
    voxels = {(0, 0, 0): 6, (1, 0, 0): 20, (2, 0, 0): 7, (0, 1, 0): 6, (3, 0, 0): 12}
    parts = {(2, 0, 0): "3", (0, 1, 0): "2"}
    template = Template(voxels, parts)
    behaviors, _ = CheckAssets.ResolveBehaviors(template, self.palette)
    self.assertEqual(CheckAssets.CountCost(template, behaviors), {"stone": 2, "timber": 1, "foundation": 1})

  def test_words_split_identifiers_and_skip_numbers(self):
    words = CheckAssets.Words("PALETTE_BEHAVIOR paletteBehavior 0x7d8b4fu Earthrise std::uint32_t")
    self.assertEqual(words, ["palette", "behavior", "palette", "behavior", "earthrise", "std", "uint", "32", "t"])


if __name__ == "__main__":
  unittest.main()
