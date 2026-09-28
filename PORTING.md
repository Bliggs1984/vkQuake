# Tier 2 port: RayTracedGL1 on vkQuake 1.36.0 (`rt-1.36`)

Working notes for re-attaching the RT renderer (2022 fork, base 1.20.3) to vkQuake 1.36.0.
Plan: tier 2 of the 2026 modernisation assessment (rebase onto vkQuake 1.36.0), approved 2026-08-31.
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
- **MD5 models**: done (Stage 5). `GLMesh_UploadBuffers` keeps a CPU copy of each MD5 surface in
  `hdr->rtskinned`; `RT_SkinAliasSurface` (rt_gl_mesh.c) skins it per frame with md5.vert's maths and
  rt_r_alias.c streams it like a `.mdl` pose. MD3 replacements stay off under RT (`load_enhanced_md3`).
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

- [x] Stage 0: vanilla 1.36.0 builds (v143), SDL 3.4.12, Steam API up, achievement fired (31 Aug)
- [x] steam.c multi-library fix (`e0d31a7`)
- [x] Submodule + patches + ovrd assets on `rt-1.36`
- [x] RT-Release build configuration in vcxproj + sln (RT_RENDERER, RTGL1 lib, no PCH, linker .map). No RT-Debug yet.
- [x] All 16 `rt_*.c` compile clean under /WX; shared files guarded (`#ifdef RT_RENDERER`, vanilla Release still builds 0 errors)
- [x] **RT-Release links and BOOTS (2026-09-04)**: RTGL1 instance up (DLSS + FSR2 detected), console/menu, demo1
  plays with ray-traced world, lightmapped->RT lights, HUD, viewmodel.
  = **Stage 1 checkpoint reached; Stage 2/3 visibly working on first boot** (10-minute play test passed).
- [x] Stage 2/3 (2026-09-05): fork's behavioural RT changes re-applied to 17 shared files under RT_RENDERER
  (`git diff 1.20.3 modernise-2026 -- Quake/<file>` was the worklist; upstream remote + tag 1.20.3 in the repo);
  rt_*.c audited against 1.36 semantics (24 fixes, see commit 867d8679). hipnotic/rogue/mg1 render from the
  Steam re-release data; id1 demo loop runs. Stage 3 items verified in game on 2026-09-28 (menus, HUD detail, muzzle flash, co-op, F switch).
- [~] Stage 4: UPGRADING.md written; Steam API verified loading in the RT build (steam_api64 + steamclient64
  in-process when basedir is under the Steam install); RT config moved to `%APPDATA%\QuakeRT`; zip built.
  2026-09-28: an installed build loads steam_api64, steamclient64 and Steam's overlay Vulkan
  layer in-process; achievements use vanilla 1.36's unchanged svc_achievement path (fired in Stage 0).
- [x] DLSS 4.5 default (2026-09-28): fresh config starts on DLSS Quality, preset M; FSR 2 fallback without DLSS;
  `Upscaler: ...` console line. Verified nvngx_dlss.dll loaded and the log line on the RTX 5080.
- [x] Stage 5 (2026-09-28): MD5 enhanced models under RT (re-release id1: 59 models, mg3: 5). Verified soldier,
  dog, player (chase cam), v_rock in e1m1 and mg3 map1. Also fixed skybox faces uploaded with vertexCount 0
  (RG_WRONG_ARGUMENT abort on skybox maps). Sweep OK: id1 start/e1m1/e2m1/e4m1, hip1m1, r1m1, dopa e5m1,
  mg3 start/map1.

### Runtime bugs fixed on first boot (all "compiles but wrong" 1.36 struct/API changes)

- `Draw_SubPic` dereferenced `rgb` (1.36 passes NULL for white) -> crash on the loading logo.
- `Draw_TryCachePic` parsed files as raw .lmp; 1.36 loads `gfx/crosshair-000.png` through it -> garbage
  1196314761x169478669 texture. Now uses `Image_LoadImage` like 1.36 (any format).
