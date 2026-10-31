"""Checks the build's shape: the solution, the property sheet, every project file and its sources.

It enforces what AGENTS.md assigns to it (the Enforcement table in §1, and §2 and §3) and what
ADR-010 decision 9 adds:
- one solution, Earthrise.slnx, naming every project, with x64 and ARM64 in Debug and Release;
- Build/Earthrise.props holds the mandated values (AGENTS.md §3, R16, ADR-011's shader settings),
  and Debug and Release, and x64 and ARM64, differ only as AGENTS.md §3 allows;
- every project imports the sheet, in the one import order ADR-010 sets, and overrides nothing;
- every source file is registered in its .vcxproj and .filters, filters are functional, and a .h
  shares its filter with its .cpp;
- project folders are flat except Shader/, and shaders are named for their stage;
- R2 (no affixes on type names), R7 (file names), R11 (one spelling per family);
- Windows headers stay out of Core, Protocol, Content and Simulation (ADR-007), and only
  Engine/framework.h defines the Windows macros (AGENTS.md §4);
- .clang-tidy's header filter names exactly the solution's projects, anchored (ADR-010).

Usage:
    python Build/CheckProjectFiles.py              the repository this script sits in
    python Build/CheckProjectFiles.py --root DIR   another tree, such as a fixture
"""

import argparse
import re
import sys
from pathlib import Path

import ProjectModel as Model

# ── What the property sheet must say ────────────────────────────────────────────────────────

MANDATED = {
  ("Property", "PlatformToolset"): "v145",
  ("Property", "CharacterSet"): "Unicode",
  ("Property", "WindowsTargetPlatformVersion"): "10.0",
  ("Property", "OutDir"): r"$(SolutionDir)$(Platform)\$(Configuration)" + "\\",
  ("Property", "IntDir"): r"$(SolutionDir)$(Platform)\$(Configuration)\Intermediate\$(ProjectName)" + "\\",
  ("ClCompile", "LanguageStandard"): "stdcpplatest",
  ("ClCompile", "ConformanceMode"): "true",
  ("ClCompile", "WarningLevel"): "Level4",
  ("ClCompile", "TreatWarningAsError"): "true",
  ("ClCompile", "FloatingPointModel"): "Precise",
  ("ClCompile", "DebugInformationFormat"): "ProgramDatabase",
  ("ClCompile", "PrecompiledHeader"): "Use",
  ("ClCompile", "PrecompiledHeaderFile"): "pch.h",
  ("FXCompile", "ShaderModel"): "6.0",
  ("FXCompile", "HeaderFileOutput"): r"$(ProjectDir)CompiledShader\%(Filename).h",
  ("FXCompile", "VariableName"): "g_%(Filename)",
  ("FXCompile", "ObjectFileOutput"): "",
}
MANDATED_BY_CONFIGURATION = {
  "Debug": {
    ("Property", "UseDebugLibraries"): "true",
    ("Property", "LinkIncremental"): "true",
    ("ClCompile", "Optimization"): "Disabled",
    ("ClCompile", "RuntimeLibrary"): "MultiThreadedDebugDLL",
  },
  "Release": {
    ("Property", "UseDebugLibraries"): "false",
    ("Property", "WholeProgramOptimization"): "true",
    ("Property", "LinkIncremental"): "false",
    ("ClCompile", "Optimization"): "MaxSpeed",
    ("ClCompile", "RuntimeLibrary"): "MultiThreadedDLL",
    ("ClCompile", "FunctionLevelLinking"): "true",
    ("ClCompile", "IntrinsicFunctions"): "true",
    ("Link", "EnableCOMDATFolding"): "true",
    ("Link", "OptimizeReferences"): "true",
  },
}
DEBUG_DEFINE = {"Debug": "_DEBUG", "Release": "NDEBUG"}
ARCH = {"x64": "/arch:AVX2", "ARM64": "/arch:armv8.0+lse"}

