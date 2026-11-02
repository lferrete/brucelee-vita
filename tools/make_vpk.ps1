#requires -Version 5.1
<#
  Builds brucelee.vpk from the SELF produced by CMake.

  CMake's vita_create_vpk macro escapes spaces in the asset paths for MSYS
  make; this script invokes vita-pack-vpk with native Windows argument quoting.
#>
# Port code Copyright (C) 2026 Leandro Ferrete.
param(
    [string]$BuildDir = "",
    [string]$OutFile = ""
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$vitasdk = if ($env:VITASDK) { $env:VITASDK } else { "C:\vitasdk" }
if (-not $BuildDir) { $BuildDir = Join-Path $projectRoot "build" }
if (-not $OutFile)  { $OutFile  = Join-Path $BuildDir "brucelee.vpk" }

$eboot = Join-Path $BuildDir "eboot.bin"
if (-not (Test-Path -LiteralPath $eboot)) {
    throw "eboot.bin not found in $BuildDir. Build the target first."
}

$sfo = Join-Path $BuildDir "param.sfo"
& (Join-Path $vitasdk "bin\vita-mksfoex.exe") `
    -d "ATTRIBUTE2=12" `
    -s "APP_VER=01.00" `
    -s "TITLE_ID=BRUCELEE1" `
    -s "PORT_AUTHOR=Leandro Ferrete" `
    -s "PORT_VERSION=1.0" `
    "Bruce Lee: Dragon Warrior" `
    $sfo

$assets = Join-Path $projectRoot "extras\livearea"
$provenance = Join-Path $projectRoot "extras\provenance.txt"
$loading = Join-Path $projectRoot "extras\loading.rgb565"
& (Join-Path $vitasdk "bin\vita-pack-vpk.exe") `
    -a "$assets\icon0.png=sce_sys/icon0.png" `
    -a "$assets\pic0.png=sce_sys/pic0.png" `
    -a "$assets\startup.png=sce_sys/livearea/contents/startup.png" `
    -a "$assets\bg0.png=sce_sys/livearea/contents/bg0.png" `
    -a "$assets\template.xml=sce_sys/livearea/contents/template.xml" `
    -a "$provenance=sce_sys/provenance.txt" `
    -a "$loading=loading.rgb565" `
    -s $sfo `
    -b $eboot `
    $OutFile

if (-not (Test-Path -LiteralPath $OutFile)) {
    throw "vita-pack-vpk did not create $OutFile"
}

# Release fingerprint + author attribution next to the VPK.
$hash = (Get-FileHash -LiteralPath $OutFile -Algorithm SHA256).Hash
$sums = Join-Path $BuildDir "SHA256SUMS.txt"
@(
    "# Bruce Lee: Dragon Warrior - PS Vita port by Leandro Ferrete",
    ("{0}  {1}" -f $hash, (Split-Path -Leaf $OutFile))
) | Set-Content -LiteralPath $sums -Encoding ASCII

Write-Host ("VPK: {0} ({1:N0} bytes)" -f $OutFile, (Get-Item -LiteralPath $OutFile).Length) -ForegroundColor Green
Write-Host ("SHA256: {0}" -f $hash) -ForegroundColor Green
Write-Host ("Sums: {0}" -f $sums) -ForegroundColor Green
