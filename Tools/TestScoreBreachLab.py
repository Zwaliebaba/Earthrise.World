"""Tests for Tools/BreachLab/ScoreBreachLab.py (M3-G2), over hand-written logs in the shape `Earthrise --lab` writes
(MvpPlan-M3.md, *The lab's log*): the median and the verdict for the plan's two sets of six, and each check failing by
name.

Usage:
  python3 Tools/TestScoreBreachLab.py         every case
  python3 Tools/TestScoreBreachLab.py -v      with each case's name
"""

import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS / "BreachLab"))

import ScoreBreachLab  # noqa: E402

HEADER = ("kind,id,n,key,answer,correct,secondsToAnswer,integrityOn,passabilityOn,cameraTurns,zoomNotches,liveHash,"
          "headlessHash,matched")
# A key that splits 8 and 8, open on the odd items.
KEYS = {f"L{index:02}": "open" if index % 2 else "shut" for index in range(1, 17)}
HASH = "0123456789abcdef"


def Log(_participant, _right, _keys=None, _mismatched=None):
  """A log in which the first _right of L01 to L16 are answered right and the rest wrong; _mismatched names an item
  whose live hash did not match."""
  keys = _keys or KEYS
  lines = [f"# participant: {_participant}", "# build id: 0000000000000001", "# server-data hash: 0000000000000002",
           "# item-set hash: d2bbb4766d8d02cc", "# display size: 1920x1080", "# UI scale: 1",
           "# start time: 2026-11-02 10:00:00 UTC", HEADER, "command,120,6.00,FireEngine 380 9 180,click",
           f"item,P0,1,shut,open,0,3.10,1,0,0,0.0,{HASH},{HASH},1"]
  for index, (itemId, key) in enumerate(keys.items()):
    right = index < _right
    answer = key if right else ("shut" if key == "open" else "open")
    headless = "fedcba9876543210" if itemId == _mismatched else HASH
    matched = "0" if itemId == _mismatched else "1"
    lines.append(f"item,{itemId},2,{key},{answer},{1 if right else 0},4.50,1,0,1,2.0,{HASH},{headless},{matched}")
  return "\n".join(lines) + "\n"


class ScoreBreachLabTests(unittest.TestCase):
  def Session(self, _logs):
    """A folder of logs, one a participant, and the scorer's exit status, standard output and standard error on it."""
    folder = tempfile.TemporaryDirectory()
    self.addCleanup(folder.cleanup)
    for name, text in _logs.items():
      (Path(folder.name) / name).write_text(text, encoding="utf-8")
    out, err = io.StringIO(), io.StringIO()
    with redirect_stdout(out), redirect_stderr(err):
      status = ScoreBreachLab.Main([folder.name, "--json"])
    return status, out.getvalue(), err.getvalue()

  def Six(self, _rights):
    return {f"BreachLab-P{index + 1:02}-20261102-100000.csv": Log(f"P{index + 1:02}", right)
            for index, right in enumerate(_rights)}

  def test_six_with_eleven_eleven_ten_twelve_nine_thirteen_pass(self):
    status, out, _ = self.Session(self.Six([11, 11, 10, 12, 9, 13]))
    self.assertEqual(status, 0)
    scores = json.loads(out)
    self.assertEqual(scores["median"], 0.6875)
    self.assertEqual(scores["verdict"], "PASS")
    self.assertEqual([entry["right"] for entry in scores["participants"]], [11, 11, 10, 12, 9, 13])

  def test_six_with_ten_ten_eleven_nine_twelve_eight_fail(self):
    status, out, _ = self.Session(self.Six([10, 10, 11, 9, 12, 8]))
    self.assertEqual(status, 0)
    scores = json.loads(out)
    self.assertEqual(scores["median"], 0.625)
    self.assertEqual(scores["verdict"], "FAIL")
    self.assertIn("11 right", scores["note"])

  def test_a_log_whose_hash_did_not_match_fails_by_name(self):
    logs = self.Six([11, 11, 10, 12, 9, 13])
    logs["BreachLab-P04-20261102-100000.csv"] = Log("P04", 12, _mismatched="L07")
    status, out, err = self.Session(logs)
    self.assertEqual(status, 1)
    self.assertEqual(out, "")
    self.assertIn("BreachLab-P04-20261102-100000.csv", err)
    self.assertIn("L07", err)

  def test_keys_that_disagree_or_do_not_split_eight_and_eight_fail(self):
    logs = self.Six([11, 11, 10, 12, 9, 13])
    flipped = dict(KEYS)
    flipped["L01"] = "shut"
    flipped["L02"] = "open"
    logs["BreachLab-P02-20261102-100000.csv"] = Log("P02", 11, _keys=flipped)
    status, _, err = self.Session(logs)
    self.assertEqual(status, 1)
    self.assertIn("L01", err)
    lopsided = {itemId: "open" for itemId in KEYS}
    status, _, err = self.Session({"BreachLab-P01-20261102-100000.csv": Log("P01", 11, _keys=lopsided)})
    self.assertEqual(status, 1)
    self.assertIn("16 open", err)

  def test_a_log_missing_an_item_fails(self):
    text = "\n".join(line for line in Log("P01", 11).splitlines() if not line.startswith("item,L16,")) + "\n"
    status, _, err = self.Session({"BreachLab-P01-20261102-100000.csv": text})
    self.assertEqual(status, 1)
    self.assertIn("L16", err)


if __name__ == "__main__":
  unittest.main()
