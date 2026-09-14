$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$contentRoot=[IO.Path]::GetFullPath((Join-Path $projectRoot 'Content'))+[IO.Path]::DirectorySeparatorChar
$manifest=Get-Content -LiteralPath (Join-Path $projectRoot 'Saved/Presentation/approved_cleanup_backup.json') -Raw -Encoding utf8 | ConvertFrom-Json
$fallback=Get-Content -LiteralPath (Join-Path $projectRoot 'Saved/Presentation/cleanup_fallback_approved.json') -Raw -Encoding utf8 | ConvertFrom-Json
if (!$fallback.reference_checked_in_editor -or !$manifest.verified) {throw 'Editor reference check and verified backup are required'}
$results=@()
foreach ($entry in $fallback.files) {
    $approved=@($manifest.files | Where-Object {$_.path -ceq $entry.path -and $_.sha256 -ceq $entry.sha256})
    if ($approved.Count -ne 1) {throw 'File is outside approved manifest'}
    $targetPath=[IO.Path]::GetFullPath((Join-Path $projectRoot $entry.path))
    if (!$targetPath.StartsWith($contentRoot,[StringComparison]::OrdinalIgnoreCase)) {throw "Outside Content: $targetPath"}
    $backupPath=[IO.Path]::GetFullPath((Join-Path $manifest.backup_dir $entry.path))
    if ((Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {throw "Backup mismatch: $backupPath"}
    if (Test-Path -LiteralPath $targetPath) {
        if ((Get-FileHash -LiteralPath $targetPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.sha256) {throw "Changed package: $targetPath"}
        Remove-Item -LiteralPath $targetPath
    }
    $results+=[pscustomobject]@{path=$entry.path;deleted=!(Test-Path -LiteralPath $targetPath)}
}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $projectRoot 'Saved/Presentation/cleanup_fallback_result.json') -Encoding utf8
Write-Output "Removed verified orphan packages: $($results.Count)"
