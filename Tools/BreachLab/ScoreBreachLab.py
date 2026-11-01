"""Scores a breach lab session (M3-G2; GameDesign.md, *Hypotheses*, H2, and *M3's values*, item 3).

Reads every log that `Earthrise --lab` wrote into a folder (MvpPlan-M3.md, *The lab's log*), checks them, and prints
each participant's accuracy over L01 to L16, the median over the participants, and PASS or FAIL against 0.65.

The checks, each of which names the log and the item that failed it:
- every item row's live state hash matched its headless one: the live run reached the key's world;
- every log answers L01 to L16 once each, and the practice item P0, which is not scored;
- the keys agree across the logs, and split 8 open and 8 shut, as the item set does.

At 16 items 0.65 needs 11 right: 11 of 16 is 0.6875, and 10 is 0.625 (Design/Review/PaperTests/BreachPrediction.md,
*Scoring*). With six participants the median is the mean of the third and fourth best, so it passes when those two add
up to 21 or more.

Usage:
  python3 Tools/BreachLab/ScoreBreachLab.py <folder>          print the scores
  python3 Tools/BreachLab/ScoreBreachLab.py <folder> --json   print them as JSON

Exit status 0 when the logs pass their checks, whether the median passes or not, and 1 when a check fails, in which case
nothing is scored. Standard library only.
"""

import argparse
import csv
import json
import statistics
import sys
from pathlib import Path

PASS_MARK = 0.65
PRACTICE = "P0"
SCORED = tuple(f"L{index:02}" for index in range(1, 17))
COLUMNS = ("kind", "id", "n", "key", "answer", "correct", "secondsToAnswer", "integrityOn", "passabilityOn",
           "cameraTurns", "zoomNotches", "liveHash", "headlessHash", "matched")
NOTE = "At 16 items 0.65 needs 11 right: 11 of 16 is 0.6875, and 10 is 0.625."


class LogError(Exception):
  """A log that fails a check: the message names the log and the item."""


def ReadLog(_path):
  """The participant and the item rows of one log, keyed by item id. Raises LogError for a log that is not one."""
  participant = None
  rows = {}
  lines = _path.read_text(encoding="utf-8").splitlines()
  body = []
  for line in lines:
    if line.startswith("#"):
      key, _, value = line[1:].partition(":")
      if key.strip() == "participant":
        participant = value.strip()
    elif line:
      body.append(line)
  if participant is None:
    raise LogError(f"{_path.name}: no participant")
  reader = csv.reader(body)
  header = next(reader, None)
  if header is None or tuple(header) != COLUMNS:
    raise LogError(f"{_path.name}: its header is not the lab's")
  for row in reader:
    if not row or row[0] != "item":
      continue  # the free fire's commands
    if len(row) != len(COLUMNS):
      raise LogError(f"{_path.name}: a row of {len(row)} columns, not {len(COLUMNS)}")
    item = dict(zip(COLUMNS, row))
    if item["id"] in rows:
      raise LogError(f"{_path.name}: {item['id']} answered twice")
    rows[item["id"]] = item
  return participant, rows


def CheckLog(_path, _rows):
  """Raises LogError unless every item matched its headless hash and the log answers P0 and L01 to L16."""
  for itemId, item in _rows.items():
    if item["matched"] != "1" or item["liveHash"] != item["headlessHash"]:
      raise LogError(f"{_path.name}: {itemId}'s live hash {item['liveHash']} did not match its headless "
                     f"{item['headlessHash']}")
    if item["key"] not in ("open", "shut") or item["answer"] not in ("open", "shut"):
      raise LogError(f"{_path.name}: {itemId}'s key or answer is not open or shut")
    if item["correct"] != ("1" if item["key"] == item["answer"] else "0"):
      raise LogError(f"{_path.name}: {itemId}'s correct does not agree with its key and its answer")
  expected = {PRACTICE, *SCORED}
  if set(_rows) != expected:
    missing = sorted(expected - set(_rows))
    extra = sorted(set(_rows) - expected)
    raise LogError(f"{_path.name}: missing {', '.join(missing) or 'nothing'}, "
                   f"and unknown {', '.join(extra) or 'nothing'}")


def Score(_folder):
  """The session's scores, as a dict. Raises LogError when a log fails a check."""
  logs = sorted(Path(_folder).glob("*.csv"))
  if not logs:
    raise LogError(f"{_folder}: no logs")
  participants = []
  keys = None
  for path in logs:
    participant, rows = ReadLog(path)
    CheckLog(path, rows)
    logKeys = {itemId: rows[itemId]["key"] for itemId in SCORED}
    if keys is None:
      keys = logKeys
      opens = sum(1 for key in keys.values() if key == "open")
      if opens != len(SCORED) // 2:
        raise LogError(f"{path.name}: the keys split {opens} open and {len(SCORED) - opens} shut, not 8 and 8")
    for itemId in SCORED:
      if logKeys[itemId] != keys[itemId]:
        raise LogError(f"{path.name}: {itemId}'s key is {logKeys[itemId]}, where {logs[0].name} has {keys[itemId]}")
    right = sum(1 for itemId in SCORED if rows[itemId]["correct"] == "1")
    participants.append({"participant": participant, "log": path.name, "right": right, "items": len(SCORED),
                         "accuracy": right / len(SCORED)})
  median = statistics.median(entry["accuracy"] for entry in participants)
  return {"participants": participants, "median": median, "passMark": PASS_MARK,
          "verdict": "PASS" if median >= PASS_MARK else "FAIL", "note": NOTE}


def Main(_arguments):
  parser = argparse.ArgumentParser(description="Scores a breach lab session's logs.")
  parser.add_argument("folder", help="the folder of the session's logs")
  parser.add_argument("--json", action="store_true", help="print the scores as JSON")
  options = parser.parse_args(_arguments)
  try:
    scores = Score(options.folder)
  except LogError as error:
    print(f"ScoreBreachLab: {error}", file=sys.stderr)
    return 1
  if options.json:
    print(json.dumps(scores, indent=2))
    return 0
  for entry in scores["participants"]:
    print(f"{entry['participant']}: {entry['right']} of {entry['items']}, {entry['accuracy']:.4f}  ({entry['log']})")
  print(f"median: {scores['median']:.4f} against {PASS_MARK}: {scores['verdict']}")
  print(NOTE)
  return 0


if __name__ == "__main__":
  sys.exit(Main(sys.argv[1:]))
