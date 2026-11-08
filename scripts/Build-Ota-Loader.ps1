#requires -Version 7.0
param([ValidateRange(1,32)][int]$Jobs = 4)
. "$PSScriptRoot/Set-LocalEnvironment.ps1"
Enable-SdkEnvironment
$candidateProject = Join-Path $ProjectRoot 'apps/ota_loader/project'
$candidateBoard = $SdkLock.board
$candidateRun = Join-Path $ProjectRoot ("artifacts/ota_loader/{0}" -f (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $candidateRun -Force | Out-Null
$candidateExit = 1
Push-Location $candidateProject
try {
    & scons "--board=$candidateBoard" "-j$Jobs" *>&1 | Tee-Object -FilePath (Join-Path $candidateRun 'build.log')
    $candidateExit = $LASTEXITCODE
} finally { Pop-Location }
$candidateBuild = Join-Path $candidateProject "build_${candidateBoard}_hcpu"
$candidateError = $null
if ($candidateExit -ne 0) { $candidateError = "SCons exited $candidateExit" }
$candidateFiles = @()
if ($candidateExit -eq 0) {
    try {
        foreach ($name in 'main.elf','main.bin','main.map','.config','rtconfig.h','ptab.h','custom_mem_map.h') {
            $path = Join-Path $candidateBuild $name
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing $name" }
        }
        foreach ($name in 'sftool_param.json','download.bat','uart_download.bat') {
            if (Test-Path -LiteralPath (Join-Path $candidateBuild $name)) { throw "Unexpected deploy manifest: $name" }
        }
        $map = Get-Content -LiteralPath (Join-Path $candidateBuild 'main.map') -Raw -Encoding utf8
        if ($map -notmatch '(?m)^ROM\s+0x12820000\s+0x00060000\s') { throw 'Unexpected loader ROM geometry' }
        if ((Get-Item -LiteralPath (Join-Path $candidateBuild 'main.bin')).Length -gt 0x60000) { throw 'Loader probe exceeds candidate region' }
        $symbols = (& arm-none-eabi-nm (Join-Path $candidateBuild 'main.elf')) -join "`n"
        if ($LASTEXITCODE -ne 0) { throw 'nm failed' }
        $symbols | Set-Content -LiteralPath (Join-Path $candidateRun 'symbols.txt') -Encoding utf8
        if ($symbols -notmatch '(?m)^[0-9a-f]+ T BSP_PowerUpCustom$') { throw 'Strong startup override missing' }
        foreach ($name in 'wf_ota_loader_read_probe','wf_ota_physical_read','wf_ota_journal_load','wristflow_aw32001_inhibit') {
            if ($symbols -notmatch "(?m)^[0-9a-f]+ T $name`$") { throw "Missing probe implementation: $name" }
        }
        if ($symbols -match '(?m)\b(aw32001_init|aw32001_charge_enable|dfu_engine_erase|dfu_engine_write|wf_ota_journal_commit|wf_ota_journal_provision|__fsym___cmd_fal|HAL_FLASH_ALIAS_CFG|HAL_FLASH_SET_ALIAS_RANGE|HAL_FLASH_SET_ALIAS_OFFSET)\s*$') {
            throw 'Unexpected automatic charger, writable FAL shell, journal mutation or alias implementation'
        }
        if ($symbols -match '(?m)\b__fsym_\w+\s*$') { throw 'Unexpected interactive shell export' }
        $config = Get-Content -LiteralPath (Join-Path $candidateBuild 'rtconfig.h') -Raw -Encoding utf8
        if ($config -match '(?m)^#define (BSP_USING_CHARGER|CHARGE_USING_AW32001|BSP_USING_LCD|BSP_USING_TOUCHD|BSP_USING_PM|RT_USING_PM|RT_USING_FINSH|BLUETOOTH|USING_DFU_V2|BSP_USING_DFU|PKG_USING_FLASHDB)\b') {
            throw 'Unexpected persistent/radio/display service configuration'
        }
        if ($config -notmatch '(?m)^#define CUSTOM_MEM_MAP\b') { throw 'Project memory map not selected' }
        foreach ($name in 'main.elf','main.bin','main.map','.config','rtconfig.h','ptab.h','custom_mem_map.h') {
            $path = Join-Path $candidateBuild $name
            $candidateFiles += [ordered]@{path=$path;bytes=(Get-Item -LiteralPath $path).Length;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
        }
    } catch { $candidateError = $_.Exception.Message }
}
$candidateSources = @(Get-ChildItem -LiteralPath $candidateProject -File | Where-Object { $_.Name -match '^(SConstruct|SConscript|Kconfig.*|proj.conf|rtconfig.py)$' })
$candidateSources += @(Get-ChildItem -LiteralPath (Join-Path $candidateProject "${candidateBoard}_hcpu") -File)
$candidateSources += @(Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'apps/ota_loader/src') -File)
foreach ($name in 'apps/product/src/aw32001_inhibit.c','apps/product/src/aw32001_inhibit.h','core/ota_ab.c','core/ota_ab.h','core/ota_journal.c','core/ota_journal.h','scripts/Build-Ota-Loader.ps1','scripts/Set-LocalEnvironment.ps1','sdk.lock.json') {
    $candidateSources += Get-Item -LiteralPath (Join-Path $ProjectRoot $name)
}
$record = [ordered]@{
    kind='standalone-ota-loader-probe'; command="scons --board=$candidateBoard -j$Jobs"
    exit_code=$candidateExit; artifact_validation_passed=($candidateExit -eq 0 -and $null -eq $candidateError)
    artifact_validation_error=$candidateError; deployable=$false; hardware_verified=$false
    sdk_commit=(Get-CheckedOutput git @('-C',$SdkRoot,'rev-parse','HEAD'))
    sdk_changes=@(Get-CheckedOutput git @('-C',$SdkRoot,'status','--short'))
    board=$candidateBoard; build_directory=$candidateBuild; artifacts=$candidateFiles
    project_sources=@($candidateSources | ForEach-Object { [ordered]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()} })
}
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $candidateRun 'result.json') -Encoding utf8
Write-Host "Loader probe evidence: $candidateRun"
if (-not $record.artifact_validation_passed) { throw "Loader probe build/validation failed: $candidateError" }
Write-Host 'BUILD SUCCEEDED: standalone loader probe; no deployment scripts or device operations'
