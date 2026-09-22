#requires -Version 7.0
param(
    [ValidateSet('hello', 'ble', 'bringup')]
    [string]$Example = 'hello',
    [ValidateRange(1, 32)]
    [int]$Jobs = 4
)
. "$PSScriptRoot/Set-LocalEnvironment.ps1"
# Piped Rich output otherwise defaults to 80 columns and clips the size table.
$env:COLUMNS = '120'
Enable-SdkEnvironment
$projects = @{
    hello = Join-Path $SdkRoot 'example/get-started/hello_world/rtt/project'
    ble = Join-Path $SdkRoot 'example/ble/peripheral/project'
    bringup = Join-Path $ProjectRoot 'apps/bringup/project'
}
$projectPath = $projects[$Example]
$board = $SdkLock.board
$runDir = Join-Path $ProjectRoot ("artifacts/{0}/{1}" -f $Example, (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $runDir -Force | Out-Null
$buildExit = 1
$startedAt = (Get-Date).ToString('o')
Push-Location $projectPath
try {
    & scons "--board=$board" "-j$Jobs" *>&1 | Tee-Object -FilePath (Join-Path $runDir 'build.log')
    $buildExit = $LASTEXITCODE
}
finally { Pop-Location }

$buildDir = Join-Path $projectPath "build_${board}_hcpu"
if (-not (Test-Path $buildDir)) { $buildDir = Join-Path $projectPath "build_$board" }
$files = @()
if (Test-Path $buildDir) {
    $files = @(Get-ChildItem $buildDir -Recurse -File | Where-Object {
        $_.Extension -in '.elf', '.axf', '.bin', '.hex', '.map' -or $_.Name -in '.config', 'rtconfig.h', 'sftool_param.json'
    } | ForEach-Object {
        [ordered]@{ path = $_.FullName; bytes = $_.Length; sha256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    })
    foreach ($name in '.config', 'rtconfig.h') {
        $configPath = Join-Path $buildDir $name
        if (Test-Path $configPath) { Copy-Item -LiteralPath $configPath -Destination $runDir }
    }
}
$sourceFiles = @(Get-ChildItem -LiteralPath $projectPath -File)
$sourceFiles += @(Get-ChildItem -LiteralPath (Join-Path $projectPath '../src') -Recurse -File)
$sourceHashes = @($sourceFiles | Where-Object {
    $_.Name -match '^(SConstruct|SConscript|Kconfig.*|proj.conf|rtconfig.py)$' -or $_.Extension -in '.c', '.h'
} | ForEach-Object {
    [ordered]@{ path = $_.FullName; sha256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
})
$firmwarePath = $null
$artifactValidationError = $null
if ($buildExit -eq 0) {
    try {
        foreach ($required in 'main.elf', '.config', 'rtconfig.h', 'sftool_param.json') {
            $requiredPath = Join-Path $buildDir $required
            if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf) -or (Get-Item -LiteralPath $requiredPath).Length -eq 0) {
                throw "Required build artifact missing or empty: $requiredPath"
            }
        }
        # The SDK's legacy and YAML partition formats emit different binary layouts.
        $manifest = Get-Content -LiteralPath (Join-Path $buildDir 'sftool_param.json') -Raw -Encoding utf8 | ConvertFrom-Json
        $images = @($manifest.write_flash.files)
        foreach ($name in 'main.bin', 'bootloader.bin', 'ftab.bin') {
            $matchingImages = @($images | Where-Object { [IO.Path]::GetFileName($_.path) -eq $name })
            if ($matchingImages.Count -ne 1) { throw "Expected one $name in sftool_param.json." }
            if ($name -eq 'main.bin') { $firmwarePath = Join-Path $buildDir $matchingImages[0].path }
        }
        foreach ($image in $images) {
            if ([string]::IsNullOrWhiteSpace($image.path)) { throw 'Empty image path in sftool_param.json.' }
            $imagePath = Join-Path $buildDir $image.path
            if (-not (Test-Path -LiteralPath $imagePath -PathType Leaf) -or (Get-Item -LiteralPath $imagePath).Length -eq 0) {
                throw "Manifest image missing or empty: $imagePath"
            }
        }
    }
    catch { $artifactValidationError = $_.Exception.Message }
}
$record = [ordered]@{
    started_at = $startedAt
    date = (Get-Date).ToString('o')
    example = $Example
    command = "scons --board=$board -j$Jobs"
    working_directory = $projectPath
    sdk_commit = (Get-CheckedOutput git @('-C', $SdkRoot, 'rev-parse', 'HEAD'))
    submodules = @(Get-CheckedOutput git @('-C', $SdkRoot, 'submodule', 'status', '--recursive'))
    sdk_changes = @(Get-CheckedOutput git @('-C', $SdkRoot, 'status', '--short'))
    python = (Get-CheckedOutput python @('--version'))
    compiler = @(Get-CheckedOutput arm-none-eabi-gcc @('--version'))[0]
    scons = @(Get-CheckedOutput scons @('--version'))
    exit_code = $buildExit
    artifact_validation_passed = ($buildExit -eq 0 -and $null -eq $artifactValidationError)
    artifact_validation_error = $artifactValidationError
    firmware_path = $firmwarePath
    hardware_verified = $false
    project_sources = $sourceHashes
    build_directory = $buildDir
    artifacts = $files
}
$record | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $runDir 'result.json') -Encoding utf8
Write-Host "Build evidence: $runDir"
if ($buildExit -ne 0) { throw "Build failed with exit code $buildExit. See build.log." }
if ($artifactValidationError) { throw $artifactValidationError }
Write-Host "BUILD SUCCEEDED: $Example ($board)"
Write-Host "Firmware: $firmwarePath"
