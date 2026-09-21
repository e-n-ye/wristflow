#requires -Version 7.0
. "$PSScriptRoot/Set-LocalEnvironment.ps1"
$logDir = Join-Path $ProjectRoot 'artifacts/logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$logPath = Join-Path $logDir ("install-{0}.log" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
Push-Location $SdkRoot
try {
    & ./install.ps1 *>&1 | Tee-Object -FilePath $logPath
    if ($LASTEXITCODE -ne 0) { throw "SDK installation failed. See $logPath" }
}
finally { Pop-Location }
