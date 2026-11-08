param([switch]$Package,[string]$ArchiveDirectory='build/windows',[ValidateSet('Shipping','Development')][string]$Configuration='Shipping')
$ErrorActionPreference='Stop'
$workspaceRoot=Split-Path -Parent $PSScriptRoot
$engineRoot=if($env:YOHaku_UE_ROOT){$env:YOHaku_UE_ROOT}else{'C:/Program Files/Epic Games/UE_5.8'}
$nativeProject=Join-Path $workspaceRoot 'apps/ue/Yohaku.uproject'
$nativeArchive=[IO.Path]::GetFullPath((Join-Path $workspaceRoot $ArchiveDirectory))
if(-not $nativeArchive.StartsWith($workspaceRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Archive must remain inside workspace'}
& "$engineRoot/Engine/Build/BatchFiles/Build.bat" YohakuEditor Win64 Development "-Project=$nativeProject" -WaitMutex -NoHotReloadFromIDE -gather -NoUBA
if($LASTEXITCODE -ne 0){throw 'Native editor build failed'}
if($Package){
 & "$engineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $nativeProject '-run=pythonscript' "-script=$workspaceRoot/apps/ue/Tools/check_materials.py" -NullRHI -unattended -nosplash -nosound -NoSourceControl -NoLiveCoding
 if($LASTEXITCODE -ne 0){throw 'Furniture material usage or catalog assignment validation failed'}
 & "$engineRoot/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun "-project=$nativeProject" -noP4 -platform=Win64 "-clientconfig=$Configuration" -build -cook -stage -pak -archive "-archivedirectory=$nativeArchive" '-ubtargs=-NoUBA' -unattended -utf8output
 if($LASTEXITCODE -ne 0){throw 'Windows packaging failed'}
 $runtimeTools=Join-Path $nativeArchive 'Windows/Yohaku/Tools'
 if(-not(Test-Path (Join-Path $nativeArchive 'Windows/Yohaku.exe'))){$runtimeTools=Join-Path $nativeArchive 'Yohaku/Tools'}
 New-Item -ItemType Directory -Force -Path $runtimeTools | Out-Null
 Copy-Item -LiteralPath (Join-Path $workspaceRoot 'apps/ue/Tools/local-3d-worker.py') -Destination (Join-Path $runtimeTools 'local-3d-worker.py') -Force
 Copy-Item -LiteralPath (Join-Path $workspaceRoot 'apps/ue/Tools/local_3d_geometry.py') -Destination (Join-Path $runtimeTools 'local_3d_geometry.py') -Force
}
