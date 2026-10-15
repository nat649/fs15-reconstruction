param([Parameter(Mandatory=$true)][string]$PackageRoot)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
foreach ($map in @('Map01','Map02')) {
  foreach ($scenario in @('starting-fleet','tip')) {
    $hashes = @()
    foreach ($variant in @('workspace','package')) {
        $root = if ($variant -eq 'workspace') { $projectRoot } else { $PackageRoot }
        $exe = Join-Path $root 'FS15EngineRebuilt.exe'
        $report = Join-Path $projectRoot "build/gameplay-$variant-$map-$scenario.json"
        $capture = Join-Path $projectRoot "build/gameplay-$variant-$map-$scenario.png"
        $arguments = '--root "{0}" --map {1} --play --smoke --report "{2}" --capture "{3}"' -f $root,$map,$report,$capture
        if ($scenario -eq 'tip') { $arguments += ' --scenario tip' }
        $process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $env:TEMP -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "Gameplay package check failed: $variant $map" }
        $data = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
        $expected = if ($map -eq 'Map01') { 13 } else { 12 }
        if ($data.original_engine_loaded -or $data.visible_vehicle_gpu_samples -le 0 -or
            $data.mission.vehicles -ne $expected -or $data.mission.silos -le 0 -or
            $data.mission.density_width -ne 4096 -or $data.mission.collision_triangles -le 0 -or
            $data.mission.recovered_animations -le 0) {
            throw "Gameplay fleet/density/native bridge unavailable: $variant $map"
        }
        if ($scenario -eq 'tip' -and ($data.mission.unloaded_l -le 0 -or
            $data.mission.active_animations -le 0 -or
            [Math]::Abs($data.mission.implement_fill_l + $data.mission.unloaded_l - $data.mission.implement_capacity_l) -gt 0.001 -or
            [Math]::Abs($data.mission.farm_wheat_l - $data.mission.unloaded_l - 5000) -gt 0.001)) {
            throw "Packaged original animated discharge/conservation failed: $variant $map"
        }
        $hashes += (Get-FileHash -LiteralPath $capture -Algorithm SHA256).Hash
    }
    if ($hashes[0] -ne $hashes[1]) { throw "Packaged gameplay renders differently: $map $scenario" }
  }
}
Write-Output 'Both original starting fleets, recovered animated discharge and identical packaged renders verified.'
