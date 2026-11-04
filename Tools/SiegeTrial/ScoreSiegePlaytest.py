"""Scores the Siege playtest, M5's verdict on H2 (M5-B2; MvpPlan-M5.md, *The session* and *The session's folder*;
GameDesign.md, *Hypotheses*, H2, and *M5's values*).

Reads a session's folder: `Session.csv`, the facilitator's sheet; `Siege/`, two trial logs a participant from
`Earthrise --siege <id> --game <1|2>`; and `BreachLab/`, one lab log a participant from `Earthrise --lab <id>`. It
checks the folder, and prints each participant's games and H2's three clauses over the eight RTS players:
- clustering (item 9): a game clusters when every one of its main points, the structures that took a quarter or more of
  its siege work, is the nearest section or the gatehouse, or when it did no siege work; the clause fails when five or
  more of the eight voxel games cluster;
- prediction (item 11): the median of the eight's lab accuracies, against 0.65, as ScoreBreachLab.py scores it;
- preference (item 10): how many of the eight preferred the game they played in the voxel version, against 7.
H2 passes when all three pass (item 17). Reported beside them, under no mark: the control games' clustering, the voxel
games' breach points on each layout, each version's wins, the preferences that went to the game the participant won,
the deck's figures (item 13), and the novices', on their own.

It refuses, naming the cause, and scores nothing when:
- the sheet does not read, or names an id its group or cell disagrees with;
- a participant not dropped lacks a game 1 log, a game 2 log or a lab log, or a log names a participant the sheet does not;
- a trial log's version, layout or game disagrees with its id's cell;
- a log's build id is not among the sheet's, or its server-data hash is not the sheet's;
- ScoreBreachLab.py refuses the lab's folder;
- the RTS group, those not dropped, is not eight.

With --pilot it reads the pilot's folder (item 14): pilot ids only, any number of them, and no verdict.

Usage:
  python3 Tools/SiegeTrial/ScoreSiegePlaytest.py <folder> [--pilot] [--json]

Exit status 0 when the folder passes its checks, whether H2 passes or not, and 1 when a check fails. Standard library only.
"""

import argparse
import csv
import json
import re
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "BreachLab"))

import ReadSiegeLog  # noqa: E402
import ScoreBreachLab  # noqa: E402

SHEET = "Session.csv"
SHEET_COLUMNS = ("participant", "group", "cell", "preferred", "dropped")
GROUPS = {"S": "rts", "N": "novice", "X": "pilot"}
ID_FORM = re.compile(r"^([SNX])(\d\d)$")
RTS_PLAYERS = 8  # item 3
CELL_COUNT = 4
# Each cell's two games, as (version, layout): version order crossed with layout (item 4; MvpPlan-M5.md, *The session*).
CELLS = {
  0: (("voxel", "Siege"), ("control", "SiegeMirror")),
  1: (("control", "Siege"), ("voxel", "SiegeMirror")),
  2: (("voxel", "SiegeMirror"), ("control", "Siege")),
  3: (("control", "SiegeMirror"), ("voxel", "Siege")),
}
CLUSTERING_FAILS_AT = 5  # of the eight voxel games (item 9)
PREFERENCE_MARK = 7  # of eight (item 10)
PREDICTION_MARK = ScoreBreachLab.PASS_MARK  # item 11
DECK_LOST_TICKS = 135  # a collapse's warning for an 8 m piece, 6.75 s (item 13; M2's input 4)
TICKS_PER_SECOND = 20
DECK_SLIM_SHARE_QUARTERS = 4  # more than a quarter lost slims the deck (item 13)


class ScoreError(Exception):
  """A folder that fails a check: the message names the file and the cause."""


def CellOf(_participant):
  """_participant's cell, (number - 1) mod 4, and their group, from the id's form. Raises ScoreError for another form."""
  match = ID_FORM.match(_participant)
  if match is None or int(match.group(2)) == 0:
    raise ScoreError(f"{_participant} is not an id of the form S, N or X and two digits from 01")
  return (int(match.group(2)) - 1) % CELL_COUNT, GROUPS[match.group(1)]