# AGENTS.md §3: the whole of what may differ between Debug and Release, with the shader compiler's
# counterpart of Optimization (ADR-011). Between x64 and ARM64, only /arch.
ALLOWED_DEBUG_RELEASE = {
  ("Property", "UseDebugLibraries"),
  ("Property", "WholeProgramOptimization"),
  ("Property", "LinkIncremental"),
  ("ClCompile", "Optimization"),
  ("ClCompile", "PreprocessorDefinitions"),
  ("ClCompile", "RuntimeLibrary"),
  ("ClCompile", "FunctionLevelLinking"),
  ("ClCompile", "IntrinsicFunctions"),
  ("Link", "EnableCOMDATFolding"),
  ("Link", "OptimizeReferences"),
  ("FXCompile", "DisableOptimizations"),
  ("FXCompile", "EnableDebuggingInformation"),
  ("FXCompile", "AdditionalOptions"),
}
# Debug's shaders carry their debug information inside them, and dxc refuses the option without it.
DEBUG_SHADER_OPTION = "-Qembed_debug"
ALLOWED_PLATFORM = {("ClCompile", "AdditionalOptions")}

WINDOWS_MACROS = {"NOMINMAX", "WIN32_LEAN_AND_MEAN", "NOMCX", "NOSERVICE", "NOHELP"}
MACRO_OWNER = Path("Engine") / "framework.h"
WINDOWS_FREE_PROJECTS = {"Core", "Protocol", "Content", "Simulation"}  # ADR-007, decision 3
WINDOWS_INCLUDE = re.compile(r'#\s*include\s*[<"](windows\.h|d3d12[^>"]*|dxgi[^>"]*|winrt/[^>"]*|framework\.h)[>"]', re.I)

# ── What a project file may hold (ADR-010, decision 4) ──────────────────────────────────────

IMPORT_ORDER = [
  r"$(VCTargetsPath)\Microsoft.Cpp.Default.props",
  r"$(MSBuildThisFileDirectory)..\Build\Earthrise.props",
  r"$(VCTargetsPath)\Microsoft.Cpp.targets",
]
ROOT_ELEMENTS = {"ItemGroup", "PropertyGroup", "Import", "ItemDefinitionGroup", "Target"}
GLOBALS = {"VCProjectVersion", "ProjectGuid", "RootNamespace", "Keyword"}
CONFIGURATION_TYPES = {"StaticLibrary", "Application", "DynamicLibrary"}
PROJECT_SETTINGS = {
  ("ClCompile", "AdditionalIncludeDirectories"),
  ("ClCompile", "PreprocessorDefinitions"),
  ("Link", "SubSystem"),
  ("Link", "AdditionalDependencies"),
  ("Link", "AdditionalLibraryDirectories"),
  ("Manifest", "AdditionalManifestFiles"),
}
UNIT_TEST_INCLUDE = r"$(VCInstallDir)Auxiliary\VS\UnitTest\include"
UNIT_TEST_LIB = r"$(VCInstallDir)Auxiliary\VS\UnitTest\lib"
GENERATED_INCLUDE = r"$(IntDir)Generated"
ITEM_KIND = {".h": "ClInclude", ".cpp": "ClCompile", ".hlsl": "FXCompile", ".hlsli": "None", ".manifest": "Manifest",
             ".rc": "ResourceCompile"}
FORBIDDEN_EXTENSIONS = {".hpp", ".hh", ".hxx", ".cc", ".cxx", ".c", ".inl"}
DEFAULT_FILTERS = {"Source Files", "Header Files", "Resource Files"}

# R7: PascalCase, plus the names MSBuild and the Visual Studio wizards spell their own way.
FILE_NAME = re.compile(r"^[A-Z][A-Za-z0-9]*\.(h|cpp)$")
FILE_NAME_EXCEPTIONS = {"pch.h", "pch.cpp", "framework.h", "targetver.h", "Resource.h"}
SHADER_STAGES = {"VS": "Vertex", "PS": "Pixel", "CS": "Compute", "GS": "Geometry", "HS": "Hull", "DS": "Domain",
                 "MS": "Mesh", "AS": "Amplification"}
SHADER_NAME = re.compile(r"^([A-Z][A-Za-z0-9]*?)(" + "|".join(SHADER_STAGES) + r")\.hlsl$")
SHARED_SHADER_NAME = re.compile(r"^[A-Z][A-Za-z0-9]*\.hlsli$")

