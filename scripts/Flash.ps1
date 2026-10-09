#requires -Version 7.0
param(
    [Parameter(Mandatory)]
    [ValidateSet('hello', 'ble', 'bringup', 'ui_demo', 'watch', 'product')]
    [string]$Example,
    [Parameter(Mandatory)]
    [ValidatePattern('^COM[1-9][0-9]*$')]
    [string]$Port,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$lock = Get-Content -LiteralPath (Join-Path $projectRoot 'sdk.lock.json') -Raw -Encoding utf8 | ConvertFrom-Json
if ($lock.board -ne 'sf32lb52-lchspi-ulp') { throw "Unexpected board: $($lock.board)" }

$projects = @{
    hello = 'vendor/SiFli-SDK/example/get-started/hello_world/rtt/project'
    ble = 'vendor/SiFli-SDK/example/ble/peripheral/project'
    bringup = 'apps/bringup/project'
    ui_demo = 'apps/ui_demo/project'
    watch = 'vendor/SiFli-SDK/example/multimedia/lvgl/watch/project'
    product = 'apps/product/project'
}
$buildDir = Join-Path $projectRoot (Join-Path $projects[$Example] "build_$($lock.board)_hcpu")
$manifestPath = Join-Path $buildDir 'sftool_param.json'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Build output missing: $manifestPath. Build $Example first."
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding utf8 | ConvertFrom-Json
if ($manifest.chip -ne 'SF32LB52' -or $manifest.memory -ne 'NOR' -or $manifest.write_flash.verify -ne $true) {
    throw 'The generated flash manifest does not match the locked Huangshan board.'
}
$expectedAddresses = @{
    'bootloader.bin' = '0x12010000'
    'main.bin' = '0x12020000'
    'ftab.bin' = '0x12000000'
}
$entries = @($manifest.write_flash.files)
if ($entries.Count -ne $expectedAddresses.Count) { throw 'The flash manifest must contain exactly three images.' }
$images = @()
foreach ($entry in $entries) {
    $name = [IO.Path]::GetFileName($entry.path)
    if (-not $expectedAddresses.ContainsKey($name) -or $entry.address -ne $expectedAddresses[$name]) {
        throw "Unexpected flash image or address: $($entry.path)@$($entry.address)"
    }
    if (@($images | Where-Object { $_.name -eq $name }).Count -ne 0) { throw "Duplicate image: $name" }
    $path = [IO.Path]::GetFullPath((Join-Path $buildDir $entry.path))
    $relative = [IO.Path]::GetRelativePath($buildDir, $path)
    if ([IO.Path]::IsPathRooted($relative) -or $relative -eq '..' -or $relative.StartsWith("..$([IO.Path]::DirectorySeparatorChar)")) {
        throw "Image path escapes build directory: $($entry.path)"
    }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -eq 0) {
        throw "Flash image missing or empty: $path"
    }
    $images += [ordered]@{
        name = $name
        path = $path
        address = $entry.address
        bytes = (Get-Item -LiteralPath $path).Length
        sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

$tool = Join-Path $projectRoot '.tools/sifli/tools/sftool/0.2.5/sftool.exe'
if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { throw "sftool missing: $tool" }
$args = @('-p', $Port, '-c', 'SF32LB52', '-m', 'nor', '-b', '500000',
          '--connect-attempts', '3', '--after', 'soft_reset', 'write_flash', '--verify')
$args += @($images | ForEach-Object { "$($_.path)@$($_.address)" })

Write-Host "Target: $Example ($($lock.board)) on $Port"
foreach ($image in $images) {
    Write-Host ("{0} -> {1}, {2} bytes, SHA-256 {3}" -f $image.name, $image.address, $image.bytes, $image.sha256)
}
if ($DryRun) {
    Write-Host 'DRY RUN: manifest and images validated; device was not opened.'
    return
}

$recordDir = Join-Path $projectRoot ("artifacts/flash/{0}/{1}" -f $Example, (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $recordDir -Force | Out-Null
$exitCode = 1
try {
    & $tool @args 2>&1 | Tee-Object -FilePath (Join-Path $recordDir 'sftool.log')
    $exitCode = $LASTEXITCODE
}
finally {
    [ordered]@{
        date = (Get-Date).ToString('o')
        example = $Example
        board = $lock.board
        port = $Port
        tool = $tool
        command = "$tool $($args -join ' ')"
        images = $images
        exit_code = $exitCode
        hardware_verified = $false
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $recordDir 'result.json') -Encoding utf8
}
if ($exitCode -ne 0) { throw "Flash failed with exit code $exitCode. See $recordDir/sftool.log." }
Write-Host "FLASH SUCCEEDED: $Example on $Port"
Write-Host "Flash evidence: $recordDir"
