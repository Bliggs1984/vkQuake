# Quake RT

Quake with real-time path-traced lighting and NVIDIA DLSS 4.5, on a modern engine.

Quake RT adds a path-tracing renderer ([RayTracedGL1](https://github.com/sultim-t/RayTracedGL1)) to
[vkQuake](https://github.com/Novum/vkQuake) 1.36. It continues
[sultim-t/vkquake-rt](https://github.com/sultim-t/vkquake-rt) (the 2022 "Quake: Ray Traced" by Sultim Tsyrendashiev),
moved from vkQuake 1.20.3 to 1.36.0.

> **Made with AI.** This 2026 update (Quake RT 1.1 and 2.0) was adapted using **[Claude](https://claude.ai)**,
> Anthropic's AI model, through Claude Code, by someone who doesn't code. Bliggs1984 described what to build
> and play-tested each step. Claude wrote the code: the port to vkQuake 1.36, the DLSS 4.5 integration,
> the MD5 model support, the fixes and the installer.
> Commits written this way carry a `Co-Authored-By: Claude` line.

## Download and play

**[Download the installer from Releases](https://github.com/Bliggs1984/vkquake-rt/releases/latest)**:
`QuakeRT-Setup-<version>.exe`. Double-click it, then use the "Quake RT" icon on your desktop.
[HOW-TO-PLAY.txt](Packaging/Windows/HOW-TO-PLAY.txt) is the plain-English guide the installer shows.

You need:

* Windows 10 or 11 (64-bit)
* An NVIDIA RTX graphics card (RTX 2060 or newer). Recent AMD cards (RX 6000+) should work via FSR 2.
* Your own copy of Quake: Steam, GOG or Epic. It is found automatically.

Windows may say "Windows protected your PC" because the installer isn't code-signed. Click
**More info → Run anyway**.

## What's in 2.0

* vkQuake 1.36.0 base with SDL3, Steam achievements and rich presence, and the Remastered/Original chooser.
  The 2021 re-release and its add-ons (Scourge of Armagon, Dissolution of Eternity, Dimension of the Past,
  Dimension of the Machine, Dawn of the Machine) load with no extra setup.
* DLSS 4.5 (Super Resolution, preset M) on by default at Quality. DLAA and presets K/L/M are in Options → Video.
  AMD FSR 2 is used automatically on GPUs without DLSS.
* The re-release's enhanced MD5 models (monsters, weapons, items, player) are path-traced too.
* Starts fullscreen. Press `F` in game to flip between ray-traced and classic rendering.

[UPGRADING.md](UPGRADING.md) has the details, the console variables and the known gaps.
[PORTING.md](PORTING.md) covers how the renderer was moved to 1.36.

## Building (Windows)

Prerequisites: Git, Visual Studio 2022 or Build Tools 2022 (C++ workload), CMake ≥ 3.21, Ninja,
[Vulkan SDK](https://vulkan.lunarg.com/) 1.3+, Python 3.
(`winget install Kitware.CMake Ninja-build.Ninja KhronosGroup.VulkanSDK Microsoft.VisualStudio.2022.BuildTools`)

```powershell
git clone --recurse-submodules -b rt-1.36 https://github.com/Bliggs1984/vkquake-rt.git
cd vkquake-rt
./scripts/build.ps1            # RT-Release build + Dist/QuakeRT/ and Dist/quake-rt-<version>-win64.zip
./scripts/make-installer.ps1   # Dist/QuakeRT-Setup-<version>.exe (needs: winget install JRSoftware.InnoSetup)
```

`build.ps1` builds RayTracedGL1 (submodule plus `Patches/RTGL1`), compiles its shaders, fetches the
[NVIDIA DLSS SDK](https://github.com/NVIDIA/DLSS) and builds `vkQuake.exe` in the `RT-Release` configuration.
The `Release` configuration still builds stock vkQuake. The ray tracer is a compile-time switch (`RT_RENDERER`).

Branches: `rt-1.36` is Quake RT 2.x on vkQuake 1.36. `modernise-2026` is the 1.1.0 update of the original
fork (vkQuake 1.20.3, DLSS 4.5). `master` is the original 2022 code.

## Credits and licence

* [vkQuake](https://github.com/Novum/vkQuake): Axel Gneiting and contributors (see [README-vkQuake.md](README-vkQuake.md)),
  based on QuakeSpasm, QuakeSpasm-Spiked, FitzQuake and id Software's Quake.
* [vkquake-rt](https://github.com/sultim-t/vkquake-rt) and [RayTracedGL1](https://github.com/sultim-t/RayTracedGL1):
  Sultim Tsyrendashiev.
* 2026 update (vkQuake 1.36 port, DLSS 4.5, MD5 models, installer): adapted using Claude (Anthropic) via Claude Code,
  directed and play-tested by Bliggs1984, who doesn't code.
* NVIDIA DLSS: NVIDIA Corporation. AMD FSR 2: AMD. Third-party notices are in
  [Packaging/Windows/THIRD_PARTY_NOTICES.txt](Packaging/Windows/THIRD_PARTY_NOTICES.txt).

GPL-2.0 (see [LICENSE.txt](LICENSE.txt)). Quake game data is not included. This is a fan project,
not affiliated with id Software, Bethesda, NVIDIA or AMD.
