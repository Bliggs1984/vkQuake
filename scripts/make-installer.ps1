# Builds Dist\QuakeRT-Setup-<version>.exe (Inno Setup 6) from an already packaged Dist\QuakeRT folder.
# Run scripts\build.ps1 first (or package the RT-Release output into Dist\QuakeRT yourself).
#   winget install JRSoftware.InnoSetup      (one-off)
#   .\scripts\make-installer.ps1
param(
    [string]$Iscc = '',
    [string]$Redist = ''
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$version = (Select-String -Path (Join-Path $Root 'Quake\quakever.h') -Pattern '#define\s+QUAKERT_VERSION\s+"([^"]+)"').Matches[0].Groups[1].Value
$source = Join-Path $Root 'Dist\QuakeRT'
if (-not (Test-Path (Join-Path $source 'vkQuake.exe'))) { throw "No packaged build in $source - run scripts\build.ps1 first" }

if (-not $Iscc) {
    $Iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe") |
        Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Iscc) { throw 'Inno Setup 6 not found (winget install JRSoftware.InnoSetup)' }

# Microsoft's redistributable from the Visual Studio install that built the binaries (same toolset version)
if (-not $Redist) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = & $vswhere -latest -products * -property installationPath
    $Redist = Get-ChildItem (Join-Path $vs 'VC\Redist\MSVC') -Recurse -Filter vc_redist.x64.exe | Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $Redist -or -not (Test-Path $Redist)) { throw 'vc_redist.x64.exe not found; pass -Redist' }

& $Iscc /Q "/DAppVersion=$version" "/DSourceDir=$source" "/DRedist=$Redist" (Join-Path $Root 'Packaging\Windows\QuakeRT.iss')
if ($LASTEXITCODE -ne 0) { throw "ISCC failed ($LASTEXITCODE)" }
Get-Item (Join-Path $Root "Dist\QuakeRT-Setup-$version.exe")
