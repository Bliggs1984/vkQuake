# Tier 2 port: RayTracedGL1 on vkQuake 1.36.0 (`rt-1.36`)

Working notes for re-attaching the RT renderer (2022 fork, base 1.20.3) to vkQuake 1.36.0.
Spec: `C:\Utils\AIstuff\QuakeRT\MODERNISATION_ASSESSMENT.md` §5 + approved /spec (2026-08-31).
The 2022 RT code is on branch `modernise-2026`; retrieve any original with
`git show modernise-2026:Quake/<file>`.

## Architecture

- `RT_RENDERER` is a **compile-time switch** (msbuild configurations `RT-Release` / `RT-Debug`).
  The vanilla Vulkan renderer stays in-tree and must keep building (`Release`/`Debug`) — this is
  the regression guard and keeps future upstream merges cheap.
- RT variants of renderer files live beside the originals as `Quake/rt_*.c`; the RT configurations
  compile those and exclude the corresponding `gl_*/r_*` originals. Gameplay files are shared, with
  small `#if RT_RENDERER` sections.
- Shared headers (`glquake.h`, `gl_texmgr.h`, `gl_model.h`, `gl_heap.h`) carry `#if RT_RENDERER`
  sections where the fork changed types (`cb_context_t`, `gltexture_t`, `vulkan_globals_t` → RT).
- RTGL1 = submodule `RayTracedGL1` (upstream `quake` @ 9efa82d) + `Patches/RTGL1` (DLSS 4.5, DLAA,
  presets, discrete-GPU pick, glslang-2026 shader fixes) — identical to the 1.1.0 build.

## File disposition

| Fork change (vs 1.20.3) | Plan |
|---|---|
| `gl_vidsdl.c` (−3.6k), `gl_rmisc.c` (−3.2k), `gl_draw.c`, `gl_texmgr.c`, `r_world.c` (+1.5k), `r_brush.c`, `r_alias.c`, `r_sprite.c`, `r_part.c`, `r_part_fte.c`, `gl_sky.c`, `gl_rlight.c`, `gl_rmain.c`, `gl_mesh.c`, `gl_warp.c` (gutted), `gl_heap.c` (gutted) | **RT replacement files** `rt_*.c`, seeded from the fork's versions, then adapted to 1.36 engine internals (tasks API, atomics, new Draw_/R_ entry points the 1.36 game code calls) |
| `menu.c` (290), `sbar.c` (118), `cl_main.c` (112), `pr_ext.c` (197), `gl_model.c` (86), `common.c`, `gl_screen.c`, `view.c`, `cl_tent.c`, `host_cmd.c`, `gl_fog.c`, `cl_input.c`, `cmd.c`, `default_cfg.h`, `quakedef.h` | **Inline `#if RT_RENDERER` guards** re-applied by hand onto the 1.36 versions |
| `glquake.h` (−375), `gl_texmgr.h`, `gl_model.h`, `gl_heap.h` | **Shared headers with `#if RT_RENDERER` sections** |
| Fork's Steam-copy helper in `common.c` | **Dropped** — 1.36 auto-detects Steam/GOG/EGS installs itself (plus our multi-library fix in `steam.c`) |
| Fork's `-basedir`/music copying, 4K first-run box | **Dropped** (superseded upstream) |
| DLSS preset cvar `rt_dlss_preset`, DLAA option values, upscaler menu | Re-apply onto `rt_vidsdl.c` + `menu.c` exactly as on `modernise-2026` (cvar names/values unchanged) |

Upstream 1.36 features that interact with RT and need decisions during the port:
- **MD5 models**: Stage 5 (optional). Until then `rt_alias.c` draws the classic `.mdl` path only;
  `Mod_LoadMD5*` stays vanilla-only (`#if !RT_RENDERER` at the load-selection site) so mg3 uses
  `.mdl` fallbacks under RT.
- **Steam achievements / rich presence**: engine-side only (`steam.c`, `cl_parse.c`) — no renderer
  contact, works under RT unchanged.
- **New upstream renderer features** (WBOIT/MBOIT, palettes, WAD3, dynamic shadows): vanilla-only;
  RTGL1 has its own equivalents or ignores them.

## Order of attack (Stage 1 → 3)

1. vcxproj: add `RT-Release`/`RT-Debug` configs (define `RT_RENDERER=1`, RTGL1 include/lib,
   exclude vanilla renderer files, include `rt_*.c`, post-build DLL copies).  ⟵ in progress
2. Seed `rt_*.c` from `modernise-2026`, headers get `#if` sections; iterate on compile errors —
   the error list *is* the task list (missing 1.36 APIs, changed types, tasks/threading changes).
3. Boot: RTGL1 instance + 2D (console/menu) via RTGL1 rasterizer = **Stage 1 checkpoint**.
4. World: brush geometry, textures/materials, lightmapped→RT lights, sky, water, portals = **Stage 2**.
5. Dynamic: alias models, sprites, particles, viewmodel, HUD, dlights, `rt_*` cvars/menu = **Stage 3**.

## Status

- [x] Stage 0: vanilla 1.36.0 builds (v143), SDL 3.4.12, Steam API up, achievement fired (Brett, 31 Aug)
- [x] steam.c multi-library fix (`e0d31a7`)
- [x] Submodule + patches + ovrd assets on `rt-1.36`
- [ ] RT build configurations in vcxproj
- [ ] `rt_*.c` seeded and compiling
- [ ] Stage 1 checkpoint: boots to console/menu under RTGL1
- [ ] Stage 2 world, Stage 3 dynamic, Stage 4 ship, Stage 5 MD5 (optional)

`scripts/build.ps1` still targets the `modernise-2026` layout; adapt after the vcxproj configs exist
(`-p:PlatformToolset=v143`, embedded shader project needs `glslangValidator` from the Vulkan SDK).
