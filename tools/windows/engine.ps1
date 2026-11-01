<#
.SYNOPSIS
Build, launch, validate or package Archura using the current CMake presets.
.EXAMPLE
.\tools\windows\engine.ps1 -Action Run -Configuration Debug -Console
.EXAMPLE
.\tools\windows\engine.ps1 -Action Package -Manifest .\samples\standalone\game.json -Output .\build\packages\sample
.EXAMPLE
.\tools\windows\engine.ps1 -Action BuildAll -WhatIf
#>
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param(
    [ValidateSet('Build', 'BuildAll', 'Run', 'Package', 'Validate')]
    [string]$Action = 'Build',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [switch]$SkipBuild,
    [switch]$Console,
    [ValidateSet('auto', 'opengl', 'vulkan')]
    [string]$Graphics = 'auto',
    [string]$ProjectPath,
    [string[]]$RuntimeArguments = @(),
    [string]$Manifest,
    [string]$Output,
    [ValidateSet('desktop', 'contracts', 'loop', 'all')]
    [string]$ValidationProfile = 'desktop'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$script:CommandExitCode = 0
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$callerDirectory = (Get-Location).ProviderPath

function Get-AbsoluteArgumentPath {
    param([string]$Value)
    if ([IO.Path]::IsPathRooted($Value)) { return [IO.Path]::GetFullPath($Value) }
    return [IO.Path]::GetFullPath((Join-Path $callerDirectory $Value))
}

function Invoke-Tool {
    param([string]$Program, [string[]]$ToolArguments, [string]$Directory)
    $displayArguments = ($ToolArguments | ForEach-Object { '"' + $_.Replace('"', '\"') + '"' }) -join ' '
    if ($PSCmdlet.ShouldProcess("$Directory : $Program $displayArguments", 'Execute')) {
        Push-Location -LiteralPath $Directory
        try {
            & $Program @ToolArguments
            if ($LASTEXITCODE -ne 0) {
                $script:CommandExitCode = $LASTEXITCODE
                throw "$Program exited with code $LASTEXITCODE."
            }
        } finally {
            Pop-Location
        }
    }
}

function Build-Configuration {
    param([string[]]$Configurations)
    Invoke-Tool -Program 'cmake' -ToolArguments @('--preset', 'windows-x64', '-DARCHURA_BUILD_STARTUP_TESTS=OFF') -Directory $repositoryRoot
    foreach ($buildConfiguration in $Configurations) {
        Invoke-Tool -Program 'cmake' -ToolArguments @('--build', '--preset', ('windows-' + $buildConfiguration.ToLowerInvariant()), '--target', 'ArchuraEngine', 'ArchuraServer', 'ArchuraLauncher') -Directory $repositoryRoot
    }
}

try {
    if (-not (Test-Path -LiteralPath (Join-Path $repositoryRoot 'CMakePresets.json') -PathType Leaf)) {
        throw "Cannot locate the Archura repository at $repositoryRoot."
    }
    if ($SkipBuild -and $Action -notin @('Run', 'Package')) {
        throw '-SkipBuild is only available for Run or Package.'
    }
    if ($Action -eq 'BuildAll' -and $PSBoundParameters.ContainsKey('Configuration')) {
        throw 'BuildAll always builds Debug and Release; use Build for a single configuration.'
    }
    # Resolve user-supplied paths before changing to build/runtime directories.
    if ($ProjectPath) { $ProjectPath = Get-AbsoluteArgumentPath $ProjectPath }
    if ($Manifest) { $Manifest = Get-AbsoluteArgumentPath $Manifest }
    if ($Output) { $Output = Get-AbsoluteArgumentPath $Output }
    $runtimeDirectory = Join-Path $repositoryRoot ('build\windows-x64\bin\' + $Configuration)

    switch ($Action) {
        'Build' { Build-Configuration @($Configuration) }
        'BuildAll' { Build-Configuration @('Debug', 'Release') }
        'Run' {
            if (-not $SkipBuild) { Build-Configuration @($Configuration) }
            $executable = Join-Path $runtimeDirectory 'ArchuraEngine.exe'
            if (-not $WhatIfPreference -and -not (Test-Path -LiteralPath $executable -PathType Leaf)) {
                throw "Runtime not found: $executable. Run without -SkipBuild to build it."
            }
            $launchArguments = @('--graphics=' + $Graphics)
            if ($Console) { $launchArguments += '--console' }
            if ($ProjectPath) { $launchArguments += '--project=' + $ProjectPath }
            $launchArguments += $RuntimeArguments
            Invoke-Tool -Program $executable -ToolArguments $launchArguments -Directory $runtimeDirectory
        }
        'Package' {
            if (-not $Manifest -or -not $Output) {
                throw 'Package requires -Manifest <game.json> and -Output <new-directory>.'
            }
            if (-not $SkipBuild) { Build-Configuration @($Configuration) }
            Invoke-Tool -Program 'python' -ToolArguments @((Join-Path $repositoryRoot 'tools\package_game.py'),
                '--manifest', $Manifest, '--runtime-dir', $runtimeDirectory, '--output', $Output) -Directory $repositoryRoot
        }
        'Validate' {
            $validationArguments = @((Join-Path $repositoryRoot 'tools\validate_local.py'), '--profile', $ValidationProfile)
            if ($PSBoundParameters.ContainsKey('Configuration')) {
                $validationArguments += @('--configuration', $Configuration)
            }
            Invoke-Tool -Program 'python' -ToolArguments $validationArguments -Directory $repositoryRoot
        }
    }
} catch {
    Write-Error -ErrorAction Continue $_
    if ($script:CommandExitCode -eq 0) { $script:CommandExitCode = 1 }
}
exit $script:CommandExitCode
