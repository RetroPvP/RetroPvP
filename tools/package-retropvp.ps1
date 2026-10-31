param(
    [string] $RomPath,
    [string] $RuntimePath,
    [string] $OutputPath,
    [switch] $Force
)

$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$romCandidate = Join-Path $projectRoot 'roms\smb.nes'
if ([string]::IsNullOrWhiteSpace($RomPath) -and (Test-Path -LiteralPath $romCandidate -PathType Leaf)) {
    $RomPath = $romCandidate
}
if ([string]::IsNullOrWhiteSpace($RomPath)) {
    Add-Type -AssemblyName System.Windows.Forms
    $romDialog = New-Object System.Windows.Forms.OpenFileDialog
    $romDialog.Title = 'Select the supported SMB1 ROM'
    $romDialog.Filter = 'NES ROMs (*.nes;*.unf;*.unif)|*.nes;*.unf;*.unif|All files (*.*)|*.*'
    if ($romDialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
        throw 'Provide a local ROM path with -RomPath to create the tester package.'
    }
    $RomPath = $romDialog.FileName
}
$outputDirectory = Split-Path $projectRoot -Parent
$workspaceRoot = Split-Path $outputDirectory -Parent
$profilePath = Join-Path $projectRoot 'config\games\smb1.json'
$profile = Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
$expectedRomSha1 = ([string]$profile.sha1).ToUpperInvariant()
$expectedRomSize = [long]$profile.rom_size

if (-not (Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "ROM not found: $RomPath"
}
$romItem = Get-Item -LiteralPath $RomPath
$romSha1 = (Get-FileHash -Algorithm SHA1 -LiteralPath $RomPath).Hash.ToUpperInvariant()
if ($romItem.Length -ne $expectedRomSize -or $romSha1 -ne $expectedRomSha1) {
    throw "The ROM does not match profile '$($profile.profile_id)' (size $expectedRomSize bytes, SHA-1 $expectedRomSha1)."
}

$frontend = Join-Path $projectRoot 'build\retropvp-frontend\retroarch.exe'
$core = Join-Path $projectRoot 'build\mesen2_libretro.dll'
if ([string]::IsNullOrWhiteSpace($RuntimePath)) {
    if (-not [string]::IsNullOrWhiteSpace($env:RETROPVP_RETROARCH_DIR)) {
        $RuntimePath = $env:RETROPVP_RETROARCH_DIR
    } else {
        $runtimeConfig = Join-Path $projectRoot 'build\retroarch-runtime-path.txt'
        if (Test-Path -LiteralPath $runtimeConfig -PathType Leaf) {
            $RuntimePath = (Get-Content -LiteralPath $runtimeConfig -Raw).Trim()
        } else {
            $RuntimePath = Join-Path $projectRoot 'build\runtime\RetroArch-Win64'
        }
    }
}
$runtimeSource = [System.IO.Path]::GetFullPath($RuntimePath)
if (-not (Test-Path -LiteralPath $runtimeSource -PathType Container)) {
    throw "RetroArch runtime assets are missing: $runtimeSource"
}
foreach ($requiredFile in @($frontend, $core)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required build/runtime file is missing: $requiredFile"
    }
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $outputDirectory 'RetroPvP-Tester-Package.zip'
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)
if ((Test-Path -LiteralPath $OutputPath) -and -not $Force) {
    throw "Package already exists: $OutputPath (use -Force to replace it)."
}

