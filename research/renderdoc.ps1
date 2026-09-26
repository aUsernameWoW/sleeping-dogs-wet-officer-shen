<#
.SYNOPSIS
    Frame captures of Sleeping Dogs: DE with RenderDoc, and the analysis scripts in research\renderdoc.

.EXAMPLE
    .\research\renderdoc.ps1 launch                      # start the game under RenderDoc; PrtScn / F12 captures
    .\research\renderdoc.ps1 report  <capture.rdc>       # character draws: permutation, wetness, textures
    .\research\renderdoc.ps1 frame   <capture.rdc>       # the presented image as PNG
    .\research\renderdoc.ps1 targets <capture.rdc> -Event 3212   # render targets at an event (G-buffer)
    .\research\renderdoc.ps1 preview <capture.rdc>       # re-render with the de / default / strong shaders

.NOTES
    Needs RenderDoc (winget: BaldurKarlsson.RenderDoc) and, for report/preview, `python research\shaders.py
    unpack` first (names.json, variants.json). Captures and results go to build\research (gitignored).
    `launch` starts sdhdship.exe directly, which needs steam_appid.txt (307690) in the game folder so the game
    doesn't restart itself through Steam; it's created if missing (delete it when you're done). Steam must be
    running. The analysis scripts run inside qrenderdoc (`--python`, its own Python 3.8, module `renderdoc`)
    and quit before its window opens; `__file__` isn't defined there, so they take everything from env vars.
#>
param(
    [Parameter(Mandatory, Position = 0)] [ValidateSet('launch', 'report', 'frame', 'targets', 'preview')] [string] $Command,
    [Parameter(Position = 1)] [string] $Capture,
    [int] $Event,
    [string] $Out,
    [string] $GameDir = $(if ($env:SDDE_DIR) { $env:SDDE_DIR } else { 'D:\SteamLibrary\steamapps\common\SleepingDogsDefinitiveEdition' }),
    [string] $RenderDocDir = 'C:\Program Files\RenderDoc'
)

$ErrorActionPreference = 'Stop'
$build = Join-Path (Split-Path $PSScriptRoot) 'build\research'
New-Item -ItemType Directory -Force $build | Out-Null

if ($Command -eq 'launch') {
    $appId = Join-Path $GameDir 'steam_appid.txt'
    if (-not (Test-Path $appId)) {
        Set-Content -Path $appId -Value '307690' -NoNewline -Encoding ascii
        Write-Host "Created $appId (delete it when you no longer launch the game this way)"
    }
    $captures = Join-Path $build 'captures'
    New-Item -ItemType Directory -Force $captures | Out-Null
    Write-Host "Captures go to $captures"
    & (Join-Path $RenderDocDir 'renderdoccmd.exe') capture -w -d $GameDir -c (Join-Path $captures 'sdde') (Join-Path $GameDir 'sdhdship.exe')
    return
}

if (-not $Capture) { throw "$Command needs a capture file" }
$rdc = (Resolve-Path $Capture).Path
$stem = [IO.Path]::GetFileNameWithoutExtension($rdc)
$env:RDC = $rdc
$env:NAMES = Join-Path $build 'names.json'
$env:VARIANTS = Join-Path $build 'variants.json'
switch ($Command) {
    'report'  { $env:OUT = if ($Out) { $Out } else { Join-Path $build "$stem-report" } }
    'frame'   { $env:OUT = if ($Out) { $Out } else { Join-Path $build "$stem.png" } }
    'targets' { if (-not $Event) { throw 'targets needs -Event' }; $env:EID = "$Event"; $env:OUT = if ($Out) { $Out } else { Join-Path $build "$stem-e$Event" } }
    'preview' { $env:OUT = if ($Out) { $Out } else { Join-Path $build "$stem-preview" } }
}
if ($Command -in 'report', 'preview' -and -not (Test-Path $env:NAMES)) { throw 'run "python research\shaders.py unpack" first' }

$script = Join-Path $PSScriptRoot "renderdoc\$Command.py"
$p = Start-Process (Join-Path $RenderDocDir 'qrenderdoc.exe') -ArgumentList '--python', "`"$script`"" -PassThru
if (-not $p.WaitForExit(900000)) { $p | Stop-Process; throw 'qrenderdoc timed out' }
Write-Host "$Command -> $env:OUT"
if (Test-Path "$($env:OUT).err.txt") { Get-Content "$($env:OUT).err.txt" }
if ($Command -eq 'report') { Get-Content (Join-Path $env:OUT 'report.txt') | Select-Object -Last 20 }
