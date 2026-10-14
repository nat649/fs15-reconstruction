param([string]$VisualStudioPath = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolsRoot = Join-Path $projectRoot 'tools'
$definitions = @(
    @{ Name='luajit-decompiler'; Url='https://github.com/Dr-MTN/luajit-decompiler.git'; Branch='master'; Commit='e4552fda8bece1aa7b9ca12025179eb1b11dac1e' },
    @{ Name='LuaJIT'; Url='https://github.com/LuaJIT/LuaJIT.git'; Branch='v2.1'; Commit='2a8e017ef1ece6a95b983ffca073d30609dce7c4' }
)
foreach ($definition in $definitions) {
    $directory = Join-Path $toolsRoot $definition.Name
    if (-not (Test-Path -LiteralPath $directory)) {
        git clone --no-checkout --depth 1 --branch $definition.Branch $definition.Url $directory
        if ($LASTEXITCODE -ne 0) { throw "Clone failed: $($definition.Name)" }
        git -C $directory fetch --depth 1 origin $definition.Commit
        if ($LASTEXITCODE -ne 0) { throw "Pinned revision unavailable: $($definition.Name)" }
        git -C $directory checkout --detach $definition.Commit
        if ($LASTEXITCODE -ne 0) { throw "Checkout failed: $($definition.Name)" }
    }
    $actual = git -C $directory rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $actual -ne $definition.Commit) {
        throw "Unexpected existing tool checkout: $directory. Expected $($definition.Commit); existing files were preserved."
    }
}
$runtime = Join-Path $toolsRoot 'LuaJIT\src\luajit.exe'
if (-not (Test-Path -LiteralPath $runtime)) {
    if (-not $VisualStudioPath) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Pass -VisualStudioPath with a Visual Studio C++ installation.' }
        $VisualStudioPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    }
    $vcvars = Join-Path $VisualStudioPath 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path -LiteralPath $vcvars)) { throw "Missing x64 build environment: $vcvars" }
    Push-Location (Join-Path $toolsRoot 'LuaJIT\src')
    try {
        $command = 'call "{0}" && call msvcbuild.bat nogc64 static' -f $vcvars
        & $env:ComSpec /d /s /c $command
        if ($LASTEXITCODE -ne 0) { throw 'LuaJIT build failed' }
    } finally { Pop-Location }
}
$metadata = @{
    luajit = @{ repository=$definitions[1].Url; commit=$definitions[1].Commit; build='Windows x64, nogc64 static' }
    decompiler = @{ repository=$definitions[0].Url; commit=$definitions[0].Commit; adapter='scripts/ljd_driver.py; distinct literal AST nodes' }
}
$metadata | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $projectRoot 'analysis\lua-toolchain.json') -Encoding utf8
& $runtime -v
if ($LASTEXITCODE -ne 0) { throw 'LuaJIT verification failed' }
