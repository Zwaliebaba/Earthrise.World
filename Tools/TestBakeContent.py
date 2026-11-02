"""Tests for Tools/BakeContent.py: the server half's ServerData file of ADR-016, and the client half's rule for ids and
its line models (ADR-018), font (ADR-019) and debris table.

Each test bakes the real sources into a temporary folder, or bakes a copy of them with one thing changed, and reads
the file back by hand, with the layout BakeContent.py's docstring states. The reader in Simulation (M2-B6) is tested
against the same file in SimulationTests; these tests pin the bytes from the bake's side.

Usage:
  python3 Tools/TestBakeContent.py         every case
  python3 Tools/TestBakeContent.py -v      with each case's name
"""

import json
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))

import BakeContent  # noqa: E402
import CheckAssets  # noqa: E402

RECORD_FIELDS = ("exists", "behavior", "support", "spanVoxels", "flags", "rubbleIndex")


def Bake(_root, _out, *_halves):
  """Runs the bake in the tree at _root into _out, for the halves named ("client", "server"). Returns the result."""
  arguments = []
  for half in _halves:
    arguments += [f"--{half}", str(_out / half)]
  return subprocess.run([sys.executable, str(_root / "Tools" / "BakeContent.py"), *arguments], capture_output=True, text=True)


class ServerFile:
  """The file, read back by hand: its header, its body, and its sections by tag."""

  def __init__(self, _bytes):
    self.magic, self.version, self.hash = _bytes[:4], *struct.unpack_from("<IQ", _bytes, 4)
    self.body = _bytes[16:]
    self.sharedHash, count = struct.unpack_from("<QI", self.body, 0)
    self.sections, self.versions = {}, {}
    for entry in range(count):
      at = 12 + 16 * entry
      tag = self.body[at : at + 4].decode("ascii")
      version, offset, length = struct.unpack_from("<III", self.body, at + 4)
      self.versions[tag] = version
      self.sections[tag] = self.body[offset : offset + length]

  def Material(self, _index):
    return dict(zip(RECORD_FIELDS, self.sections["MATL"][6 * _index : 6 * _index + 6]))

  def Templates(self):
    """[(id, name, critical part, (w, h, d), edge, stone, timber, indices, parts)] in the file's order."""
    data, at = self.sections["TMPL"], 4
    templates = []

    def Text():
      nonlocal at
      (length,) = struct.unpack_from("<I", data, at)
      at += 4 + length
      return data[at - length : at].decode("utf-8")

    for _ in range(struct.unpack_from("<I", data, 0)[0]):
      texts = [Text(), Text(), Text()]
      width, height, depth, edge, stone, timber = struct.unpack_from("<6I", data, at)
      at += 24
      volume = width * height * depth
      indices, parts = data[at : at + volume], data[at + volume : at + 2 * volume]
      at += 2 * volume
      templates.append((*texts, (width, height, depth), edge, stone, timber, indices, parts))
    return templates

  def Units(self):
    """[{kind, navigationClass, squadFigures, attackType, hitPoints, armor, attack, reloadTicks, rangeVoxels, bonus,
    visionVoxels, speedMetersPerTick, digCubicMetersPerTick}] in the file's order, 76 bytes a record."""
    data, units = self.sections["UNIT"], []
    (count,) = struct.unpack_from("<I", data, 0)
    self.unitBytes = len(data) - 4
    for number in range(count):
      at = 4 + 76 * number
      kind, navigation, figures, attackType = struct.unpack_from("<4B", data, at)
      hitPoints, melee, pierce, crush, attack = struct.unpack_from("<5i", data, at + 4)
      reload, reach = struct.unpack_from("<Ii", data, at + 24)
      bonus = list(struct.unpack_from("<8i", data, at + 32))
      vision, speed, dig = struct.unpack_from("<3i", data, at + 64)
      units.append({"kind": kind, "navigationClass": navigation, "squadFigures": figures, "attackType": attackType,
                    "hitPoints": hitPoints, "armor": (melee, pierce, crush), "attack": attack, "reloadTicks": reload,
                    "rangeVoxels": reach, "bonus": bonus, "visionVoxels": vision, "speedMetersPerTick": speed,
                    "digCubicMetersPerTick": dig})
    return units

  def Structures(self):
    """{template: (threshold, capacity, stand height, vision, garrisoned vision, hit points, hit points a m3)}."""
    data, at, structures = self.sections["STRC"], 4, {}
    for _ in range(struct.unpack_from("<I", data, 0)[0]):
      (length,) = struct.unpack_from("<I", data, at)
      template = data[at + 4 : at + 4 + length].decode("utf-8")
      at += 4 + length
      structures[template] = struct.unpack_from("<2B", data, at) + struct.unpack_from("<5i", data, at + 2)
      at += 22
    self.structuresEndExactly = at == len(data)
    return structures

  def ScenarioIds(self):
    """The scenarios' ids, in the file's order: each record begins with its id."""
    data = self.sections["SCEN"]
    (count,) = struct.unpack_from("<I", data, 0)
    (length,) = struct.unpack_from("<I", data, 4)
    return count, data[8 : 8 + length].decode("utf-8")

  def Rules(self):
    data, at, rules = self.sections["RULE"], 4, {}
    for _ in range(struct.unpack_from("<I", data, 0)[0]):
      (length,) = struct.unpack_from("<I", data, at)
      key = data[at + 4 : at + 4 + length].decode("utf-8")
      (rules[key],) = struct.unpack_from("<q", data, at + 4 + length)
      at += 4 + length + 8
    return rules