# R2: a type name carries no prefix or affix.
DECLARATION = re.compile(r"\b(?:class|struct|union|concept|enum(?:\s+class|\s+struct)?)\s+(?:\[\[[^\]]*\]\]\s*)?([A-Za-z_]\w*)")
ALIAS = re.compile(r"\busing\s+([A-Za-z_]\w*)\s*=")
AFFIX = re.compile(r"^(?:[CSEI][A-Z][a-z0-9]|Base[A-Z]|Abstract[A-Z])|(?:Base|Abstract|Impl|_t)$")

# R11: the SDK's spelling of each family, so the other one is a defect in an identifier.
OTHER_SPELLINGS = re.compile(r"colour|initialis|serialis|normalis|quantis|synchronis|behaviour|neighbour|centre|grey|cancell")


class Report:
  def __init__(self, _root):
    self.root = _root
    self.errors = []
    self.solutionUnreadable = False

  def Error(self, _path, _message):
    where = _path.relative_to(self.root).as_posix() if isinstance(_path, Path) and _path.is_absolute() else str(_path)
    self.errors.append(f"{where}: {_message}")


def StripCommentsAndStrings(_text):
  """C++ source with comments, string and character literals blanked, so names are found only in code."""
  out = []
  i, length = 0, len(_text)
  while i < length:
    two = _text[i : i + 2]
    if two == "//":
      end = _text.find("\n", i)
      i = length if end < 0 else end
    elif two == "/*":
      end = _text.find("*/", i + 2)
      i = length if end < 0 else end + 2
      out.append(" ")
    elif _text.startswith('R"', i):
      delimiter_end = _text.find("(", i + 2)
      delimiter = _text[i + 2 : delimiter_end]
      end = _text.find(")" + delimiter + '"', delimiter_end)
      i = length if end < 0 else end + len(delimiter) + 2
      out.append('""')
    elif _text[i] in "\"'":
      quote = _text[i]
      i += 1
      while i < length and _text[i] != quote:
        i += 2 if _text[i] == "\\" else 1
      i += 1
      out.append(quote * 2)
    else:
      out.append(_text[i])
      i += 1
  return "".join(out)


# ── The property sheet ──────────────────────────────────────────────────────────────────────


