param([switch]$SkipCore,[string]$Executable='build/windows/Yohaku.exe')
$ErrorActionPreference='Stop'
$workspaceRoot=Split-Path -Parent $PSScriptRoot
$engineRoot=if($env:YOHaku_UE_ROOT){$env:YOHaku_UE_ROOT}else{'C:/Program Files/Epic Games/UE_5.8'}
$nativeProject=Join-Path $workspaceRoot 'apps/ue/Yohaku.uproject'
$nativeExe=Join-Path $workspaceRoot $Executable
$testRoot=Join-Path $workspaceRoot 'artifacts/native-packaged-validation'
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null
if(-not $SkipCore){
 & "$engineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $nativeProject '-ExecCmds=Automation RunTests Yohaku' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$workspaceRoot/artifacts/native-core-tests" -NullRHI -unattended -nosplash -nosound -NoSourceControl -NoLiveCoding "-abslog=$workspaceRoot/artifacts/native-core-tests.log"
 if($LASTEXITCODE -ne 0){throw 'Core automation failed'}
 $core=Get-Content "$workspaceRoot/artifacts/native-core-tests/index.json" -Raw | ConvertFrom-Json
 if($core.failed -ne 0 -or $core.succeeded -lt 4){throw 'Native automation did not pass all four suites'}
}
foreach($mode in @('YohakuSmoke','YohakuReloadSmoke')){
 $startTime=Get-Date
 $nativeArgs=@("-$mode","-YohakuLayoutRoot=$testRoot",'-RenderOffscreen','-unattended','-nosplash','-nosound','-windowed','-ForceRes','-ResX=1600','-ResY=1000',"-abslog=$testRoot/$mode.log")
 $testProcess=Start-Process -FilePath $nativeExe -ArgumentList $nativeArgs -WindowStyle Hidden -PassThru
 if(-not $testProcess.WaitForExit(120000)){throw "Native $mode timed out"}
 if($testProcess.ExitCode -ne 0){throw "Native $mode failed: $($testProcess.ExitCode)"}
 $evidenceName=if($mode -eq 'YohakuSmoke'){'runtime-evidence.json'}else{'restart-evidence.json'}
 $evidencePath=Join-Path $testRoot $evidenceName
 if(-not(Test-Path $evidencePath) -or (Get-Item $evidencePath).LastWriteTime -lt $startTime){throw 'Missing fresh runtime evidence'}
 $evidence=Get-Content $evidencePath -Raw | ConvertFrom-Json
 if(-not $evidence.passed){throw "Native checks failed: $($evidence.checks -join ', ')"}
 Write-Host "$mode PASS ($($evidence.checks.Count) runtime checks)"
}
$first=Get-Content "$testRoot/runtime-evidence.json" -Raw | ConvertFrom-Json
$second=Get-Content "$testRoot/restart-evidence.json" -Raw | ConvertFrom-Json
if($first.scene -ne $second.scene){throw 'Restart did not preserve the exact serialized scene'}
Write-Host 'Packaged runtime and cross-process restoration PASS'
