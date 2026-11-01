"""Tests for the checkers: a checker that passes an empty tree proves nothing (ADR-010, decision 9).

Each case copies Build/Fixtures/Good, with the real Build/Earthrise.props beside it, into a temporary
folder, breaks one thing, and asserts the checker fails for that reason. The good fixture itself
must pass, and so must the repository.

RunClangTidyTests runs the real clang-tidy, with the repository's .clang-tidy, over the fixture. It
needs clang-tidy and a Developer PowerShell's INCLUDE, so it runs in CI's Windows job, where
REQUIRE_CLANG_TIDY turns a missing tool into a failure instead of a skip (AGENTS.md §6).

Usage:
    python Build/TestCheckers.py                       every case
    python Build/TestCheckers.py -v                    with each case's name
    python Build/TestCheckers.py RunClangTidyTests -v  only clang-tidy's
"""

import contextlib
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

BUILD = Path(__file__).resolve().parent
ROOT = BUILD.parent
sys.path.insert(0, str(BUILD))

import CheckFormat  # noqa: E402
import CheckProjectFiles  # noqa: E402
import RunClangTidy  # noqa: E402

GOOD = BUILD / "Fixtures" / "Good"


def Edit(_root, _relative, _old, _new):
  path = _root / _relative
  text = path.read_text(encoding="utf-8")
  assert _old in text, f"fixture text not found in {_relative}: {_old!r}"
  path.write_text(text.replace(_old, _new, 1), encoding="utf-8")


def Write(_root, _relative, _text):
  path = _root / _relative
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_text(_text, encoding="utf-8")


def Rename(_root, _old, _new):
  """Renames a project everywhere: folders, files and every mention in the fixture's text."""
  for path in sorted(_root.rglob("*"), key=lambda p: -len(p.parts)):
    if path.is_file():
      path.write_text(path.read_text(encoding="utf-8").replace(_old, _new), encoding="utf-8")
    if _old in path.name:
      path.rename(path.with_name(path.name.replace(_old, _new)))


def AddToProject(_root, _include, _kind="ClInclude", _filter="Rendering"):
  Edit(_root, "Sample/Sample.vcxproj", '<ClInclude Include="pch.h" />', f'<ClInclude Include="pch.h" />\n    <{_kind} Include="{_include}" />')
  Edit(_root, "Sample/Sample.vcxproj.filters", "  </ItemGroup>\n</Project>",
       f'    <{_kind} Include="{_include}">\n      <Filter>{_filter}</Filter>\n    </{_kind}>\n  </ItemGroup>\n</Project>')


SHEET = "Build/Earthrise.props"
PROJECT = "Sample/Sample.vcxproj"
FILTERS = "Sample/Sample.vcxproj.filters"
HEADER = "Sample/VoxelRecord.h"
SOURCE = "Sample/VoxelRecord.cpp"
SETTING = "  <ItemGroup>\n    <ClInclude Include=\"pch.h\" />"


def Setting(_tool, _name, _value):
  return f"  <ItemDefinitionGroup>\n    <{_tool}>\n      <{_name}>{_value}</{_name}>\n    </{_tool}>\n  </ItemDefinitionGroup>\n" + SETTING


