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
        if ($data.decoded_meshes -gt 0 -and ($data.rendered_instances -le 0 -or $data.visible_object_gpu_samples -le 0)) {
            throw "Native engine $variant $map did not rasterize original objects" }
        if ($data.decoded_meshes -gt 0 -and (-not $data.environment_ready -or $data.original_shader_injections -ne 5 -or $data.shadow_depth_pixels -le 0)) {
            throw "Recovered environment/shader/shadow startup failed for $variant $map" }
        if ($data.road_alignment_samples -gt 0 -and $data.road_median_height_error -gt 0.25) {
            throw "Original road meshes and height map no longer align for $map" }
        $reports += $data
        $images += (Get-FileHash -LiteralPath $image -Algorithm SHA256).Hash.ToLower()
    }
    if ($images[0] -ne $images[1]) { throw "Packaged 3D terrain differs for $map" }
    $results += @{
        map=$map; ready=$true; driver=$reports[0].driver; featureLevel=$reports[0].feature_level
        vertices=$reports[0].vertices; triangles=$reports[0].triangles
        sourceWidth=$reports[0].source_width; sourceHeight=$reports[0].source_height
        worldWidth=$reports[0].world_width; worldDepth=$reports[0].world_depth
        sceneNodes=$reports[0].scene_nodes; sourceShapeNodes=$reports[0].source_shape_nodes; skippedShapeNodes=$reports[0].skipped_shape_nodes
        visibleGpuSamples=$reports[0].visible_gpu_samples; cameraMathCheck=$true
        decodedMeshes=$reports[0].decoded_meshes; renderedInstances=$reports[0].rendered_instances
        objectTriangles=$reports[0].object_triangles; terrainLayers=$reports[0].terrain_layers
        visibleObjectGpuSamples=$reports[0].visible_object_gpu_samples
        recoveredEnvironment=$reports[0].environment_ready; originalShaderInjections=$reports[0].original_shader_injections
        animatedDraws=$reports[0].animated_draws; shadowDepthPixels=$reports[0].shadow_depth_pixels; shadowResolution=$reports[0].shadow_resolution
        roadAlignmentSamples=$reports[0].road_alignment_samples
        roadMedianHeightError=$reports[0].road_median_height_error; roadFlippedZError=$reports[0].road_flipped_z_error
        identicalPackageRender=$true; imageSha256=$images[0]
    }
}
@{ scope='Original GE6 map/sky resources and shader injections; recovered environment curves, local shadows; no physics'
   originalEngineLoaded=$false; maps=$results; syntheticWarpFixture='tests/engine_fixture.py'
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $projectRoot 'analysis\engine-validation.json') -Encoding utf8
Write-Output 'Native engine startup and identical packaged terrain rendering verified for both maps.'
