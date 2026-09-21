<#
.SYNOPSIS
    Packages a local Windows build of OpenKJ into an Inno Setup installer.

.DESCRIPTION
    Does what the CI workflow does, but from your own build folder and your own
    MSVC GStreamer install:
      1. copies build\openkj.exe into a fresh output\ folder
      2. runs windeployqt to add Qt and the Visual C++ runtime
      3. copies the GStreamer runtime (without its development files)
      4. adds the licence, fonts, unzip.exe and OpenSSL DLLs the installer expects
      5. compiles cd\openkj64.iss with the given version number

    The installer ends up in installer\OpenKJ-<version>-64bit-setup.exe.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File cd\package-windows.ps1 -Version 2.2.0
#>
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$QtDir = "C:\Qt\5.15.2\msvc2019_64",
    [string]$GstDir = "C:\Program Files\gstreamer\1.0\msvc_x86_64",
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

function Step($message) { Write-Host "==> $message" -ForegroundColor Cyan }

# --- Check the pieces are where we expect -----------------------------------------------
$exe = Join-Path $BuildDir "openkj.exe"
if (-not (Test-Path $exe)) { throw "No $exe - build OpenKJ first (cmake --build build)." }
$windeployqt = Join-Path $QtDir "bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) { throw "windeployqt not found at $windeployqt - pass -QtDir." }
if (-not (Test-Path (Join-Path $GstDir "bin\gstreamer-1.0-0.dll"))) { throw "GStreamer not found at $GstDir - pass -GstDir." }
$iscc = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw "Inno Setup 6 not found. Install it with: winget install JRSoftware.InnoSetup" }

# --- Fresh output folder --------------------------------------------------------------
Step "Preparing output folder"
if (Test-Path output) { Remove-Item output -Recurse -Force }
New-Item -ItemType Directory output | Out-Null
Copy-Item $exe output\OpenKJ.exe

Step "Running windeployqt"
& $windeployqt --release --compiler-runtime output\OpenKJ.exe
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# --- GStreamer runtime ------------------------------------------------------------------
# GStreamer finds its plugins relative to the folder its DLL lives in, so the tree is copied
# as-is and the bin DLLs are also placed next to OpenKJ.exe, the same layout the CI uses.
Step "Copying GStreamer runtime from $GstDir"
robocopy $GstDir output /E /XD include pkgconfig cmake gtk-doc man doc gir-1.0 /XF *.lib *.a *.la *.pdb *.def /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed copying GStreamer (exit code $LASTEXITCODE)" }
Copy-Item (Join-Path $GstDir "bin\*.dll") output\

# --- Extras the installer script expects -------------------------------------------------
Step "Adding licence, fonts, unzip and OpenSSL"
Copy-Item LICENSE output\LICENSE.txt
$depsUrl = "https://storage.googleapis.com/okj-installer-deps"
$depsDir = Join-Path $repo "installer-deps"
New-Item -ItemType Directory -Force $depsDir | Out-Null
$deps = "unzip.exe", "Roboto-Bold.ttf", "Roboto-Medium.ttf", "Roboto-Regular.ttf", "SourceCodePro-Medium.ttf", "ssl-x86_64-1.1.1.zip"
foreach ($dep in $deps) {
    $target = Join-Path $depsDir $dep
    if (-not (Test-Path $target)) {
        Write-Host "    downloading $dep"
        Invoke-WebRequest -Uri "$depsUrl/$dep" -OutFile $target -UseBasicParsing
    }
    if ($dep -notlike "*.zip") { Copy-Item $target output\ }
}
# OpenSSL DLLs go flat next to the exe (Qt needs them for HTTPS to the request server)
$sslTemp = Join-Path $env:TEMP "okj-ssl"
if (Test-Path $sslTemp) { Remove-Item $sslTemp -Recurse -Force }
Expand-Archive (Join-Path $depsDir "ssl-x86_64-1.1.1.zip") -DestinationPath $sslTemp
Get-ChildItem $sslTemp -Recurse -Filter *.dll | Copy-Item -Destination output\

# --- Build the installer ------------------------------------------------------------------
Step "Compiling installer for version $Version"
$iss = Get-Content cd\openkj64.iss -Raw
$iss = $iss -replace '#define MyAppVersion ".*"', "#define MyAppVersion `"$Version`""
Set-Content inst.iss $iss -Encoding UTF8
if (Test-Path installer) { Remove-Item installer -Recurse -Force }
& $iscc inst.iss /Oinstaller
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed" }
Remove-Item inst.iss

$final = "installer\OpenKJ-$Version-64bit-setup.exe"
Move-Item installer\OpenKJ.exe $final
$size = "{0:N0} MB" -f ((Get-Item $final).Length / 1MB)
Step "Done: $final ($size)"
Write-Host ""
Write-Host "To publish it as a GitHub release (needs the GitHub CLI):"
Write-Host "    gh release create $Version `"$final`" --title `"OpenKJ $Version`" --generate-notes"
