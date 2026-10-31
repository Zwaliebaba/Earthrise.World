#pragma once

// The minimum Windows is Windows 11 (ADR-010, decision 6). Naming its first release, 21H2, as
// NTDDI_VERSION lets <sdkddkver.h> derive _WIN32_WINNT and WINVER, which Windows 10 and 11 share.
#include <winsdkver.h>

#define NTDDI_VERSION NTDDI_WIN10_CO

#include <sdkddkver.h>