def CheckSheet(_report, _root):
  path = _root / Model.PROPERTY_SHEET
  if not path.exists():
    _report.Error(Model.PROPERTY_SHEET, "the property sheet is missing (ADR-010, decision 4)")
    return
  sheet = Model.ParseXml(path)
  imports = [element.get("Project") for element in sheet if Model.LocalName(element) == "Import"]
  if imports != [r"$(VCTargetsPath)\Microsoft.Cpp.props"]:
    _report.Error(path, r"the sheet must import $(VCTargetsPath)\Microsoft.Cpp.props once, and nothing else")
  try:
    evaluated = {pair: Model.EvaluateSheet(sheet, *pair) for pair in Model.AllConfigurations()}
  except Model.ConditionError as error:
    _report.Error(path, f"{error}; the sheet may condition only on $(Configuration) and $(Platform)")
    return

  for (configuration, platform), values in evaluated.items():
    where = f"{configuration}|{platform}"
    expected = dict(MANDATED)
    expected.update(MANDATED_BY_CONFIGURATION[configuration])
    for key, value in expected.items():
      actual = values.get(key)
      if actual != value:
        _report.Error(path, f"{where}: {key[0]} {key[1]} is {actual!r}, must be {value!r}")
    options = values.get(("ClCompile", "AdditionalOptions"), [])
    archs = [option for option in options if option.lower().startswith("/arch:")]
    if archs != [ARCH[platform]]:
      _report.Error(path, f"{where}: /arch must be exactly {ARCH[platform]} (R16), found {archs}")
    for option in options:
      lowered = option.lower()
      if lowered.startswith(("/fp:", "/std:", "/w", "/permissive", "/zc:")):
        _report.Error(path, f"{where}: {option} in AdditionalOptions; its property is mandated instead (R16)")
    defines = set(values.get(("ClCompile", "PreprocessorDefinitions"), []))
    if DEBUG_DEFINE[configuration] not in defines or DEBUG_DEFINE["Release" if configuration == "Debug" else "Debug"] in defines:
      _report.Error(path, f"{where}: PreprocessorDefinitions must hold {DEBUG_DEFINE[configuration]} and not the other")
    for macro in sorted(defines & (WINDOWS_MACROS | {"UNICODE", "_UNICODE"})):
      _report.Error(path, f"{where}: {macro} is defined in the sheet; framework.h or CharacterSet owns it (AGENTS.md §4)")
    if (DEBUG_SHADER_OPTION in values.get(("FXCompile", "AdditionalOptions"), [])) != (configuration == "Debug"):
      _report.Error(path, f"{where}: {DEBUG_SHADER_OPTION} belongs in Debug's shader options and nowhere else (ADR-011)")

  for platform in Model.PLATFORMS:
    debug, release = evaluated[("Debug", platform)], evaluated[("Release", platform)]
    for key in sorted(set(debug) | set(release)):
      if debug.get(key) == release.get(key):
        continue
      if key not in ALLOWED_DEBUG_RELEASE:
        _report.Error(path, f"{platform}: {key[0]} {key[1]} differs between Debug and Release (AGENTS.md §3)")
      elif key == ("ClCompile", "PreprocessorDefinitions"):
        if (set(debug.get(key, [])) ^ set(release.get(key, []))) - {"_DEBUG", "NDEBUG"}:
          _report.Error(path, f"{platform}: Debug and Release defines differ beyond _DEBUG and NDEBUG")
      elif key == ("FXCompile", "AdditionalOptions"):
        if (set(debug.get(key, [])) ^ set(release.get(key, []))) - {DEBUG_SHADER_OPTION}:
          _report.Error(path, f"{platform}: Debug and Release shader options differ beyond {DEBUG_SHADER_OPTION}")
  for configuration in Model.CONFIGURATIONS:
    x64, arm64 = evaluated[(configuration, "x64")], evaluated[(configuration, "ARM64")]
    for key in sorted(set(x64) | set(arm64)):
      if x64.get(key) == arm64.get(key):
        continue
      if key not in ALLOWED_PLATFORM:
        _report.Error(path, f"{configuration}: {key[0]} {key[1]} differs between x64 and ARM64 (AGENTS.md §3)")
      else:
        rest = lambda options: [option for option in options if not option.lower().startswith("/arch:")]
        if rest(x64.get(key, [])) != rest(arm64.get(key, [])):
          _report.Error(path, f"{configuration}: x64 and ARM64 options differ beyond /arch (AGENTS.md §3)")


# ── The solution ────────────────────────────────────────────────────────────────────────────


def FindVcxprojs(_root):
  found = []
  for path in sorted(_root.rglob("*.vcxproj")):
    relative = path.relative_to(_root)
    if relative.parts[0] in Model.SKIPPED_FOLDERS or relative.parts[: len(Model.FIXTURES.parts)] == Model.FIXTURES.parts:
      continue
    found.append(path)
  return found