def ReadSheet(_folder):
  """The sheet's comment lines and its rows, keyed by participant."""
  path = _folder / SHEET
  if not path.is_file():
    raise ScoreError(f"{_folder}: no {SHEET}")
  lines = path.read_text(encoding="utf-8").splitlines()
  comments = ReadSiegeLog.ReadComments(lines)
  for key in ("build ids", "server-data hash"):
    if key not in comments:
      raise ScoreError(f"{SHEET}: no {key} line")
  body = [line for line in lines if line and not line.startswith("#")]
  reader = csv.reader(body)
  if tuple(next(reader, ())) != SHEET_COLUMNS:
    raise ScoreError(f"{SHEET}: its header is not {','.join(SHEET_COLUMNS)}")
  rows = {}
  for row in reader:
    if len(row) != len(SHEET_COLUMNS):
      raise ScoreError(f"{SHEET}: a row of {len(row)} columns, not {len(SHEET_COLUMNS)}")
    entry = dict(zip(SHEET_COLUMNS, row))
    cell, group = CellOf(entry["participant"])
    if entry["group"] != group or entry["cell"] != str(cell):
      raise ScoreError(f"{SHEET}: {entry['participant']} is group {group} in cell {cell}, not {entry['group']} in {entry['cell']}")
    if not entry["dropped"] and entry["preferred"] not in ("first", "second"):
      raise ScoreError(f"{SHEET}: {entry['participant']} preferred neither first nor second")
    if entry["participant"] in rows:
      raise ScoreError(f"{SHEET}: {entry['participant']} twice")
    rows[entry["participant"]] = entry
  return {"buildIds": set(comments["build ids"].split()), "serverDataHash": comments["server-data hash"], "rows": rows}


def CheckBuild(_name, _comments, _sheet):
  if _comments.get("build id") not in _sheet["buildIds"]:
    raise ScoreError(f"{_name}: build id {_comments.get('build id')} is not among the sheet's")
  if _comments.get("server-data hash") != _sheet["serverDataHash"]:
    raise ScoreError(f"{_name}: server-data hash {_comments.get('server-data hash')} is not the sheet's")


def DeckWaits(_path):
  """Each opening and warning the log records on screen: where it began, and the ticks until it was framed, or None."""
  framed = {}
  places = []
  for row in csv.reader(line for line in _path.read_text(encoding="utf-8").splitlines() if line and not line.startswith("#")):
    if row and row[0] == "onscreen":
      places.append((row[1], row[2], row[3], row[4]))
    elif row and row[0] == "framed":
      framed[(row[1], row[2], row[3])] = None if row[4] == "never" else round(float(row[4]) * TICKS_PER_SECOND)
  return [{"place": place, "framedTicks": framed.get((tick, structure, reason))} for tick, structure, reason, place in places]


def ReadGames(_folder, _sheet):
  """Each participant's two games, read and checked, keyed by participant, each a list of game 1 and game 2."""
  games = {}
  for path in sorted((_folder / "Siege").glob("Siege-*.csv")):
    comments = ReadSiegeLog.ReadComments(path.read_text(encoding="utf-8").splitlines())
    participant = comments.get("participant")
    if participant not in _sheet["rows"]:
      raise ScoreError(f"{path.name}: {participant} is not on the sheet")
    if _sheet["rows"][participant]["dropped"]:
      raise ScoreError(f"{path.name}: {participant} is dropped, so their logs belong in Dropped/")
    CheckBuild(path.name, comments, _sheet)
    try:
      run = ReadSiegeLog.ReadLog(path)
    except ReadSiegeLog.LogError as error:
      raise ScoreError(str(error)) from error
    game = comments.get("game")
    if game not in ("1", "2"):
      raise ScoreError(f"{path.name}: no game line, so it was not run with --game")
    cell, _ = CellOf(participant)
    version, layout = CELLS[cell][int(game) - 1]
    if (run["version"], run["scenario"], comments.get("cell")) != (version, layout, str(cell)):
      raise ScoreError(f"{path.name}: game {game} of cell {cell} is the {version} version on {layout}, not the "
                       f"{run['version']} version on {run['scenario']}")
    played = games.setdefault(participant, [None, None])
    if played[int(game) - 1] is not None:
      raise ScoreError(f"{path.name}: {participant}'s game {game} twice")
    run["game"] = int(game)
    run["waits"] = DeckWaits(path)
    played[int(game) - 1] = run
  return games


def ReadLab(_folder, _sheet):
  """Each participant's lab accuracy, keyed by participant, after ScoreBreachLab.py's checks."""
  lab = _folder / "BreachLab"
  for path in sorted(lab.glob("*.csv")):
    comments = ReadSiegeLog.ReadComments(path.read_text(encoding="utf-8").splitlines())
    if comments.get("participant") not in _sheet["rows"]:
      raise ScoreError(f"{path.name}: {comments.get('participant')} is not on the sheet")
    CheckBuild(path.name, comments, _sheet)
  try:
    scores = ScoreBreachLab.Score(lab)
  except ScoreBreachLab.LogError as error:
    raise ScoreError(f"the lab: {error}") from error
  accuracies = {}
  for entry in scores["participants"]:
    if entry["participant"] in accuracies:
      raise ScoreError(f"{entry['log']}: {entry['participant']}'s second lab log")
    accuracies[entry["participant"]] = entry["accuracy"]
  return accuracies


