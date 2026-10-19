param([switch]$Packaged)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if ($Packaged) {
    $executable = Join-Path $projectRoot 'Saved/Ultimate/Package/Windows/HSR.exe'
    if (!(Test-Path -LiteralPath $executable)) { throw "Practice package missing: $executable" }
    $arguments = @('/Game/Maps/VerticalSlice/Map_HertaSupportSection', '-HSRUltimatePractice', '-windowed', '-ResX=1280', '-ResY=720')
} else {
    $executable = 'E:/programs/Epic Games/UE_5.6/Engine/Binaries/Win64/UnrealEditor.exe'
    $arguments = @(('"' + (Join-Path $projectRoot 'HSR.uproject') + '"'), '/Game/Maps/VerticalSlice/Map_HertaSupportSection', '-game', '-HSRUltimatePractice', '-windowed', '-ResX=1280', '-ResY=720')
}
# This launcher explicitly opens the interactive game the player is about to test.
Start-Process -FilePath $executable -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Normal
