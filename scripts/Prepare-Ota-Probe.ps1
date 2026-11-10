#requires -Version 7.0
param([Parameter(Mandatory)][string]$FirstBackup,[Parameter(Mandatory)][string]$SecondBackup,
      [Parameter(Mandatory)][string]$Out,[ValidateRange(1,32)][int]$Jobs=4)
$ErrorActionPreference='Stop'
. "$PSScriptRoot/Set-LocalEnvironment.ps1"
Enable-SdkEnvironment
$Out=[IO.Path]::GetFullPath($Out)
& python "$PSScriptRoot/ota_probe_package.py" prepare --first $FirstBackup --second $SecondBackup --out $Out
if ($LASTEXITCODE -ne 0) {throw 'Offline backup preparation failed'}
$plan=Get-Content -LiteralPath (Join-Path $Out 'plan.json') -Raw -Encoding utf8 | ConvertFrom-Json
& "$PSScriptRoot/Build-Ota-Slot-Probe.ps1" -Jobs $Jobs -Tag $plan.tag_hex -EvidenceDir (Join-Path $Out 'slot-build')
& python "$PSScriptRoot/ota_probe_package.py" bind --out $Out
if ($LASTEXITCODE -ne 0) {throw 'Probe binding failed'}
& "$PSScriptRoot/Build-Ota-Boot.ps1" -Jobs $Jobs -ProbeBinding (Join-Path $Out 'binding.json') -EvidenceDir (Join-Path $Out 'boot-build')
& python "$PSScriptRoot/ota_probe_package.py" finish --out $Out
if ($LASTEXITCODE -ne 0) {throw 'Execution review package failed'}
Write-Host "REVIEW PACKAGE READY: $Out; no device commands or deployment authorization"