def Summary(_participant, _entry, _games, _lab):
  """One participant's record: their games, the version they preferred, and their lab accuracy."""
  first, second = _games
  voxel = first if first["version"] == "voxel" else second
  control = second if voxel is first else first
  preferred = first if _entry["preferred"] == "first" else second
  return {
    "participant": _participant,
    "group": _entry["group"],
    "cell": int(_entry["cell"]),
    "games": [{
      "game": run["game"],
      "version": run["version"],
      "layout": run["scenario"],
      "outcome": run["outcome"],
      "seconds": run["seconds"],
      "mainPoints": run["mainPoints"],
      "breachPoint": run["breachPoint"],
      "clusters": run["clusters"],
      "opened": [breach["structure"] for breach in run["breaches"]],
    } for run in _games],
    "voxelClusters": voxel["clusters"],
    "controlClusters": control["clusters"],
    "voxelLayout": voxel["scenario"],
    "voxelBreachRoles": sorted({point["role"] for point in voxel["mainPoints"] if point["structure"] in voxel["breachPoint"]}),
    "preferredVersion": preferred["version"],
    "wonVersions": [run["version"] for run in _games if run["outcome"] == "won"],
    "labAccuracy": _lab[_participant],
  }


def Deck(_games):
  """Item 13's figures over every game: the openings and warnings that began under the deck or off screen, how many went
  unframed for longer than a collapse's warning, and whether that is more than a quarter."""
  hidden = [wait for games in _games for run in games for wait in run["waits"] if wait["place"] in ReadSiegeLog.HIDDEN]
  lost = [wait for wait in hidden if wait["framedTicks"] is None or wait["framedTicks"] > DECK_LOST_TICKS]
  return {"hidden": len(hidden), "lost": len(lost), "slims": len(lost) * DECK_SLIM_SHARE_QUARTERS > len(hidden)}


def Clauses(_records):
  """H2's three clauses over _records, the RTS group's, each with its figure and its verdict."""
  clustering = sum(1 for record in _records if record["voxelClusters"])
  prediction = statistics.median(record["labAccuracy"] for record in _records)
  preference = sum(1 for record in _records if record["preferredVersion"] == "voxel")
  clauses = {
    "clustering": {"clustered": clustering, "of": len(_records), "failsAt": CLUSTERING_FAILS_AT,
                   "verdict": "FAIL" if clustering >= CLUSTERING_FAILS_AT else "PASS"},
    "prediction": {"median": prediction, "mark": PREDICTION_MARK, "verdict": "PASS" if prediction >= PREDICTION_MARK else "FAIL"},
    "preference": {"preferredVoxel": preference, "of": len(_records), "mark": PREFERENCE_MARK,
                   "verdict": "PASS" if preference >= PREFERENCE_MARK else "FAIL"},
  }
  clauses["h2"] = "PASS" if all(clause["verdict"] == "PASS" for clause in clauses.values()) else "FAIL"
  return clauses


def Unmarked(_records):
  """What is reported beside the clauses, under no mark, for _records."""
  layouts = {}
  for record in _records:
    roles = layouts.setdefault(record["voxelLayout"], {})
    for role in record["voxelBreachRoles"] or ["none"]:
      roles[role] = roles.get(role, 0) + 1
  return {
    "controlClustered": sum(1 for record in _records if record["controlClusters"]),
    "voxelBreachRolesByLayout": layouts,
    "wins": {version: sum(1 for record in _records if version in record["wonVersions"]) for version in ("voxel", "control")},
    "preferredTheVersionWon": sum(1 for record in _records
                                  if record["wonVersions"] == [record["preferredVersion"]]),
  }


