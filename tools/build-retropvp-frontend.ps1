$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$wsl = Join-Path $env:WINDIR 'System32\wsl.exe'
$linuxRoot = (& $wsl -d Ubuntu-24.04 --exec wslpath -a -u $projectRoot).Trim()
if ($LASTEXITCODE -ne 0 -or -not $linuxRoot) {
    throw 'Could not convert the project path for WSL.'
}

& $wsl -d Ubuntu-24.04 --exec /bin/bash "$linuxRoot/tools/build-retropvp-frontend.sh"
if ($LASTEXITCODE -ne 0) {
    throw "RetroPvP frontend build failed with exit code $LASTEXITCODE."
}

$frontend = Join-Path $projectRoot 'build\retropvp-frontend\retroarch.exe'
if (-not (Test-Path -LiteralPath $frontend)) {
    throw "Build completed without the expected frontend: $frontend"
}

Write-Output "Frontend: $frontend"
