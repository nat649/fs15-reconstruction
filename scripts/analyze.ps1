param(
    [ValidateSet('launcher','game-runtime','server')][string]$Target = 'launcher',
    [int]$Limit = 200,
    [int]$TimeoutSeconds = 300,
    [switch]$Existing,
    [string[]]$Addresses = @(),
    [string]$ExportName = '',
    [string]$LeafReference = '',
    [string[]]$Inspect = @(),
    [string[]]$Instructions = @(),
    [switch]$Dependencies,
    [string[]]$Vtables = @(),
    [switch]$FullExport,
    [switch]$RecoverBindings,
    [ValidateRange(1,600)][int]$FunctionTimeoutSeconds = 10,
    [string]$JavaHome = $env:JAVA_HOME
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$jdk = $JavaHome
if (-not $jdk) {
    $javaCommand = Get-Command java -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($javaCommand) { $jdk = Split-Path -Parent (Split-Path -Parent $javaCommand.Source) }
}
$ghidra = Join-Path $projectRoot 'tools\ghidra_11.4.2_PUBLIC'
if (-not $jdk -or -not (Test-Path -LiteralPath (Join-Path $jdk 'bin\java.exe'))) { throw 'Set JAVA_HOME or pass -JavaHome with the path to a JDK 21 installation.' }
$env:JAVA_HOME = $jdk
$env:PATH = "$jdk\bin;$env:PATH"
$cacheRoot = Join-Path $projectRoot 'analysis\ghidra-cache'
New-Item -ItemType Directory -Path $cacheRoot -Force | Out-Null
$projectDir = Join-Path $projectRoot 'analysis\ghidra-projects'
$logDir = Join-Path $projectRoot 'analysis\logs'
New-Item -ItemType Directory -Path $projectDir,$logDir -Force | Out-Null
$binaries = @{
    'launcher' = 'inputs\FarmingSimulator2015.exe'
    'game-runtime' = 'inputs\runtime\FarmingSimulator2015Game.runtime.bin'
    'server' = 'inputs\dedicatedServer.exe'
}
$binary = Join-Path $projectRoot $binaries[$Target]
if (-not $ExportName) { $ExportName = $Target }
if ($ExportName -notmatch '^[a-zA-Z0-9_-]+$') { throw 'Invalid export directory name' }
$out = Join-Path $projectRoot "recovered\decompiled\$ExportName"
$arguments = @($projectDir, "FS15_$Target")
if ($Existing) {
    $arguments += @('-process', (Split-Path -Leaf $binary), '-noanalysis')
} else {
    if (-not (Test-Path -LiteralPath $binary)) { throw "Missing input: $binary" }
    $arguments += @('-import', $binary)
}
$arguments += @('-analysisTimeoutPerFile', "$TimeoutSeconds", '-max-cpu', '4', '-scriptPath', (Join-Path $PSScriptRoot 'ghidra'))
if ($RecoverBindings) {
    if (-not $Existing -or $Target -ne 'game-runtime') { throw 'Binding recovery requires the existing game-runtime analysis.' }
    $bindingIndex = Get-Content -LiteralPath (Join-Path $projectRoot 'analysis/engine-bindings.json') -Raw | ConvertFrom-Json
    $bindingSeeds = @($bindingIndex.bindings | Where-Object { $_.implementation_kind -eq 'code_label' -or $_.pseudocode_reference_kind -eq 'code_label' } | Select-Object -ExpandProperty implementation -Unique)
    $arguments += @('-postScript', 'RecoverBindingFunctions.java', (Join-Path $projectRoot 'analysis/binding-discovery'))
    $arguments += $bindingSeeds
} elseif ($FullExport) {
    if (-not $Existing) { throw 'Full export requires -Existing: use the previously analyzed, verified program.' }
    $arguments += @('-postScript', 'ExportFullProgram.java', $out, "$FunctionTimeoutSeconds")
} elseif ($Vtables.Count) {
    if (-not $Existing -or $Target -ne 'game-runtime') { throw 'Vtable discovery requires the existing game-runtime analysis.' }
    foreach ($vtableSpec in $Vtables) {
        if ($vtableSpec -notmatch '^[0-9a-fA-F]+:[1-9][0-9]*$') { throw 'Use a quoted table-address:slot-count pair.' }
    }
    $vtableMarker = Join-Path $projectRoot 'analysis/vtable-discovery/complete.txt'
    if (Test-Path -LiteralPath $vtableMarker) { Remove-Item -LiteralPath $vtableMarker }
    $arguments += @('-postScript', 'RecoverVtableFunctions.java', (Join-Path $projectRoot 'analysis/vtable-discovery'))
    $arguments += $Vtables
} elseif ($Dependencies) {
    if (-not $Existing -or $Target -ne 'game-runtime') { throw 'Dependency export requires the existing game-runtime analysis.' }
    $dependencyMarker = Join-Path $projectRoot "analysis\native-dependencies\dependencies-complete.txt"
    if (Test-Path -LiteralPath $dependencyMarker) { Remove-Item -LiteralPath $dependencyMarker }
    $arguments += @('-postScript', 'ExportDependencies.java', (Join-Path $projectRoot 'analysis/native-dependencies'))
} elseif ($Instructions.Count) {
    foreach ($instructionAddress in $Instructions) {
        if ($instructionAddress -notmatch '^(0x)?[0-9a-fA-F]+$') { throw "Invalid instruction address: $instructionAddress (quote hexadecimal addresses)." }
    }
    $instructionMarker = Join-Path $projectRoot "analysis\inspection\$ExportName\instructions-complete.txt"
    if (Test-Path -LiteralPath $instructionMarker) { Remove-Item -LiteralPath $instructionMarker }
    $arguments += @('-postScript', 'ExportInstructions.java', (Join-Path $projectRoot "analysis\inspection\$ExportName"))
    $arguments += $Instructions
} elseif ($Inspect.Count) {
    $arguments += @('-postScript', 'InspectAddresses.java', (Join-Path $projectRoot "analysis\inspection\$ExportName"))
    $arguments += $Inspect
} elseif ($LeafReference) {
    $arguments += @('-postScript', 'ExportLeafReference.java', $LeafReference, (Join-Path $projectRoot 'inputs\reference'))
} else {
    $arguments += @('-postScript', 'ExportAnalysis.java', $out, "$Limit")
    $arguments += $Addresses
}
$arguments += @('-log', (Join-Path $logDir "$ExportName.log"), '-scriptlog', (Join-Path $logDir "$ExportName-script.log"))
$javaArguments = @('-Xmx2G', '-XX:ParallelGCThreads=2', '-XX:CICompilerCount=2',
    '-Djava.system.class.loader=ghidra.GhidraClassLoader', '-Dfile.encoding=UTF8',
    '-Duser.country=US', '-Duser.language=en', '-Dlog4j.skipJansi=true',
    "-Dapplication.settingsdir=$cacheRoot", "-Dapplication.cachedir=$cacheRoot",
    "-Duser.home=$cacheRoot", "-Djava.io.tmpdir=$cacheRoot", '-cp',
    (Join-Path $ghidra 'Ghidra\Framework\Utility\lib\Utility.jar'),
    'ghidra.Ghidra', 'ghidra.app.util.headless.AnalyzeHeadless') + $arguments
& (Join-Path $jdk 'bin\java.exe') @javaArguments
if ($LASTEXITCODE -ne 0) { throw "Ghidra failed with exit code $LASTEXITCODE" }
if ($Instructions.Count -and -not (Test-Path -LiteralPath $instructionMarker)) { throw 'Ghidra returned without completing the instruction export; inspect the script log.' }
if ($Dependencies -and -not (Test-Path -LiteralPath $dependencyMarker)) { throw 'Ghidra returned without completing the dependency export; inspect the script log.' }
if ($Vtables.Count -and -not (Test-Path -LiteralPath $vtableMarker)) { throw 'Ghidra returned without completing the vtable audit; inspect the script log.' }
if ($FullExport) {
    $exportProgress = Get-Content -LiteralPath (Join-Path $out 'progress.json') -Raw | ConvertFrom-Json
    if (-not $exportProgress.complete_pass) { throw 'Ghidra returned without completing the catalogue export; inspect the script log and resume.' }
    if ($exportProgress.failed -gt 0) { Write-Warning "$($exportProgress.failed) native functions remain unresolved; see decompiled-index.csv." }
}
