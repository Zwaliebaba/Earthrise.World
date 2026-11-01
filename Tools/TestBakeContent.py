"""Tests for the server half of Tools/BakeContent.py: the ServerData file of ADR-016.

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
    self.assertEqual(["MATL", "TMPL", "RULE"], list(self.file.sections))
    self.assertEqual({"MATL": 1, "TMPL": 1, "RULE": 1}, self.file.versions)
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
    self.assertEqual(9175, rules["cavalrySpeedMetersPerTick"])
    self.assertEqual(2293, rules["machinesSpeedMetersPerTick"])
    self.assertEqual(20, rules["warningBaseTicks"])
    self.assertEqual(1607, rules["gravityMetersPerTickSquared"])
    self.assertEqual(sorted(rules), list(rules))

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


class HandWrittenEncodingTests(unittest.TestCase):
  """The encoding against bytes written out by hand, so the reader in Simulation has a second statement of it."""

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
