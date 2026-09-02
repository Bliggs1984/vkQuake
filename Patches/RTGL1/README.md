# RayTracedGL1 patches

`RayTracedGL1/` is a git submodule pinned to the upstream `quake` branch of
<https://github.com/sultim-t/RayTracedGL1> (last upstream commit `9efa82d`, Feb 2023).

The `.patch` files in this directory are applied on top of that commit by
`scripts/build.ps1` (idempotent: already-applied patches are detected and skipped).
They exist so the engine can be built from a plain clone without depending on a
personal fork of RTGL1. If you maintain a fork, push the patched tree there, point
`.gitmodules` at it, and delete the patches.

| Patch | What it does |
|---|---|
| `0001-modernise-2026.patch` | DLSS SDK 310.x (DLSS 4 / 4.5) API compatibility (`Init_with_ProjectID` loader args, `Shutdown1`, renamed logging struct, deprecated sharpening removed); DLSS render-preset selection (`RgRenderDlssPreset`: auto / K / L / M); native-resolution AA mode (`RG_RENDER_RESOLUTION_MODE_DLAA`, also FSR2 Native AA); prefer a discrete GPU when several ray-tracing capable devices are present; `CMakePresets.json`; accept `DLSS_SDK_PATH` as a CMake variable and the new `lib/Windows_x86_64/x64` import-library layout; shaders build with glslang ≥ 14 (no opaque return types) and `CmPrepareFinal.comp` includes `LPM/ffx_a.h` explicitly — the shader script's `-I` order came from a Python `set`, so the older `CAS/ffx_a.h` (without `opAAddOne*`) could shadow it at random. |

Regenerate after editing the submodule working tree:

```powershell
git -C RayTracedGL1 diff > Patches/RTGL1/0001-modernise-2026.patch
```
