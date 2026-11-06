param(
    [Parameter(Mandatory)][ValidateSet('Regression','Integrated','Streaming','Controls','Events','Hiding','Live')][string]$Suite,
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$RunId = (Get-Date -Format 'yyyyMMdd_HHmmss'),
    [string]$EngineRoot = 'C:/Program Files/Epic Games/UE_5.8',
    [string]$PackageRoot,
    [string]$OutputRoot = $env:TEMP,
    [switch]$Cold,
    [switch]$Render,
    [ValidateRange(10,120)][int]$MaxFPS = 60
)
$ErrorActionPreference = 'Stop'
$projectPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'HorrorGameFramework/HorrorGameFramework.uproject'
$cases = @{
    Regression = @('HorrorFramework','','HorrorAuditRegression','/Game/Maps/FrameworkIntegrated_Demo')
    Integrated = @('HorrorFramework.IntegratedDemo.Playthrough','IntegratedDemoAutomation','HorrorIntegratedTest','/Game/Maps/FrameworkIntegrated_Demo')
    Streaming = @('HorrorFramework.Streaming.DemoLifecycle','StreamingDemoAutomation','HorrorStage7','/Game/Maps/FrameworkStreaming_Demo')
    Controls = @('HorrorFramework.Controls.Runtime','ControlsRuntimeTest','HorrorControlsTest','/Game/Maps/FrameworkIntegrated_Demo')
    Events = @('HorrorFramework.Events.Runtime','EventsRuntimeTest','HorrorEventsTest','/Game/Maps/FrameworkIntegrated_Demo')
    Hiding = @('HorrorFramework.Hiding.Runtime','HidingRuntimeTest','HorrorHidingTest','/Game/Maps/FrameworkIntegrated_Demo')
    Live = @('HorrorFramework.Validation.LivePursuit','FrameworkLiveAudit','HorrorAuditLive','/Game/Maps/FrameworkIntegrated_Demo')
}
$case = $cases[$Suite]
$testName = $case[0]
$flag = $case[1]
if ($Cold) {
    switch ($Suite) {
        Streaming { $testName='HorrorFramework.Streaming.ColdRestore'; $flag='StreamingDemoColdRead' }
        Controls { $testName='HorrorFramework.Controls.ColdRead'; $flag='ControlsColdRead' }
        Events { $testName='HorrorFramework.Events.ColdRead'; $flag='EventsColdRead' }
        Hiding { $testName='HorrorFramework.Hiding.ColdRead'; $flag='HidingColdRead' }
        default { throw "The $Suite suite has no cold-read protocol." }
    }
}
$kind = if ($Cold) {'Cold'} else {'Runtime'}
$reportPath = Join-Path $OutputRoot "FrameworkValidation_${RunId}_${Suite}_${kind}"
$userPath = Join-Path $OutputRoot ($case[2] + '_' + $RunId)
if (Test-Path -LiteralPath $reportPath) { throw 'Report already exists. Choose another RunId.' }
if (!$Cold -and (Test-Path -LiteralPath $userPath)) { throw 'Test user directory already exists. Choose another RunId.' }
if ($Cold -and !(Test-Path -LiteralPath $userPath)) { throw 'Run the corresponding runtime suite with this RunId first.' }
if ($PackageRoot) {
    if ($Suite -eq 'Regression') { throw 'Run Regression in the Editor; some tests require EditorContext.' }
    $executable = Join-Path $PackageRoot 'HorrorGameFramework/Binaries/Win64/HorrorGameFramework.exe'
    $engineArgs = @()
} else {
    $executable = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    $engineArgs = @($projectPath)
    if ($Suite -ne 'Regression') { $engineArgs += '-game' }
}
if (!(Test-Path -LiteralPath $executable)) { throw "Executable not found: $executable" }
if (!(Test-Path -LiteralPath $projectPath)) { throw "Project not found: $projectPath" }
$execCommand = "Automation RunTests $testName;Quit"
if ($Suite -ne 'Controls') { $execCommand = "t.MaxFPS $MaxFPS,$execCommand" }
$engineArgs += @($case[3], '-unattended', '-nosplash', "-UserDir=$userPath", "-abslog=$reportPath.log",
    "-ReportExportPath=$reportPath", "-ExecCmds=$execCommand", '-TestExit=Automation Test Queue Empty')
# Video tests must exercise project settings without console/config timing overrides.
# Other suites override only this process so a fixed timestep cannot mask low-FPS tests.
if ($Suite -ne 'Controls') { $engineArgs += '-ini:Engine:[/Script/Engine.Engine]:bUseFixedFrameRate=False' }
if ($flag) { $engineArgs += "-$flag" }
if ($Render) {
    $engineArgs += @('-RenderOffscreen','-windowed','-ResX=1280','-ResY=720')
    switch ($Suite) {
        Integrated { $engineArgs += '-IntegratedDemoCapture' }
        Controls { $engineArgs += '-ControlsCaptureRender' }
        Events { $engineArgs += '-EventsCaptureRender' }
        Hiding { $engineArgs += '-HidingCaptureRender' }
    }
} else { $engineArgs += '-nullrhi' }
# Packaged games use the Windows GUI subsystem; wait explicitly for completion.
$quotedArguments = foreach ($argument in $engineArgs) {
    $escaped = [regex]::Replace($argument, '(\\*)"', '$1$1\"')
    '"' + [regex]::Replace($escaped, '(\\+)$', '$1$1') + '"'
}
$process = Start-Process -FilePath $executable -ArgumentList ($quotedArguments -join ' ') -WindowStyle Hidden -Wait -PassThru `
    -RedirectStandardOutput ($reportPath + '_stdout.log') -RedirectStandardError ($reportPath + '_stderr.log')
$engineExitCode = $process.ExitCode
if (!(Test-Path -LiteralPath "$reportPath/index.json")) { throw "No automation report. Check $reportPath.log (exit $engineExitCode)." }
$report = Get-Content -LiteralPath "$reportPath/index.json" -Raw | ConvertFrom-Json
[pscustomobject]@{Suite=$Suite; Success=$report.succeeded; WithWarnings=$report.succeededWithWarnings; Failed=$report.failed; EngineExit=$engineExitCode; Report=$reportPath}
foreach ($test in $report.tests) {
    $test.entries | Where-Object {$_.event.type -in @('Error','Warning')} | ForEach-Object { $_.event.message }
}
if ($engineExitCode -ne 0 -or $report.failed -gt 0 -or ($report.succeeded + $report.succeededWithWarnings) -lt 1) { exit 1 }
exit 0
