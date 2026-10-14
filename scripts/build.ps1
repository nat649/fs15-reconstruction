param([string]$Generator = '', [string]$PythonExe = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'
$configureArguments = @('-S', $projectRoot, '-B', $buildDir, '-A', 'x64')
if ($Generator) { $configureArguments += @('-G', $Generator) }
if ($PythonExe) { $configureArguments += "-DPython3_EXECUTABLE=$PythonExe" }
cmake @configureArguments
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
cmake --build $buildDir --config Release
if ($LASTEXITCODE -ne 0) { throw 'C++ compilation failed' }
ctest --test-dir $buildDir -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Reconstruction checks failed' }
Copy-Item -LiteralPath (Join-Path $buildDir 'Release\FS15LauncherRebuilt.exe') -Destination (Join-Path $projectRoot 'FS15LauncherRebuilt.exe')
Copy-Item -LiteralPath (Join-Path $buildDir 'Release\FS15Core.dll') -Destination (Join-Path $projectRoot 'FS15Core.dll')
