"""Tests Tools/SiegeTrial/ReadSiegeLog.py over hand-written logs (M4-J2), one for each layout: the Siege castle, whose
weak section is E3, and its mirror, whose weak section is W3; and their siege work, main points, breach points and
clustering (M5-B1).

Usage: python3 Tools/TestReadSiegeLog.py [-v]
"""

import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "SiegeTrial"))

import ReadSiegeLog  # noqa: E402

HEAD = """# participant: {participant}
# version: voxel
# build id: 0000000000001234
# server-data hash: 000000000000abcd
# scenario: {scenario}
# nearest: S3
# weak: {weak}
# gate: GH
# display: 1920 x 1080
# ui scale: 1.5
# start time: 2026-11-03 07:00:00 UTC
kind,tick,a,b,c,d,e
"""

ROWS = """shot,600,1,60,300 9 250,301 12 251,{weak}
shot,900,1,60,300 9 250,-,-
stone,600,1,150,shot,,
stone,900,1,150,shot,,
stone,950,2,25,repair,,
onscreen,9000,{weak},warning,deck,80,
opened,12000,{weak},,,,
onscreen,12000,{weak},opened,off,0,
opened,13000,GH,,,,
onscreen,13000,GH,opened,uncovered,0,
framed,13000,GH,opened,0.00,,
framed,9000,{weak},warning,4.50,,
framed,12000,{weak},opened,never,,
passed,10,{weak},attacker,7,,
passed,10,{weak},defender,2,,
passed,11,{weak},attacker,12,,
outcome,30000,won,1500.00,,,
"""


def Write(_folder, _name, _scenario, _weak, _participant="P01", _hits=()):
  hits = "".join(f"hit,{700 + index},1,{work},{structure},{hitPoints},\n" for index, (work, structure, hitPoints) in enumerate(_hits))
  text = HEAD.format(participant=_participant, scenario=_scenario, weak=_weak) + ROWS.format(weak=_weak) + hits
  (_folder / _name).write_text(text, encoding="utf-8", newline="\n")


def Stones(_structure, _count):
  return [("stone", _structure, 100)] * _count


def Blows(_structure, _count, _hitPoints=100):
  return [("blow", _structure, _hitPoints)] * _count


def Dug(_structure, _cubicMeters):
  return [("trench", _structure, 4)] * _cubicMeters