- `qmodel_t.extradata` is an array in 1.36 (`extradata[PV_SIZE]`); rt_r_sprite.c cast it directly. Use
  `Mod_Extradata (mod)` everywhere (only sprite code was affected; rt_gl_mesh.c indexes `[PV_QUAKE1]` on purpose).
- `_rt_firsttime` must be in the early `CFG_ReadCvars` list or the 1440p prompt blocks every start.

### Crash triage recipe (no debugger installed)

Windows Event Log gives module + offset; RT-Release now writes `Build-vkQuake\x64\RT-Release\vkQuake.map`.
A local smoke script launched the build, screenshotted the game window and dumped the log.
Symbolize: find the largest map address <= 0x140000000 + offset (see the PowerShell one-liner in the session log).

### Known oddities

- `-condebug` log (`%APPDATA%\vkQuake\qconsole.log`) stopped appearing after the first runs; not investigated.
- `+cl_startdemos 0` on the command line does not stop the demo loop (quake.rc runs `startdemos` after stuffcmds).

### Alias path (done, 2026-09-04)

- `rt_gl_mesh.c`: `GL_MakeAliasModelDisplayLists (m, hdr)` is self-contained: dedups `triangles`/`stverts`
  into `numverts_vbo`/`numindexes`, then fills `m->rtvertices` (numposes x numverts_vbo `RgVertex`, index
  = pose*numverts_vbo + v) and `m->rtindices` (reversed winding) straight from `poseverts[]`, which is only
  valid inside `Mod_LoadAliasModel`. 1.36 entry points provided: `GLMesh_UploadBuffers` (MD5 CPU copy; MD3 no-op),
  `GLMesh_DeleteMeshBuffers (aliashdr_t*)` (finds the owning qmodel via `extradata[PV_QUAKE1]`),
  `GLMesh_DeleteAllMeshBuffers`.
- `rt_r_alias.c`: 1.36's `R_SetupAliasFrame` / `R_EntityPoseAt` / `R_GetEntityLerpedTransform` (entlerp_t)
  copied in; RT draw path (`GetPoseVertices` -> `rgUploadGeometry`) unchanged. TODO(rt): `netstate.scale`
  (3-arg `R_RotateForEntity`), `r_lerpturn` is a file-local cvar until rt_gl_rmain.c registers it,
  1.36 spotlight/KEX dlight lighting not applied (RTGL1 lights models itself).

### Build/packaging

- `scripts/build.ps1` now builds `RT-<Configuration>` with `-p:PlatformToolset=v143`, reads
  `QUAKERT_VERSION` from `quakever.h` (2.0.0-dev; RT banner "QuakeRT x (vkQuake 1.36.0)"), copies only
  THIRD_PARTY_NOTICES.txt + vkQuakeFullscreen.bat from Packaging/Windows.
- Local diagnostic scripts (not in the repo): `rtcompile.ps1` (rt_*.c, RT defines),
  `vkcompile.ps1` (shared files, vanilla defines), `rtsweep.ps1` (shared files in the RT-Release set, RT
  defines), `rtset.py` (RT-Release inclusion list from the vcxproj).

### Data-set smoke tests under RT (2026-09-04, Steam re-release data)

| Data | Map | Result |
|---|---|---|
| id1 (smoke-id1) | demo1 | renders: world, RT lights, HUD, viewmodel |
| hipnotic | hip1m1 | renders (skylight, metal walls, grunt) |
| rogue | r1m1 | renders |
| mg1 (Dimension of the Machine) | mge1m1 | renders after ~40 s load |
| mg3 (Dawn of the Machine) | map1, start | renders with MD5 models (after the skybox fix, 2026-09-28) |

Shared `%APPDATA%\vkQuake\vkQuake.cfg` is written by whichever build ran last; the RT build logs
"Unknown command" for vanilla-only cvars (`r_alphasort`, `r_quadparticles`, `r_rtshadows`) — harmless,
could be registered as inert cvars under RT to keep the console quiet.