def CheckSolution(_report, _root):
  """Returns the solution's projects, or None when there is no solution yet."""
  solutions = sorted(list(_root.glob("*.slnx")) + list(_root.glob("*.sln")))
  if not solutions:
    for stray in FindVcxprojs(_root):
      _report.Error(stray, "a project with no solution (ADR-010, decision 1)")
    return None
  if len(solutions) > 1:
    _report.Error(_root, "more than one solution: " + ", ".join(path.name for path in solutions))
    _report.solutionUnreadable = True
    return None
  solution = solutions[0]
  if solution.name != "Earthrise.slnx":
    _report.Error(solution, "the solution is Earthrise.slnx (ADR-010, decision 1)")
    _report.solutionUnreadable = True
    return None
  platforms, buildTypes = Model.SolutionPlatforms(solution), Model.SolutionBuildTypes(solution)
  if platforms and sorted(platforms) != sorted(Model.PLATFORMS):
    _report.Error(solution, f"the platforms are x64 and ARM64 (AGENTS.md §3), found {platforms}")
  if buildTypes and sorted(buildTypes) != sorted(Model.CONFIGURATIONS):
    _report.Error(solution, f"the configurations are Debug and Release, found {buildTypes}")

  projects = []
  for relative in Model.SolutionProjects(solution):
    path = _root / relative
    if relative.suffix != ".vcxproj" or len(relative.parts) != 2 or relative.parts[0] != relative.stem:
      _report.Error(solution, f"{relative.as_posix()}: a project is <Name>/<Name>.vcxproj at the root (ADR-010)")
      continue
    if not path.exists():
      _report.Error(solution, f"{relative.as_posix()} does not exist")
      continue
    projects.append(Model.LoadProject(path))
  listed = {project.path.resolve() for project in projects}
  for stray in FindVcxprojs(_root):
    if stray.resolve() not in listed:
      _report.Error(stray, "a project the solution does not name")
  return projects


# ── One project ─────────────────────────────────────────────────────────────────────────────


def CheckProjectFile(_report, _project, _names):
  root, path, name = _project.root, _project.path, _project.name
  isTests = name.endswith("Tests")
  if isTests and name[: -len("Tests")] not in _names:
    _report.Error(path, f"a test suite is <Project>Tests (R9), and there is no project {name[: -len('Tests')]}")
  configurations = Model.ProjectConfigurations(_project)
  expected = sorted(f"{c}|{p}" for c, p in Model.AllConfigurations())
  if sorted(configurations) != expected:
    _report.Error(path, f"configurations must be exactly {expected} (AGENTS.md §3), found {sorted(configurations)}")
  for element in root.iter():
    if element.get("Condition") is not None:
      _report.Error(path, f"a Condition on <{Model.LocalName(element)}>; per-configuration settings live in the sheet")
  imports = [element.get("Project") for element in root if Model.LocalName(element) == "Import"]
  if imports != IMPORT_ORDER:
    _report.Error(path, "imports must be exactly, in order: " + ", ".join(IMPORT_ORDER) + " (ADR-010)")
  for element in root:
    kind = Model.LocalName(element)
    if kind not in ROOT_ELEMENTS:
      _report.Error(path, f"<{kind}> is not allowed in a project file (ADR-010)")
    if kind == "PropertyGroup":
      label = element.get("Label")
      allowed = GLOBALS if label == "Globals" else {"ConfigurationType"} if label == "Configuration" else set()
      for child in element:
        if Model.LocalName(child) not in allowed:
          _report.Error(path, f"property {Model.LocalName(child)} belongs in the sheet, not the project (ADR-010)")
      if label == "Globals":
        namespace = element.find("m:RootNamespace", Model.NS)
        if namespace is None or (namespace.text or "").strip() != name:
          _report.Error(path, f"RootNamespace must be {name} (R9)")
      if label == "Configuration":
        configurationType = element.find("m:ConfigurationType", Model.NS)
        value = (configurationType.text or "").strip() if configurationType is not None else ""
        if value not in CONFIGURATION_TYPES:
          _report.Error(path, f"ConfigurationType {value!r} is not one of {sorted(CONFIGURATION_TYPES)}")
        elif (value == "DynamicLibrary") != isTests:
          _report.Error(path, "test suites, and only test suites, are DLLs (CppUnitTest, AGENTS.md R14)")
  settings = Model.ProjectSettings(_project)
  for key, value in settings.items():
    if key not in PROJECT_SETTINGS:
      _report.Error(path, f"{key[0]} {key[1]} is set in the project; it belongs in the sheet (ADR-010)")
  for directory in settings.get(("ClCompile", "AdditionalIncludeDirectories"), []):
    if directory == "%(AdditionalIncludeDirectories)" or directory == GENERATED_INCLUDE:
      continue
    if directory == UNIT_TEST_INCLUDE and isTests:
      continue
    other = directory[len("$(SolutionDir)") :].rstrip("\\/") if directory.startswith("$(SolutionDir)") else None
    if other == name:
      _report.Error(path, "a project does not put its own folder on the include path (AGENTS.md §3)")
    elif other not in _names:
      _report.Error(path, f"include directory {directory}: only $(SolutionDir)<OtherProject> (AGENTS.md §3)")
  for directory in settings.get(("Link", "AdditionalLibraryDirectories"), []):
    if directory != "%(AdditionalLibraryDirectories)" and not (isTests and directory == UNIT_TEST_LIB):
      _report.Error(path, f"library directory {directory}: only the unit-test library, in a test suite")
  for define in settings.get(("ClCompile", "PreprocessorDefinitions"), []):
    if define in WINDOWS_MACROS | {"_DEBUG", "NDEBUG", "UNICODE", "_UNICODE"}:
      _report.Error(path, f"{define} is defined by the project; the sheet, CharacterSet or framework.h owns it")