class ReadSiegeLogTests(unittest.TestCase):

  def setUp(self):
    self.directory = tempfile.TemporaryDirectory()
    self.folder = Path(self.directory.name)

  def tearDown(self):
    self.directory.cleanup()

  def Json(self):
    output = io.StringIO()
    with redirect_stdout(output):
      status = ReadSiegeLog.Main([str(self.folder), "--json"])
    self.assertEqual(0, status)
    return json.loads(output.getvalue())

  def test_ReadsTheSiegesWeakBreachAtE3AndTheGate(self):
    Write(self.folder, "Siege-P01-voxel-Siege-20261103-070000.csv", "Siege", "E3")
    (run,) = self.Json()
    self.assertEqual("won", run["outcome"])
    self.assertEqual(1500.0, run["seconds"])
    self.assertEqual([("E3", "weak"), ("GH", "gate")], [(breach["structure"], breach["role"]) for breach in run["breaches"]])
    self.assertEqual({"1": 2}, run["shots"])
    self.assertEqual({"shot": 3.0, "repair": 0.0}, run["stone"]["1"])
    self.assertEqual({"shot": 0.0, "repair": 0.25}, run["stone"]["2"])
    self.assertEqual({"attacker": 7, "defender": 2}, run["passed"]["E3"]["10"])
    self.assertEqual({"attacker": 12, "defender": 0}, run["passed"]["E3"]["11"])
    # One breach of two and the one warning began hidden; the warning was framed in 4.5 s, the weak breach never.
    self.assertEqual({"total": 2, "hidden": 1}, run["onscreen"]["opened"])
    self.assertEqual({"total": 1, "hidden": 1}, run["onscreen"]["warning"])
    self.assertEqual(4.5, run["framed"]["warning"])
    self.assertEqual(0.0, run["framed"]["opened"])
    self.assertEqual({"opened": 1, "warning": 0}, run["never"])

  def test_NamesABreachAtW3InTheMirrorAsTheWeakSection(self):
    Write(self.folder, "Siege-P02-voxel-SiegeMirror-20261103-080000.csv", "SiegeMirror", "W3", "P02")
    (run,) = self.Json()
    self.assertEqual("SiegeMirror", run["scenario"])
    self.assertEqual("weak", run["breaches"][0]["role"])
    self.assertEqual("W3", run["breaches"][0]["structure"])

  def test_StonesOnE3AndBlowsOnTheGateMakeE3TheWeakBreachPointThatDoesNotCluster(self):
    Write(self.folder, "Siege-P01-voxel-Siege-20261103-070000.csv", "Siege", "E3", _hits=Stones("E3", 10) + Blows("GH", 4))
    (run,) = self.Json()
    self.assertEqual({"stone": 1000, "blow": 0, "trench": 0, "total": 1000}, run["work"]["E3"])
    self.assertEqual([("E3", "weak"), ("GH", "gate")], [(point["structure"], point["role"]) for point in run["mainPoints"]])
    self.assertEqual(["E3"], run["breachPoint"])
    self.assertFalse(run["clusters"])

  def test_TheSameWorkOnW3InTheMirrorIsTheWeakBreachPoint(self):
    Write(self.folder, "Siege-P02-voxel-SiegeMirror-20261103-080000.csv", "SiegeMirror", "W3", "P02",
          _hits=Stones("W3", 10) + Blows("GH", 4))
    (run,) = self.Json()
    self.assertEqual(["W3"], run["breachPoint"])
    self.assertEqual("weak", run["mainPoints"][0]["role"])

  def test_FortyStonesOnS3And342CubicMetresBesideE3MakeBothMainPoints(self):
    # 4,000 hit points on S3 and 1,368 on E3: E3 takes 25.5 % of the work, so both are main points, S3 the breach point,
    # and E3, the weak section, keeps the run from clustering.
    Write(self.folder, "Siege-P01-voxel-Siege-20261103-070000.csv", "Siege", "E3", _hits=Stones("S3", 40) + Dug("E3", 342))
    (run,) = self.Json()
    self.assertEqual([("S3", "nearest"), ("E3", "weak")], [(point["structure"], point["role"]) for point in run["mainPoints"]])
    self.assertEqual(["S3"], run["breachPoint"])
    self.assertFalse(run["clusters"])

  def test_WorkOnTheNearestAndTheGateAloneClusters(self):
    Write(self.folder, "Siege-P01-voxel-Siege-20261103-070000.csv", "Siege", "E3",
          _hits=Stones("S3", 12) + Blows("GH", 6) + Stones("N4", 1))
    (run,) = self.Json()
    self.assertEqual(["S3", "GH"], [point["structure"] for point in run["mainPoints"]])
    self.assertTrue(run["clusters"])

  def test_ATieNamesEachAsTheBreachPoint(self):
    Write(self.folder, "Siege-P01-voxel-Siege-20261103-070000.csv", "Siege", "E3", _hits=Stones("E3", 5) + Stones("S3", 5))
    (run,) = self.Json()
    self.assertEqual(["E3", "S3"], run["breachPoint"])

  def test_ALogWithNoHitRowsReadsAndClusters(self):
    # M4's logs, written before the hit rows, still read; a run with no siege work counts as clustering.
    Write(self.folder, "Siege-P01-voxel-Siege-20261103-070000.csv", "Siege", "E3")
    (run,) = self.Json()
    self.assertEqual({}, run["work"])
    self.assertEqual([], run["mainPoints"])
    self.assertEqual([], run["breachPoint"])
    self.assertTrue(run["clusters"])

  def test_RefusesAHitOfNoKnownWork(self):
    Write(self.folder, "Siege-P01-voxel-Siege-20261103-070000.csv", "Siege", "E3", _hits=[("arrow", "E3", 1)])
    errors = io.StringIO()
    sys.stderr, saved = errors, sys.stderr
    try:
      self.assertEqual(1, ReadSiegeLog.Main([str(self.folder)]))
    finally:
      sys.stderr = saved
    self.assertIn("no siege work named arrow", errors.getvalue())

  def test_ASectionOfNoRoleIsAnother(self):
    self.assertEqual("another", ReadSiegeLog.RoleOf("N4", {"nearest": "S3", "weak": "E3", "gate": "GH"}))
    self.assertEqual("nearest", ReadSiegeLog.RoleOf("S3", {"nearest": "S3", "weak": "E3", "gate": "GH"}))

  def test_PrintsEachRunInWords(self):
    Write(self.folder, "Siege-P01-voxel-Siege-20261103-070000.csv", "Siege", "E3")
    output = io.StringIO()
    with redirect_stdout(output):
      self.assertEqual(0, ReadSiegeLog.Main([str(self.folder)]))
    text = output.getvalue()
    self.assertIn("won at 25:00", text)
    self.assertIn("breach E3 (weak) at 10:00", text)
    self.assertIn("1 of 2 breaches under the deck or off screen", text)
    self.assertIn("no siege work; clusters", text)

  def test_RefusesARowOfTheWrongWidthByName(self):
    Write(self.folder, "Siege-P01-voxel-Siege-20261103-070000.csv", "Siege", "E3")
    path = next(self.folder.glob("*.csv"))
    path.write_text(path.read_text(encoding="utf-8") + "opened,1,E3\n", encoding="utf-8", newline="\n")
    errors = io.StringIO()
    sys.stderr, saved = errors, sys.stderr
    try:
      self.assertEqual(1, ReadSiegeLog.Main([str(self.folder)]))
    finally:
      sys.stderr = saved
    self.assertIn(path.name, errors.getvalue())
    self.assertIn("3 columns", errors.getvalue())


if __name__ == "__main__":
  unittest.main()
