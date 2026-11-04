param([switch]$Cli)
$ErrorActionPreference = 'Stop'
if ($Cli) { throw 'Run dotnet run --project checks/interop to use the developer audio console.' }
$app = Join-Path $PSScriptRoot 'output\managed\Auraminus\Release\net9.0-windows\Auraminus.exe'
if (-not (Test-Path -LiteralPath $app)) { throw 'Build Auraminus first: .\tools\build.ps1' }
Start-Process -FilePath $app -WorkingDirectory (Split-Path $app) -WindowStyle Hidden