BROKEN = {
  # The project file.
  "the sheet is not imported": (lambda r: Edit(r, PROJECT, '<Import Project="$(MSBuildThisFileDirectory)..\\Build\\Earthrise.props" />', ""),
                                "imports must be exactly"),
  "a mandated setting is overridden": (lambda r: Edit(r, PROJECT, SETTING, Setting("ClCompile", "WarningLevel", "Level3")),
                                       "WarningLevel is set in the project"),
  "a project sets its own optimisation": (lambda r: Edit(r, PROJECT, SETTING, Setting("ClCompile", "Optimization", "Disabled")),
                                          "Optimization is set in the project"),
  "a property outside the sheet": (lambda r: Edit(r, PROJECT, SETTING, "  <PropertyGroup>\n    <OutDir>bin\\</OutDir>\n  </PropertyGroup>\n" + SETTING),
                                   "property OutDir belongs in the sheet"),
  "a condition in a project": (lambda r: Edit(r, PROJECT, '<PropertyGroup Label="Configuration">',
                                             '<PropertyGroup Label="Configuration" Condition="\'$(Configuration)\'==\'Debug\'">'),
                               "a Condition on"),
  "a Win32 configuration": (lambda r: Edit(r, PROJECT, '<ItemGroup Label="ProjectConfigurations">',
                                           '<ItemGroup Label="ProjectConfigurations">\n    <ProjectConfiguration Include="Debug|Win32" />'),
                            "configurations must be exactly"),
  "a project on its own include path": (lambda r: Edit(r, PROJECT, SETTING, Setting("ClCompile", "AdditionalIncludeDirectories", "$(SolutionDir)Sample")),
                                        "own folder on the include path"),
  "a Windows macro defined by a project": (lambda r: Edit(r, PROJECT, SETTING, Setting("ClCompile", "PreprocessorDefinitions", "NOMINMAX;%(PreprocessorDefinitions)")),
                                           "NOMINMAX is defined by the project"),
  "a library built as a DLL": (lambda r: Edit(r, PROJECT, "<ConfigurationType>StaticLibrary", "<ConfigurationType>DynamicLibrary"),
                               "only test suites, are DLLs"),
  "pch.cpp does not create the header": (lambda r: Edit(r, PROJECT, "\n      <PrecompiledHeader>Create</PrecompiledHeader>\n", "\n"),
                                         "only pch.cpp creates the precompiled header"),
  "a bake outside Content": (lambda r: Edit(r, PROJECT, SETTING, "  <ItemGroup>\n    <BakeInput Include=\"$(SolutionDir)GameData\\Server\\Palette.json\" />\n  </ItemGroup>\n" + SETTING),
                             "only Content bakes"),
  # The sheet.
  "a wrong mandated value": (lambda r: Edit(r, SHEET, "<FloatingPointModel>Precise", "<FloatingPointModel>Fast"),
                             "FloatingPointModel is 'Fast'"),
  "Release drifts from Debug": (lambda r: Edit(r, SHEET, "<Optimization>MaxSpeed</Optimization>",
                                               "<Optimization>MaxSpeed</Optimization>\n      <LanguageStandard>stdcpp20</LanguageStandard>"),
                                "LanguageStandard differs between Debug and Release"),
  "Debug shaders lose their embedded debug information": (
    lambda r: Edit(r, SHEET, "      <AdditionalOptions>-Qembed_debug %(AdditionalOptions)</AdditionalOptions>\n", ""),
    "-Qembed_debug belongs in Debug's shader options"),
  "Release shaders embed debug information": (
    lambda r: Edit(r, SHEET, "<EnableDebuggingInformation>false</EnableDebuggingInformation>",
                   "<EnableDebuggingInformation>false</EnableDebuggingInformation>\n      <AdditionalOptions>-Qembed_debug</AdditionalOptions>"),
    "-Qembed_debug belongs in Debug's shader options"),
  "Debug shader options drift": (
    lambda r: Edit(r, SHEET, "-Qembed_debug %(AdditionalOptions)", "-Qembed_debug -Od %(AdditionalOptions)"),
    "shader options differ beyond -Qembed_debug"),
  "ARM64 drifts from x64": (lambda r: Edit(r, SHEET, "<AdditionalOptions>/arch:armv8.0+lse %(AdditionalOptions)",
                                           "<AdditionalOptions>/arch:armv8.0+lse /Gy %(AdditionalOptions)"),
                            "differ beyond /arch"),
  "FMA contraction switched on": (lambda r: Edit(r, SHEET, "<AdditionalOptions>/arch:AVX2", "<AdditionalOptions>/arch:AVX2 /fp:contract"),
                                  "/fp:contract in AdditionalOptions"),
  "the wrong ARM64 floor": (lambda r: Edit(r, SHEET, "/arch:armv8.0+lse", "/arch:armv8.2"), "/arch must be exactly /arch:armv8.0+lse"),
  "a condition the checker cannot read": (lambda r: Edit(r, SHEET, "Condition=\"'$(Configuration)'=='Release'\">\n    <LinkIncremental>",
                                                         "Condition=\"'$(UseDebugLibraries)'=='false'\">\n    <LinkIncremental>"),
                                          "may condition only on"),
  # Files and filters.
  "a file not in the project": (lambda r: Write(r, "Sample/Extra.cpp", '#include "pch.h"\n'), "not registered in Sample.vcxproj"),
  "a file in the project but not the filters": (lambda r: (Write(r, "Sample/Extra.h", "#pragma once\n"),
                                                           Edit(r, PROJECT, '<ClInclude Include="pch.h" />', '<ClInclude Include="pch.h" />\n    <ClInclude Include="Extra.h" />')),
                                                "in the project but not in the filters"),
  "a registered file that does not exist": (lambda r: AddToProject(r, "Gone.h"), "registered but does not exist"),
  "a file-kind filter": (lambda r: (Edit(r, FILTERS, '<Filter Include="Rendering">', '<Filter Include="Source Files">'),
                                    Edit(r, FILTERS, "<Filter>Rendering</Filter>\n    </ClInclude>", "<Filter>Source Files</Filter>\n    </ClInclude>"),
                                    Edit(r, FILTERS, "<Filter>Rendering</Filter>\n    </ClCompile>", "<Filter>Source Files</Filter>\n    </ClCompile>")),
                         "filters are functional"),
  "a header apart from its .cpp": (lambda r: Edit(r, FILTERS, "<Filter>Rendering</Filter>\n    </ClCompile>", "<Filter>Build</Filter>\n    </ClCompile>"),
                                   "sit in different filters"),
  "a header in a subfolder": (lambda r: Write(r, "Sample/src/Thing.h", "#pragma once\n"), "project folders are flat"),
  "a .hpp file": (lambda r: Write(r, "Sample/Thing.hpp", "#pragma once\n"), "(R7)"),
  "a file not in PascalCase": (lambda r: (Write(r, "Sample/thing_util.h", "#pragma once\n"), AddToProject(r, "thing_util.h")), "PascalCase (R7)"),
  # Shaders.
  "a shader not named for its stage": (lambda r: ((r / "Sample/Shader/VoxelVS.hlsl").rename(r / "Sample/Shader/Voxel.hlsl"),
                                                  Edit(r, PROJECT, "Shader\\VoxelVS.hlsl", "Shader\\Voxel.hlsl"),
                                                  Edit(r, FILTERS, "Shader\\VoxelVS.hlsl", "Shader\\Voxel.hlsl")),
                                       "named <Shader><Stage>.hlsl"),
  "shared HLSL compiled on its own": (lambda r: (Edit(r, PROJECT, '<None Include="Shader\\RayBox.hlsli" />', '<FXCompile Include="Shader\\RayBox.hlsli" />'),
                                                 Edit(r, FILTERS, '<None Include="Shader\\RayBox.hlsli">\n      <Filter>Shader</Filter>\n    </None>',
                                                      '<FXCompile Include="Shader\\RayBox.hlsli">\n      <Filter>Shader</Filter>\n    </FXCompile>')),
                                      "must be None"),
  "a shader of the wrong type": (lambda r: Edit(r, PROJECT, "<ShaderType>Vertex</ShaderType>", "<ShaderType>Pixel</ShaderType>"),
                                 "ShaderType must be Vertex"),
  # Sources.
  "an I-prefixed interface": (lambda r: Edit(r, HEADER, "struct VoxelRecord", "class IRenderer\n{\n};\n\nstruct VoxelRecord"), "IRenderer carries a prefix"),
  "a Base-suffixed class": (lambda r: Edit(r, HEADER, "struct VoxelRecord", "class RendererBase\n{\n};\n\nstruct VoxelRecord"), "RendererBase carries a prefix"),
  "the other spelling in an identifier": (lambda r: Edit(r, HEADER, "std::uint32_t paletteIndex;", "std::uint32_t colourIndex;"), "(R11)"),
  "a Windows macro defined in source": (lambda r: Edit(r, SOURCE, '#include "pch.h"', '#define NOMINMAX\n#include "pch.h"'), "only Engine/framework.h does"),
  "Windows in a Windows-free project": (lambda r: (Rename(r, "Sample", "Core"),
                                                    Edit(r, "Core/VoxelRecord.cpp", '#include "pch.h"', '#include "pch.h"\n\n#include <windows.h>')),
                                        "Windows and Direct3D stay in Engine"),
  # The solution and the registry.
  "a second solution": (lambda r: Write(r, "Other.sln", ""), "more than one solution"),
  "a project the solution does not name": (lambda r: shutil.copytree(r / "Sample", r / "Stray") and (r / "Stray/Sample.vcxproj").rename(r / "Stray/Stray.vcxproj"),
                                           "a project the solution does not name"),
  "source outside any project": (lambda r: Write(r, "Loose.cpp", "int x;\n"), "source outside any project"),
  "a project missing from the registry": (lambda r: Edit(r, ".clang-tidy", "(Sample|", "("), "does not name Sample"),
  "an unanchored registry": (lambda r: Edit(r, ".clang-tidy", "'(^|[/\\\\])(", "'("), "anchored"),
}


