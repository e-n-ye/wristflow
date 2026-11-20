#requires -Version 7.0
param([ValidateRange(1,32)][int]$Jobs = 4, [switch]$AndroidReceiver)
. "$PSScriptRoot/Set-LocalEnvironment.ps1"
Enable-SdkEnvironment
$candidateProject = Join-Path $ProjectRoot 'apps/ota_loader/project'
$candidateBoard = $SdkLock.board
$uiProject = Join-Path $ProjectRoot 'ui/ota_receiver'
$candidateRun = Join-Path $ProjectRoot ("artifacts/ota_loader/{0}" -f (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $candidateRun -Force | Out-Null
$candidateExit = 1
Push-Location $candidateProject
try {
    $argsForScons = @("--board=$candidateBoard", "-j$Jobs")
    if ($AndroidReceiver) { $argsForScons += 'wf_android_receiver=1' }
    & scons @argsForScons *>&1 | Tee-Object -FilePath (Join-Path $candidateRun 'build.log')
    $candidateExit = $LASTEXITCODE
} finally { Pop-Location }
$candidateBuild = Join-Path $candidateProject "build_${candidateBoard}_hcpu"
$candidateError = $null
$enrollAddress = $null
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
        if ($map -notmatch '(?m)^ROM\s+0x12820000\s+0x00080000\s') { throw 'Unexpected loader ROM geometry' }
        if ((Get-Item -LiteralPath (Join-Path $candidateBuild 'main.bin')).Length -gt 0x80000) { throw 'Loader exceeds its partition' }
        $symbols = (& arm-none-eabi-nm (Join-Path $candidateBuild 'main.elf')) -join "`n"
        if ($LASTEXITCODE -ne 0) { throw 'nm failed' }
        $symbols | Set-Content -LiteralPath (Join-Path $candidateRun 'symbols.txt') -Encoding utf8
        if ($symbols -notmatch '(?m)^[0-9a-f]+ T BSP_PowerUpCustom$') { throw 'Strong startup override missing' }
        $required = @('wf_ota_physical_read','wf_ota_journal_load','wristflow_aw32001_inhibit','wf_ota_release_verify_slot','mbedtls_pk_verify')
        if ($AndroidReceiver) {
            $enrolled = [regex]::Match($symbols, '(?m)^([0-9a-f]+) [RrTt] wf_ota_original_a\r?$')
            if (!$enrolled.Success) { throw 'Detached enrollment is missing from receiver ELF' }
            $enrollAddress = [Convert]::ToUInt32($enrolled.Groups[1].Value,16)
            $bin = [IO.File]::ReadAllBytes((Join-Path $candidateBuild 'main.bin'))
            $offset = [long]$enrollAddress - 0x12820000
            if ($offset -lt 0 -or $offset+336 -gt $bin.Length) { throw 'Detached enrollment outside receiver binary' }
            $digest = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([byte[]]$bin[$offset..($offset+335)])).ToLowerInvariant()
            if ($digest -cne 'fdda8fed5e37c097a0a9fa7d9a8b590b7025d0451c81d35af4c78c93af7dad85') { throw 'Detached CycleLog enrollment changed' }
            $required += @('wf_ota_android_start','wf_dfu_request','wf_ota_loader_install','wf_ota_journal_commit','ble_serial_tran_init',
                           'wf_ota_display_publish','screen_ota_receiver_create','lv_arc_set_value',
                           'sifli_nvds_flash_adaptor_init','sifli_nvds_flash_adaptor_read','sifli_nvds_flash_adaptor_write')
            # Bond removal can be garbage-collected in this profile. If linked,
            # it must resolve to the strong loader backend.
            if ($symbols -match '(?m)^[0-9a-f]+ [Ww] sifli_nvds_flash_adaptor_delete$') { throw 'Persistent NVDS delete hook linked' }
            if ($map -notmatch '(?s)\.bss\.l2_non_ret_bss_ota\s+0x60[0-7][0-9a-f]{5}\s+0x400014\b') { throw 'Missing bounded receiver storage in PSRAM' }
            if ($map -notmatch '(?s)\.bss\.l1_non_ret_bss_frambuf\s+0x20[0-7][0-9a-f]{5}\s+0x4920\b') { throw 'Expected one 390x24 RGB565 buffer in SRAM' }
            if ($symbols -match '(?m)\b(lv_draw_epic_init|lv_touchscreen_create)\s*$') { throw 'Unexpected GPU or touch input in OTA UI' }
        } else { $required += 'wf_ota_loader_read_probe' }
        foreach ($name in $required) {
            if ($symbols -notmatch "(?m)^[0-9a-f]+ T $name`$") { throw "Missing probe implementation: $name" }
        }
        if ($symbols -match '(?m)\b(aw32001_init|aw32001_charge_enable|dfu_engine_erase|dfu_engine_write|dfu_package_install_set|wf_ota_journal_provision|fdb_kvdb_init|__fsym___cmd_fal|HAL_FLASH_ALIAS_CFG|HAL_FLASH_SET_ALIAS_RANGE|HAL_FLASH_SET_ALIAS_OFFSET)\s*$') {
            throw 'Unexpected charger, stock installer, database initialization, provisioning or alias implementation'
        }
        if (!$AndroidReceiver -and $symbols -match '(?m)\bwf_ota_journal_commit\s*$') { throw 'Writable probe' }
        if ($symbols -match '(?m)\b__fsym_\w+\s*$') { throw 'Unexpected interactive shell export' }
        $config = Get-Content -LiteralPath (Join-Path $candidateBuild 'rtconfig.h') -Raw -Encoding utf8
        if ($config -match '(?m)^#define (BSP_USING_CHARGER|CHARGE_USING_AW32001|BSP_USING_TOUCHD|BSP_USING_PM|RT_USING_PM|RT_USING_FINSH|USING_DFU_V2|BSP_USING_DFU|BSP_USING_LCD_FRAMEBUFFER)\b') {
            throw 'Unexpected charger, touch, power, shell, stock DFU or full framebuffer configuration'
        }
        if (!$AndroidReceiver -and $config -match '(?m)^#define (BLUETOOTH|PKG_USING_FLASHDB|WF_OTA_ANDROID_RECEIVER|BSP_USING_LCD|PKG_USING_LITTLEVGL2RTT)\b') { throw 'Radio or display in read-only probe' }
        if ($AndroidReceiver -and $config -notmatch '(?m)^#define LCD_USING_CO5300\b') { throw 'Receiver must use its CO5300 display' }
        if ($AndroidReceiver -and $config -notmatch '(?m)^#define BLE_CM_BOND_DISABLE\b') { throw 'Bonding must be disabled in the upgrade environment' }
        if ($config -notmatch '(?m)^#define CUSTOM_MEM_MAP\b') { throw 'Project memory map not selected' }
        if ($AndroidReceiver) { $frozen = New-Item -ItemType Directory -Path (Join-Path $candidateRun 'frozen') }
        foreach ($name in 'main.elf','main.bin','main.map','.config','rtconfig.h','ptab.h','custom_mem_map.h') {
            $path = Join-Path $candidateBuild $name
            if ($AndroidReceiver) {
                Copy-Item -LiteralPath $path -Destination $frozen.FullName -ErrorAction Stop
                $path = Join-Path $frozen.FullName $name
            }
            $candidateFiles += [ordered]@{path=$path;bytes=(Get-Item -LiteralPath $path).Length;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
        }
    } catch { $candidateError = $_.Exception.Message }
}
$candidateSources = @(Get-ChildItem -LiteralPath $candidateProject -File | Where-Object { $_.Name -match '^(SConstruct|SConscript|Kconfig.*|proj.conf|rtconfig.py)$' })
$candidateSources += @(Get-ChildItem -LiteralPath (Join-Path $candidateProject "${candidateBoard}_hcpu") -File)
$candidateSources += @(Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'apps/ota_loader/src') -File)
if ($AndroidReceiver) {
    $candidateSources += @(Get-ChildItem -LiteralPath $uiProject -File |
        Where-Object { $_.Name -match '\.(xml|c|h|cmake)$' })
    foreach ($dir in 'fonts','screens') {
        $candidateSources += @(Get-ChildItem -LiteralPath (Join-Path $uiProject $dir) -File)
    }
    foreach ($name in 'apps/product/src/product_power.c','apps/product/src/product_power.h','core/power_state.h','core/endurance.c','core/endurance.h') {
        $candidateSources += Get-Item -LiteralPath (Join-Path $ProjectRoot $name)
    }
}
foreach ($name in 'apps/product/src/aw32001_inhibit.c','apps/product/src/aw32001_inhibit.h','core/ota_ab.c','core/ota_ab.h','core/ota_journal.c','core/ota_journal.h','scripts/Build-Ota-Loader.ps1','scripts/Set-LocalEnvironment.ps1','sdk.lock.json') {
    $candidateSources += Get-Item -LiteralPath (Join-Path $ProjectRoot $name)
}
$record = [ordered]@{
    kind=$(if ($AndroidReceiver) {'standalone-ota-android-receiver'} else {'standalone-ota-loader-probe'}); command="scons $($argsForScons -join ' ')"
    exit_code=$candidateExit; artifact_validation_passed=($candidateExit -eq 0 -and $null -eq $candidateError)
    original_a_enrollment_address=$enrollAddress
    artifact_validation_error=$candidateError; deployable=$false; hardware_verified=$false
    sdk_commit=(Get-CheckedOutput git @('-C',$SdkRoot,'rev-parse','HEAD'))
    sdk_changes=@(Get-CheckedOutput git @('-C',$SdkRoot,'status','--short'))
    board=$candidateBoard; build_directory=$candidateBuild; artifacts=$candidateFiles
    project_sources=@($candidateSources | ForEach-Object { [ordered]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()} })
}
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $candidateRun 'result.json') -Encoding utf8
Write-Host "Loader probe evidence: $candidateRun"
if (-not $record.artifact_validation_passed) { throw "Loader probe build/validation failed: $candidateError" }
Write-Host "BUILD SUCCEEDED: $($record.kind)"
