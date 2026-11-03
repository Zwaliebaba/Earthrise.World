"""Reads the Siege trial's logs (M4-J2; MvpPlan-M4.md, *The trial's log*; GameDesign.md, *Hypotheses*, H2).

Reads every log that `Earthrise --siege` wrote into a folder and prints, for each run:
- its participant, version and layout, its outcome and the game time it ended at;
- each breach, a section's first foot route from the camp, with its role from the log's comment lines: the section
  nearest the camp, the weak one, the gatehouse, or another;
- the shots each player threw, and the stone each spent on shots and on repair, in cubic metres;
- the figures each game minute through each breach, the attacker's and the defender's;
- how many breaches and collapse warnings began under the deck or off screen, out of how many, and the median game
  seconds before the player framed them, with how many were never framed (MvpPlan.md, M4, input 25).

Usage:
  python3 Tools/SiegeTrial/ReadSiegeLog.py <folder>          print each run
  python3 Tools/SiegeTrial/ReadSiegeLog.py <folder> --json   print them as JSON

Exit status 0 when every log reads, and 1 when one does not, naming it. Standard library only.
"""

import argparse
import csv
import json
import statistics
import sys
from pathlib import Path

COLUMNS = ("kind", "tick", "a", "b", "c", "d", "e")
ROLES = ("nearest", "weak", "gate")
HIDDEN = ("deck", "off")
HUNDREDTHS = 100


class LogError(Exception):
  """A log that does not read: the message names the log and the line."""


def ReadComments(_lines):
  """The `# key: value` lines at the log's head, as a dictionary."""
  comments = {}
  for line in _lines:
    if not line.startswith("#"):
      break
    key, _, value = line[1:].partition(":")
    comments[key.strip()] = value.strip()
  return comments


def RoleOf(_structure, _comments):
  """The breach's role: nearest, weak, gate or another."""
  for role in ROLES:
    if _comments.get(role) == _structure:
      return role
  return "another"


def ReadLog(_path):
  """One run's summary. Raises LogError for a log that is not one."""
  lines = _path.read_text(encoding="utf-8").splitlines()
  comments = ReadComments(lines)
  for key in ("participant", "version", "scenario", "nearest", "weak"):
    if key not in comments:
      raise LogError(f"{_path.name}: no {key} line")
  body = [line for line in lines if line and not line.startswith("#")]
  reader = csv.reader(body)
  header = next(reader, None)
  if header is None or tuple(header) != COLUMNS:
    raise LogError(f"{_path.name}: its header is not the trial's")
  run = {
    "log": _path.name,
    "participant": comments["participant"],
    "version": comments["version"],
    "scenario": comments["scenario"],
    "outcome": None,
    "seconds": None,
    "breaches": [],
    "shots": {},
    "stone": {},
    "passed": {},
    "onscreen": {"opened": {"total": 0, "hidden": 0}, "warning": {"total": 0, "hidden": 0}},
    "framed": {"opened": [], "warning": []},
    "never": {"opened": 0, "warning": 0},
  }
  for number, row in enumerate(reader, start=2 + len(comments)):
    if len(row) != len(COLUMNS):
      raise LogError(f"{_path.name}, line {number}: {len(row)} columns, not {len(COLUMNS)}")
    kind, tick, a, b, c, d, _ = row
    try:
      if kind == "opened":
        run["breaches"].append({"tick": int(tick), "structure": a, "role": RoleOf(a, comments)})
      elif kind == "shot":
        run["shots"][a] = run["shots"].get(a, 0) + 1
      elif kind == "stone":
        spent = run["stone"].setdefault(a, {"shot": 0.0, "repair": 0.0})
        spent[c] += int(b) / HUNDREDTHS
      elif kind == "passed":
        structure = run["passed"].setdefault(a, {})
        minute = structure.setdefault(tick, {"attacker": 0, "defender": 0})
        minute[b] += int(c)
      elif kind == "onscreen":
        counts = run["onscreen"][b]
        counts["total"] += 1
        counts["hidden"] += 1 if c in HIDDEN else 0
      elif kind == "framed":
        if c == "never":
          run["never"][b] += 1
        else:
          run["framed"][b].append(float(c))
      elif kind == "outcome":
        run["outcome"] = a
        run["seconds"] = float(b)
    except (KeyError, ValueError) as error:
      raise LogError(f"{_path.name}, line {number}: {kind} row does not read ({error})") from error
  for reason in ("opened", "warning"):
    seconds = run["framed"][reason]
    run["framed"][reason] = statistics.median(seconds) if seconds else None
  return run


def Clock(_seconds):
  """Game seconds as m:ss."""
  whole = int(_seconds)
  return f"{whole // 60}:{whole % 60:02}"


def Print(_runs):
  for run in _runs:
    ended = f"{run['outcome']} at {Clock(run['seconds'])}" if run["outcome"] else "no outcome"
    print(f"{run['log']}: {run['participant']}, {run['version']}, {run['scenario']}, {ended}")
    for breach in run["breaches"]:
      print(f"  breach {breach['structure']} ({breach['role']}) at {Clock(breach['tick'] / 20)}")
    for player, shots in sorted(run["shots"].items()):
      print(f"  player {player}: {shots} shots")
    for player, spent in sorted(run["stone"].items()):
      print(f"  player {player}: {spent['shot']:.2f} m3 on shots, {spent['repair']:.2f} m3 on repair")
    for structure, minutes in sorted(run["passed"].items()):
      each = ", ".join(f"{minute}: {figures['attacker']}/{figures['defender']}" for minute, figures in
                       sorted(minutes.items(), key=lambda _item: int(_item[0])))
      print(f"  through {structure}, attacker/defender a minute: {each}")
    for reason, word in (("opened", "breaches"), ("warning", "warnings")):
      counts = run["onscreen"][reason]
      median = run["framed"][reason]
      framed = f"{median:.2f} s median to frame" if median is not None else "none framed"
      print(f"  {counts['hidden']} of {counts['total']} {word} under the deck or off screen, {framed}, "
            f"{run['never'][reason]} never framed")


def Main(_arguments):
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument("folder", type=Path)
  parser.add_argument("--json", action="store_true", help="print the runs as JSON")
  options = parser.parse_args(_arguments)
  logs = sorted(options.folder.glob("Siege-*.csv"))
  try:
    runs = [ReadLog(path) for path in logs]
  except LogError as error:
    print(f"ReadSiegeLog: {error}", file=sys.stderr)
    return 1
  if options.json:
    print(json.dumps(runs, indent=2, sort_keys=True))
  else:
    Print(runs)
  return 0


if __name__ == "__main__":
  sys.exit(Main(sys.argv[1:]))
