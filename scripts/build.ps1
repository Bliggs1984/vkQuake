<#
.SYNOPSIS
  One-shot build of Quake: Ray Traced on Windows (RayTracedGL1 + shaders + vkQuake + packaged dist folder).

.DESCRIPTION
  Steps:
    1. init the RayTracedGL1 submodule and apply Patches/RTGL1/*.patch (idempotent)
    2. locate MSVC (vswhere) and the Vulkan SDK, fetch the NVIDIA DLSS SDK if needed
    3. cmake --preset x64-Release  ->  RayTracedGL1/Build/x64-Release/RayTracedGL1.dll
    4. compile RTGL1 shaders (GenerateShaders.py -> RayTracedGL1/Build/*.spv)
    5. msbuild Windows/VisualStudio/vkquake.sln (RT-Release|x64 = vkQuake 1.36 + RT_RENDERER; vanilla Release stays untouched)
    6. assemble Dist/QuakeRT/ (exe, DLLs, nvngx_dlss.dll, ovrd/) and zip it

.PARAMETER Configuration   Release (default) or Debug -> builds the RT-Release / RT-Debug vkQuake configurations
.PARAMETER PlatformToolset MSVC toolset for msbuild (default v143; the 1.36 vcxproj says v145 but v143 builds it fine)
.PARAMETER DlssSdkPath     path to a clone of https://github.com/NVIDIA/DLSS (default: $env:DLSS_SDK_PATH, else Build/DLSS-SDK is cloned)
.PARAMETER NoDlss          build RTGL1 without DLSS support
.PARAMETER SkipPackage     stop after compiling
#>
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')] [string] $Configuration = 'Release',
    [string] $PlatformToolset = 'v143',
    [string] $DlssSdkPath = $env:DLSS_SDK_PATH,
    [string] $DlssSdkTag = 'v310.7.0',
    [switch] $NoDlss,
    [switch] $SkipPackage
)

$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Rtgl1 = Join-Path $Root 'RayTracedGL1'
$BuildDir = Join-Path $Root 'Build'
New-Item -ItemType Directory -Force $BuildDir | Out-Null

function Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }

# ---------------------------------------------------------------- 1. submodule + patches
Step 'RayTracedGL1 submodule'
if (-not (Test-Path (Join-Path $Rtgl1 'CMakeLists.txt'))) {
    git -C $Root submodule update --init --recursive RayTracedGL1
    if ($LASTEXITCODE) { throw 'git submodule update failed' }
}
Get-ChildItem (Join-Path $Root 'Patches\RTGL1') -Filter '*.patch' | Sort-Object Name | ForEach-Object {
    git -C $Rtgl1 apply --check --reverse $_.FullName 2>$null
    if ($LASTEXITCODE -eq 0) { Write-Host "  already applied: $($_.Name)"; return }
    git -C $Rtgl1 apply --whitespace=nowarn $_.FullName
    if ($LASTEXITCODE) { throw "failed to apply $($_.Name)" }
    Write-Host "  applied: $($_.Name)"
}

# ---------------------------------------------------------------- 2. toolchain
Step 'Toolchain'
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw 'vswhere.exe not found - install Visual Studio 2022 or Build Tools 2022 with the C++ workload' }
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw 'No Visual Studio with MSVC x64 tools found' }
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
Write-Host "  MSVC: $vsPath"

# import the vcvars64 environment into this process
$envDump = cmd /c "`"$vcvars`" >nul 2>&1 && set"
foreach ($line in $envDump) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}

if (-not $env:VULKAN_SDK -or -not (Test-Path $env:VULKAN_SDK)) {
    $sdk = Get-ChildItem 'C:\VulkanSDK' -Directory -ErrorAction SilentlyContinue | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
    if (-not $sdk) { throw 'Vulkan SDK not found - set VULKAN_SDK or install from https://vulkan.lunarg.com' }
    $env:VULKAN_SDK = $sdk.FullName
}
$env:PATH = "$env:VULKAN_SDK\Bin;$env:LOCALAPPDATA\Microsoft\WinGet\Links;$env:PATH"
Write-Host "  Vulkan SDK: $env:VULKAN_SDK"
foreach ($tool in 'cmake', 'ninja', 'glslc', 'msbuild') {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "$tool not found on PATH" }
}
$python = (Get-Command python -ErrorAction SilentlyContinue)
if (-not $python -or (& python --version 2>&1) -notmatch '^Python 3') { $python = Get-Command py -ErrorAction SilentlyContinue }
if (-not $python) { throw 'Python 3 not found (needed for shader generation)' }

if (-not $NoDlss) {
    if (-not $DlssSdkPath) { $DlssSdkPath = Join-Path $BuildDir 'DLSS-SDK' }
    if (-not (Test-Path (Join-Path $DlssSdkPath 'include\nvsdk_ngx_vk.h'))) {
        Step "Fetching NVIDIA DLSS SDK $DlssSdkTag"
        git clone --depth 1 -b $DlssSdkTag https://github.com/NVIDIA/DLSS.git $DlssSdkPath
        if ($LASTEXITCODE) { throw 'DLSS SDK clone failed' }
    }
    $env:DLSS_SDK_PATH = (Resolve-Path $DlssSdkPath).Path
    Write-Host "  DLSS SDK: $env:DLSS_SDK_PATH"
}

# ---------------------------------------------------------------- 3. RayTracedGL1
$preset = if ($Configuration -eq 'Debug') { 'x64-Debug' } elseif ($NoDlss) { 'x64-Release-NoDLSS' } else { 'x64-Release' }
Step "RayTracedGL1 ($preset)"
Push-Location $Rtgl1
try {
    cmake --preset $preset
    if ($LASTEXITCODE) { throw 'cmake configure failed' }
    cmake --build --preset $preset
    if ($LASTEXITCODE) { throw 'RayTracedGL1 build failed' }
    # the vcxproj looks in Build/x64-Release | Build/x64-Debug
    if ($preset -eq 'x64-Release-NoDLSS') {
        New-Item -ItemType Directory -Force (Join-Path $Rtgl1 'Build\x64-Release') | Out-Null
        Copy-Item (Join-Path $Rtgl1 'Build\x64-Release-NoDLSS\RayTracedGL1.*') (Join-Path $Rtgl1 'Build\x64-Release') -Force
    }

    # ------------------------------------------------------------ 4. shaders
    Step 'RTGL1 shaders'
    Push-Location (Join-Path $Rtgl1 'Source\Shaders')
    try {
        & $python.Source GenerateShaders.py -r
        if ($LASTEXITCODE) { throw 'shader generation failed' }
    } finally { Pop-Location }
    $spv = Get-ChildItem (Join-Path $Rtgl1 'Build') -Filter '*.spv'
    if ($spv.Count -lt 40) { throw "expected ~50 .spv files in RayTracedGL1/Build, found $($spv.Count)" }
    Write-Host "  $($spv.Count) shaders"
} finally { Pop-Location }

# ---------------------------------------------------------------- 5. vkQuake
$vkConfig = "RT-$Configuration"
Step "vkQuake ($vkConfig|x64, $PlatformToolset)"
$env:RTGL1_SDK_PATH = $Rtgl1
msbuild (Join-Path $Root 'Windows\VisualStudio\vkquake.sln') -m -v:minimal -nologo "-p:Configuration=$vkConfig" '-p:Platform=x64' "-p:PlatformToolset=$PlatformToolset"
if ($LASTEXITCODE) { throw 'vkQuake build failed' }
$outDir = Join-Path $Root "Windows\VisualStudio\Build-vkQuake\x64\$vkConfig"
if (-not (Test-Path (Join-Path $outDir 'vkQuake.exe'))) { throw "vkQuake.exe not found in $outDir" }

if ($SkipPackage) { Write-Host "`nBuild finished: $outDir" -ForegroundColor Green; return }

# ---------------------------------------------------------------- 6. package
$version = (Select-String -Path (Join-Path $Root 'Quake\quakever.h') -Pattern '#define\s+QUAKERT_VERSION\s+"([^"]+)"').Matches[0].Groups[1].Value
$dist = Join-Path $Root 'Dist\QuakeRT'
Step "Packaging $dist (version $version)"
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Force "$dist\ovrd\shaders" | Out-Null

Copy-Item (Join-Path $outDir 'vkQuake.exe') $dist
Copy-Item (Join-Path $outDir '*.dll') $dist
if (-not $NoDlss) { Copy-Item (Join-Path $env:DLSS_SDK_PATH 'lib\Windows_x86_64\rel\nvngx_dlss.dll') $dist }
Copy-Item (Join-Path $Root 'Misc\ovrd\*') "$dist\ovrd" -Recurse -Force
Copy-Item (Join-Path $Rtgl1 'Tools\BlueNoise_LDR_RGBA_128.ktx2') "$dist\ovrd"
Copy-Item (Join-Path $Rtgl1 'Build\*.spv') "$dist\ovrd\shaders"
Copy-Item (Join-Path $Root 'Packaging\Windows\THIRD_PARTY_NOTICES.txt') $dist
Copy-Item (Join-Path $Root 'Packaging\Windows\HOW-TO-PLAY.txt') $dist
Copy-Item (Join-Path $Root 'LICENSE.txt') $dist

$zip = Join-Path $Root "Dist\quake-rt-$version-win64.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path "$dist\*" -DestinationPath $zip
Write-Host "`nDone: $zip" -ForegroundColor Green
