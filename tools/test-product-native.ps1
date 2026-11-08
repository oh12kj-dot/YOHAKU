param([string]$Executable='build/windows/Yohaku.exe')
$ErrorActionPreference='Stop'
$workspaceRoot=Split-Path -Parent $PSScriptRoot
$nativeExe=Join-Path $workspaceRoot $Executable
$testRoot=Join-Path $workspaceRoot ('artifacts/native-candidate-validation/'+[guid]::NewGuid().ToString())
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null
foreach($mode in @('YohakuProductSmoke','YohakuCandidateReloadSmoke')){
$startTime=Get-Date
$nativeArgs=@("-$mode","-YohakuLayoutRoot=$testRoot","-YohakuCandidateRoot=$testRoot/candidates",'-RenderOffscreen','-unattended','-nosplash','-nosound','-windowed','-ForceRes','-ResX=1600','-ResY=1000',"-abslog=$testRoot/$mode.log")
$testProcess=Start-Process -FilePath $nativeExe -ArgumentList $nativeArgs -WindowStyle Hidden -PassThru
if(-not $testProcess.WaitForExit(120000)){throw 'Native product research timed out'}
if($testProcess.ExitCode -ne 0){throw "Native product process failed: $($testProcess.ExitCode)"}
$evidenceName=if($mode -eq 'YohakuProductSmoke'){'product-evidence.json'}else{'candidate-restart.json'}
$evidencePath=Join-Path $testRoot $evidenceName
if(-not(Test-Path $evidencePath) -or (Get-Item $evidencePath).LastWriteTime -lt $startTime){throw 'Missing fresh product evidence'}
$evidence=Get-Content $evidencePath -Raw -Encoding UTF8 | ConvertFrom-Json
if(-not $evidence.passed -or -not $evidence.sceneDirtyAndHistoryUnchanged -or -not $evidence.candidateSaveOrOfflineReloadPassed){throw "Native product research failed: $($evidence.status)"}
$product=$evidence.product | ConvertFrom-Json
if($product.dimensionStatus -ne 'single-source-unverified' -or $product.modelStatus -ne 'not-generated'){throw 'Research status boundary failed'}
Write-Host "$mode PASS; scene/dirty/history preserved"
}
$first=Get-Content "$testRoot/product-evidence.json" -Raw -Encoding UTF8 | ConvertFrom-Json
$second=Get-Content "$testRoot/candidate-restart.json" -Raw -Encoding UTF8 | ConvertFrom-Json
if($first.product -ne $second.product){throw 'Offline restart changed product evidence'}
Write-Host "Live save and offline cross-process restoration PASS: $testRoot"