$stageRoot = Join-Path $workspaceRoot 'work\RetroPvP-package-staging'
$workRoot = [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot 'work'))
$stagePath = [System.IO.Path]::GetFullPath($stageRoot)
$temporaryArchive = Join-Path $workRoot ("RetroPvP-package-{0}.zip" -f [guid]::NewGuid().ToString('N'))
$workPrefix = $workRoot.TrimEnd('\') + '\'
if (-not $stagePath.StartsWith($workPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to stage outside work/: $stagePath"
}
if (Test-Path -LiteralPath $stagePath) {
    Remove-Item -LiteralPath $stagePath -Recurse -Force
}
New-Item -ItemType Directory -Path $stagePath -Force | Out-Null

try {
    $runtimeDestination = Join-Path $stagePath 'runtime'
    $runtimeCoreDestination = Join-Path $runtimeDestination 'cores'
    New-Item -ItemType Directory -Path $runtimeCoreDestination -Force | Out-Null

    Copy-Item -LiteralPath $frontend -Destination (Join-Path $runtimeDestination 'retroarch.exe')
    Copy-Item -LiteralPath $core -Destination (Join-Path $runtimeCoreDestination 'mesen2_libretro.dll')

    foreach ($directory in @('autoconfig')) {
        $sourceDirectory = Join-Path $runtimeSource $directory
        if (Test-Path -LiteralPath $sourceDirectory -PathType Container) {
            Copy-Item -LiteralPath $sourceDirectory -Destination $runtimeDestination -Recurse
        }
    }
    $ozoneAssetsSource = Join-Path $runtimeSource 'assets\ozone'
    if (-not (Test-Path -LiteralPath $ozoneAssetsSource -PathType Container)) {
        throw "Required Ozone menu assets are missing: $ozoneAssetsSource"
    }
    $ozoneDestination = Join-Path $runtimeDestination 'assets\ozone'
    New-Item -ItemType Directory -Path $ozoneDestination -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $ozoneAssetsSource 'png') -Destination $ozoneDestination -Recurse
    foreach ($font in @('regular.ttf', 'bold.ttf')) {
        Copy-Item -LiteralPath (Join-Path $ozoneAssetsSource $font) -Destination $ozoneDestination
    }

    New-Item -ItemType Directory -Path (Join-Path $stagePath 'roms') -Force | Out-Null
    Copy-Item -LiteralPath $RomPath -Destination (Join-Path $stagePath 'roms\smb.nes')
    New-Item -ItemType Directory -Path (Join-Path $stagePath 'config\games') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $stagePath 'tools') -Force | Out-Null
    Copy-Item -LiteralPath $profilePath -Destination (Join-Path $stagePath 'config\games\smb1.json')
    Copy-Item -LiteralPath (Join-Path $projectRoot 'Start-RetroPvP.cmd') -Destination $stagePath
    Copy-Item -LiteralPath (Join-Path $projectRoot 'tools\run-retroarch.ps1') -Destination (Join-Path $stagePath 'tools\run-retroarch.ps1') -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE') -Destination $stagePath
    Copy-Item -LiteralPath (Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md') -Destination $stagePath

    $quickStart = @'
RetroPvP — tester package

Windows x64. Extract this folder to a writable location and start Start-RetroPvP.cmd.

For an online session, choose RetroPvP > Host via Relay > Copy Host Address. Send the copied address to the other player. They choose RetroPvP > Join and paste it into Host address.

P1/P2 controls, emulator hotkeys, volume and relay region can be changed from the RetroPvP menu. Settings, saves and logs are kept in retroarch-userdata beside this launcher.
'@
    [System.IO.File]::WriteAllText((Join-Path $stagePath 'README-Tester.txt'), $quickStart + "`r`n", (New-Object System.Text.UTF8Encoding($false)))

    $stagedRomSha1 = (Get-FileHash -Algorithm SHA1 -LiteralPath (Join-Path $stagePath 'roms\smb.nes')).Hash.ToUpperInvariant()
    if ($stagedRomSha1 -ne $expectedRomSha1) {
        throw 'The staged ROM fingerprint changed during packaging.'
    }

    New-Item -ItemType Directory -Path (Split-Path $OutputPath -Parent) -Force | Out-Null
    Compress-Archive -Path (Join-Path $stagePath '*') -DestinationPath $temporaryArchive -CompressionLevel Optimal

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($temporaryArchive)
    try {
        $entryNames = @($archive.Entries | ForEach-Object { $_.FullName.Replace('\', '/') })
        foreach ($requiredEntry in @(
            'Start-RetroPvP.cmd', 'tools/run-retroarch.ps1', 'runtime/retroarch.exe',
            'runtime/cores/mesen2_libretro.dll', 'roms/smb.nes', 'config/games/smb1.json',
            'README-Tester.txt'
        )) {
            if ($requiredEntry -notin $entryNames) {
                throw "Package verification failed; missing archive entry: $requiredEntry"
            }
        }
    } finally {
        $archive.Dispose()
    }

    if (Test-Path -LiteralPath $OutputPath) {
        Remove-Item -LiteralPath $OutputPath -Force
    }
    Move-Item -LiteralPath $temporaryArchive -Destination $OutputPath

    $packageItem = Get-Item -LiteralPath $OutputPath
    Write-Output "Package: $OutputPath"
    Write-Output ("Size: {0:N1} MB" -f ($packageItem.Length / 1MB))
    Write-Output "ROM SHA-1 verified: $romSha1"
    Write-Output 'Archive contents verified.'
} finally {
    if (Test-Path -LiteralPath $stagePath) {
        Remove-Item -LiteralPath $stagePath -Recurse -Force
    }
    if (Test-Path -LiteralPath $temporaryArchive) {
        Remove-Item -LiteralPath $temporaryArchive -Force
    }
}