def Copy(_destination):
  shutil.copytree(GOOD, _destination, dirs_exist_ok=True)
  (_destination / "Build").mkdir(exist_ok=True)
  shutil.copy(BUILD / "Earthrise.props", _destination / "Build" / "Earthrise.props")


class CheckProjectFilesTests(unittest.TestCase):
  def test_the_good_fixture_passes(self):
    with tempfile.TemporaryDirectory() as folder:
      root = Path(folder)
      Copy(root)
      report, projects = CheckProjectFiles.Check(root)
      self.assertEqual(report.errors, [])
      self.assertEqual(len(projects), 2)

  def test_the_repository_passes(self):
    report, _ = CheckProjectFiles.Check(ROOT)
    self.assertEqual(report.errors, [])

  def test_each_broken_fixture_fails_for_its_reason(self):
    for name, (breakIt, expected) in BROKEN.items():
      with self.subTest(name), tempfile.TemporaryDirectory() as folder:
        root = Path(folder)
        Copy(root)
        breakIt(root)
        report, _ = CheckProjectFiles.Check(root)
        self.assertTrue(any(expected in error for error in report.errors), f"{name}: expected {expected!r} in {report.errors}")


CLANG_FORMAT = shutil.which("clang-format-18") or shutil.which("clang-format")


