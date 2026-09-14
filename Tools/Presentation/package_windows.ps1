$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$uat = 'E:/programs/Epic Games/UE_5.6/Engine/Build/BatchFiles/RunUAT.bat'
& $uat BuildCookRun "-project=$projectRoot/HSR.uproject" -noP4 -platform=Win64 -clientconfig=Development -build -cook -iterate -stage -pak -iostore -archive "-archivedirectory=$projectRoot/Saved/Presentation/Package" -unattended -utf8output
exit $LASTEXITCODE
