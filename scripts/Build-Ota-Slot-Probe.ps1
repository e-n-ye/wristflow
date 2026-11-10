#requires -Version 7.0
param([ValidateRange(1,32)][int]$Jobs = 4, [ValidatePattern('^[0-9a-f]{32}$')][string]$Tag = ('0'*32), [string]$EvidenceDir)
. "$PSScriptRoot/Set-LocalEnvironment.ps1"
Enable-SdkEnvironment
$probeProject = Join-Path $ProjectRoot 'apps/ota_slot_probe/project'
$probeBoard = $SdkLock.board
$probeRun = if ($EvidenceDir) {[IO.Path]::GetFullPath($EvidenceDir)} else {Join-Path $ProjectRoot ("artifacts/ota_slot_probe/{0}" -f (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))}
if (Test-Path -LiteralPath $probeRun) {throw 'Evidence directory must be new'}
New-Item -ItemType Directory -Path $probeRun -Force | Out-Null
Push-Location $probeProject
try {
    & scons "--board=$probeBoard" "-j$Jobs" "wf_probe_tag=$Tag" *>&1 | Tee-Object -FilePath (Join-Path $probeRun 'build.log')
    $probeExit = $LASTEXITCODE
} finally { Pop-Location }
$probeBuild = Join-Path $probeProject "build_${probeBoard}_hcpu"
$probeError = $null
$probeArtifacts = @()
if ($probeExit -ne 0) { $probeError = "SCons exited $probeExit" }
if ($probeExit -eq 0) {
    try {
        foreach ($name in 'main.elf','main.bin','main.map','.config','rtconfig.h','ptab.h','link_copy.lds') {
            if (-not (Test-Path -LiteralPath (Join-Path $probeBuild $name) -PathType Leaf)) { throw "Missing $name" }
        }
        foreach ($name in 'sftool_param.json','download.bat','uart_download.bat','ftab','bootloader') {
            if (Test-Path -LiteralPath (Join-Path $probeBuild $name)) { throw "Unexpected deployment artifact: $name" }
        }
        & python (Join-Path $PSScriptRoot 'ota_slot_check.py') --elf (Join-Path $probeBuild 'main.elf') --bin (Join-Path $probeBuild 'main.bin') --output (Join-Path $probeRun 'elf-check.json')
        if ($LASTEXITCODE -ne 0) { throw 'Slot probe ELF validation failed' }
        foreach ($name in 'main.elf','main.bin','main.map','.config','rtconfig.h','ptab.h','link_copy.lds') {
            $path = Join-Path $probeBuild $name
            $probeArtifacts += [ordered]@{path=$path;bytes=(Get-Item -LiteralPath $path).Length;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
        }
    } catch { $probeError = $_.Exception.Message }
}
$probeSources = @(Get-ChildItem -LiteralPath $probeProject -File | Where-Object { $_.Name -match '^(SConstruct|SConscript|Kconfig.*|proj.conf|rtconfig.py|link.lds)$' })
$probeSources += @(Get-ChildItem -LiteralPath (Join-Path $probeProject "${probeBoard}_hcpu") -File)
$probeSources += @(Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'apps/ota_slot_probe/src') -File)
$probeSources+=@(Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'apps/ota_probe_support') -File)
foreach ($name in 'scripts/Build-Ota-Slot-Probe.ps1','scripts/ota_slot_check.py','scripts/Set-LocalEnvironment.ps1','sdk.lock.json') {
    $probeSources += Get-Item -LiteralPath (Join-Path $ProjectRoot $name)
}
$record = [ordered]@{
    kind='ota-slot-execution-probe'; command="scons --board=$probeBoard -j$Jobs wf_probe_tag=$Tag"; probe_tag=$Tag
    exit_code=$probeExit; artifact_validation_passed=($probeExit -eq 0 -and $null -eq $probeError)
    artifact_validation_error=$probeError; deployable=$false; hardware_verified=$false
    sdk_commit=(Get-CheckedOutput git @('-C',$SdkRoot,'rev-parse','HEAD'))
    sdk_changes=@(Get-CheckedOutput git @('-C',$SdkRoot,'status','--short'))
    board=$probeBoard; build_directory=$probeBuild; artifacts=$probeArtifacts
    project_sources=@($probeSources | ForEach-Object { [ordered]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()} })
}
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $probeRun 'result.json') -Encoding utf8
Write-Host "Slot probe evidence: $probeRun"
if (-not $record.artifact_validation_passed) { throw "Slot probe build/validation failed: $probeError" }
Write-Host 'BUILD SUCCEEDED: A-linked minimal slot probe; no device operations'
