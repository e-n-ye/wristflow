#requires -Version 7.0
param(
    [Parameter(Mandatory)][ValidateSet('Provision','Sign')][string]$Mode,
    [string]$ManifestBase64
)
$ErrorActionPreference = 'Stop'
$keyName = 'WristFlow.OTA.Release.v1'
$provider = [System.Security.Cryptography.CngProvider]::MicrosoftSoftwareKeyStorageProvider
$source = Join-Path $PSScriptRoot '../apps/ota_loader/src/release_public_key.inc'

if ([System.Security.Cryptography.CngKey]::Exists($keyName, $provider)) {
    $key = [System.Security.Cryptography.CngKey]::Open($keyName, $provider)
} else {
    if ($Mode -ne 'Provision' -or (Test-Path -LiteralPath $source)) {
        throw 'The approved signing key is absent from the current user store. Restore it or authorize a separate trust rotation.'
    }
    $options = [System.Security.Cryptography.CngKeyCreationParameters]::new()
    $options.Provider = $provider
    $options.KeyUsage = [System.Security.Cryptography.CngKeyUsages]::Signing
    $options.ExportPolicy = [System.Security.Cryptography.CngExportPolicies]::AllowExport
    $options.Parameters.Add([System.Security.Cryptography.CngProperty]::new(
        'Length', [BitConverter]::GetBytes([int]2048), [System.Security.Cryptography.CngPropertyOptions]::None))
    $key = [System.Security.Cryptography.CngKey]::Create([System.Security.Cryptography.CngAlgorithm]::Rsa, $keyName, $options)
}
try {
    $rsa = [System.Security.Cryptography.RSACng]::new($key)
    try {
        $der = $rsa.ExportSubjectPublicKeyInfo()
        if ($rsa.KeySize -ne 2048 -or $der.Length -ne 294) { throw 'Expected an RSA-2048 SPKI public key' }
        $lines = for ($i = 0; $i -lt $der.Length; $i += 12) {
            ($der[$i..([Math]::Min($i + 11, $der.Length - 1))] | ForEach-Object { '0x{0:x2},' -f $_ }) -join ' '
        }
        $publicSource = $lines -join "`n"
        if (Test-Path -LiteralPath $source) {
            $pinned = [IO.File]::ReadAllText($source).Replace("`r`n", "`n").TrimEnd()
            if ($pinned -cne $publicSource) { throw 'Local signer does not match the loader public key' }
        } elseif ($Mode -eq 'Provision') {
            $file = [IO.File]::Open($source, [IO.FileMode]::CreateNew)
            try { $file.Write([Text.Encoding]::ASCII.GetBytes($publicSource + "`n")) } finally { $file.Dispose() }
        } else { throw 'Missing loader public key' }
        if ($Mode -eq 'Provision') {
            [Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($der)).ToLowerInvariant()
        } else {
            $manifest = [Convert]::FromBase64String($ManifestBase64)
            if ($manifest.Length -ne 80 -or [Text.Encoding]::ASCII.GetString($manifest, 0, 4) -cne 'WFR1') {
                throw 'Expected a fixed WristFlow release manifest'
            }
            $signature = $rsa.SignData($manifest, [System.Security.Cryptography.HashAlgorithmName]::SHA256,
                [System.Security.Cryptography.RSASignaturePadding]::Pkcs1)
            [Convert]::ToBase64String($signature)
        }
    } finally { $rsa.Dispose() }
} finally { $key.Dispose() }
