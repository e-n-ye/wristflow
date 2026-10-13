[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SourceRoot,
    [Parameter(Mandatory)][string]$JavaHome,
    [Parameter(Mandatory)][string]$AndroidSdk,
    [Parameter(Mandatory)][string]$OutputRoot,
    [string]$GradleCache,
    [switch]$CheckOnly
)
$ErrorActionPreference = 'Stop'
$lock = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'upstream.lock.json') -Raw -Encoding utf8 | ConvertFrom-Json
$source = (Resolve-Path -LiteralPath $SourceRoot).Path
$patch = Join-Path $PSScriptRoot $lock.patch.file
if ((Get-FileHash -LiteralPath $patch -Algorithm SHA256).Hash.ToLowerInvariant() -ne $lock.patch.sha256) {
    throw 'Patch hash mismatch.'
}
$head = & git -C $source rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $head -ne $lock.upstream.commit) { throw 'Upstream HEAD does not match the lock.' }
$encoding = [System.Text.UTF8Encoding]::new($false, $true)
foreach ($entry in $lock.patched_files.PSObject.Properties) {
    $text = [System.IO.File]::ReadAllText((Join-Path $source $entry.Name), $encoding).Replace("`r`n", "`n")
    $hash = [Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($encoding.GetBytes($text))).ToLowerInvariant()
    if ($hash -ne $entry.Value) { throw "Patched source mismatch: $($entry.Name)" }
}
$status = @(& git -C $source status --porcelain --untracked-files=all)
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect the upstream checkout.' }
foreach ($line in $status) {
    if ($line.Substring(3) -notin $lock.patched_files.PSObject.Properties.Name) {
        throw 'Checkout contains changes outside this patch. Preserve them and use a separate checkout.'
    }
}
$jdk = (Resolve-Path -LiteralPath $JavaHome).Path
$sdk = (Resolve-Path -LiteralPath $AndroidSdk).Path
$output = [System.IO.Path]::GetFullPath($OutputRoot)
$project = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if ($output.TrimEnd('\') -eq $source.TrimEnd('\') -or $output.TrimEnd('\') -eq $project.TrimEnd('\')) {
    throw 'OutputRoot must be a dedicated artifact directory, not a project root.'
}
foreach ($package in @($lock.build.sdk_platform, ('build-tools;' + $lock.build.build_tools))) {
    $packageDirectory = $package.Replace(';', '/')
    $packageFile = Join-Path $sdk ($packageDirectory + '/package.xml')
    if (-not (Test-Path -LiteralPath $packageFile)) { throw "Missing SDK package: $package" }
    [xml]$packageXml = Get-Content -LiteralPath $packageFile -Raw
    $packageNode = $packageXml.SelectSingleNode("//*[local-name()='localPackage']")
    if (-not $packageNode -or $packageNode.GetAttribute('path') -ne $package) {
        throw "SDK package metadata mismatch: $package"
    }
}
if (-not (Test-Path -LiteralPath (Join-Path $jdk 'bin/java.exe'))) { throw 'JDK is missing.' }
$javaVersion = @(& (Join-Path $jdk 'bin/java.exe') -version 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0 -or $javaVersion -notmatch ('version "' + $lock.build.jdk_major + '\.')) {
    throw 'JDK major version does not match the lock.'
}
$localProperties = Join-Path $source 'local.properties'
if (Test-Path -LiteralPath $localProperties) {
    $sdkLine = Get-Content -LiteralPath $localProperties | Where-Object { $_ -match '^sdk\.dir=' } | Select-Object -First 1
    if ($sdkLine) {
        $existingSdk = $sdkLine.Substring(8).Replace('\\', '\').Replace('\:', ':').Replace('/', '\')
        if ([System.IO.Path]::GetFullPath($existingSdk) -ne $sdk) { throw 'local.properties points to a different SDK.' }
    }
}
if ($CheckOnly) { Write-Output 'Pinned source, patch, JDK and SDK checks passed.'; return }
$run = Join-Path $output (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
[System.IO.Directory]::CreateDirectory($run) | Out-Null
$saved = @{}
foreach ($name in @('JAVA_HOME', 'ANDROID_HOME', 'ANDROID_SDK_ROOT', 'GRADLE_USER_HOME', 'PATH')) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
$gradleArgs = @('--no-daemon', '--max-workers=4', '--console=plain', '--stacktrace',
    ':app:testMainlineWristflowUnitTest', '--tests',
    'nodomain.freeyourgadget.gadgetbridge.service.devices.banglejs.WristFlowMetadataTest',
    ':app:assembleMainlineWristflow')
try {
    $env:JAVA_HOME = $jdk
    $env:ANDROID_HOME = $sdk
    $env:ANDROID_SDK_ROOT = $sdk
    $env:GRADLE_USER_HOME = if ($GradleCache) { [System.IO.Path]::GetFullPath($GradleCache) } else { Join-Path $output 'gradle-home' }
    $env:PATH = (Join-Path $jdk 'bin') + [System.IO.Path]::PathSeparator + $saved['PATH']
    Push-Location -LiteralPath $source
    try {
        # Process-local paths only. This build neither installs an APK nor accesses a device.
        & (Join-Path $source 'gradlew.bat') @gradleArgs *> (Join-Path $run 'gradle.log')
        $code = $LASTEXITCODE
    } finally { Pop-Location }
    $result = [ordered]@{ upstream_commit = $head; patch_sha256 = $lock.patch.sha256; command = $gradleArgs; exit_code = $code }
    if ($code -eq 0) {
        $apk = @(Get-ChildItem -LiteralPath (Join-Path $source 'app/build/outputs/apk/mainline/wristflow') -Filter '*.apk' -File)
        if ($apk.Count -ne 1) { throw 'Expected exactly one mainlineWristflow APK.' }
        $destination = Join-Path $run $apk[0].Name
        Copy-Item -LiteralPath $apk[0].FullName -Destination $destination
        $result.apk = $destination
        $result.apk_sha256 = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8
    Write-Output "Gradle exit=$code; evidence=$run"
    if ($code -ne 0) { throw "Gradle failed. Preserve the log: $run" }
} finally {
    foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
}
