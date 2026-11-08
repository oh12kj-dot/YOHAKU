param([string]$Executable='build/windows/Yohaku.exe')
$ErrorActionPreference='Stop'
$workspaceRoot=Split-Path -Parent $PSScriptRoot
$nativeExe=[IO.Path]::GetFullPath((Join-Path $workspaceRoot $Executable))
$packageRoot=Split-Path -Parent $nativeExe
$testRoot=Join-Path $workspaceRoot ('artifacts/native-network-validation/'+[guid]::NewGuid().ToString())
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null
$nativeArgs=@('-YohakuProductSmoke',"-YohakuLayoutRoot=$testRoot","-YohakuCandidateRoot=$testRoot/candidates",'-RenderOffscreen','-unattended','-nosplash','-nosound','-windowed','-ForceRes','-ResX=1600','-ResY=1000')
$startTime=Get-Date
$nativeProcess=Start-Process -FilePath $nativeExe -ArgumentList $nativeArgs -WindowStyle Hidden -PassThru
$samples=@()
$observedChild=$false
while(-not $nativeProcess.HasExited){
 if(((Get-Date)-$startTime).TotalSeconds -gt 120){throw 'Native network test timed out'}
 $owners=@(Get-CimInstance Win32_Process | Where-Object {($_.ProcessId -eq $nativeProcess.Id -or $_.ParentProcessId -eq $nativeProcess.Id) -and $_.ExecutablePath -and $_.ExecutablePath.StartsWith($packageRoot+'\',[StringComparison]::OrdinalIgnoreCase) -and $_.Name -like 'Yohaku*.exe'})
 $ownerIds=@($owners | Select-Object -ExpandProperty ProcessId)
 if(@($owners | Where-Object {$_.ProcessId -ne $nativeProcess.Id}).Count -gt 0){$observedChild=$true}
 $tcp=@(Get-NetTCPConnection -State Listen -ErrorAction Stop | Where-Object {$_.OwningProcess -in $ownerIds} | Select-Object LocalAddress,LocalPort,OwningProcess)
 $udp=@(Get-NetUDPEndpoint -ErrorAction Stop | Where-Object {$_.OwningProcess -in $ownerIds} | Select-Object LocalAddress,LocalPort,OwningProcess)
 if($ownerIds.Count){$samples+=@{time=[DateTime]::UtcNow.ToString('o');owners=$ownerIds;tcp=$tcp;udp=$udp}}
 Start-Sleep -Milliseconds 250
 $nativeProcess.Refresh()
}
$nativeProcess.WaitForExit()
$passed=$nativeProcess.ExitCode -eq 0 -and $observedChild -and $samples.Count -ge 3 -and @($samples | Where-Object {$_.tcp.Count -gt 0 -or $_.udp.Count -gt 0}).Count -eq 0
@{passed=$passed;exitCode=$nativeProcess.ExitCode;observedChild=$observedChild;samples=$samples} | ConvertTo-Json -Depth 7 | Set-Content "$testRoot/network-evidence.json" -Encoding UTF8
if(-not $passed){throw "Native listeners detected or observation incomplete: $testRoot"}
$evidence=Get-Content "$testRoot/product-evidence.json" -Raw -Encoding UTF8 | ConvertFrom-Json
if(-not $evidence.passed){throw "Product research failed during listener check: $testRoot"}
Write-Host "No TCP listeners or UDP endpoints in $($samples.Count) process samples; live product fetch/save PASS: $testRoot"
