param(
    [string] $RetroArchRuntimePath
)

$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$retroArchVersion = '1.22.2'
if ([string]::IsNullOrWhiteSpace($RetroArchRuntimePath)) {
    if (-not [string]::IsNullOrWhiteSpace($env:RETROPVP_RETROARCH_DIR)) {
        $RetroArchRuntimePath = $env:RETROPVP_RETROARCH_DIR
    } else {
        $RetroArchRuntimePath = Join-Path $projectRoot 'build\runtime\RetroArch-Win64'
    }
}
$retroArchRuntimePath = [System.IO.Path]::GetFullPath($RetroArchRuntimePath)
$retroArchRoot = Split-Path $retroArchRuntimePath -Parent
$retroArchExe = Join-Path $retroArchRuntimePath 'retroarch.exe'
$retroArchArchive = Join-Path $env:TEMP "RetroArch-$retroArchVersion.7z"
$retroArchUrl = "https://buildbot.libretro.com/stable/$retroArchVersion/windows/x86_64/RetroArch.7z"
$sevenZipCommand = Get-Command '7z.exe' -ErrorAction SilentlyContinue
$wsl = Join-Path $env:WINDIR 'System32\wsl.exe'

if (-not (Test-Path -LiteralPath $wsl)) {
    throw 'WSL is required for the MinGW-w64 cross-build.'
}
if (-not $sevenZipCommand) {
    throw '7z.exe is required to extract RetroArch. Install 7-Zip or add its command-line executable to PATH.'
}

$buildTools = & $wsl -d Ubuntu-24.04 --exec /bin/sh -c 'for tool in git make gcc python3 x86_64-w64-mingw32-g++; do command -v "$tool" >/dev/null 2>&1 || exit 1; done' 2>$null
if ($LASTEXITCODE -ne 0) {
    & $wsl -d Ubuntu-24.04 -u root --exec apt-get update
    if ($LASTEXITCODE -ne 0) { throw 'WSL apt package index update failed.' }
    & $wsl -d Ubuntu-24.04 -u root --exec apt-get install -y git make gcc python3 g++-mingw-w64-x86-64
    if ($LASTEXITCODE -ne 0) { throw 'WSL build tool installation failed.' }
}

if (Test-Path -LiteralPath $retroArchExe) {
    Write-Output "RetroArch already exists at $retroArchExe; leaving it untouched."
} else {
    if (Test-Path -LiteralPath $retroArchRoot) {
        throw "The target folder exists but has no retroarch.exe: $retroArchRoot"
    }

    Invoke-WebRequest -Uri $retroArchUrl -OutFile $retroArchArchive
    New-Item -ItemType Directory -Path $retroArchRoot | Out-Null
    & $sevenZipCommand.Source x $retroArchArchive "-o$retroArchRoot" -y | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'RetroArch archive extraction failed.' }
    if (-not (Test-Path -LiteralPath $retroArchExe)) {
        throw "The extracted RetroArch archive does not contain retroarch.exe at $retroArchExe."
    }
}

$wslBuildTools = & $wsl -d Ubuntu-24.04 --exec /bin/sh -c 'printf "make="; make --version | head -n 1; printf "gcc="; gcc --version | head -n 1; printf "python="; python3 --version; printf "mingw="; x86_64-w64-mingw32-g++ --version | head -n 1'
if ($LASTEXITCODE -ne 0) { throw 'WSL build tool verification failed.' }
Write-Output $wslBuildTools
$runtimeConfigDir = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Path $runtimeConfigDir -Force | Out-Null
[System.IO.File]::WriteAllText(
    (Join-Path $runtimeConfigDir 'retroarch-runtime-path.txt'),
    $retroArchRuntimePath,
    (New-Object System.Text.UTF8Encoding($false))
)
Write-Output "RetroArch path: $retroArchExe"