def Score(_folder, _pilot=False):
  """The session's scores, as a dict. Raises ScoreError when the folder fails a check."""
  folder = Path(_folder)
  sheet = ReadSheet(folder)
  games = ReadGames(folder, sheet)
  lab = ReadLab(folder, sheet)
  records = []
  for participant, entry in sheet["rows"].items():
    if entry["dropped"]:
      continue
    if _pilot != (entry["group"] == "pilot"):
      raise ScoreError(f"{SHEET}: {participant} is {entry['group']}, " + ("not a pilot" if _pilot else "a pilot, whose folder is Pilot/"))
    played = games.get(participant, [None, None])
    for game, run in enumerate(played, start=1):
      if run is None:
        raise ScoreError(f"{participant} has no game {game} log")
    if participant not in lab:
      raise ScoreError(f"{participant} has no lab log")
    records.append(Summary(participant, entry, played, lab))
  rts = [record for record in records if record["group"] == "rts"]
  novices = [record for record in records if record["group"] == "novice"]
  scores = {"participants": records, "deck": Deck(games.values()), "dropped": sorted(
    participant for participant, entry in sheet["rows"].items() if entry["dropped"])}
  if _pilot:
    scores["pilot"] = Unmarked(records) if records else {}
    return scores
  if len(rts) != RTS_PLAYERS:
    raise ScoreError(f"{SHEET}: {len(rts)} RTS players not dropped, not {RTS_PLAYERS}")
  scores["clauses"] = Clauses(rts)
  scores["unmarked"] = Unmarked(rts)
  if novices:
    scores["novices"] = {"clustered": sum(1 for record in novices if record["voxelClusters"]),
                         "labMedian": statistics.median(record["labAccuracy"] for record in novices),
                         "preferredVoxel": sum(1 for record in novices if record["preferredVersion"] == "voxel"),
                         "of": len(novices)}
  return scores


def Print(_scores):
  for record in _scores["participants"]:
    print(f"{record['participant']} ({record['group']}, cell {record['cell']}): preferred the {record['preferredVersion']} "
          f"version; lab {record['labAccuracy']:.4f}")
    for game in record["games"]:
      points = ", ".join(f"{point['structure']} ({point['role']})" for point in game["mainPoints"]) or "none"
      ended = f"{game['outcome']} at {ReadSiegeLog.Clock(game['seconds'])}" if game["outcome"] else "no outcome"
      print(f"  game {game['game']}: {game['version']} on {game['layout']}, {ended}; main points {points}; "
            f"{'clusters' if game['clusters'] else 'does not cluster'}")
  deck = _scores["deck"]
  print(f"deck: {deck['lost']} of {deck['hidden']} openings and warnings that began under it or off screen went unframed "
        f"past {DECK_LOST_TICKS / TICKS_PER_SECOND:.2f} s; " + ("slim it" if deck["slims"] else "the owner's call"))
  if "clauses" not in _scores:
    return
  clauses = _scores["clauses"]
  clustering, prediction, preference = clauses["clustering"], clauses["prediction"], clauses["preference"]
  print(f"clustering: {clustering['clustered']} of {clustering['of']} voxel games cluster, failing at "
        f"{clustering['failsAt']}: {clustering['verdict']}")
  print(f"prediction: median {prediction['median']:.4f} against {prediction['mark']}: {prediction['verdict']}")
  print(f"preference: {preference['preferredVoxel']} of {preference['of']} preferred the voxel version, against "
        f"{preference['mark']}: {preference['verdict']}")
  print(f"H2: {clauses['h2']}")
  unmarked = _scores["unmarked"]
  print(f"under no mark: {unmarked['controlClustered']} control games cluster; voxel wins {unmarked['wins']['voxel']}, "
        f"control wins {unmarked['wins']['control']}; {unmarked['preferredTheVersionWon']} preferred the one version they won")
  for layout, roles in sorted(unmarked["voxelBreachRolesByLayout"].items()):
    print(f"  voxel breach points on {layout}: " + ", ".join(f"{role} {count}" for role, count in sorted(roles.items())))
  if "novices" in _scores:
    novices = _scores["novices"]
    print(f"novices, apart: {novices['clustered']} of {novices['of']} voxel games cluster; lab median "
          f"{novices['labMedian']:.4f}; {novices['preferredVoxel']} preferred the voxel version")


def Main(_arguments):
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument("folder", type=Path)
  parser.add_argument("--pilot", action="store_true", help="read the pilot's folder: pilot ids only, and no verdict")
  parser.add_argument("--json", action="store_true", help="print the scores as JSON")
  options = parser.parse_args(_arguments)
  try:
    scores = Score(options.folder, options.pilot)
  except ScoreError as error:
    print(f"ScoreSiegePlaytest: {error}", file=sys.stderr)
    return 1
  if options.json:
    print(json.dumps(scores, indent=2, sort_keys=True))
  else:
    Print(scores)
  return 0


if __name__ == "__main__":
  sys.exit(Main(sys.argv[1:]))
