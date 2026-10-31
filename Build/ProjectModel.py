"""Reading the solution, the project files and the property sheet (ADR-010).

Shared by CheckProjectFiles.py and RunClangTidy.py. Standard library only. It understands exactly
the shapes ADR-010 allows: one `.slnx`, `.vcxproj` files without conditions, and a property sheet
whose conditions name only $(Configuration) and $(Platform).
"""

import re
import xml.etree.ElementTree as ElementTree
from dataclasses import dataclass, field
from pathlib import Path

MSBUILD_NAMESPACE = "http://schemas.microsoft.com/developer/msbuild/2003"
NS = {"m": MSBUILD_NAMESPACE}
CONFIGURATIONS = ("Debug", "Release")
PLATFORMS = ("x64", "ARM64")
PROPERTY_SHEET = Path("Build") / "Earthrise.props"

# Folders no checker looks into: version control, IDE and build output, and content that is not C++.
SKIPPED_FOLDERS = {".git", ".vs", ".github", ".claude", "x64", "ARM64", "GameData", "Design", "Tools", "__pycache__"}
FIXTURES = Path("Build") / "Fixtures"


def LocalName(_element):
  return _element.tag.split("}", 1)[-1]


def ParseXml(_path):
  return ElementTree.parse(_path).getroot()


@dataclass
class Item:
  kind: str  # ClCompile, ClInclude, FXCompile, None, ProjectReference, ...
  include: str  # as written, with backslashes
  metadata: dict = field(default_factory=dict)


@dataclass
class Project:
  name: str
  path: Path  # the .vcxproj
  root: ElementTree.Element
  items: list = field(default_factory=list)

  @property
  def folder(self):
    return self.path.parent

  def ItemPaths(self, _kinds=None):
    return [item for item in self.items if _kinds is None or item.kind in _kinds]


def LoadProject(_path):
  root = ParseXml(_path)
  project = Project(name=_path.stem, path=_path, root=root)
  for group in root.findall("m:ItemGroup", NS):
    for element in group:
      kind = LocalName(element)
      if kind == "ProjectConfiguration":
        continue
      metadata = {LocalName(child): (child.text or "").strip() for child in element}
      project.items.append(Item(kind=kind, include=element.get("Include", ""), metadata=metadata))
  return project


def ProjectConfigurations(_project):
  return [element.get("Include") for element in _project.root.iter(f"{{{MSBUILD_NAMESPACE}}}ProjectConfiguration")]


def SolutionProjects(_solutionPath):
  """Project paths named by a .slnx, relative to the solution's folder, in the file's order."""
  root = ParseXml(_solutionPath)
  paths = [element.get("Path") for element in root.iter() if LocalName(element) == "Project"]
  return [Path(path.replace("\\", "/")) for path in paths if path]


def SolutionPlatforms(_solutionPath):
  root = ParseXml(_solutionPath)
  return [element.get("Name") for element in root.iter() if LocalName(element) == "Platform"]


def SolutionBuildTypes(_solutionPath):
  root = ParseXml(_solutionPath)
  return [element.get("Name") for element in root.iter() if LocalName(element) == "BuildType"]


# ── The property sheet, evaluated for one configuration and platform ─────────────────────────

_CONDITION = re.compile(r"^\s*'\$\((Configuration|Platform)\)'\s*==\s*'([^']*)'\s*$")
_PAIR_CONDITION = re.compile(r"^\s*'\$\(Configuration\)\|\$\(Platform\)'\s*==\s*'([^'|]*)\|([^']*)'\s*$")


class ConditionError(ValueError):
  pass


def ConditionHolds(_condition, _configuration, _platform):
  """Evaluates the only conditions the sheet may use. Anything else raises ConditionError."""
  if _condition is None or not _condition.strip():
    return True
  holds = True
  for part in re.split(r"\s+and\s+", _condition.strip(), flags=re.IGNORECASE):
    pair = _PAIR_CONDITION.match(part)
    if pair:
      holds = holds and pair.group(1) == _configuration and pair.group(2) == _platform
      continue
    single = _CONDITION.match(part)
    if not single:
      raise ConditionError(f"unsupported condition: {part.strip()}")
    value = _configuration if single.group(1) == "Configuration" else _platform
    holds = holds and single.group(2) == value
  return holds


# Settings whose values accumulate rather than replace, written with a %(Name) back-reference.
LIST_SETTINGS = {
  "AdditionalOptions": " ",
  "PreprocessorDefinitions": ";",
  "AdditionalIncludeDirectories": ";",
  "AdditionalLibraryDirectories": ";",
  "AdditionalDependencies": ";",
  "AdditionalManifestFiles": ";",
}


def _Merge(_name, _old, _new):
  separator = LIST_SETTINGS.get(_name)
  if separator is None:
    return _new
  tokens = [token for token in re.split(r"[;\s]+" if separator == ";" else r"\s+", _new) if token]
  merged = []
  for token in tokens:
    if token == f"%({_name})":
      merged.extend(_old or [])
    else:
      merged.append(token)
  return merged


def EvaluateSheet(_root, _configuration, _platform):
  """Returns {(scope, name): value} for one configuration and platform. Scope is "Property" for
  a PropertyGroup property, or the tool's name (ClCompile, Link, FXCompile) for an
  ItemDefinitionGroup setting. List settings come back as lists of tokens."""
  values = {}
  for group in _root:
    kind = LocalName(group)
    if kind not in ("PropertyGroup", "ItemDefinitionGroup"):
      continue
    if not ConditionHolds(group.get("Condition"), _configuration, _platform):
      continue
    if kind == "PropertyGroup":
      for element in group:
        if element.get("Condition") is not None:
          raise ConditionError(f"condition on a property ({LocalName(element)}) instead of its group")
        values[("Property", LocalName(element))] = (element.text or "").strip()
    else:
      for tool in group:
        for element in tool:
          if element.get("Condition") is not None:
            raise ConditionError(f"condition on a setting ({LocalName(element)}) instead of its group")
          key = (LocalName(tool), LocalName(element))
          values[key] = _Merge(LocalName(element), values.get(key), (element.text or "").strip())
  return values


def AllConfigurations():
  return [(configuration, platform) for configuration in CONFIGURATIONS for platform in PLATFORMS]


def ProjectSettings(_project):
  """The project's own ItemDefinitionGroup settings: {(tool, name): value}."""
  values = {}
  for group in _project.root.findall("m:ItemDefinitionGroup", NS):
    for tool in group:
      for element in tool:
        key = (LocalName(tool), LocalName(element))
        values[key] = _Merge(LocalName(element), values.get(key), (element.text or "").strip())
  return values


def ExpandMacros(_value, _macros):
  for name, replacement in _macros.items():
    _value = _value.replace(f"$({name})", replacement)
  return _value
