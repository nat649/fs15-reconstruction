param(
    [ValidateSet('map01','map02')][string]$Map = 'map01',
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $sdl = Join-Path $root 'build/ge6-objects-deps/SDL2-2.30.0'
    if (-not (Test-Path -LiteralPath "$sdl/cmake/sdl2-config.cmake")) {
        $archive = Join-Path $root 'build/ge6-objects-deps/SDL2-devel-2.30.0-VC.zip'
        New-Item -ItemType Directory -Force -Path (Split-Path $archive) | Out-Null
        Invoke-WebRequest -Uri 'https://github.com/libsdl-org/SDL/releases/download/release-2.30.0/SDL2-devel-2.30.0-VC.zip' -OutFile $archive
        $expected = '6413358B67F19B5398E902C576518D5350394A863EB53EB27BB9A81E75A36958'
        if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) { throw 'SDL2 archive hash differs' }
        Expand-Archive -LiteralPath $archive -DestinationPath (Split-Path $sdl) -Force
    }
    & cmake -S . -B build/ge6-objects-build -DFS15_PORTABLE_CORE_ONLY=ON -DFS15_BUILD_GE6_OBJECTS=ON "-DSDL2_DIR=$sdl/cmake"
    if ($LASTEXITCODE) { throw 'GE6 object configuration failed' }
    & cmake --build build/ge6-objects-build --config Release --target FS15GE6Objects --parallel 4
    if ($LASTEXITCODE) { throw 'GE6 object compilation failed' }
    Copy-Item -LiteralPath "$sdl/lib/x64/SDL2.dll" -Destination 'build/ge6-objects-build/src/ge6_objects/Release/SDL2.dll'
    Copy-Item -LiteralPath "$sdl/COPYING.txt" -Destination 'build/ge6-objects-build/src/ge6_objects/Release/SDL2-COPYING.txt'
    & $Python scripts/prepare_ge6_objects.py --scene "recovered/gar/data/maps/$Map.i3d" --output "build/ge6-objects/$Map"
    if ($LASTEXITCODE) { throw 'Stage original resources first; Python needs Pillow and NumPy. See docs/GE6_OBJECT_RENDERING.md.' }
    Write-Host "Ready: build/ge6-objects-build/src/ge6_objects/Release/FS15GE6Objects.exe --scene build/ge6-objects/$Map/scene.ge6scene"
} finally { Pop-Location }
