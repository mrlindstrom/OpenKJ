<# Copies the Windows GStreamer runtime into OpenKJ's flat application layout. #>
param(
    [Parameter(Mandatory = $true)][string]$GstDir,
    [Parameter(Mandatory = $true)][string]$OutputDir
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path (Join-Path $GstDir 'bin'))) { throw "No GStreamer bin directory at $GstDir" }
New-Item -ItemType Directory -Force $OutputDir | Out-Null

# Keep plugins, runtime data, helpers and licence files in their original tree.
# SDK headers, import/static libraries, symbols and development tools are not runtime files.
robocopy $GstDir $OutputDir /E /XD bin include pkgconfig cmake gtk-doc man doc gdb gir-1.0 /XF *.lib *.a *.la *.pdb *.def /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "Copying GStreamer failed (robocopy exit code $LASTEXITCODE)" }

# Windows must find these DLLs beside OpenKJ.exe before main() runs. Do not also
# ship a second set under bin/, where the full SDK originally installed them.
Copy-Item (Join-Path $GstDir 'bin\*.dll') $OutputDir

# Keep the GLib/GIO and image-loader helpers, but not standalone SDK utilities.
# GLib searches beside its own DLL for helpers. Their child processes can then
# resolve the same DLLs without another bin/ copy.
foreach ($pattern in @('gspawn-*-helper*.exe', 'gio-querymodules.exe', 'gdk-pixbuf-query-loaders.exe')) {
    Get-ChildItem (Join-Path $GstDir 'bin') -Filter $pattern |
        Copy-Item -Destination $OutputDir
}

$files = Get-ChildItem $OutputDir -File -Recurse
$bytes = ($files | Measure-Object -Property Length -Sum).Sum
Write-Host ("Staged runtime: {0} files, {1:N1} MiB" -f $files.Count, ($bytes / 1MB))
# Robocopy uses nonzero success codes, unlike the GitHub PowerShell wrapper.
$global:LASTEXITCODE = 0
