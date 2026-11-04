"""Tests Tools/SiegeTrial/ScoreSiegePlaytest.py over hand-written session folders (M5-B2): eight RTS players in the four
cells, their two trial logs and their lab logs, and the facilitator's sheet; then each clause failing alone, the folder's
refusals, a dropped participant, the pilot's folder and the deck's figures.

Usage: python3 Tools/TestScoreSiegePlaytest.py [-v]
"""

import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "SiegeTrial"))

import ScoreSiegePlaytest  # noqa: E402

BUILD = "0000000000001234"
SERVER_DATA = "000000000000abcd"
ROLES = {"Siege": {"nearest": "S3", "weak": "E3"}, "SiegeMirror": {"nearest": "S2", "weak": "W3"}}
LAB_COLUMNS = ",".join(ScoreSiegePlaytest.ScoreBreachLab.COLUMNS)
KEYS = ["open"] * 8 + ["shut"] * 8


def SiegeLog(_participant, _game, _cell, _version, _layout, _target, _outcome="clock", _build=BUILD, _rows=""):
  """A trial log whose ten stones all struck _target, a role or a structure's name."""
  roles = ROLES[_layout]
  structure = roles.get(_target, _target)
  group = ScoreSiegePlaytest.GROUPS[_participant[0]]
  head = (f"# participant: {_participant}\n# version: {_version}\n# build id: {_build}\n# server-data hash: {SERVER_DATA}\n"
          f"# scenario: {_layout}\n# nearest: {roles['nearest']}\n# weak: {roles['weak']}\n# gate: GH\n"
          f"# display: 1920 x 1080\n# ui scale: 1.5\n# start time: 2026-11-04 09:00:00 UTC\n"
          f"# game: {_game}\n# cell: {_cell}\n# group: {group}\nkind,tick,a,b,c,d,e\n")
  hits = "".join(f"hit,{1000 + index},1,stone,{structure},100,\n" for index in range(10)) if _target else ""
  return head + hits + _rows + f"outcome,40000,{_outcome},2000.00,,,\n"


def LabLog(_participant, _right):
  """A lab log answering P0 and L01 to L16, the first _right of them right."""
  rows = [f"item,P0,1,shut,shut,1,5.0,1,0,0,0,{'ab' * 8},{'ab' * 8},1"]
  for index, key in enumerate(KEYS):
    answer = key if index < _right else ("shut" if key == "open" else "open")
    rows.append(f"item,L{index + 1:02},2,{key},{answer},{1 if answer == key else 0},5.0,1,0,0,0,{'cd' * 8},{'cd' * 8},1")
  return (f"# participant: {_participant}\n# build id: {BUILD}\n# server-data hash: {SERVER_DATA}\n" + LAB_COLUMNS + "\n" +
          "\n".join(rows) + "\n")


CRATER_KEYS = ["trebuchet", "ram", "trebuchet", "trebuchet", "ram", "ram", "trebuchet", "ram", "trebuchet"]  # C0 to C8


def CraterLog(_participant, _right, _skip=None):
  """A crater log answering C0 to C8, the first _right of C1 to C8 right, and C0 right; _skip leaves one item out."""
  rows = []
  for index, key in enumerate(CRATER_KEYS):
    itemId = f"C{index}"
    if itemId == _skip:
      continue
    right = index == 0 or index <= _right
    answer = key if right else ("ram" if key == "trebuchet" else "trebuchet")
    rows.append(f"item,{itemId},{key},{answer},{1 if right else 0},2.000")
  return (f"# participant: {_participant}\n# build id: {BUILD}\n# server-data hash: {SERVER_DATA}\n# item-set hash: 00000000000000c2\n"
      "kind,id,key,answer,correct,secondsToAnswer\n" + "\n".join(rows) + "\n")


def WriteCraters(_folder, _rights, _skip=None):
  (_folder / "Craters").mkdir(exist_ok=True)
  for participant, right in _rights.items():
    (_folder / "Craters" / f"Craters-{participant}-20261104-120000.csv").write_text(
      CraterLog(participant, right, _skip if participant == "S04" else None), encoding="utf-8", newline="\n")


