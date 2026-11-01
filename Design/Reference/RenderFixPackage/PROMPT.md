# Task: bring the renderer to v5 visual parity

Read `README.md` first; the asset import there comes before item 1. Context: compare the current game render (`screenshots/current-render.png`) against `reference/Earthrise Siege Scene v5.dc.html` and `specs/Renderer Look Spec v2.md`. World voxel = 1 m (simulated); units, engines, debris = 0.5 m models (display only). Do not change camera or zoom.

Work through the items in order. After each one, take a screenshot at the default framing and compare it with v5. Keep the changes deterministic: the same seed and collapse id must give the same result on every client.

## 1. Stone material (spec §3–5)
- Built templates (walls, towers, keep) must use material **6 – dressed stone `#c6bca7`**, not 4 (raw `#ddd2b9`). Audit the template loaders and the material ids that are written.
- Apply the per-voxel HSL **lightness** jitter to stone: ±0.035 for dressed, ±0.02 for raw, using the spec's hash on the 1 m cell. Hue and saturation must not change.
- Structures must use the **structure AO curve** (0.52 / 0.68 / 0.84 / 1.00), not the terrain curve. Merlon gaps and the inner corners of the walls should show clear darkening.
- Acceptance: dressed and raw stone can be told apart side by side; each wall block reads as a separate block.

## 2. Openings
- **Root cause (fixed in the package):** the 0.5 m → 1 m conversion used a majority vote, so 0.5 m-wide slits lost to the surrounding stone. The keep shipped with 4 opening voxels instead of 54, and the gatehouse with none. The corrected `assets/world/templates/*-1m.{json,vox}` in this package include them; `manifest.json` stone costs are updated (keep 1668, tower 469, gatehouse 575 m³).
- **Re-import** T1, T2, T4 and T5 from this package. If the game converts templates itself, apply the same rule: any material-20 voxel in a 2×2×2 block makes the 1 m cell an opening.
- Check the result against these rules. Openings are 1 × 2 m dark voxels (material 20, `#2e2a27`):
  - **Keep:** a row of slits on each face, one per storey (every ~4 m vertically, about every 3 m horizontally).
  - **Towers:** 1–2 slits per face, per storey.
  - **Curtain walls:** an optional slit every ~8 m below the wall walk.
- Openings are template data, not decals, so a breach removes them like any other voxel.

## 3. Keep shape
- Match the asset sheet: 12 × 12 × 16 m with the top storey stepped back and battlements on top. Replace the current squat keep with the flat timber top. The banner stays on the top.

## 4. Breach
- The test map must start with one breached west wall section:
  - Cut a ragged notch through the full 2 m thickness, at least 4 m wide, down to ~1–2 m above ground, with an irregular edge (±1 voxel).
  - World rubble (material 8, ~12% drawn in material 6) forms a **ramp** from outside the ditch up through the notch into the bailey.
  - Scatter 0.5 m model debris over the rubble, seeded from the collapse id (spec §1). It must not block paths or shots.
- Acceptance check 3: the wall visibly has a gap; the rubble reads as wall that came down, not as a pile in front of an intact wall.

## 5. Quarry as moat
- Move the raw-stone quarry to **under the south wall**, as in v5.
- Carve it as a **stepped pit below ground**: 1 m terraces, 3–4 steps deep, with the walls and floor in raw building stone (material 4) and common rock (3) at the bottom.
- Remove stray grass voxels inside the pit. Grass (1) only goes on the top face of exposed earth, never on stone.
- Acceptance: the pit reads as dug out, and the raw stone is visibly lighter than the dressed walls above it.

## 6. Ditch and attack lane
- The ditch and rampart run along the west and south-west fronts: ditch 4–5 m wide, 2 m deep with 1 m steps; rampart 2 m high.
- Leave one **undug stretch** of about 8–10 m in front of the breach, so the attackers' approach is readable.

## 7. Shadows (spec §6)
- Fit the directional shadow camera tightly to the visible holding.
- Enable PCF with a 3×3 kernel or better (soft), and raise the shadow-map resolution if needed (4096² on desktop, 2048² on mobile).
- Tune the normal and depth bias until the sawtooth or dither pattern on the terraced slope east of the keep is gone, without peter-panning under units.
- Acceptance: the shadows on the slopes are clean and continuous, as in v5.

## 8. Terrain noise
- Remove isolated single-voxel height bumps on open plains: no lone +1 m cell surrounded by lower neighbours. Apply a minimum run length of ~3 cells for height changes, or a smoothing pass after generation.
- Keep the designed terraces (ditch, rampart, quarry, field edges).
- Acceptance check 1: open grass reads calm at the default framing, with no scattered dark dashes.

## Done when
- Spec §8 checks 1, 2 and 3 pass by eye against v5.
- Check 7 (voxel pass ≤ 8 ms at 1080p, RTX 3060-class) still holds after the shadow changes. Record the timings in ADR-006.
- Commit one change per item, with before and after screenshots in `screenshots/parity/`.
