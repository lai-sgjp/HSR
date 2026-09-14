$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$contentRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'Content')) + [IO.Path]::DirectorySeparatorChar
$manifest = Get-Content -LiteralPath (Join-Path $projectRoot 'Saved/Presentation/approved_cleanup_backup.json') -Raw -Encoding utf8 | ConvertFrom-Json
if (!$manifest.verified -or $manifest.count -ne 2093) { throw 'Verified approved manifest required' }
$results = @()
foreach ($entry in $manifest.files) {
    $sourcePath = [IO.Path]::GetFullPath((Join-Path $projectRoot $entry.path))
    if ([IO.Path]::GetExtension($sourcePath).ToLowerInvariant() -notin @('.fbx','.png')) { continue }
    if (!$sourcePath.StartsWith($contentRoot,[StringComparison]::OrdinalIgnoreCase)) { throw "Outside Content: $sourcePath" }
    $assetPath = [IO.Path]::ChangeExtension($sourcePath,'.uasset')
    if (Test-Path -LiteralPath $assetPath) { throw "Corresponding UE asset is retained: $assetPath" }
    $backupPath = [IO.Path]::GetFullPath((Join-Path $manifest.backup_dir $entry.path))
    if ((Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) { throw "Backup mismatch: $backupPath" }
    if (Test-Path -LiteralPath $sourcePath) {
        if ((Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) { throw "Source changed: $sourcePath" }
        Remove-Item -LiteralPath $sourcePath
    }
    $results += [pscustomobject]@{path=$entry.path; deleted=!(Test-Path -LiteralPath $sourcePath); bytes=$entry.bytes}
}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $projectRoot 'Saved/Presentation/cleanup_source_result.json') -Encoding utf8
Write-Output "Verified source files removed: $($results.Count)"
