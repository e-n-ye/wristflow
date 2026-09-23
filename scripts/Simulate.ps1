#requires -Version 7.0
param(
    [switch]$BuildOnly,
    [string]$HostTools = 'D:/msys64/ucrt64/bin',
    [ValidateRange(1, 32)]
    [int]$Jobs = 4
)
. "$PSScriptRoot/Set-LocalEnvironment.ps1"
if (-not $IsWindows) { throw 'The current simulator uses the official LVGL Win32 driver.' }
$compiler = Join-Path $HostTools 'cc.exe'
$ninja = Join-Path $HostTools 'ninja.exe'
foreach ($tool in $compiler, $ninja) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "Missing host tool: $tool. Set -HostTools to an existing MSYS2 UCRT64 bin directory."
    }
}
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) { $cmake = Join-Path $env:ProgramFiles 'CMake/bin/cmake.exe' }
if (-not (Test-Path -LiteralPath $cmake -PathType Leaf)) { throw 'CMake 3.20+ is required.' }
$build = Join-Path $ProjectRoot 'artifacts/simulator-build'
$executable = Join-Path $build 'wristflow_simulator.exe'
if (Get-Process -Name wristflow_simulator -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $executable }) {
    throw 'Close the current WristFlow simulator window before rebuilding.'
}
$savedPath = $env:PATH
try {
    $env:PATH = "$HostTools;$env:PATH"
    & $cmake -S (Join-Path $ProjectRoot 'tests') -B $build -G Ninja `
        "-DCMAKE_C_COMPILER=$compiler" "-DCMAKE_MAKE_PROGRAM=$ninja" `
        -DCMAKE_BUILD_TYPE=Debug -DWRISTFLOW_BUILD_SIMULATOR=ON
    if ($LASTEXITCODE -ne 0) { throw 'Simulator configure failed.' }
    & $cmake --build $build --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Simulator build failed.' }
    & (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir $build --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Host UI checks failed.' }
    Write-Host "SIMULATOR BUILD SUCCEEDED: $executable"
    if (-not $BuildOnly) {
        Start-Process -FilePath $executable -WorkingDirectory $ProjectRoot -WindowStyle Normal
    }
}
finally { $env:PATH = $savedPath }
