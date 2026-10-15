param([Parameter(Mandatory=$true)][string]$PackageRoot)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
foreach ($map in @('Map01','Map02')) {
    $hashes = @()
    foreach ($variant in @('workspace','package')) {
        $root = if ($variant -eq 'workspace') { $projectRoot } else { $PackageRoot }
        $exe = Join-Path $root 'FS15EngineRebuilt.exe'
        $report = Join-Path $projectRoot "build/gameplay-$variant-$map.json"
        $capture = Join-Path $projectRoot "build/gameplay-$variant-$map.png"
        $arguments = '--root "{0}" --map {1} --play --smoke --report "{2}" --capture "{3}"' -f $root,$map,$report,$capture
        $process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $env:TEMP -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "Gameplay package check failed: $variant $map" }
        $data = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
        $expected = if ($map -eq 'Map01') { 13 } else { 12 }
        if ($data.original_engine_loaded -or $data.visible_vehicle_gpu_samples -le 0 -or
            $data.mission.vehicles -ne $expected -or $data.mission.silos -le 0 -or
            $data.mission.density_width -ne 4096 -or $data.mission.collision_triangles -le 0) {
            throw "Gameplay fleet/density/native bridge unavailable: $variant $map"
        }
        $hashes += (Get-FileHash -LiteralPath $capture -Algorithm SHA256).Hash
    }
    if ($hashes[0] -ne $hashes[1]) { throw "Packaged gameplay renders differently: $map" }
}
Write-Output 'Both original starting fleets and identical packaged gameplay renders verified.'
