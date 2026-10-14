param([Parameter(Mandatory=$true)][string]$PackageRoot)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$results = @()
foreach ($map in @('Map01','Map02')) {
    $reports = @()
    $images = @()
    foreach ($variant in @('workspace','package')) {
        $root = if ($variant -eq 'workspace') { $projectRoot } else { $PackageRoot }
        $exe = Join-Path $root 'FS15EngineRebuilt.exe'
        $report = Join-Path $projectRoot ("build\engine-$variant-$map.json")
        $image = Join-Path $projectRoot ("build\engine-$variant-$map.png")
        $arguments = '--root "{0}" --map {1} --smoke --report "{2}" --capture "{3}"' -f $root,$map,$report,$image
        $process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $env:TEMP -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "Native engine $variant $map failed" }
        $data = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
        if (-not $data.ready -or $data.original_engine_loaded -or $data.visible_gpu_samples -le 0 -or -not $data.camera_math_check) {
            throw "Native engine $variant $map did not render terrain" }
        $reports += $data
        $images += (Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash.ToLower()
    }
    if ($images[0] -ne $images[1]) { throw "Packaged 3D terrain differs for $map" }
    $results += @{
        map=$map; ready=$true; driver=$reports[0].driver; featureLevel=$reports[0].feature_level
        vertices=$reports[0].vertices; triangles=$reports[0].triangles
        sourceWidth=$reports[0].source_width; sourceHeight=$reports[0].source_height
        worldWidth=$reports[0].world_width; worldDepth=$reports[0].world_depth
        sceneNodes=$reports[0].scene_nodes; deferredShapes=$reports[0].deferred_shape_nodes
        visibleGpuSamples=$reports[0].visible_gpu_samples; cameraMathCheck=$true
        identicalPackageRender=$true; imageSha256=$images[0]
    }
}
@{ scope='Authored D3D11 terrain bootstrap; original DEM + overview DDS; external models/physics not rendered'
   originalEngineLoaded=$false; maps=$results; syntheticWarpFixture='tests/engine_fixture.py'
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $projectRoot 'analysis\engine-validation.json') -Encoding utf8
Write-Output 'Native engine startup and identical packaged terrain rendering verified for both maps.'
