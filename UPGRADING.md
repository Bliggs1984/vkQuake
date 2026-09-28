# QuakeRT 2.0.0-dev — upgrading from Quake RT 1.x

QuakeRT 2.0.0 is the RayTracedGL1 path tracer re-attached to **vkQuake 1.36.0** (the previous releases,
1.0.x / 1.1.0, were built on vkQuake 1.20.3 from 2022). Everything the engine gained in the meantime is now
available under ray tracing: SDL3, Steam achievements and rich presence, the Remastered/Original data chooser,
automatic Steam/GOG/Epic install detection, and the 2021 re-release's expansions (Dimension of the Past,
Dimension of the Machine) loading without extra setup.

## Requirements

- Windows 10/11 x64, an NVIDIA RTX GPU (DLSS 4.5 is bundled; AMD FSR 2 is available on other Vulkan RT GPUs).
- Quake from Steam (2021 re-release or the original). GOG/Epic installs are detected too.
- Nothing else to install: the zip contains `vkQuake.exe`, `RayTracedGL1.dll`, `nvngx_dlss.dll`, the codec
  DLLs, SDL3, and the `ovrd/` folder with the ray-tracing materials and shaders.

## Running it

1. Unzip anywhere (for example `C:\Games\QuakeRT`). Do **not** copy the files into Steam's Quake folder.
2. Start `vkQuake.exe`. It finds your Quake installation by itself. If both the re-release and original data
   are present you are asked which to play; `-prefremaster` on the command line skips the question.
3. On displays taller than 1440p a one-time prompt offers to render at 1440p for better frame rates.

Config, saves and the console log now live in `%APPDATA%\QuakeRT\` (vanilla vkQuake 1.36 uses
`%APPDATA%\vkQuake\`, so the two builds keep separate settings), not next to the executable. Add `-condebug`
to write `qconsole.log` there.

### Steam achievements

The engine loads Steam's API whenever it is playing data from a Steam install and Steam is running, so
achievements unlock from a direct launch. To also get the overlay, playtime tracking and the "Now playing"
status, launch it through Steam instead: Steam library → Quake → Properties → Launch Options:

    "C:\Games\QuakeRT\vkQuake.exe" %command%

## Ray-tracing settings

All `rt_*` console variables keep the names and values from 1.1.0. Upscaling is chosen with `rt_upscale_dlss`
or `rt_upscale_fsr2` (0 off, 1 native AA / DLAA, 2 quality, 3 balanced, 4 performance, 5 ultra performance,
6 custom via `rt_renderscale`) and the DLSS model with `rt_dlss_preset` (0 auto, 1 DLSS 4 transformer K,
2 DLSS 4.5 transformer L, 3 DLSS 4.5 transformer M). The Video options menu is the ray-traced one from 1.1.0.

New in 2.0: a fresh install starts on DLSS Quality with the DLSS 4.5 model (preset M). On a GPU without DLSS
the same quality level moves to FSR 2 automatically. The console log names the upscaler in use
(`Upscaler: NVIDIA DLSS, Quality, preset DLSS 4.5 (M)`).

## What changed for players coming from 1.1.0

- Base engine 1.20.3 → 1.36.0. Expect vanilla 1.36 behaviour for everything that is not rendering:
  menus, input, netcode, save games, mod support (BSP2, FTE protocol 999, QSS extensions).
- The re-release's enhanced models (skeletal MD5: all monsters, weapons, items and the player, plus Dawn of the
  Machine's own) are now drawn under ray tracing. 1.1.0 only had the classic `.mdl` versions. Graphics options →
  Models (`r_enhancedmodels 0`) switches back to the classic models.
- The 1.1.0 command-line helpers that copied Steam data or music into a local folder are gone: 1.36 reads the
  Steam install directly.
- Windowed/fullscreen handling is SDL3's. `vid_fullscreen 2` (exclusive fullscreen) behaves like `1`.

## Known gaps in this build

- Vanilla-only graphics menu rows (anti-aliasing, render scale, anisotropy, shadows) are hidden under RT.
- Fog volumes from map scripts are not forwarded to the path tracer.
- MD3 replacement models (some community mods) are not drawn under ray tracing; the `.mdl` is used instead.
- The `screenshot` console command is not implemented under ray tracing; use Steam's screenshot key (F12)
  when launched through Steam.

## Building from source

`scripts\build.ps1` builds RayTracedGL1 (CMake), its shaders, then vkQuake in the `RT-Release`
configuration with the VS 2022 v143 toolset, and packages `Dist\quake-rt-<version>-win64.zip`. The vanilla
`Release` configuration still builds the stock vkQuake renderer and is kept as a regression guard. See
`PORTING.md` for the architecture of the `RT_RENDERER` compile-time switch.
