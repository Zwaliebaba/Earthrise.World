"""Checks that every C++ file is laid out as .clang-format says (AGENTS.md §4).

clang-format is pinned to major version 18, the one CI installs: 18 and later versions break a long
argument list in different places, so a floating version would make the check flap. Get it with
`python -m pip install clang-format==18.1.8`, or `apt install clang-format-18`.

Usage:
    python Build/CheckFormat.py                     report files that are not formatted
    python Build/CheckFormat.py --fix               rewrite them
    python Build/CheckFormat.py --clang-format PATH use this binary
"""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

import ProjectModel as Model

PINNED_MAJOR = 18
EXTENSIONS = {".h", ".cpp"}
SKIPPED_ANYWHERE = {"CompiledShader", "Intermediate"}


def CppFiles(_root):
  for path in sorted(_root.rglob("*")):
    if path.suffix not in EXTENSIONS or not path.is_file():
      continue
    relative = path.relative_to(_root)
    if relative.parts[0] in Model.SKIPPED_FOLDERS or SKIPPED_ANYWHERE & set(relative.parts):
      continue
    yield path


def Version(_binary):
  output = subprocess.run([_binary, "--version"], capture_output=True, text=True, check=True).stdout
  match = re.search(r"version (\d+)\.", output)
  return int(match.group(1)) if match else None, output.strip()


def Main(_arguments):
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
  parser.add_argument("--clang-format", default=shutil.which("clang-format-18") or "clang-format")
  parser.add_argument("--fix", action="store_true", help="rewrite files instead of reporting them")
  parser.add_argument("--any-version", action="store_true", help="do not insist on the pinned major version")
  arguments = parser.parse_args(_arguments)
  root = arguments.root.resolve()
  style = root / ".clang-format"
  if not style.exists():
    print(f"CheckFormat: {style} is missing")
    return 2
  try:
    major, description = Version(arguments.clang_format)
  except (OSError, subprocess.CalledProcessError):
    print(f"CheckFormat: cannot run {arguments.clang_format}; install clang-format {PINNED_MAJOR} (see this script's docstring)")
    return 2
  if major != PINNED_MAJOR and not arguments.any_version:
    print(f"CheckFormat: {description} is not version {PINNED_MAJOR}, which CI pins; install it (see this script's docstring)")
    return 2

  offenders = []
  files = list(CppFiles(root))
  for path in files:
    original = path.read_bytes()
    result = subprocess.run([arguments.clang_format, f"--style=file:{style}", f"--assume-filename={path}"], input=original,
                            capture_output=True, check=False)
    if result.returncode != 0:
      print(f"{path.relative_to(root).as_posix()}: clang-format failed: {result.stderr.decode(errors='replace').strip()}")
      return 2
    if result.stdout != original:
      if arguments.fix:
        path.write_bytes(result.stdout)
        print(f"fixed {path.relative_to(root).as_posix()}")
      else:
        offenders.append(path)
  for path in offenders:
    print(f"{path.relative_to(root).as_posix()}: not formatted; run python Build/CheckFormat.py --fix")
  if offenders:
    print(f"CheckFormat: {len(offenders)} of {len(files)} file(s) not formatted")
    return 1
  print(f"CheckFormat: OK, {len(files)} file(s)")
  return 0


if __name__ == "__main__":
  sys.exit(Main(sys.argv[1:]))