def CheckItems(_report, _project, _names):
  path, folder = _project.path, _project.folder
  registered = {}
  for item in _project.items:
    if item.kind == "BakeInput":
      # The bake's inputs (ADR-010, decision 7): content sources, read by Content's bake target only.
      if _project.name != "Content":
        _report.Error(path, f"BakeInput {item.include}: only Content bakes (ADR-010, decision 7)")
      elif not item.include.startswith((r"$(SolutionDir)GameData" + "\\", r"$(SolutionDir)Tools" + "\\")) or item.metadata:
        _report.Error(path, f"BakeInput {item.include}: bake inputs are files under GameData\\ or Tools\\")
      continue
    if item.kind == "ProjectReference":
      target = item.include.replace("\\", "/")
      match = re.fullmatch(r"\.\./([A-Za-z0-9]+)/([A-Za-z0-9]+)\.vcxproj", target)
      if not match or match.group(1) != match.group(2) or match.group(1) not in _names:
        _report.Error(path, f"ProjectReference {item.include} names no project of the solution")
      continue
    relative = Path(item.include.replace("\\", "/"))
    if ".." in relative.parts or relative.is_absolute():
      _report.Error(path, f"{item.include}: items live inside the project folder")
      continue
    expectedKind = ITEM_KIND.get(relative.suffix.lower())
    if expectedKind is None:
      _report.Error(path, f"{item.include}: an unexpected file kind in the project")
    elif item.kind != expectedKind:
      _report.Error(path, f"{item.include} is registered as {item.kind}, must be {expectedKind} (AGENTS.md §2)")
    allowedMetadata = {"ClCompile": {"PrecompiledHeader"}, "FXCompile": {"ShaderType"}}.get(item.kind, set())
    for key in item.metadata:
      if key not in allowedMetadata:
        _report.Error(path, f"{item.include}: per-file {key} is not allowed; settings live in the sheet")
    if item.kind == "ClCompile":
      creates = item.metadata.get("PrecompiledHeader")
      if (relative.name == "pch.cpp") != (creates == "Create") or (creates not in (None, "Create")):
        _report.Error(path, f"{item.include}: only pch.cpp creates the precompiled header, and it must")
    if item.kind == "FXCompile":
      match = SHADER_NAME.match(relative.name)
      if match and item.metadata.get("ShaderType") != SHADER_STAGES[match.group(2)]:
        _report.Error(path, f"{item.include}: ShaderType must be {SHADER_STAGES[match.group(2)]} for its name")
    registered[relative.as_posix().lower()] = relative.as_posix()
    if not (folder / relative).exists():
      _report.Error(path, f"{item.include} is registered but does not exist")
  if "pch.h" not in registered or "pch.cpp" not in registered:
    _report.Error(path, "every C++ project has pch.h and pch.cpp (ADR-010, decision 5)")
  return registered


