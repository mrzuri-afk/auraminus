param([switch]$CppOnly, [switch]$CSharpOnly, [switch]$Clean, [string]$ToolchainDirectory, [string]$DotnetDirectory)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($ToolchainDirectory) { $env:PATH = "$ToolchainDirectory;$env:PATH" }
if ($DotnetDirectory) { $env:DOTNET_ROOT = $DotnetDirectory; $env:PATH = "$DotnetDirectory;$env:PATH" }
$engine = Join-Path $root 'engine'
$build = Join-Path $root 'output\native'
if ($CppOnly -and $CSharpOnly) { throw 'Choose only one of -CppOnly or -CSharpOnly.' }
if ($Clean -and (Test-Path -LiteralPath $build)) {
    $resolved = (Resolve-Path -LiteralPath $build).Path
    if (-not $resolved.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Build path escaped workspace.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
if (-not $CSharpOnly) {
    $generator = if (Get-Command g++ -ErrorAction SilentlyContinue) { 'MinGW Makefiles' } else { 'Visual Studio 17 2022' }
    cmake -S $engine -B $build -G $generator -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE) { throw 'Native configure failed.' }
    cmake --build $build --config Release --parallel 4
    if ($LASTEXITCODE) { throw 'Native build failed.' }
    ctest --test-dir $build -C Release --output-on-failure
    if ($LASTEXITCODE) { throw 'Native tests failed.' }
}
if (-not $CppOnly) {
    dotnet build (Join-Path $root 'desktop\Auraminus.csproj') -c Release
    if ($LASTEXITCODE) { throw 'Desktop build failed.' }
}
Write-Host 'Auraminus build complete.' -ForegroundColor Green
