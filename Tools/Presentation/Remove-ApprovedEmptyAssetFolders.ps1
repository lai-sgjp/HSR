$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$contentRoot=[IO.Path]::GetFullPath((Join-Path $projectRoot 'Content'))+[IO.Path]::DirectorySeparatorChar
$manifest=Get-Content -LiteralPath (Join-Path $projectRoot 'Saved/Presentation/approved_cleanup_backup.json') -Raw -Encoding utf8 | ConvertFrom-Json
$folders=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($entry in $manifest.files) {
    $folderPath=[IO.Path]::GetDirectoryName([IO.Path]::GetFullPath((Join-Path $projectRoot $entry.path)))
    while ($folderPath.StartsWith($contentRoot,[StringComparison]::OrdinalIgnoreCase)) {
        [void]$folders.Add($folderPath)
        $folderPath=[IO.Path]::GetDirectoryName($folderPath)
    }
}
$removed=0
foreach ($folderPath in ($folders | Sort-Object Length -Descending)) {
    if (!$folderPath.StartsWith($contentRoot,[StringComparison]::OrdinalIgnoreCase)) {throw 'Folder escaped Content'}
    if ((Test-Path -LiteralPath $folderPath) -and @(Get-ChildItem -LiteralPath $folderPath -Force).Count -eq 0) {
        Remove-Item -LiteralPath $folderPath
        $removed++
    }
}
Write-Output "Removed empty approved asset folders: $removed"