@unittest.skipUnless(CLANG_FORMAT, "clang-format is not installed")
class CheckFormatTests(unittest.TestCase):
  def Run(self, _root, *_arguments):
    return CheckFormat.Main(["--root", str(_root), "--clang-format", CLANG_FORMAT, "--any-version", *_arguments])

  def test_the_good_fixture_is_formatted(self):
    with tempfile.TemporaryDirectory() as folder:
      root = Path(folder)
      Copy(root)
      shutil.copy(ROOT / ".clang-format", root / ".clang-format")
      self.assertEqual(self.Run(root), 0)

  def test_a_misformatted_file_fails_and_fix_repairs_it(self):
    with tempfile.TemporaryDirectory() as folder:
      root = Path(folder)
      Copy(root)
      shutil.copy(ROOT / ".clang-format", root / ".clang-format")
      Edit(root, SOURCE, "  return _xVoxels", "      return    _xVoxels")
      self.assertEqual(self.Run(root), 1)
      self.assertEqual(self.Run(root, "--fix"), 0)
      self.assertEqual(self.Run(root), 0)


class RunClangTidyCommandTests(unittest.TestCase):
  """The commands come from the sheet, not from the script (ADR-010, decision 8)."""

  def Switches(self, _root):
    commands = RunClangTidy.Commands(_root, "clang-tidy", "Debug", "x64")
    self.assertTrue(commands)
    return [command[command.index("--") + 1:] for _, command in commands]

  def test_the_language_comes_from_the_sheet(self):
    with tempfile.TemporaryDirectory() as folder:
      root = Path(folder)
      Copy(root)
      for switches in self.Switches(root):
        self.assertIn("/std:c++latest", switches)
        self.assertIn("/permissive-", switches)
        self.assertIn("/EHsc", switches)

  def test_a_changed_sheet_changes_the_commands(self):
    with tempfile.TemporaryDirectory() as folder:
      root = Path(folder)
      Copy(root)
      Edit(root, SHEET, "<LanguageStandard>stdcpplatest</LanguageStandard>", "<LanguageStandard>stdcpp20</LanguageStandard>")
      Edit(root, SHEET, "<ConformanceMode>true</ConformanceMode>", "<ConformanceMode>false</ConformanceMode>")
      for switches in self.Switches(root):
        self.assertIn("/std:c++20", switches)
        self.assertNotIn("/std:c++latest", switches)
        self.assertIn("/permissive", switches)
        self.assertNotIn("/permissive-", switches)

  def test_a_value_it_cannot_map_is_refused(self):
    with tempfile.TemporaryDirectory() as folder:
      root = Path(folder)
      Copy(root)
      Edit(root, SHEET, "<LanguageStandard>stdcpplatest</LanguageStandard>", "<LanguageStandard>stdcpp99</LanguageStandard>")
      with contextlib.redirect_stdout(io.StringIO()) as output:
        self.assertEqual(RunClangTidy.Main(["--root", str(root), "--print-commands"]), 2)
      self.assertIn("LanguageStandard", output.getvalue())


