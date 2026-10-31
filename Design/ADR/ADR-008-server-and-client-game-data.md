# ADR-008 — Server data is baked into files; client data is compiled in

**Status:** Accepted, 2026-10-31, by the owner. It supersedes, in part, three of [ADR-005](ADR-005-content-formats.md)'s decisions:
- decision 1, on where content sources live;
- decision 3, on committing a `.vox` copy beside each source;
- decision 9, on baking, for the server only.

ADR-005's format, palette, decorations, counted costs and saves stand. Like ADR-002 to ADR-007, this ADR is written ahead of the code it governs, except for the `.vox` tools, which land with it.

## Context

- **The owner wants content split by its user.** Server and client each get their own source tree, `GameData\Server` and `GameData\Client`, and their own output folder, `<outputdir>\ServerData` and `<outputdir>\GameData`.
- **ADR-005 baked everything into the executable,** so that a replay always runs against the content its build carried ([ADR-002](ADR-002-authoritative-simulation.md), decision 6).
- **The simulation is the server.** It runs in the same process in single-player and on a host in Worlds (ADR-002). Some data only the server needs: unit statistics, wave scripts, AI tuning, scenarios, generator parameters and where resources lie. That data need never reach a Worlds client ([ADR-003](ADR-003-world-as-seed-plus-edits.md)), and it is also what the playtests tune.
- **Some content is read by both sides:**
  - **templates:** the simulation needs each voxel's behaviour, its critical part and the counted cost; the client needs the shape, the colours and the cost, for placement previews and the HUD;
  - **the palette:** the simulation needs behaviours, and the client needs colours.
- **The owner authors in Claude Design,** which writes JSON layer files and `.vox` files, and sometimes edits a `.vox` in MagicaVoxel. A `.vox` cannot hold critical parts, part names, thresholds, the voxel size or costs. Committing both formats has meant keeping two copies in agreement.

## Decision

1. **Sources live in `GameData/`** at the repository root, outside every project directory:
   - **`Server/`:** content the simulation decides by: the templates and the palette. Later it also holds statistics, scenarios, waves and generator parameters.
   - **`Client/`:** content only the client reads: the models and the font. Later it also holds audio sources.
   - **`Illustrations/`:** squads, earthworks and material samples, which neither side reads and nothing bakes.
   - **`Manifest.json`:** the asset check's expectations.
2. **Shared content has one source, on the server's side.** A template or the palette lives in `GameData/Server/`. The bake writes the simulation view to the server's data, and only what the client needs into the client: shape, palette indices, colours and cost. Critical-part tags, thresholds and behaviours stay on the server.
3. **The client has everything compiled in, as ADR-005 decided.**
   - `Tools/BakeContent.py` writes generated C++ headers into the build's intermediate directory, and they compile into the `Content` library.
   - The client's tables come from `GameData/Client/` and from the client view of `GameData/Server/`.
   - Nothing the client draws or reads is loaded from disk, except what decision 6 allows.
4. **The server reads `<outputdir>\ServerData` at startup.**
   - **The format is our own:** the bake writes a binary of its own design, little-endian and versioned.
   - **Every check runs at bake time:** decorations resolved, costs counted, parts named, structures connected (ADR-005). The server's reader only checks the header and the version. It refuses to start on a file that is missing, corrupt or of another version.
   - **No C++ rebuild:** the bake tool can run on its own, so server-only data is tuned without recompiling.
   - **The server parses no JSON and no `.vox`.**
5. **Two hashes keep the halves honest,** both from a pinned hash in `Core`:
   - **The shared hash** covers the client view of the shared sources. The bake writes it into both the server's data and the client's tables. They must match when the in-process game starts, and when a client connects to a server. Otherwise the game refuses: a client and a server that disagree on a template would preview one building and build another.
   - **The server-data hash** covers all of `ServerData`. Saves, replays and playtest logs record it with the build, and a replay or load refuses a mismatch. This extends ADR-002, decision 6: a replay is promised within one build and one set of server data.
6. **`<outputdir>\GameData` holds client content too large to compile in,** audio first. The MVP leaves it empty until audio lands, and a later ADR sets its format with the audio.
7. **The `.json` layer file is the only committed voxel source.**
   - **`Tools/ExportVox.py`** writes a `.vox` beside each layer file for MagicaVoxel. `.gitignore` keeps those copies out of the repository.
   - **`Tools/ImportVox.py`** writes an edited `.vox` back into its `.json`. It keeps each remaining voxel's part, leaves added voxels without one for the author to set, and adds legend entries for new colours.
   - **The committed `.vox` copies and the check that they agree are removed.** The `.png` previews stay; nothing reads them, and they may lag an edit.
8. **Shaders stay compiled into the executable** (AGENTS.md §2). They are code, versioned with the executable that binds them.

## How the tools were checked

These checks were run by script on the day, against the 27 `.vox` files then committed:
- **Export:** for 21 files, the exporter reproduced the package's `SIZE` and `XYZI` chunks byte for byte. The other 6 were the figures and T2 that an earlier script had rewritten; for those, the exporter wrote the same voxels in a different order. Every palette entry the package defined matched.
- **Import:** importing all 27 committed `.vox` files, and separately exporting then importing every layer file, left every `.json` byte-identical, line endings included.
- **An edit:** an import that added T9's four missing posts kept the roof's parts and reported the four new voxels without a part.

## Consequences for the code

- **The solution layout changes** (MvpPlan.md, Proposed):
  - `Content` becomes the client's compiled-in content and depends only on `Core`;
  - `Simulation` reads `ServerData` itself and no longer depends on `Content`;
  - the in-process executable compares the shared hash of the two halves at startup.
- **The asset check,** `Tools/CheckAssets.py` in M4, drops the `.vox` agreement test. It adds two tests: nothing under `GameData/Client/` reaches the server's bake, and nothing server-only reaches the client's tables.
- **The output folders are build output,** beside the executables in the build's output directory, and are never committed (AGENTS.md §2). The solution layout ADR in M1 fixes where `<outputdir>` is.
- **Installs:** a Worlds client install can leave `ServerData` out. The trials and Skirmish run the simulation in-process, so their installs carry it. The split therefore keeps server-only data off a machine only where a client-only install exists.
- **Single-player data can be edited.** In single-player the player can edit `ServerData`. Nothing in the trials relies on it staying untouched, and the playtest logs record its hash.

## What this forecloses

- Client content loaded from files, other than decision 6's.
- The server parsing source formats at runtime.
- A save or replay that runs against server data other than what it recorded.
- Committed `.vox` copies, and shared content with two sources.

## Left open

- The `ServerData` format in detail, written with the bake tool in M4.
- Where `<outputdir>` is, settled with the solution layout in M1.
- Whether a running Worlds season may take new server data, and how its saves would follow.