class ServerBakeTests(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    cls.folder = tempfile.TemporaryDirectory()
    cls.out = Path(cls.folder.name)
    result = Bake(ROOT, cls.out, "client", "server")
    if result.returncode != 0:
      raise RuntimeError(result.stdout + result.stderr)
    cls.path = cls.out / "server" / BakeContent.SERVER_FILE
    cls.file = ServerFile(cls.path.read_bytes())

  @classmethod
  def tearDownClass(cls):
    cls.folder.cleanup()

  # ── The file ────────────────────────────────────────────────────────────────────────────────────

  def test_the_header_hash_covers_exactly_the_body(self):
    raw = self.path.read_bytes()
    self.assertEqual(b"ERSD", self.file.magic)
    self.assertEqual(1, self.file.version)
    self.assertEqual(BakeContent.Fnv1a64(raw[16:]), self.file.hash)
    self.assertNotEqual(BakeContent.Fnv1a64(raw[15:]), self.file.hash)

  def test_the_shared_hash_is_the_client_halfs(self):
    header = (self.out / "client" / "SharedHashValue.h").read_text(encoding="utf-8")
    clientHash = int(re.search(r"SHARED_HASH = 0x([0-9a-f]{16})ULL", header).group(1), 16)
    self.assertEqual(clientHash, self.file.sharedHash)

  def test_the_sections_come_in_order_at_version_one(self):
    self.assertEqual(["MATL", "TMPL", "RULE", "UNIT", "STRC", "SCEN"], list(self.file.sections))
    self.assertEqual({"MATL": 1, "TMPL": 1, "RULE": 1, "UNIT": 1, "STRC": 1, "SCEN": 1}, self.file.versions)
    self.assertEqual(256 * 6, len(self.file.sections["MATL"]))

  def test_a_second_bake_leaves_the_file_untouched(self):
    with tempfile.TemporaryDirectory() as folder:
      out = Path(folder)
      self.assertEqual(0, Bake(ROOT, out, "server").returncode)
      written = (out / "server" / BakeContent.SERVER_FILE).stat().st_mtime_ns
      second = Bake(ROOT, out, "server")
      self.assertEqual(0, second.returncode)
      self.assertIn("kept ServerData.bin", second.stdout)
      self.assertEqual(written, (out / "server" / BakeContent.SERVER_FILE).stat().st_mtime_ns)

  # ── MATL ────────────────────────────────────────────────────────────────────────────────────────

  def test_each_material_leaves_its_rubble(self):
    expected = {4: 8, 6: 8, 7: 25, 12: 25, 2: 26, 1: 26, 3: 28, 10: 0, 11: 0, 5: 0, 9: 0}
    for index, rubble in expected.items():
      with self.subTest(index):
        self.assertEqual(rubble, self.file.Material(index)["rubbleIndex"])

  def test_terrain_is_ground_and_structure_is_not(self):
    self.assertEqual(1, self.file.Material(4)["flags"] & 1)
    self.assertEqual(0, self.file.Material(6)["flags"] & 1)
    self.assertEqual(2, self.file.Material(9)["flags"])  # water: liquid, not ground

  def test_spans_come_from_the_rules(self):
    self.assertEqual({"exists": 1, "behavior": 3, "support": 2, "spanVoxels": 2, "flags": 0, "rubbleIndex": 8}, self.file.Material(6))
    self.assertEqual(5, self.file.Material(7)["spanVoxels"])
    self.assertEqual(3, self.file.Material(10)["spanVoxels"])
    self.assertEqual(2, self.file.Material(3)["spanVoxels"])
    self.assertEqual(3, self.file.Material(5)["support"])  # bedrock anchors

  def test_each_decoration_host_index_takes_its_hosts_rule(self):
    palette = json.loads((ROOT / "GameData" / "Server" / "Palette.json").read_text(encoding="utf-8"))
    hosts = {"timber": 7, "buildingStone": 6}
    decorating = [entry for entry in palette["materials"] if "decorates" in entry]
    self.assertEqual(3, len(decorating))
    for entry in decorating:
      with self.subTest(entry["name"]):
        self.assertEqual(self.file.Material(hosts[entry["behavior"]]), self.file.Material(entry["index"]))
    for decoration in (18, 20):
      self.assertEqual({"exists": 1, "behavior": 0, "support": 0, "spanVoxels": 0, "flags": 0, "rubbleIndex": 0},
                       self.file.Material(decoration))

  def test_an_index_the_palette_lacks_does_not_exist(self):
    self.assertEqual(0, self.file.Material(0)["exists"])
    self.assertEqual(0, self.file.Material(17)["exists"])
    self.assertEqual(0, self.file.Material(255)["exists"])

  # ── TMPL ────────────────────────────────────────────────────────────────────────────────────────

  def test_it_holds_every_template_with_t3_at_96_m3(self):
    templates = self.file.Templates()
    self.assertEqual([f"T{n}" for n in range(1, 10)], [template[0] for template in templates])
    t3 = templates[2]
    self.assertEqual(96, t3[5])
    self.assertEqual("", t3[2])  # T3 names no critical part
    self.assertEqual("Gate", templates[4][2])

  def test_no_template_voxel_is_a_decoration(self):
    for template in self.file.Templates():
      with self.subTest(template[0]):
        self.assertFalse({18, 20} & set(template[7]))
    t1 = self.file.Templates()[0]
    self.assertIn(29, set(t1[7]))  # T1's openings sit in timber

  def test_parts_lie_under_voxels(self):
    for template in self.file.Templates():
      indices, parts = template[7], template[8]
      with self.subTest(template[0]):
        self.assertTrue(all(index != 0 or part == 0 for index, part in zip(indices, parts)))
    t8 = self.file.Templates()[7]
    self.assertIn(3, set(t8[8]))  # T8 holds stock

  # ── RULE ────────────────────────────────────────────────────────────────────────────────────────

  def test_a_walking_speed_converts_to_fixed_point_a_tick(self):
    rules = self.file.Rules()
    self.assertEqual(4587, rules["footSpeedMetersPerTick"])
    self.assertNotIn("cavalrySpeedMetersPerTick", rules)  # the kinds' own speeds are UNIT's (ADR-023, decision 1)
    self.assertNotIn("machinesSpeedMetersPerTick", rules)
    self.assertEqual(20, rules["warningBaseTicks"])
    self.assertEqual(1607, rules["gravityMetersPerTickSquared"])
    self.assertEqual(16, rules["queryWorkPerTick"])
    self.assertEqual(sorted(rules), list(rules))

  def test_m4s_rules_convert_into_the_simulations_units(self):
    rules = self.file.Rules()
    self.assertEqual(150, rules["shotStoneHundredthsCubicMeters"])  # 1.5 m3, input 5
    self.assertEqual(131072, rules["arrowSpeedMetersPerTick"])  # 40 m/s, 2 m a tick
    self.assertEqual(150, rules["arrowLaunchHeightCentimeters"])
    self.assertEqual(262, rules["repairCubicMetersPerTick"])  # 0.08 m3/s, floored once
    self.assertEqual(100, rules["garrisonHealTicksPerHitPoint"])
    self.assertEqual(300, rules["ramBlowTicks"])
    self.assertEqual(10, rules["dropCrushDamagePerVoxel"])
    self.assertEqual(10, rules["commanderDecisionTicks"])

  # ── UNIT and STRC ───────────────────────────────────────────────────────────────────────────────

  def test_every_unit_kind_bakes_once_in_kind_order(self):
    units = self.file.Units()
    self.assertEqual(list(range(8)), [unit["kind"] for unit in units])
    self.assertEqual(8 * 76, self.file.unitBytes)
    archer = units[2]
    self.assertEqual({"navigationClass": 0, "squadFigures": 6, "attackType": 1, "hitPoints": 30, "armor": (0, 0, 0), "attack": 4,
                      "reloadTicks": 40, "rangeVoxels": 30, "visionVoxels": 32, "speedMetersPerTick": 4587},
                     {key: archer[key] for key in ("navigationClass", "squadFigures", "attackType", "hitPoints", "armor", "attack",
                                                   "reloadTicks", "rangeVoxels", "visionVoxels", "speedMetersPerTick")})
    outrider = units[3]
    self.assertEqual([4, 0, 6, 0, 0, 0, 20, 20], outrider["bonus"])
    self.assertEqual((1, 9175), (outrider["navigationClass"], outrider["speedMetersPerTick"]))
    self.assertEqual(655, units[5]["digCubicMetersPerTick"])  # sappers, 0.2 m3/s
    self.assertEqual((2, 300, 20), (units[6]["navigationClass"], units[6]["hitPoints"], units[6]["armor"][1]))

  def test_every_structure_bakes_in_template_order(self):
    structures = self.file.Structures()
    self.assertTrue(self.file.structuresEndExactly)
    self.assertEqual([f"T{n}" for n in range(1, 8)], list(structures))
    self.assertEqual((50, 6, 11, 10, 80, 900, 50), structures["T5"])
    self.assertEqual((90, 6, 16, 10, 80, 900, 50), structures["T2"])
    self.assertEqual(0, structures["T3"][0])  # T3 names no critical part

  # ── SCEN ────────────────────────────────────────────────────────────────────────────────────────

  def test_the_siege_castle_bakes_as_the_scenarios(self):
    count, first = self.file.ScenarioIds()
    self.assertEqual("Siege", first)
    self.assertGreaterEqual(count, 1)

  def test_a_scenario_round_trips_through_its_encoding(self):
    report = CheckAssets.Report(ROOT)
    palette, _ = CheckAssets.ReadPalette(report, ROOT)
    rules = CheckAssets.ReadRules(report, ROOT)
    units = CheckAssets.ReadUnits(report, ROOT)
    structures = CheckAssets.ReadStructures(report, ROOT, CheckAssets.TemplateCriticalParts(report, ROOT))
    scenarios = CheckAssets.ReadScenarios(report, ROOT, palette, units, structures, rules)
    self.assertEqual([], report.problems)
    self.assertEqual(self.file.sections["SCEN"], BakeContent.ServerScenarios(scenarios))
    siege = scenarios[0]
    self.assertEqual((240, 182, 439, 381, 9), siege["site"])
    self.assertEqual(4, next(s for s in siege["structures"] if s["name"] == "E3")["foundationDepthVoxels"])
    self.assertEqual({"version": 0, "attacker": 1, "defender": 2, "keep": "KEEP", "timeLimitTicks": 48000, "holdTicks": 2400,
                      "ringCells": 3, "campMin": (325, 9, 192), "campMax": (355, 9, 212), "nearest": "S3", "weak": "E3"}, siege["trial"])
    self.assertEqual(12000, siege["players"][0]["stone"])

  def test_a_span_that_is_not_a_whole_number_of_voxels_fails(self):
    with tempfile.TemporaryDirectory() as folder:
      root = Path(folder)
      shutil.copytree(TOOLS, root / "Tools", ignore=shutil.ignore_patterns("__pycache__"))
      shutil.copytree(ROOT / "GameData" / "Server", root / "GameData" / "Server")
      path = root / "GameData" / "Server" / "Rules.json"
      data = json.loads(path.read_text(encoding="utf-8"))
      data["values"]["timberSpanMeters"] = "5.5"
      path.write_text(json.dumps(data), encoding="utf-8")
      result = Bake(root, root / "out", "server")
      self.assertEqual(1, result.returncode)
      self.assertIn("timberSpanMeters: 5.5 comes to 5.5 voxels, not a whole number", result.stdout)
      self.assertFalse((root / "out" / "server" / BakeContent.SERVER_FILE).exists())


class ClientIdTests(unittest.TestCase):
  """An id names a generated array, so it is an identifier, upper case in the array's name: a pose's S3c is MODEL_S3C_INDICES,
  and no two ids may differ only in case."""

  @classmethod
  def setUpClass(cls):
    cls.palette, _ = CheckAssets.ReadPalette(CheckAssets.Report(ROOT), ROOT)

  def Views(self, _ids):
    """Reads copies of S3c's layer file carrying _ids, in a temporary folder, as models. Returns (views, problems)."""
    source = json.loads((BakeContent.MODELS / "earthrise-s3c-trebuchet-cocked-05m.json").read_text(encoding="utf-8"))
    with tempfile.TemporaryDirectory() as folder:
      for number, assetId in enumerate(_ids):
        (Path(folder) / f"copy-{number}-05m.json").write_text(json.dumps({**source, "id": assetId}), encoding="utf-8")
      report = CheckAssets.Report(Path(folder))
      return BakeContent.ReadViews(report, Path(folder), self.palette, False), report.problems

  def test_a_poses_lower_case_suffix_is_an_identifier(self):
    views, problems = self.Views(["S3c"])
    self.assertEqual([], problems)
    self.assertEqual(["S3c"], [view.id for view in views])
    self.assertIn("MODEL_S3C_INDICES", BakeContent.ModelHeader(views))

  def test_ids_differing_only_in_case_are_refused(self):
    _, problems = self.Views(["S3c", "S3C"])
    self.assertEqual(1, len(problems))
    self.assertIn("has an id differing only in case from S3C, as copy-1-05m.json does", problems[0])
    _, problems = self.Views(["S3c", "S3c"])
    self.assertIn("has the id S3c, as copy-0-05m.json does", problems[0])

  def test_an_id_that_is_no_identifier_is_refused(self):
    _, problems = self.Views(["s3c"])
    self.assertIn("its id 's3c' is not an identifier such as T1", problems[0])


class LineModelBakeTests(unittest.TestCase):
  """The client half's line models (ADR-018): the arrow's three segments, as Content reads them."""

  def test_the_arrow_bakes_as_three_contiguous_segments(self):
    with tempfile.TemporaryDirectory() as folder:
      out = Path(folder)
      result = Bake(ROOT, out, "client")
      self.assertEqual(0, result.returncode, result.stdout + result.stderr)
      header = (out / "client" / "LineModelTable.h").read_text(encoding="utf-8")
    self.assertIn("inline constexpr std::array<LineSegmentView, 3> LINE_MODEL_X2_SEGMENTS = {{", header)
    self.assertIn(".fromMillimeters = 0, .toMillimeters = 180, .colorSrgb = 0x000000u, .factionColor = true, .widthPixels = 3, "
                  ".tapers = false", header)
    self.assertIn(".fromMillimeters = 180, .toMillimeters = 860, .colorSrgb = 0xc79a6bu, .factionColor = false, .widthPixels = 2, "
                  ".tapers = false", header)
    self.assertIn(".fromMillimeters = 860, .toMillimeters = 1000, .colorSrgb = 0x9aa0a6u, .factionColor = false, .widthPixels = 3, "
                  ".tapers = true", header)
    self.assertIn('{.id = "X2", .name = "Arrow", .drawnLengthMillimeters = 1000, .dotBelowPixels = 6, .dotWidthPixels = 2, '
                  ".segments = {LINE_MODEL_X2_SEGMENTS}}", header)
    self.assertIn("inline constexpr std::array<LineModelView, 1> LINE_MODELS = {{", header)

  def test_a_broken_line_model_stops_the_bake(self):
    with tempfile.TemporaryDirectory() as folder:
      root = Path(folder)
      shutil.copytree(TOOLS, root / "Tools", ignore=shutil.ignore_patterns("__pycache__"))
      shutil.copytree(ROOT / "GameData", root / "GameData")
      path = root / "GameData" / "Client" / "Lines" / "earthrise-x2-arrow.json"
      data = json.loads(path.read_text(encoding="utf-8"))
      data["segments"][1]["widthPixels"] = 0
      path.write_text(json.dumps(data), encoding="utf-8")
      result = Bake(root, root / "out", "client")
      self.assertEqual(1, result.returncode)
      self.assertIn("segment 2 (shaft) has a widthPixels of 0", result.stdout)
      self.assertFalse((root / "out" / "client" / "LineModelTable.h").exists())


class FontAndDebrisBakeTests(unittest.TestCase):
  """The client half's font (ADR-019, decision 1) and debris table (GameDesign.md, M3's values, item 16)."""

  @classmethod
  def setUpClass(cls):
    with tempfile.TemporaryDirectory() as folder:
      out = Path(folder)
      result = Bake(ROOT, out, "client")
      assert result.returncode == 0, result.stdout + result.stderr
      cls.font = (out / "client" / "FontTable.h").read_text(encoding="utf-8")
      cls.debris = (out / "client" / "DebrisTable.h").read_text(encoding="utf-8")

  def test_the_digit_one_bakes_as_its_bitmap(self):
    # ..#.. .##.. ..#.. ..#.. ..#.. ..#.. .###.
    self.assertIn("inline constexpr std::array<std::uint16_t, 7> GLYPH_0031_ROWS = {4, 6, 4, 4, 4, 4, 14};", self.font)
    self.assertIn("{.codePoint = 0x0031U, .widthPixels = 5, .heightPixels = 7, .topRow = 1, .advancePixels = 6, .icon = false, "
                  ".rows = GLYPH_0031_ROWS},", self.font)
    self.assertIn("inline constexpr std::array<Glyph, 127> GLYPHS = {{", self.font)

  def test_the_glyphs_come_in_ascending_code_point(self):
    codes = [int(code, 16) for code in re.findall(r"\.codePoint = 0x([0-9A-F]+)U", self.font)]
    self.assertEqual(127, len(codes))
    self.assertEqual(sorted(codes), codes)

  def test_rock_rubble_bakes_three_stones_tinted_its_own_index(self):
    self.assertIn('inline constexpr std::array<std::string_view, 3> DEBRIS_28_MODELS = {"D1", "D2", "D3"};', self.debris)
    self.assertIn("{.rubbleIndex = 28, .models = DEBRIS_28_MODELS, .tintIndex = 28},", self.debris)
    self.assertIn("{.rubbleIndex = 25, .models = DEBRIS_25_MODELS, .tintIndex = 0},", self.debris)

  def test_a_broken_glyph_stops_the_bake(self):
    with tempfile.TemporaryDirectory() as folder:
      root = Path(folder)
      shutil.copytree(TOOLS, root / "Tools", ignore=shutil.ignore_patterns("__pycache__"))
      shutil.copytree(ROOT / "GameData", root / "GameData")
      path = root / "GameData" / "Client" / "Fonts" / "EarthriseMason-glyphs.json"
      data = json.loads(path.read_text(encoding="utf-8"))
      next(glyph for glyph in data["glyphs"] if glyph["char"] == "1")["rows"][0] = "..#..."
      path.write_text(json.dumps(data, ensure_ascii=False), encoding="utf-8")
      result = Bake(root, root / "out", "client")
      self.assertEqual(1, result.returncode)
      self.assertIn("has the row '..#...', which is not 5 wide", result.stdout)
      self.assertFalse((root / "out" / "client" / "FontTable.h").exists())


class HandWrittenEncodingTests(unittest.TestCase):
  """The encoding against bytes written out by hand, so the reader in Simulation has a second statement of it."""

  def test_a_unit_encodes_in_76_bytes(self):
    unit = {"kind": 3, "navigationClass": 1, "squadFigures": 3, "attackType": 0, "hitPoints": 60, "armorMelee": 0, "armorPierce": 2,
            "armorCrush": 0, "attack": 6, "reloadTicks": 40, "rangeVoxels": 0, "bonus": [4, 0, 6, 0, 0, 0, 20, 20], "visionVoxels": 16,
            "speedMetersPerTick": 9175, "digCubicMetersPerTick": 0}
    expected = (struct.pack("<I", 1) + bytes([3, 1, 3, 0]) + struct.pack("<5i", 60, 0, 2, 0, 6) + struct.pack("<Ii", 40, 0)
                + struct.pack("<8i", 4, 0, 6, 0, 0, 0, 20, 20) + struct.pack("<3i", 16, 9175, 0))
    self.assertEqual(expected, BakeContent.ServerUnits([unit]))
    self.assertEqual(4 + 76, len(expected))

  def test_a_structure_encodes_after_its_template_id(self):
    structure = {"template": "T4", "criticalThresholdPercent": 75, "garrisonCapacity": 6, "standHeightVoxels": 11, "visionVoxels": 10,
                 "garrisonedVisionVoxels": 80, "controlHitPoints": 900, "controlHitPointsPerCubicMeter": 50}
    expected = struct.pack("<I", 1) + struct.pack("<I", 2) + b"T4" + bytes([75, 6]) + struct.pack("<5i", 11, 10, 80, 900, 50)
    self.assertEqual(expected, BakeContent.ServerStructures([structure]))

  def test_rules_encode_in_key_order(self):
    expected = (struct.pack("<I", 2) + struct.pack("<I", 22) + b"footSpeedMetersPerTick" + struct.pack("<q", 4587)
                + struct.pack("<I", 16) + b"warningBaseTicks" + struct.pack("<q", -20))
    self.assertEqual(expected, BakeContent.ServerRules({"warningBaseTicks": -20, "footSpeedMetersPerTick": 4587}))

  def test_the_directory_points_into_the_body(self):
    raw = BakeContent.ServerFile(0x1122334455667788, {"MATL": b"ab", "TMPL": b"", "RULE": b"xyz"})
    body = (struct.pack("<QI", 0x1122334455667788, 3)
            + b"MATL" + struct.pack("<III", 1, 60, 2)
            + b"TMPL" + struct.pack("<III", 1, 62, 0)
            + b"RULE" + struct.pack("<III", 1, 62, 3)
            + b"abxyz")
    self.assertEqual(b"ERSD" + struct.pack("<IQ", 1, BakeContent.Fnv1a64(body)) + body, raw)


if __name__ == "__main__":
  unittest.main()
