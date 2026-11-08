$ErrorActionPreference='Stop'
$workspaceRoot=Split-Path -Parent $PSScriptRoot
$nativeExe=Join-Path $workspaceRoot 'build/windows/Yohaku.exe'
if(-not(Test-Path -LiteralPath $nativeExe)){throw 'Run tools/build-native.ps1 -Package first'}
& $nativeExe -windowed -ForceRes -ResX=1600 -ResY=1000
