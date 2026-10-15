param([string]$GameDir = $env:FS15_GAME_DIR, [string]$PythonExe = '', [string]$SteamExe = '', [switch]$Direct)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $GameDir) {
    $manifestPath = Join-Path $projectRoot 'build/native-engine/manifest.json'
    if (Test-Path -LiteralPath $manifestPath) {
        $localManifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        $GameDir = $localManifest.game_directory
        if (-not $GameDir) {
            $gameFlag = [Array]::IndexOf($localManifest.command, '--game-dir')
            if ($gameFlag -ge 0) { $GameDir = $localManifest.command[$gameFlag + 1] }
        }
    }
}
if (-not $GameDir) { throw 'Pass -GameDir with your installed Farming Simulator 15 directory, or set FS15_GAME_DIR.' }
if (-not $PythonExe) {
    $bundledPython = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
    if (Test-Path -LiteralPath $bundledPython) { $PythonExe = $bundledPython }
    else { $PythonExe = (Get-Command python.exe -ErrorAction Stop).Source }
}
$prepareArgs = @((Join-Path $PSScriptRoot 'prepare_native_engine.py'), '--game-dir', $GameDir, '--launch')
if ($SteamExe) { $prepareArgs += @('--steam', $SteamExe) }
if ($Direct) { $prepareArgs += '--direct' }
& $PythonExe @prepareArgs
if ($LASTEXITCODE -ne 0) { throw 'Native launch request failed. See the output and isolated profile log.' }
Write-Output 'Native GIANTS Engine launch requested. Open Career to run the recovered-script module.'