def CheckFolder(_report, _project, _registered):
  folder = _project.folder
  for file in sorted(folder.rglob("*")):
    if not file.is_file():
      continue
    relative = file.relative_to(folder)
    if relative.parts[0] == "CompiledShader" or file.suffix in (".vcxproj", ".filters", ".user"):
      continue
    suffix = file.suffix.lower()
    if suffix in FORBIDDEN_EXTENSIONS:
      _report.Error(file, f"{suffix} is not used; .h and .cpp only (R7)")
      continue
    if len(relative.parts) > 1:
      if relative.parts[0] != "Shader" or len(relative.parts) > 2:
        _report.Error(file, "project folders are flat; only Shader/ is allowed (AGENTS.md §2)")
        continue
      if suffix not in (".hlsl", ".hlsli"):
        _report.Error(file, "Shader/ holds only .hlsl and .hlsli (AGENTS.md §2)")
        continue
    elif suffix in (".hlsl", ".hlsli"):
      _report.Error(file, "shaders live in Shader/ (AGENTS.md §2)")
    if suffix in (".h", ".cpp"):
      if file.name not in FILE_NAME_EXCEPTIONS and not FILE_NAME.match(file.name):
        _report.Error(file, "a file is named for its primary type, in PascalCase (R7)")
    if suffix == ".hlsl" and not SHADER_NAME.match(file.name):
      _report.Error(file, "a shader is named <Shader><Stage>.hlsl, such as VoxelVS.hlsl (AGENTS.md §2)")
    if suffix == ".hlsli" and not SHARED_SHADER_NAME.match(file.name):
      _report.Error(file, "shared HLSL is <Name>.hlsli, in PascalCase (AGENTS.md §2)")
    if suffix in ITEM_KIND and relative.as_posix().lower() not in _registered:
      _report.Error(file, f"not registered in {_project.path.name} (AGENTS.md §2)")


def CheckFilters(_report, _project, _registered):
  filtersPath = _project.path.with_name(_project.path.name + ".filters")
  if not filtersPath.exists():
    _report.Error(_project.path, f"{filtersPath.name} is missing (AGENTS.md §2)")
    return
  root = Model.ParseXml(filtersPath)
  declared = set()
  placed = {}
  for group in root.findall("m:ItemGroup", Model.NS):
    for element in group:
      kind, include = Model.LocalName(element), element.get("Include", "")
      if kind == "Filter":
        declared.add(include)
        if include.split("\\")[0] in DEFAULT_FILTERS:
          _report.Error(filtersPath, f"filter {include!r}: filters are functional, not file kinds (AGENTS.md §2)")
        continue
      filterElement = element.find("m:Filter", Model.NS)
      key = include.replace("\\", "/")
      placed[key.lower()] = ((filterElement.text or "").strip() if filterElement is not None else None, key)
  for key, (filterName, shown) in placed.items():
    if key not in _registered:
      _report.Error(filtersPath, f"{shown} is in the filters but not in the project")
    elif not filterName:
      _report.Error(filtersPath, f"{shown} has no filter")
    elif filterName not in declared:
      _report.Error(filtersPath, f"{shown} is in filter {filterName!r}, which is not declared")
  for key, shown in _registered.items():
    if key not in placed:
      _report.Error(filtersPath, f"{shown} is in the project but not in the filters (AGENTS.md §2)")
  stems = {}
  for key, (filterName, shown) in placed.items():
    if key.endswith((".h", ".cpp")):
      stems.setdefault(shown.rsplit(".", 1)[0], set()).add(filterName)
  for stem, filters in sorted(stems.items()):
    if len(filters) > 1:
      _report.Error(filtersPath, f"{stem}.h and {stem}.cpp sit in different filters (AGENTS.md §2)")


def CheckSources(_report, _root, _project):
  for file in sorted(_project.folder.rglob("*")):
    if file.suffix not in (".h", ".cpp") or not file.is_file() or file.relative_to(_project.folder).parts[0] == "CompiledShader":
      continue
    text = file.read_text(encoding="utf-8", errors="replace")
    code = StripCommentsAndStrings(text)
    if file.name != "Resource.h":
      for pattern in (DECLARATION, ALIAS):
        for match in pattern.finditer(code):
          if len(match.group(1)) > 2 and AFFIX.search(match.group(1)):
            _report.Error(file, f"type {match.group(1)} carries a prefix or affix (R2)")
      for identifier in set(re.findall(r"[A-Za-z_]\w*", code)):
        if OTHER_SPELLINGS.search(identifier.lower()):
          _report.Error(file, f"identifier {identifier}: use the SDK's spelling (R11)")
    if _project.name in WINDOWS_FREE_PROJECTS:
      for match in WINDOWS_INCLUDE.finditer(code):
        _report.Error(file, f"{_project.name} includes {match.group(1)}; Windows and Direct3D stay in Engine (ADR-007)")
    if file.relative_to(_root) != MACRO_OWNER:
      for macro in WINDOWS_MACROS:
        if re.search(r"#\s*define\s+" + macro + r"\b", code):
          _report.Error(file, f"defines {macro}; only {MACRO_OWNER.as_posix()} does (AGENTS.md §4)")