class Session:
  """A session's folder under construction: each participant's choices, then Write."""

  def __init__(self, _folder):
    self.folder = _folder
    self.rows = []
    (self.folder / "Siege").mkdir()
    (self.folder / "BreachLab").mkdir()

  def Add(self, _participant, _voxelTarget="weak", _controlTarget="nearest", _right=12, _preferred="voxel",
          _won=(), _dropped="", _wrongVersion=False, _build=BUILD, _rows=""):
    cell, group = ScoreSiegePlaytest.CellOf(_participant)
    games = ScoreSiegePlaytest.CELLS[cell]
    preferred = ""
    if not _dropped:
      for number, (version, layout) in enumerate(games, start=1):
        target = _voxelTarget if version == "voxel" else _controlTarget
        logged = ("control" if version == "voxel" else "voxel") if _wrongVersion and number == 1 else version
        name = f"Siege-{_participant}-{logged}-{layout}-2026110{number}-090000.csv"
        text = SiegeLog(_participant, number, cell, logged, layout, target, "won" if version in _won else "clock", _build, _rows)
        (self.folder / "Siege" / name).write_text(text, encoding="utf-8", newline="\n")
      (self.folder / "BreachLab" / f"BreachLab-{_participant}-20261104-110000.csv").write_text(
        LabLog(_participant, _right), encoding="utf-8", newline="\n")
      preferred = "first" if games[0][0] == _preferred else "second"
    self.rows.append(f"{_participant},{group},{cell},{preferred},{_dropped}")
    return self

  def Write(self):
    text = f"# build ids: {BUILD}\n# server-data hash: {SERVER_DATA}\nparticipant,group,cell,preferred,dropped\n"
    (self.folder / "Session.csv").write_text(text + "\n".join(self.rows) + "\n", encoding="utf-8", newline="\n")
    return self.folder


def Eight(_session, _clustering=3, _right=12, _preferring=7, **_choices):
  """S01 to S08: the first _clustering of them break in at the nearest section, the rest at the weak one; each right on
  _right lab items; the first _preferring of them preferring the voxel version."""
  for number in range(1, 9):
    _session.Add(f"S{number:02}", _voxelTarget="nearest" if number <= _clustering else "weak", _right=_right,
                 _preferred="voxel" if number <= _preferring else "control", **_choices)
  return _session


