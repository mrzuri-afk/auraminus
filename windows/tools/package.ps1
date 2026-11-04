param([string]$ToolchainDirectory, [string]$DotnetDirectory, [switch]$PublishOnly)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$version = (Get-Content (Join-Path $root 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'VERSION must use x.y.z.' }
& (Join-Path $PSScriptRoot 'build.ps1') -ToolchainDirectory $ToolchainDirectory -DotnetDirectory $DotnetDirectory
$dist = Join-Path $root 'output\portable'
dotnet publish (Join-Path $root 'desktop\Auraminus.csproj') -c Release -r win-x64 --self-contained true `
    -p:PublishSingleFile=true -p:UseSharedCompilation=false -p:Version=$version -o $dist
if ($LASTEXITCODE) { throw 'Desktop publish failed.' }
Copy-Item -LiteralPath (Join-Path $root 'LICENSE'),(Join-Path $root 'README.md') -Destination $dist -Force
if ($PublishOnly) { Write-Host "Portable app published to $dist"; return }
$iscc = (Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source
if (-not $iscc) {
    foreach ($candidate in @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe")) {
        if (Test-Path -LiteralPath $candidate) { $iscc = $candidate; break }
    }
}
if (-not $iscc) { throw 'Portable build is ready in output/portable. Install Inno Setup 6 and put ISCC.exe on PATH to create an installer.' }
& $iscc "/DMyAppVersion=$version" (Join-Path $root 'tools\windows\setup.iss')
if ($LASTEXITCODE) { throw 'Installer compilation failed.' }
Write-Host "Installer ready: $root\output\installers\Auraminus-Setup-v$version.exe" -ForegroundColor Green
