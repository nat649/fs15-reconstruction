param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $projectRoot 'tools/I3DShapesTool'
$commit = '1a2ecf3125d6754d9100c342419003bf048dbec5'
if (-not (Test-Path -LiteralPath $source)) {
    git init $source | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Cannot create local shapes tool checkout' }
    git -C $source fetch --depth 1 https://github.com/Donkie/I3DShapesTool.git $commit
    if ($LASTEXITCODE -ne 0) { throw 'Cannot fetch the pinned upstream shapes tool' }
    git -C $source checkout --detach FETCH_HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot check out pinned source' }
}
$actual = git -C $source rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actual -ne $commit) { throw 'Unexpected shapes tool revision; existing checkout left unchanged' }
$dirty = git -C $source status --porcelain --untracked-files=no
if ($dirty) { throw 'Upstream shapes library has local edits; review them before building' }
$env:DOTNET_CLI_HOME = Join-Path $projectRoot 'build/dotnet-home'
$env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
$env:DOTNET_GENERATE_ASPNET_CERTIFICATE = 'false'
dotnet build (Join-Path $projectRoot 'src/shape_bridge/FS15ShapeBridge.csproj') -c Release -o (Join-Path $projectRoot 'build/shape-bridge') --ignore-failed-sources -p:UseSharedCompilation=false
if ($LASTEXITCODE -ne 0) { throw 'Shape bridge build failed; .NET 10 SDK and ASP.NET runtime reference pack required' }
Copy-Item -LiteralPath (Join-Path $source 'LICENSE') -Destination (Join-Path $projectRoot 'build/shape-bridge/I3DShapesTool-LICENSE.txt')
dotnet (Join-Path $projectRoot 'build/shape-bridge/FS15ShapeBridge.dll') --cipher-fixtures (Join-Path $projectRoot 'build/shapes-cipher-fixtures.json')
if ($LASTEXITCODE -ne 0) { throw 'Cannot prepare native comparison vectors' }
Write-Output "Built local mesh decoder against pinned MIT upstream $commit"
