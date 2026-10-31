param(
	[Parameter()]
	[string]$RomPath,
	[Parameter()]
	[ValidateSet('goomba', 'bill-state', 'enemy-fixtures')]
	[string]$Mode = 'goomba'
)

$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$romCandidate = Join-Path $projectRoot 'roms\smb.nes'
if ([string]::IsNullOrWhiteSpace($RomPath) -and (Test-Path -LiteralPath $romCandidate -PathType Leaf)) {
	$RomPath = $romCandidate
}
$corePath = Join-Path $projectRoot 'build\mesen2_libretro.dll'
$runnerPath = Join-Path $projectRoot 'build\smb1_headless.exe'
$sourcePath = Join-Path $PSScriptRoot 'headless\smb1_headless.cpp'
$includeCore = Join-Path $projectRoot '.third_party\MesenCE\Libretro'
$includeProfile = Join-Path $projectRoot 'build\generated'
$buildLog = Join-Path $projectRoot 'build\headless-build.log'
$wsl = Join-Path $env:WINDIR 'System32\wsl.exe'

if (-not (Test-Path -LiteralPath $corePath)) {
	throw "Built MesenCE core not found: $corePath. Run tools\build-core.ps1 first."
}
if (-not (Test-Path -LiteralPath $RomPath)) {
	throw "SMB1 ROM not found: $RomPath. Pass its local path with -RomPath."
}

function ConvertTo-WslPath([string]$Path) {
	$result = (& $wsl -d Ubuntu-24.04 --exec wslpath -a -u $Path).Trim()
	if ($LASTEXITCODE -ne 0 -or -not $result) {
		throw "Could not convert path for WSL: $Path"
	}
	return $result
}

$linuxSource = ConvertTo-WslPath $sourcePath
$linuxCoreInclude = ConvertTo-WslPath $includeCore
$linuxProfileInclude = ConvertTo-WslPath $includeProfile
$linuxRunner = ConvertTo-WslPath $runnerPath
$compileArgs = @(
	'-std=c++17', '-O2', '-Wall', '-Wextra', '-Wpedantic',
	'-static-libgcc', '-static-libstdc++',
	"-I$linuxCoreInclude", "-I$linuxProfileInclude",
	$linuxSource, '-o', $linuxRunner
)

$previousErrorActionPreference = $ErrorActionPreference
try {
	# Windows PowerShell 5.1 can promote compiler stderr warnings to terminating
	# errors when ErrorActionPreference is Stop, even when the compiler succeeds.
	$ErrorActionPreference = 'Continue'
	& $wsl -d Ubuntu-24.04 --exec x86_64-w64-mingw32-g++ @compileArgs *> $buildLog
	$compileExitCode = $LASTEXITCODE
} finally {
	$ErrorActionPreference = $previousErrorActionPreference
}
if ($compileExitCode -ne 0) {
	Get-Content -LiteralPath $buildLog | Select-Object -Last 80
	throw "Headless runner compilation failed with exit code $compileExitCode."
}

Write-Output "Headless runner: $runnerPath"
if ($Mode -eq 'bill-state') {
	& $runnerPath $corePath $RomPath '--bill-state'
} elseif ($Mode -eq 'enemy-fixtures') {
	& $runnerPath $corePath $RomPath '--enemy-fixtures'
} else {
	& $runnerPath $corePath $RomPath
}
if ($LASTEXITCODE -ne 0) {
	throw "Headless $Mode check failed with exit code $LASTEXITCODE."
}