class ScoreSiegePlaytestTests(unittest.TestCase):

  def setUp(self):
    self.directory = tempfile.TemporaryDirectory()
    self.folder = Path(self.directory.name)

  def tearDown(self):
    self.directory.cleanup()

  def Json(self, *_options):
    output = io.StringIO()
    with redirect_stdout(output):
      status = ScoreSiegePlaytest.Main([str(self.folder), "--json", *_options])
    self.assertEqual(0, status)
    return json.loads(output.getvalue())

  def Refusal(self):
    errors = io.StringIO()
    with redirect_stderr(errors), redirect_stdout(io.StringIO()):
      self.assertEqual(1, ScoreSiegePlaytest.Main([str(self.folder)]))
    return errors.getvalue()

  def test_EightWhoBreakInAtTheWeakPointPredictAndPreferPassH2(self):
    Eight(Session(self.folder)).Write()
    scores = self.Json()
    clauses = scores["clauses"]
    self.assertEqual(3, clauses["clustering"]["clustered"])
    self.assertEqual("PASS", clauses["clustering"]["verdict"])
    self.assertEqual(0.75, clauses["prediction"]["median"])
    self.assertEqual("PASS", clauses["prediction"]["verdict"])
    self.assertEqual(7, clauses["preference"]["preferredVoxel"])
    self.assertEqual("PASS", clauses["preference"]["verdict"])
    self.assertEqual("PASS", clauses["h2"])
    # The control games all went to the nearest section, and cluster; under no mark.
    self.assertEqual(8, scores["unmarked"]["controlClustered"])
    # Cells 0 and 3 play the voxel version on Siege, 1 and 2 on the mirror: the weak point on each, and the nearest for
    # the three who clustered, S01 to S03.
    self.assertEqual({"Siege": {"nearest": 1, "weak": 3}, "SiegeMirror": {"nearest": 2, "weak": 2}},
                     scores["unmarked"]["voxelBreachRolesByLayout"])

  def test_FiveClusteringVoxelGamesFailTheClauseAndFourPass(self):
    Eight(Session(self.folder), _clustering=5).Write()
    clauses = self.Json()["clauses"]
    self.assertEqual("FAIL", clauses["clustering"]["verdict"])
    self.assertEqual("PASS", clauses["prediction"]["verdict"])
    self.assertEqual("PASS", clauses["preference"]["verdict"])
    self.assertEqual("FAIL", clauses["h2"])

  def test_FourClusteringVoxelGamesPass(self):
    Eight(Session(self.folder), _clustering=4).Write()
    self.assertEqual("PASS", self.Json()["clauses"]["clustering"]["verdict"])

  def test_AGameWithNoSiegeWorkClusters(self):
    session = Eight(Session(self.folder), _clustering=4)
    session.rows.clear()
    for path in (self.folder / "Siege").glob("Siege-S05-*"):
      path.unlink()
    for number in range(1, 9):
      session.Add(f"S{number:02}", _voxelTarget=None if number == 5 else ("nearest" if number <= 4 else "weak"))
    session.Write()
    self.assertEqual(5, self.Json()["clauses"]["clustering"]["clustered"])

  def test_ALabMedianBelow065FailsPredictionAlone(self):
    Eight(Session(self.folder), _right=10).Write()
    clauses = self.Json()["clauses"]
    self.assertEqual(0.625, clauses["prediction"]["median"])
    self.assertEqual("FAIL", clauses["prediction"]["verdict"])
    self.assertEqual("PASS", clauses["clustering"]["verdict"])
    self.assertEqual("PASS", clauses["preference"]["verdict"])
    self.assertEqual("FAIL", clauses["h2"])

  def test_SixOfEightPreferringFailsPreferenceAlone(self):
    Eight(Session(self.folder), _preferring=6).Write()
    clauses = self.Json()["clauses"]
    self.assertEqual("FAIL", clauses["preference"]["verdict"])
    self.assertEqual("PASS", clauses["clustering"]["verdict"])
    self.assertEqual("PASS", clauses["prediction"]["verdict"])
    self.assertEqual("FAIL", clauses["h2"])

  def test_APreferenceForTheOneVersionWonIsCounted(self):
    Eight(Session(self.folder), _won=("voxel",)).Write()
    unmarked = self.Json()["unmarked"]
    self.assertEqual({"voxel": 8, "control": 0}, unmarked["wins"])
    self.assertEqual(7, unmarked["preferredTheVersionWon"])

  def test_RefusesALogFromABuildTheSheetDoesNotList(self):
    session = Eight(Session(self.folder))
    session.rows.pop()
    for path in (self.folder / "Siege").glob("Siege-S08-*"):
      path.unlink()
    session.Add("S08", _build="00000000deadbeef").Write()
    error = self.Refusal()
    self.assertIn("Siege-S08-", error)
    self.assertIn("build id 00000000deadbeef is not among the sheet's", error)

  def test_RefusesAGame1LogInTheWrongVersionForItsCell(self):
    session = Eight(Session(self.folder))
    session.rows.pop()
    for path in (self.folder / "Siege").glob("Siege-S08-*"):
      path.unlink()
    session.Add("S08", _wrongVersion=True).Write()
    self.assertIn("game 1 of cell 3 is the control version on SiegeMirror", self.Refusal())

  def test_RefusesAParticipantWithNoLabLog(self):
    Eight(Session(self.folder)).Write()
    (self.folder / "BreachLab" / "BreachLab-S04-20261104-110000.csv").unlink()
    self.assertIn("S04 has no lab log", self.Refusal())

  def test_RefusesSevenRtsPlayers(self):
    session = Session(self.folder)
    for number in range(1, 8):
      session.Add(f"S{number:02}")
    session.Write()
    self.assertIn("7 RTS players not dropped, not 8", self.Refusal())

  def test_ADroppedParticipantIsLeftOutAndTheirReplacementCounts(self):
    # S01 stopped in their first game; S09 takes cell 0.
    session = Eight(Session(self.folder))
    session.rows.clear()
    for path in (self.folder / "Siege").glob("Siege-S01-*"):
      path.unlink()
    (self.folder / "BreachLab" / "BreachLab-S01-20261104-110000.csv").unlink()
    session.Add("S01", _dropped="crashed in game 1")
    for number in range(2, 10):
      session.Add(f"S{number:02}", _voxelTarget="nearest" if number <= 3 else "weak", _preferred="voxel" if number <= 8 else "control")
    session.Write()
    scores = self.Json()
    self.assertEqual(["S01"], scores["dropped"])
    self.assertEqual(8, len(scores["participants"]))
    self.assertEqual(0, [record["participant"] for record in scores["participants"]].count("S01"))
    self.assertEqual(0, scores["participants"][-1]["cell"])

  def test_NovicesAreReportedApart(self):
    Eight(Session(self.folder)).Add("N01", _voxelTarget="nearest", _right=8).Add("N02", _right=10).Write()
    scores = self.Json()
    self.assertEqual({"clustered": 1, "labMedian": 0.5625, "preferredVoxel": 2, "of": 2}, scores["novices"])
    self.assertEqual(3, scores["clauses"]["clustering"]["clustered"])

  def test_ThePilotsFolderHasNoVerdict(self):
    Session(self.folder).Add("X01").Add("X02", _voxelTarget="nearest").Write()
    scores = self.Json("--pilot")
    self.assertNotIn("clauses", scores)
    self.assertEqual(2, len(scores["participants"]))
    self.assertEqual(["voxel", "control"], [game["version"] for game in scores["participants"][0]["games"]])

  def test_APilotIdInTheSessionsFolderIsRefused(self):
    Eight(Session(self.folder)).Add("X01").Write()
    self.assertIn("X01 is pilot, a pilot, whose folder is Pilot/", self.Refusal())

  def test_TheDeckSlimsWhenMoreThanAQuarterBeganHiddenWentUnframedPastAWarning(self):
    # Each game: two openings under the deck, one framed in 3 s and one never, and two warnings off screen, framed in
    # 5 s and 9 s: 2 of 4 lost, more than a quarter.
    rows = ("onscreen,9000,E3,warning,deck,80,\nframed,9000,E3,warning,3.00,,\n"
            "onscreen,9100,S3,warning,deck,60,\nframed,9100,S3,warning,never,,\n"
            "onscreen,12000,E3,opened,off,0,\nframed,12000,E3,opened,5.00,,\n"
            "onscreen,12100,N2,opened,off,0,\nframed,12100,N2,opened,9.00,,\n")
    Eight(Session(self.folder), _rows=rows).Write()
    deck = self.Json()["deck"]
    self.assertEqual({"hidden": 64, "lost": 32, "slims": True}, deck)

  def test_TheDeckIsTheOwnersCallAtAQuarter(self):
    rows = ("onscreen,9000,E3,warning,deck,80,\nframed,9000,E3,warning,3.00,,\n"
            "onscreen,9100,S3,warning,deck,60,\nframed,9100,S3,warning,6.75,,\n"
            "onscreen,12000,E3,opened,off,0,\nframed,12000,E3,opened,5.00,,\n"
            "onscreen,12100,N2,opened,off,0,\nframed,12100,N2,opened,never,,\n"
            "onscreen,13000,W1,opened,uncovered,0,\nframed,13000,W1,opened,0.00,,\n")
    Eight(Session(self.folder), _rows=rows).Write()
    self.assertEqual({"hidden": 64, "lost": 16, "slims": False}, self.Json()["deck"])

  def test_CraterItemsAtSixOfEightAnswerYesAndBearOnNothingInH2(self):
    Eight(Session(self.folder), _preferring=6).Write()
    WriteCraters(self.folder, {f"S{number:02}": 6 for number in range(1, 9)})
    scores = self.Json()
    self.assertEqual({"median": 0.75, "mark": 0.75, "items": 8, "answer": "yes"}, scores["craters"])
    self.assertEqual(6, scores["participants"][0]["craterRight"])
    self.assertEqual("FAIL", scores["clauses"]["h2"])  # preference fails alone, whatever the craters say

  def test_CraterItemsAtFiveOfEightAnswerNo(self):
    Eight(Session(self.folder)).Write()
    WriteCraters(self.folder, {f"S{number:02}": 5 for number in range(1, 9)})
    scores = self.Json()
    self.assertEqual("no", scores["craters"]["answer"])
    self.assertEqual("PASS", scores["clauses"]["h2"])

  def test_RefusesAParticipantWithNoCraterLogOnceCratersIsThere(self):
    Eight(Session(self.folder)).Write()
    WriteCraters(self.folder, {f"S{number:02}": 6 for number in range(1, 8)})
    self.assertIn("S08 has no crater log", self.Refusal())

  def test_RefusesACraterLogMissingAnItem(self):
    Eight(Session(self.folder)).Write()
    WriteCraters(self.folder, {f"S{number:02}": 6 for number in range(1, 9)}, _skip="C4")
    self.assertIn("not C0 to C8 once each", self.Refusal())

  def test_NoCratersFolderNoCraterFigures(self):
    Eight(Session(self.folder)).Write()
    self.assertNotIn("craters", self.Json())

  def test_PrintsTheVerdictInWords(self):
    Eight(Session(self.folder)).Write()
    output = io.StringIO()
    with redirect_stdout(output):
      self.assertEqual(0, ScoreSiegePlaytest.Main([str(self.folder)]))
    text = output.getvalue()
    self.assertIn("clustering: 3 of 8 voxel games cluster, failing at 5: PASS", text)
    self.assertIn("preference: 7 of 8 preferred the voxel version, against 7: PASS", text)
    self.assertIn("H2: PASS", text)


if __name__ == "__main__":
  unittest.main()
