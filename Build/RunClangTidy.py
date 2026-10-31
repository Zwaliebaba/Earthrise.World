"""Runs clang-tidy over every translation unit of the solution (AGENTS.md §1, ADR-010).

Each command is built from the project files and Build/Earthrise.props: the project's include
directories and defines, the sheet's defines for the configuration, Unicode, and the architecture's
/arch. Nothing is typed by hand, so the lint sees what the compiler sees. The Windows macros are
never passed: Engine/framework.h owns them.

It needs a Developer PowerShell, whose INCLUDE variable makes the Windows SDK and the MSVC headers
visible to clang, and it runs after a build, because Content's generated headers live in the
build's intermediate folder. CI pins the version in CLANG_TIDY_VERSION (.github/workflows/build.yml);
when that variable is set, a different clang-tidy fails the run.

Usage:
    python Build/RunClangTidy.py                    lint Debug|x64, in parallel
    python Build/RunClangTidy.py --print-commands   show the commands and stop
"""

import argparse
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import ProjectModel as Model


def Commands(_root, _clangTidy, _configuration, _platform):
  solutions = sorted(_root.glob("*.slnx"))
  if not solutions:
    return None
  sheet = Model.EvaluateSheet(Model.ParseXml(_root / Model.PROPERTY_SHEET), _configuration, _platform)
  sheetDefines = [define for define in sheet.get(("ClCompile", "PreprocessorDefinitions"), []) if not define.startswith("%(")]
  if sheet.get(("Property", "CharacterSet")) == "Unicode":
    sheetDefines += ["UNICODE", "_UNICODE"]
  sheetOptions = [option for option in sheet.get(("ClCompile", "AdditionalOptions"), []) if not option.startswith("%(")]
  commands = []
  for relative in Model.SolutionProjects(solutions[0]):
    project = Model.LoadProject(_root / relative)
    macros = {
      "SolutionDir": str(_root) + os.sep,
      "ProjectDir": str(project.folder) + os.sep,
      "IntDir": str(_root / _platform / _configuration / "Intermediate" / project.name) + os.sep,
      "VCInstallDir": os.environ.get("VCINSTALLDIR", ""),
      "Platform": _platform,
      "Configuration": _configuration,
    }
    settings = Model.ProjectSettings(project)
    includes = [str(project.folder)]
    for directory in settings.get(("ClCompile", "AdditionalIncludeDirectories"), []):
      if not directory.startswith("%("):
        includes.append(Model.ExpandMacros(directory, macros))
    defines = sheetDefines + [define for define in settings.get(("ClCompile", "PreprocessorDefinitions"), []) if not define.startswith("%(")]
    for item in project.ItemPaths({"ClCompile"}):
      source = project.folder / item.include.replace("\\", "/")
      command = [_clangTidy, "--quiet", str(source), "--", "--driver-mode=cl", "/std:c++latest", "/EHsc", "/permissive-"]
      command += sheetOptions
      command += [f"/D{define}" for define in defines]
      command += [f"/I{include}" for include in includes]
      commands.append((source, command))
  return commands


def Main(_arguments):
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
  parser.add_argument("--clang-tidy", default="clang-tidy")
  parser.add_argument("--configuration", default="Debug", choices=Model.CONFIGURATIONS)
  parser.add_argument("--platform", default="x64", choices=Model.PLATFORMS)
  parser.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
  parser.add_argument("--print-commands", action="store_true")
  arguments = parser.parse_args(_arguments)
  root = arguments.root.resolve()

  commands = Commands(root, arguments.clang_tidy, arguments.configuration, arguments.platform)
  if commands is None:
    print("RunClangTidy: no solution yet, so there is nothing to lint")
    return 0
  if arguments.print_commands:
    for _, command in commands:
      print(" ".join(command))
    return 0
  if not commands:
    print("RunClangTidy: the solution has no translation unit yet")
    return 0
  if "INCLUDE" not in os.environ:
    print("RunClangTidy: INCLUDE is not set; run from a Developer PowerShell (AGENTS.md §3)")
    return 2

  version = subprocess.run([arguments.clang_tidy, "--version"], capture_output=True, text=True, check=False).stdout
  expected = os.environ.get("CLANG_TIDY_VERSION")
  found = re.search(r"version (\d+\.\d+\.\d+)", version)
  if expected and (not found or found.group(1) != expected):
    print(f"RunClangTidy: clang-tidy {found.group(1) if found else '?'} is not the pinned {expected}")
    return 2

  def Run(_entry):
    source, command = _entry
    result = subprocess.run(command, capture_output=True, text=True, cwd=root, check=False)
    return source, result

  failures = 0
  with ThreadPoolExecutor(max_workers=arguments.jobs) as pool:
    for source, result in pool.map(Run, commands):
      if result.returncode != 0:
        failures += 1
        print(f"── {source.relative_to(root).as_posix()}")
        print((result.stdout + result.stderr).strip())
  if failures:
    print(f"RunClangTidy: {failures} of {len(commands)} translation unit(s) have findings")
    return 1
  print(f"RunClangTidy: OK, {len(commands)} translation unit(s)")
  return 0


if __name__ == "__main__":
  sys.exit(Main(sys.argv[1:]))
