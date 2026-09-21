#requires -Version 7.0
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$SdkRoot = Join-Path $ProjectRoot 'vendor/SiFli-SDK'
$env:SIFLI_SDK_TOOLS_PATH = Join-Path $ProjectRoot '.tools/sifli'
$env:UV_PYTHON_INSTALL_DIR = Join-Path $ProjectRoot '.tools/python'
$env:UV_PYTHON_BIN_DIR = Join-Path $ProjectRoot '.tools/python-bin'
$env:UV_CACHE_DIR = Join-Path $ProjectRoot '.tools/uv-cache'
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
# Match native-process decoding to Python's UTF-8 output in VS Code tasks.
$utf8 = [System.Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $utf8
[Console]::OutputEncoding = $utf8
$OutputEncoding = $utf8
$env:SIFLI_SDK_MIRROR_CHINA = '0'
$env:SIFLI_SDK_GITHUB_ASSETS = 'https://downloads.sifli.com/github_assets'
# The preset's Python metadata endpoint failed TLS on this host.
$env:UV_PYTHON_DOWNLOADS_JSON_URL = $null

$SdkLock = Get-Content (Join-Path $ProjectRoot 'sdk.lock.json') -Raw -Encoding utf8 | ConvertFrom-Json
$actualHead = & git -C $SdkRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actualHead -ne $SdkLock.sdk.commit) {
    throw 'SDK checkout does not match sdk.lock.json. Initialize the pinned SDK first.'
}
foreach ($entry in $SdkLock.submodules.PSObject.Properties) {
    $modulePath = Join-Path $SdkRoot $entry.Name
    $moduleHead = & git -C $modulePath rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or $moduleHead -ne $entry.Value) {
        throw "Submodule mismatch: $($entry.Name). Run git submodule update --init --recursive."
    }
}
$sdkChanges = & git -C $SdkRoot status --porcelain
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect SDK working tree.' }
if ($sdkChanges) { throw 'The baseline requires a clean SDK working tree. Preserve and review local changes before building.' }

function Get-CheckedOutput {
    param([string]$Command, [string[]]$Arguments)
    $output = & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Metadata command failed: $Command $Arguments" }
    return $output
}

function Enable-SdkEnvironment {
    $installHint = 'Run scripts/Install-Sdk.ps1 with network access to prepare the pinned environment.'
    $statePath = Join-Path $env:SIFLI_SDK_TOOLS_PATH 'sifli-sdk-env.json'
    try {
        $state = Get-Content -LiteralPath $statePath -Raw -Encoding utf8 | ConvertFrom-Json -AsHashtable
        if ($state.schema_version -ne 2) { throw 'Unsupported SDK environment state schema.' }
        $repoPath = [System.IO.Path]::GetFullPath($SdkRoot)
        $selectedKey = $state.repos[$repoPath].profiles.default.selected_env_key
        if (-not $selectedKey) { throw 'No installed default environment is selected for this SDK checkout.' }
        $pythonEnvPath = $state.envs[$selectedKey].python.env_path
        if (-not $pythonEnvPath) { throw 'The selected environment has no Python path.' }
        $sdkPython = Join-Path $pythonEnvPath 'Scripts/python.exe'
        if (-not (Test-Path -LiteralPath $sdkPython -PathType Leaf)) {
            throw "Installed SDK Python is missing: $sdkPython"
        }
    }
    catch { throw "Cannot load the installed SDK environment. $installHint Details: $($_.Exception.Message)" }

    Push-Location $SdkRoot
    try {
        # Use installed dependencies; export.ps1 bootstraps uv packages from PyPI on every run.
        $manager = Join-Path $SdkRoot 'tools/sdk_env.py'
        # Validate the current lock before export can attempt automatic environment repair.
        & $sdkPython $manager check --profile default --offline
        if ($LASTEXITCODE -ne 0) { throw "SDK environment check failed. $installHint" }
        $output = & $sdkPython $manager export --shell powershell --profile default --toolchain gcc --offline
        if ($LASTEXITCODE -ne 0) { throw "SDK environment export failed. $installHint" }
        $scriptPath = $output | Select-Object -Last 1
        if (-not $scriptPath -or -not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) {
            throw 'SDK environment export did not return a valid script path.'
        }
        . $scriptPath
    }
    finally { Pop-Location }
}