# ── The clang-tidy registry ─────────────────────────────────────────────────────────────────

REGISTRY_PREFIX = r"(^|[/\\])("
REGISTRY_SUFFIX = r")[/\\][A-Za-z0-9]+\.h$"
TESTS_ALTERNATIVE = "[A-Za-z0-9]+Tests"


def CheckRegistry(_report, _root, _names):
  path = _root / ".clang-tidy"
  if not path.exists():
    _report.Error(".clang-tidy", "missing")
    return
  match = re.search(r"^HeaderFilterRegex:\s*'([^']*)'", path.read_text(encoding="utf-8"), re.M)
  if not match:
    _report.Error(path, "no HeaderFilterRegex")
    return
  regex = match.group(1)
  if not (regex.startswith(REGISTRY_PREFIX) and regex.endswith(REGISTRY_SUFFIX)):
    _report.Error(path, f"HeaderFilterRegex must read {REGISTRY_PREFIX}<projects>{REGISTRY_SUFFIX}, anchored (ADR-010)")
    return
  listed = set(regex[len(REGISTRY_PREFIX) : -len(REGISTRY_SUFFIX)].split("|"))
  expected = {name for name in _names if not name.endswith("Tests")} | {TESTS_ALTERNATIVE}
  for missing in sorted(expected - listed):
    _report.Error(path, f"HeaderFilterRegex does not name {missing}, so its headers go unlinted")
  for extra in sorted(listed - expected):
    _report.Error(path, f"HeaderFilterRegex names {extra}, which is not a project of the solution")


def CheckLooseSources(_report, _root, _projects):
  folders = {project.folder.resolve() for project in _projects}
  for file in sorted(_root.rglob("*")):
    if file.suffix.lower() not in {".h", ".cpp", ".hlsl", ".hlsli"} | FORBIDDEN_EXTENSIONS or not file.is_file():
      continue
    relative = file.relative_to(_root)
    if relative.parts[0] in Model.SKIPPED_FOLDERS or relative.parts[: len(Model.FIXTURES.parts)] == Model.FIXTURES.parts:
      continue
    if not any(parent.resolve() in folders for parent in file.parents):
      _report.Error(file, "source outside any project of the solution (AGENTS.md §2)")


def Check(_root):
  report = Report(_root)
  CheckSheet(report, _root)
  projects = CheckSolution(report, _root)
  names = {project.name for project in projects or []}
  CheckRegistry(report, _root, names)
  for project in projects or []:
    if project.path.name != f"{project.name}.vcxproj":
      continue
    CheckProjectFile(report, project, names)
    registered = CheckItems(report, project, names)
    CheckFolder(report, project, registered)
    CheckFilters(report, project, registered)
    CheckSources(report, _root, project)
  if not report.solutionUnreadable:
    CheckLooseSources(report, _root, projects or [])
  return report, projects


def Main(_arguments):
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
  arguments = parser.parse_args(_arguments)
  root = arguments.root.resolve()
  report, projects = Check(root)
  for error in report.errors:
    print(error)
  if report.errors:
    print(f"CheckProjectFiles: {len(report.errors)} problem(s)")
    return 1
  if projects is None:
    print("CheckProjectFiles: OK. No solution yet, so it checked the property sheet, the clang-tidy registry and loose files")
  else:
    print(f"CheckProjectFiles: OK, {len(projects)} project(s)")
  return 0


if __name__ == "__main__":
  sys.exit(Main(sys.argv[1:]))
