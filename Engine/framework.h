#pragma once

// The one owner of the Windows macro family (AGENTS.md §4, ADR-010). Nothing else defines any of
// these, and the project files define none: a second definition is C4005, fatal under /WX.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define NOMCX
#define NOSERVICE
#define NOHELP

#include "targetver.h"

#include <windows.h>
