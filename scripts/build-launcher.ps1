$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
dotnet publish (Join-Path $projectRoot 'src/unified_launcher/FS15Reconstruction.csproj') -c Release -o (Join-Path $projectRoot 'build/FS15Launcher') --self-contained false -p:RestoreIgnoreFailedSources=true
if ($LASTEXITCODE -ne 0) { throw 'Launcher build failed' }
Write-Output (Join-Path $projectRoot 'build/FS15Launcher/FS15Reconstruction.exe')
