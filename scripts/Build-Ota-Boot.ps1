#requires -Version 7.0
param([ValidateRange(1,32)][int]$Jobs = 4)
. "$PSScriptRoot/Set-LocalEnvironment.ps1"
Enable-SdkEnvironment
$bootProject = Join-Path $ProjectRoot 'apps/ota_boot/project'
$bootBoard = $SdkLock.board
$bootRun = Join-Path $ProjectRoot ("artifacts/ota_boot/{0}" -f (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $bootRun -Force | Out-Null
Push-Location $bootProject
try {
    & scons "--board=$bootBoard" "-j$Jobs" *>&1 | Tee-Object -FilePath (Join-Path $bootRun 'build.log')
    $bootExit = $LASTEXITCODE
} finally { Pop-Location }
$bootBuild = Join-Path $bootProject "build_${bootBoard}_hcpu"
$bootError = $null
$bootArtifacts = @()
if ($bootExit -ne 0) { $bootError = "SCons exited $bootExit" }
if ($bootExit -eq 0) {
    try {
        foreach ($name in 'main.elf','main.bin','main.map','.config','rtconfig.h','ptab.h','link_copy.lds') {
            if (-not (Test-Path -LiteralPath (Join-Path $bootBuild $name) -PathType Leaf)) { throw "Missing $name" }
        }
        foreach ($name in 'sftool_param.json','download.bat','uart_download.bat','ftab','bootloader') {
            if (Test-Path -LiteralPath (Join-Path $bootBuild $name)) { throw "Unexpected deployment artifact: $name" }
        }
        $map = Get-Content -LiteralPath (Join-Path $bootBuild 'main.map') -Raw -Encoding utf8
        if ($map -notmatch '(?m)^ROM\s+0x20020000\s+0x00010000\s' -or
            $map -notmatch '(?m)^RAM\s+0x20040000\s+0x00010000\s') { throw 'Unexpected boot RAM geometry' }
        if ((Get-Item -LiteralPath (Join-Path $bootBuild 'main.bin')).Length -gt 0x10000) { throw 'Boot exceeds 64 KiB storage budget' }
        & python (Join-Path $PSScriptRoot 'ota_boot_check.py') --elf (Join-Path $bootBuild 'main.elf') --output (Join-Path $bootRun 'elf-check.json')
        if ($LASTEXITCODE -ne 0) { throw 'RAM boot ELF validation failed' }
        foreach ($name in 'main.elf','main.bin','main.map','.config','rtconfig.h','ptab.h','link_copy.lds') {
            $path = Join-Path $bootBuild $name
            $bootArtifacts += [ordered]@{path=$path;bytes=(Get-Item -LiteralPath $path).Length;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
        }
    } catch { $bootError = $_.Exception.Message }
}
$bootSources = @(Get-ChildItem -LiteralPath $bootProject -File | Where-Object { $_.Name -match '^(SConstruct|SConscript|Kconfig.*|proj.conf|rtconfig.py|link.lds)$' })
$bootSources += @(Get-ChildItem -LiteralPath (Join-Path $bootProject "${bootBoard}_hcpu") -File)
$bootSources += @(Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'apps/ota_boot/src') -File)
foreach ($name in 'scripts/Build-Ota-Boot.ps1','scripts/ota_boot_check.py','scripts/Set-LocalEnvironment.ps1','sdk.lock.json') {
    $bootSources += Get-Item -LiteralPath (Join-Path $ProjectRoot $name)
}
$record = [ordered]@{
    kind='ram-ota-boot-prototype'; command="scons --board=$bootBoard -j$Jobs"
    exit_code=$bootExit; artifact_validation_passed=($bootExit -eq 0 -and $null -eq $bootError)
    artifact_validation_error=$bootError; deployable=$false; hardware_verified=$false
    sdk_commit=(Get-CheckedOutput git @('-C',$SdkRoot,'rev-parse','HEAD'))
    sdk_changes=@(Get-CheckedOutput git @('-C',$SdkRoot,'status','--short'))
    board=$bootBoard; build_directory=$bootBuild; artifacts=$bootArtifacts
    project_sources=@($bootSources | ForEach-Object { [ordered]@{path=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()} })
}
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $bootRun 'result.json') -Encoding utf8
Write-Host "RAM boot evidence: $bootRun"
if (-not $record.artifact_validation_passed) { throw "RAM boot build/validation failed: $bootError" }
Write-Host 'BUILD SUCCEEDED: RAM boot prototype; diagnostic only, no device operations'
