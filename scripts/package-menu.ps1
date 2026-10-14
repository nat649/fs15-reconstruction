param([string]$GameDir = $env:FS15_GAME_DIR, [switch]$PrepareOnly, [string]$Destination = 'build\FS15Frontend')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$fontRoot = Join-Path $projectRoot 'recovered\gar\shared\font'
foreach ($font in @('defaultFont.font', 'defaultFont.png')) {
    $fontTarget = Join-Path $fontRoot $font
    if (-not (Test-Path -LiteralPath $fontTarget)) {
        if (-not $GameDir) { throw 'Pass -GameDir with your FS15 installation to copy the original bitmap font locally.' }
        $fontSource = Join-Path $GameDir ('shared\font\' + $font)
        if (-not (Test-Path -LiteralPath $fontSource)) { throw "Missing installed font: $fontSource" }
        New-Item -ItemType Directory -Force -Path $fontRoot | Out-Null
        Copy-Item -LiteralPath $fontSource -Destination $fontTarget
    }
}
foreach ($map in @('map01','map02')) {
    $relativePreview = "data\maps\$map\map_preview.dds"
    $previewTarget = Join-Path $projectRoot ('recovered\gar\' + $relativePreview)
    if (-not (Test-Path -LiteralPath $previewTarget)) {
        if (-not $GameDir) { throw 'Pass -GameDir with your FS15 installation to prepare the map previews.' }
        $previewSource = Join-Path $GameDir $relativePreview
        if (-not (Test-Path -LiteralPath $previewSource)) { throw "Missing installed map preview: $previewSource" }
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $previewTarget) | Out-Null
        Copy-Item -LiteralPath $previewSource -Destination $previewTarget
    }
}
if ($PrepareOnly) {
    Write-Output 'Local menu bitmap fonts and base map previews prepared.'
    return
}
$executable = Join-Path $projectRoot 'FS15MenuRebuilt.exe'
if (-not (Test-Path -LiteralPath $executable)) {
    throw 'Build the menu first: scripts/build.ps1 (requires scripts/setup_lua_tools.ps1).'
}
$packageRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Destination))
$workspacePrefix = [System.IO.Path]::GetFullPath($projectRoot).TrimEnd('\') + '\'
if (-not $packageRoot.StartsWith($workspacePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'The package destination must stay inside this workspace.'
}
New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null
$workspaceImage = Join-Path $projectRoot 'build\menu-workspace.png'
$workspaceCaptures = Join-Path $projectRoot 'build\menu-captures'
$smokeArguments = '--root "{0}" --smoke --test-inputs --capture-dir "{1}" --screenshot "{2}"' -f $projectRoot, $workspaceCaptures, $workspaceImage
$process = Start-Process -FilePath $executable -ArgumentList $smokeArguments -Wait -PassThru -WindowStyle Hidden
if ($process.ExitCode -ne 0) { throw 'Recovered menu smoke test failed; inspect logs/menu-runtime.log.' }
$runtimeLog = Get-Content -LiteralPath (Join-Path $projectRoot 'logs\menu-runtime.log')
if (-not ($runtimeLog | Select-String '^BOOT_OK ') -or -not ($runtimeLog | Select-String '^SMOKE_OK ')) {
    throw 'The menu did not reach its boot and rendered-frame checks.'
}
$sourceNames = @($runtimeLog | Where-Object { $_.StartsWith('SOURCE ') } | ForEach-Object { $_.Substring(7) } | Sort-Object -Unique)
$assetNames = @($runtimeLog | Where-Object { $_.StartsWith('ASSET ') } | ForEach-Object { $_.Substring(6) } | Sort-Object -Unique)
$recovery = Get-Content -LiteralPath (Join-Path $projectRoot 'analysis\lua-recovery.json') -Raw | ConvertFrom-Json
$manifestFiles = [System.Collections.Generic.List[object]]::new()
function Copy-PackageFile([string]$Source, [string]$Relative) {
    $target = Join-Path $packageRoot $Relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath $Source -Destination $target
    $manifestFiles.Add(@{ path=$Relative.Replace('\','/'); sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLower(); bytes=(Get-Item -LiteralPath $target).Length })
}
Copy-PackageFile $executable 'FS15MenuRebuilt.exe'
Copy-PackageFile (Join-Path $projectRoot 'src\menu\bootstrap.lua') 'bootstrap.lua'
foreach ($line in ($runtimeLog | Where-Object { $_.StartsWith('ADAPTER ') })) {
    $adapter = $line.Substring(8)
    if ($adapter -notin @('career.lua','newgame.lua')) { throw "Unexpected authored adapter: $adapter" }
    Copy-PackageFile (Join-Path $projectRoot ('src\menu\' + $adapter)) $adapter
}
Copy-PackageFile (Join-Path $projectRoot 'tools\LuaJIT\COPYRIGHT') 'licenses\LuaJIT-COPYRIGHT.txt'
foreach ($name in $sourceNames) {
    $source = Join-Path $projectRoot ('recovered\lua\' + $name)
    $relativeRecoveryName = [System.IO.Path]::ChangeExtension($name.Substring('dataS/'.Length), '.luc').Replace('\','/')
    $row = @($recovery.files | Where-Object { $_.path -eq $relativeRecoveryName })
    if ($row.Count -ne 1) { throw "Missing unique source recovery manifest entry: $name" }
    $actual = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLower()
    if ($actual -ne $row[0].source_sha256) { throw "Recovered source changed since verification: $name" }
    Copy-PackageFile $source ('lua\' + $name)
}
foreach ($name in $assetNames) {
    Copy-PackageFile (Join-Path $projectRoot ('recovered\gar\' + $name)) ('assets\' + $name)
}
$readme = @'
FS15 rebuilt frontend milestone

Double-click FS15MenuRebuilt.exe in this folder. Keep bootstrap.lua, career.lua,
newgame.lua, lua/, assets/ and licenses/ alongside it. No Steam or game executable is
started by this host. It executes recovered Lua source and original menu XML,
textures and bitmap font with independently compiled XML/render/input code.

This local package contains assets and recovered scripts from your installed
copy. It is excluded from GitHub. Do not redistribute the game assets.

Career displays twenty slots and reads existing save metadata without changing
files. Mouse wheel and keyboard navigate the original list. Empty slots reach
the original three difficulty choices and the two base-map previews. Start
reports the world-loading boundary; no game session is created or loaded.
Credits/Back and Quit work. Other unimplemented screens report their destination.
No world, physics, multiplayer or full native engine startup is implemented.
Original main.lua is not executed. Save deletion/writes are disabled.

The default local Documents/My Games/FarmingSimulator2015 profile is read only.
Use --save-dir "PATH" for another profile, or --empty-saves for empty slots.

Logs are written to logs/menu-runtime.log.
'@
$readmePath = Join-Path $packageRoot 'README.txt'
$readme | Set-Content -LiteralPath $readmePath -Encoding utf8
$manifestFiles.Add(@{ path='README.txt'; sha256=(Get-FileHash -LiteralPath $readmePath).Hash.ToLower(); bytes=(Get-Item -LiteralPath $readmePath).Length })
$manifest = @{
    scope='Independently compiled frontend; recovered career/difficulty/map selection; full game engine not rebuilt'
    originalEngineLoaded=$false
    originalMainExecuted=$false
    recoveredSources=$sourceNames.Count
    recoveredAssets=$assetNames.Count
    files=$manifestFiles
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $packageRoot 'MENU-MANIFEST.json') -Encoding utf8
$packageImage = Join-Path $projectRoot 'build\menu-packaged.png'
$packageExe = Join-Path $packageRoot 'FS15MenuRebuilt.exe'
$packageCaptures = Join-Path $projectRoot 'build\package-menu-captures'
$packageArguments = '--smoke --test-inputs --capture-dir "{0}" --screenshot "{1}"' -f $packageCaptures, $packageImage
$packageProcess = Start-Process -FilePath $packageExe -ArgumentList $packageArguments -WorkingDirectory $env:TEMP -Wait -PassThru -WindowStyle Hidden
if ($packageProcess.ExitCode -ne 0) { throw "Packaged menu failed without workspace path; inspect $packageRoot\logs\menu-runtime.log." }
if ((Get-FileHash -LiteralPath $workspaceImage).Hash -ne (Get-FileHash -LiteralPath $packageImage).Hash) {
    throw 'Packaged and workspace menu render differently.'
}
$captureNames = @($runtimeLog | Where-Object { $_.StartsWith('CAPTURE ') } | ForEach-Object { $_.Substring(8) } | Sort-Object -Unique)
foreach ($requiredCapture in @('CareerScreen','DifficultyScreen','MapSelectionScreen','CreditsScreen')) {
    if ($requiredCapture -notin $captureNames) { throw "Required frontend capture missing: $requiredCapture" }
}
foreach ($name in $captureNames) {
    $workspaceCapture = Join-Path $workspaceCaptures ($name + '.png')
    $packageCapture = Join-Path $packageCaptures ($name + '.png')
    if (-not (Test-Path -LiteralPath $packageCapture) -or
        (Get-FileHash -LiteralPath $workspaceCapture).Hash -ne (Get-FileHash -LiteralPath $packageCapture).Hash) {
        throw "Packaged frontend capture differs: $name"
    }
}
$zipPath = Join-Path $projectRoot 'build\FS15MenuRebuilt-local.zip'
$zipPaths = @($manifestFiles | ForEach-Object { Join-Path $packageRoot $_.path }) + @(Join-Path $packageRoot 'MENU-MANIFEST.json')
# Compress the directory structure, keeping the folder runnable after extraction.
# Logs are omitted. The directory itself remains available for direct launching.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipStream = [System.IO.File]::Open($zipPath, [System.IO.FileMode]::Create)
try {
    $archive = [System.IO.Compression.ZipArchive]::new($zipStream, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in $zipPaths) {
            $entry = $file.Substring($packageRoot.Length + 1).Replace('\','/')
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file, $entry) | Out-Null
        }
    } finally { $archive.Dispose() }
} finally { $zipStream.Dispose() }
$summary = @{
    scope=$manifest.scope
    recoveredSources=$sourceNames.Count
    recoveredAssets=$assetNames.Count
    nativeEngineDependency=$false
    workspaceSmokePassed=$true
    packageSmokePassed=$true
    identicalRender=$true
    sourceHashesVerified=$true
    matchingScreenCaptures=$captureNames
    saveMetadataReadOnly=$true
    imageSha256=(Get-FileHash -LiteralPath $packageImage).Hash.ToLower()
    packageSha256=(Get-FileHash -LiteralPath $zipPath).Hash.ToLower()
}
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $projectRoot 'analysis\menu-validation.json') -Encoding utf8
Write-Output "Runnable local menu: $packageExe"
Write-Output "Local package: $zipPath"
