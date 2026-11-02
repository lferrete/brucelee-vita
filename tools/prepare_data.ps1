#requires -Version 5.1
<#
  Bruce Lee: Dragon Warrior - PS Vita data preparation

  Extracts the two native libraries and the game package from the legally
  obtained Android APK, then lays them out under .\data\ ready to be copied to
  ux0:data/brucelee/ on the Vita.

  Usage:
    powershell -ExecutionPolicy Bypass -File tools\prepare_data.ps1
    powershell -ExecutionPolicy Bypass -File tools\prepare_data.ps1 -Apk path\to\game.apk -Zpak path\to\data.zpak
#>
param(
    [string]$Apk = "",
    [string]$Zpak = "",
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression.FileSystem

$projectRoot = Split-Path -Parent $PSScriptRoot
$root = Split-Path -Parent $projectRoot
if (-not $OutDir) { $OutDir = Join-Path $projectRoot "data" }

if (-not $Apk) {
    $apkFile = Get-ChildItem -LiteralPath $root -Filter "*.apk" -File | Select-Object -First 1
    if (-not $apkFile) { throw "APK not found. Pass -Apk explicitly." }
    $Apk = $apkFile.FullName
}
if (-not $Zpak) {
    $candidates = @(
        (Join-Path $root "data.zpak"),
        (Join-Path $root "10938-cache-Bruce-Lee-Dragon-Warrior-v1.15.26\com.digitallegends.brucelee\files\content\data.zpak")
    )
    $found = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $found) {
        $found = Get-ChildItem -LiteralPath $root -Recurse -Filter "data.zpak" -File -ErrorAction SilentlyContinue |
                 Select-Object -First 1 -ExpandProperty FullName
    }
    if (-not $found) { throw "data.zpak not found. Pass -Zpak explicitly." }
    $Zpak = $found
}

Write-Host "APK:  $Apk"
Write-Host "Zpak: $Zpak"
Write-Host "Out:  $OutDir"

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
foreach ($d in @(
    "lib\armeabi", "assets", "content", "logs", "shader_cache",
    "files", "cache", "sdcard", "game", "ext0", "fonts", "save", "content0"
)) {
    New-Item -ItemType Directory -Force -Path (Join-Path $OutDir $d) | Out-Null
}

# --- APK: native libraries and bootstrap zpak -----------------------------
$zip = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $Apk))
try {
    $map = @{
        "lib/armeabi/libdefault.so"        = "lib\armeabi\libdefault.so"
        "lib/armeabi/libzplatform.so"      = "lib\armeabi\libzplatform.so"
        "lib/armeabi/libjava-activity.so"  = "lib\armeabi\libjava-activity.so"
        "assets/BruceLee.zpak"             = "assets\BruceLee.zpak"
    }
    foreach ($entryName in $map.Keys) {
        $entry = $zip.GetEntry($entryName)
        if (-not $entry) { Write-Warning "APK entry missing: $entryName"; continue }
        $dst = Join-Path $OutDir $map[$entryName]
        [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $dst, $true)
        Write-Host ("  {0} -> {1} ({2:N0} bytes)" -f $entryName, $map[$entryName], $entry.Length)
    }

    # The game's own icon is the source for the LiveArea artwork.
    $iconEntry = $zip.GetEntry("res/drawable/icon.jpg")
    if ($iconEntry) {
        $iconDst = Join-Path $projectRoot "extras\livearea\source_icon.jpg"
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $iconDst) | Out-Null
        [System.IO.Compression.ZipFileExtensions]::ExtractToFile($iconEntry, $iconDst, $true)
        Write-Host "  res/drawable/icon.jpg -> extras\livearea\source_icon.jpg"
    }
} finally {
    $zip.Dispose()
}

# --- data.zpak: Core.pakh / Core.pakd000 ----------------------------------
$zip = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $Zpak))
try {
    foreach ($entry in $zip.Entries) {
        if ($entry.FullName -eq "Core.pakh" -or $entry.FullName -eq "Core.pakd000") {
            $dst = Join-Path $OutDir ("content\" + $entry.FullName)
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $dst, $true)
            Write-Host ("  {0} -> content\{0} ({1:N0} bytes)" -f $entry.FullName, $entry.Length)
        } elseif ($entry.FullName -eq "metadata.xml") {
            # This is the *content container* metadata; keep it next to the pak.
            $dst = Join-Path $OutDir "content\metadata.xml"
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $dst, $true)
            Write-Host "  metadata.xml -> content\metadata.xml"
        }
    }
} finally {
    $zip.Dispose()
}

# --- application metadata (inside assets/BruceLee.zpak) -------------------
# ZPlatform reads <content root>/metadata.xml and expects the *application*
# manifest: it must contain <platforms><platform type="android"> so that
# zcMetadataGetTitleId() can match getSystemString(1) == "android".
$zpakPath = Join-Path $OutDir "assets\BruceLee.zpak"
if (Test-Path -LiteralPath $zpakPath) {
    $zip = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $zpakPath))
    try {
        $entry = $zip.GetEntry("metadata.xml")
        if ($entry) {
            $dst = Join-Path $OutDir "metadata.xml"
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $dst, $true)
            Write-Host "  assets/BruceLee.zpak:metadata.xml -> metadata.xml (application manifest)"
            $gameDst = Join-Path $OutDir "game\metadata.xml"
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $gameDst) | Out-Null
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $gameDst, $true)
            Write-Host "  assets/BruceLee.zpak:metadata.xml -> game\metadata.xml"
        } else {
            Write-Warning "assets/BruceLee.zpak has no metadata.xml"
        }
    } finally {
        $zip.Dispose()
    }
}

# Keep the original data.zpak too: ZSystem may open it as a ZIP package
# instead of using the extracted Core.pakh/Core.pakd000 pair.
Copy-Item -LiteralPath $Zpak -Destination (Join-Path $OutDir "content\data.zpak") -Force
Write-Host "  data.zpak -> content\data.zpak"

# --- CPU/memory pseudo-files for ZPlatform system queries -----------------
foreach ($f in @("cpuinfo", "meminfo")) {
    $src = Join-Path $projectRoot "extras\$f"
    if (Test-Path -LiteralPath $src) {
        Copy-Item -LiteralPath $src -Destination (Join-Path $OutDir $f) -Force
    } else {
        Set-Content -LiteralPath (Join-Path $OutDir $f) -Value "" -Encoding ascii
    }
}

# --- A plain-text marker for the user -------------------------------------
@"
Bruce Lee: Dragon Warrior - PS Vita data
========================================

Copy this whole folder to:

    ux0:data/brucelee/

Required layout:

    ux0:data/brucelee/
      lib/armeabi/libdefault.so
      lib/armeabi/libzplatform.so
      lib/armeabi/libjava-activity.so
      assets/BruceLee.zpak
      content/Core.pakh
      content/Core.pakd000
      metadata.xml
      logs/
      shader_cache/

The application does not include or redistribute the original game code or
assets. You must own a legal copy of the Android release.
"@ | Set-Content -LiteralPath (Join-Path $OutDir "LEIA-ME.txt") -Encoding utf8

Write-Host ""
Write-Host "Done. Copy '$OutDir' to 'ux0:data/brucelee' on the Vita." -ForegroundColor Green
