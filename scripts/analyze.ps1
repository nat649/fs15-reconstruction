param(
    [ValidateSet('launcher','game-runtime','server')][string]$Target = 'launcher',
    [int]$Limit = 200,
    [int]$TimeoutSeconds = 300,
    [switch]$Existing,
    [string[]]$Addresses = @(),
    [string]$ExportName = '',
    [string]$LeafReference = '',
    [string[]]$Inspect = @(),
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
if ($Inspect.Count) {
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
