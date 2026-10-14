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
# Read the terrain header only; stop before the large object scene begins.
foreach ($map in @('map01','map02')) {
    $settings = [System.Xml.XmlReaderSettings]::new()
    $settings.DtdProcessing = [System.Xml.DtdProcessing]::Prohibit
    $settings.XmlResolver = $null
    $reader = [System.Xml.XmlReader]::Create((Join-Path $projectRoot "recovered/gar/data/maps/$map.i3d"), $settings)
    $fileNames = @{}
    $layerFiles = @()
    try {
        while ($reader.Read()) {
            if ($reader.NodeType -eq [System.Xml.XmlNodeType]::Element -and $reader.LocalName -eq 'File') {
                $fileNames[$reader.GetAttribute('fileId')] = $reader.GetAttribute('filename')
            }
            if ($reader.NodeType -eq [System.Xml.XmlNodeType]::Element -and $reader.LocalName -eq 'Layer') {
                $layerFiles += [System.IO.Path]::ChangeExtension($fileNames[$reader.GetAttribute('detailMapId')], '.dds')
                $layerFiles += $fileNames[$reader.GetAttribute('weightMapId')]
            }
            if ($reader.NodeType -eq [System.Xml.XmlNodeType]::EndElement -and $reader.LocalName -eq 'Layers') { break }
        }
    } finally { $reader.Dispose() }
    foreach ($file in $layerFiles) {
        $relative = 'data/maps/' + $file
        $target = Join-Path $projectRoot ('recovered/gar/' + $relative)
        if (Test-Path -LiteralPath $target) { continue }
        if (-not $GameDir) { throw 'Pass -GameDir to prepare original terrain layers.' }
        $source = Join-Path $GameDir $relative
        if (-not (Test-Path -LiteralPath $source)) { throw "Missing original terrain layer: $source" }
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
        Copy-Item -LiteralPath $source -Destination $target
    }
}
Write-Output 'Both original terrain inputs and material layers prepared locally; installation unchanged.'
