$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$wsl = Join-Path $env:WINDIR 'System32\wsl.exe'
$linuxRoot = (& $wsl -d Ubuntu-24.04 --exec wslpath -a -u $projectRoot).Trim()
if ($LASTEXITCODE -ne 0 -or -not $linuxRoot) {
    throw 'Could not convert the project path for WSL.'
}

& $wsl -d Ubuntu-24.04 --exec /bin/bash "$linuxRoot/tools/build-core.sh"
if ($LASTEXITCODE -ne 0) {
    throw "MesenCE core build failed with exit code $LASTEXITCODE."
}

$corePath = Join-Path $projectRoot 'build\mesen2_libretro.dll'
if (-not (Test-Path -LiteralPath $corePath)) {
    throw "Build completed without the expected core: $corePath"
}

Write-Output "Core: $corePath"