CLANG_TIDY = shutil.which("clang-tidy")
CLANG_TIDY_READY = CLANG_TIDY is not None and "INCLUDE" in os.environ
CLANG_TIDY_REQUIRED = bool(os.environ.get("REQUIRE_CLANG_TIDY"))


@unittest.skipUnless(CLANG_TIDY_READY or CLANG_TIDY_REQUIRED, "clang-tidy needs a Developer PowerShell (INCLUDE) and clang-tidy on PATH")
class RunClangTidyTests(unittest.TestCase):
  """The repository's .clang-tidy is not vacuous: the good fixture passes it, and a broken one does not."""

  def setUp(self):
    if not CLANG_TIDY_READY:
      self.fail("REQUIRE_CLANG_TIDY is set, but clang-tidy or INCLUDE is missing")

  def Run(self, _root):
    # The repository's configuration, with the fixture's own registry of projects.
    fixtureRegistry = re.search(r"^HeaderFilterRegex:.*$", (GOOD / ".clang-tidy").read_text(encoding="utf-8"), re.M).group(0)
    configuration = re.sub(r"^HeaderFilterRegex:.*$", lambda _match: fixtureRegistry, (ROOT / ".clang-tidy").read_text(encoding="utf-8"), count=1, flags=re.M)
    (_root / ".clang-tidy").write_text(configuration, encoding="utf-8")
    with contextlib.redirect_stdout(io.StringIO()) as output:
      result = RunClangTidy.Main(["--root", str(_root), "--clang-tidy", CLANG_TIDY, "--jobs", "1"])
    return result, output.getvalue()

  def test_the_good_fixture_is_clean(self):
    with tempfile.TemporaryDirectory() as folder:
      root = Path(folder)
      Copy(root)
      result, output = self.Run(root)
      self.assertEqual(result, 0, output)

  def test_a_naming_violation_is_found(self):
    with tempfile.TemporaryDirectory() as folder:
      root = Path(folder)
      Copy(root)
      Edit(root, SOURCE, "std::uint32_t PackPosition(std::uint32_t _xVoxels,", "std::uint32_t PackPosition(std::uint32_t xVoxels,")
      Edit(root, SOURCE, "return _xVoxels", "return xVoxels")
      result, output = self.Run(root)
      self.assertEqual(result, 1, output)
      self.assertIn("readability-identifier-naming", output)


if __name__ == "__main__":
  unittest.main()
