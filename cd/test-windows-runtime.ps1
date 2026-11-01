param(
    [Parameter(Mandatory = $true)][string]$GstDir,
    [Parameter(Mandatory = $true)][string]$OutputDir
)

$ErrorActionPreference = 'Stop'
$output = (Resolve-Path $OutputDir).Path
$sourcePlugins = Join-Path $GstDir 'lib\gstreamer-1.0'

# Removing duplicate libraries must not silently remove a codec or a dependency.
foreach ($dll in Get-ChildItem (Join-Path $GstDir 'bin') -Filter *.dll) {
    $copy = Join-Path $output $dll.Name
    if (-not (Test-Path $copy)) { throw "Missing runtime DLL: $($dll.Name)" }
    if ((Get-FileHash $dll.FullName).Hash -ne (Get-FileHash $copy).Hash) {
        throw "Runtime DLL differs from SDK: $($dll.Name)"
    }
}
foreach ($plugin in Get-ChildItem $sourcePlugins -Filter *.dll -Recurse) {
    $relative = $plugin.FullName.Substring($sourcePlugins.Length).TrimStart('\')
    $copy = Join-Path (Join-Path $output 'lib\gstreamer-1.0') $relative
    if (-not (Test-Path $copy)) { throw "Missing plugin: $relative" }
    if ((Get-FileHash $plugin.FullName).Hash -ne (Get-FileHash $copy).Hash) {
        throw "Plugin differs from SDK: $relative"
    }
}
if (Test-Path (Join-Path $output 'bin')) { throw 'Duplicate GStreamer bin directory in package' }
foreach ($pattern in @('gspawn-*-helper*.exe', 'gio-querymodules.exe', 'gdk-pixbuf-query-loaders.exe')) {
    foreach ($helper in Get-ChildItem (Join-Path $GstDir 'bin') -Filter $pattern) {
        if (-not (Test-Path (Join-Path $output $helper.Name))) { throw "Missing runtime helper: $($helper.Name)" }
    }
}

# Temporarily put the SDK's diagnostic tools beside the staged DLLs. They are
# removed before packaging, including on failure, and cannot use the SDK PATH.
$tools = @('gst-inspect-1.0.exe', 'gst-launch-1.0.exe')
$variables = @('PATH', 'GST_PLUGIN_SYSTEM_PATH_1_0', 'GST_PLUGIN_PATH_1_0', 'GST_REGISTRY_1_0', 'GST_REGISTRY_FORK')
$saved = @{}
foreach ($name in $variables) { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }
$temp = Join-Path $env:RUNNER_TEMP ('openkj-runtime-' + [Guid]::NewGuid())
New-Item -ItemType Directory $temp | Out-Null
$app = $null
try {
    foreach ($tool in $tools) { Copy-Item (Join-Path $GstDir "bin\$tool") $output }
    $env:PATH = "$output;$env:SystemRoot\System32;$env:SystemRoot"
    $env:GST_PLUGIN_SYSTEM_PATH_1_0 = Join-Path $output 'lib\gstreamer-1.0'
    $env:GST_PLUGIN_PATH_1_0 = ''
    $env:GST_REGISTRY_1_0 = Join-Path $temp 'registry.bin'
    $env:GST_REGISTRY_FORK = 'no'
    $inspect = Join-Path $output $tools[0]
    $launch = Join-Path $output $tools[1]

    foreach ($element in @('uridecodebin', 'appsrc', 'appsink', 'audioconvert', 'audioresample', 'videoconvert', 'videoscale', 'pitch', 'wasapisink', 'avdec_h264', 'mpg123audiodec', 'vorbisenc', 'lamemp3enc', 'wavenc')) {
        & $inspect $element | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Packaged element failed to load: $element" }
    }
    foreach ($format in @(
        @{ Encoder = @('vorbisenc', '!', 'oggmux'); Extension = 'ogg' },
        @{ Encoder = @('lamemp3enc'); Extension = 'mp3' },
        @{ Encoder = @('wavenc'); Extension = 'wav' }
    )) {
        $file = Join-Path $temp ("test." + $format.Extension)
        $arguments = @('-q', 'audiotestsrc', 'num-buffers=16', '!', 'audioconvert', '!', 'audioresample', '!') + $format.Encoder + @('!', 'filesink', "location=$file")
        & $launch @arguments
        if ($LASTEXITCODE -ne 0) { throw "Packaged audio encoding failed: $($format.Extension)" }
        & $launch -q uridecodebin "uri=$(([Uri]$file).AbsoluteUri)" '!' audioconvert '!' fakesink
        if ($LASTEXITCODE -ne 0) { throw "Packaged audio playback failed: $($format.Extension)" }
    }
    & $launch -q videotestsrc num-buffers=16 '!' videoconvert '!' videoscale '!' 'video/x-raw,format=BGRx' '!' fakesink
    if ($LASTEXITCODE -ne 0) { throw 'Packaged video conversion failed' }

    $app = Start-Process (Join-Path $output 'OpenKJ.exe') -PassThru -WorkingDirectory $output
    Start-Sleep -Seconds 5
    if ($app.HasExited) { throw "Packaged OpenKJ exited during startup: $($app.ExitCode)" }
    Write-Host 'Packaged runtime passed plugin loading, audio encode/decode, video conversion and app startup checks.'
} finally {
    if ($app -and -not $app.HasExited) {
        Stop-Process -Id $app.Id -Force
        $app.WaitForExit()
    }
    foreach ($name in $variables) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) }
    foreach ($tool in $tools) { Remove-Item (Join-Path $output $tool) -ErrorAction SilentlyContinue }
    Remove-Item $temp -Recurse -Force
}
