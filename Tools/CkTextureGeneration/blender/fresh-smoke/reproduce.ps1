[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$BlenderPath,
    [Parameter(Mandatory=$true)][string]$PythonPath,
    [Parameter(Mandatory=$true)][string]$OutputPath,
    [Parameter(Mandatory=$true)][ValidateNotNullOrEmpty()][string]$Gpu,
    [Nullable[int]]$OpenClPlatformIndex,
    [string]$Config=(Join-Path $PSScriptRoot 'variation.json'),
    [switch]$PreflightOnly,
    [switch]$Pack
)
$ErrorActionPreference='Stop'
foreach ($candidate in @($BlenderPath,$PythonPath,$Config)) {
    if (-not [System.IO.Path]::IsPathFullyQualified($candidate) -or -not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "Existing absolute file path required: $candidate"
    }
}
if (-not [System.IO.Path]::IsPathFullyQualified($OutputPath)) { throw 'OutputPath must be absolute' }
$BlenderPath=(Resolve-Path -LiteralPath $BlenderPath).Path
$PythonPath=(Resolve-Path -LiteralPath $PythonPath).Path
$Config=(Resolve-Path -LiteralPath $Config).Path
$deviceOptions=@('--gpu',$Gpu)
if ($PSBoundParameters.ContainsKey('OpenClPlatformIndex')) { $deviceOptions+=@('--platform-index',[string]$OpenClPlatformIndex) }
& $PythonPath -B (Join-Path $PSScriptRoot 'preflight.py') --config $Config --output $OutputPath @deviceOptions
if ($LASTEXITCODE -ne 0) { throw 'Smoke path/config preflight failed' }
$packer=Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'pack_flipbook.py'
if (-not (Test-Path -LiteralPath $packer -PathType Leaf)) { throw "Missing packer: $packer" }
& $PythonPath -B -c 'import numpy; from PIL import Image'
if ($LASTEXITCODE -ne 0) { throw 'Preview requires NumPy and Pillow in the selected Python environment' }
if ($PreflightOnly) { return }
$runPath=Join-Path $OutputPath 'simulation'
& $BlenderPath --background --factory-startup --threads 8 --python-exit-code 1 --python (Join-Path $PSScriptRoot 'recipe.py') -- --config $Config --out $runPath @deviceOptions
if ($LASTEXITCODE -ne 0) { throw "Fresh bake failed: $LASTEXITCODE" }
foreach ($mode in @('lit','opacity')) {
    $extinction=if ($mode -eq 'lit') { '2.3' } else { '0.65' }
    $renderPath=Join-Path $OutputPath ('render-'+$mode)
    & $BlenderPath --background --factory-startup --threads 8 --python-exit-code 1 --python (Join-Path $PSScriptRoot 'render.py') -- --run $runPath --out $renderPath --mode $mode --extinction $extinction --start 17
    if ($LASTEXITCODE -ne 0) { throw "Native $mode render failed: $LASTEXITCODE" }
    if ($Pack) {
        & $PythonPath -B $packer --input (Join-Path $renderPath 'frames') --output (Join-Path $OutputPath ('packed-'+$mode)) --fps 8 --columns 4 --rows 4
        if ($LASTEXITCODE -ne 0) { throw "Packing $mode failed: $LASTEXITCODE" }
    } else {
        & $PythonPath -B $packer --input (Join-Path $renderPath 'frames') --output (Join-Path $OutputPath ('preview-'+$mode)) --fps 8 --columns 4 --rows 4 --preview-only
        if ($LASTEXITCODE -ne 0) { throw "Preview $mode failed: $LASTEXITCODE" }
    }
}
