param([string]$GameDir = $env:FS15_GAME_DIR)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$relativeFiles = @(
    'data/maps/map01.i3d', 'data/maps/map02.i3d',
    'data/maps/map01/map01_dem.png', 'data/maps/map02/americanMap_dem.png',
    'data/maps/map01/pda_map_H.dds', 'data/maps/map02/pda_map_H.dds'
)
foreach ($relative in $relativeFiles) {
    $target = Join-Path $projectRoot ('recovered/gar/' + $relative)
    if (Test-Path -LiteralPath $target) { continue }
    if (-not $GameDir) { throw 'Pass -GameDir with your own FS15 installation to prepare terrain assets.' }
    $source = Join-Path $GameDir $relative
    if (-not (Test-Path -LiteralPath $source)) { throw "Missing installed terrain input: $source" }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath $source -Destination $target
}
Write-Output 'Both base-map I3D/DEM/overview inputs prepared locally. No game installation files were replaced.'
