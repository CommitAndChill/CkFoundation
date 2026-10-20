[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$BlenderPath,
    [Parameter(Mandatory=$true)][string]$Spec,
    [Parameter(Mandatory=$true)][string]$OutputPath,
    [switch]$PreflightOnly
)
$ErrorActionPreference = 'Stop'
foreach ($candidate in @($BlenderPath, $Spec)) {
    if (-not [System.IO.Path]::IsPathFullyQualified($candidate) -or -not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "Existing absolute file path required: $candidate"
    }
}
if (-not [System.IO.Path]::IsPathFullyQualified($OutputPath)) { throw 'OutputPath must be absolute' }
if (Test-Path -LiteralPath $OutputPath) { throw 'Existing output preserved; choose a new directory' }
$pluginRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..')).TrimEnd('\','/')
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
if ($resolvedOutput.Equals($pluginRoot, [StringComparison]::OrdinalIgnoreCase) -or $resolvedOutput.StartsWith($pluginRoot + [System.IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Output must be outside plugin source'
}
$recipe = Get-Content -LiteralPath $Spec -Raw | ConvertFrom-Json
if ($recipe.family -notin @('erosion','smoke2d','smoke3d','shockwave','flipbook','metal','flow','normal_probe')) { throw 'Unsupported procedural family' }
if ($PreflightOnly) { Write-Output 'PROCEDURAL_PATH_PREFLIGHT_PASS (Blender not launched)'; return }
& $BlenderPath --background --factory-startup --python-exit-code 1 --python (Join-Path $PSScriptRoot 'generate.py') -- --spec $Spec --output $OutputPath
if ($LASTEXITCODE -ne 0) { throw "Native procedural render failed: $LASTEXITCODE" }
