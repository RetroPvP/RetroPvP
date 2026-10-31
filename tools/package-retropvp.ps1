param(
    [string] $RomPath,
    [string] $RuntimePath,
    [string] $OutputPath,
    [switch] $NoRom,
    [switch] $Force
)

$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$outputDirectory = Split-Path $projectRoot -Parent
$workspaceRoot = Split-Path $outputDirectory -Parent
$profilePath = Join-Path $projectRoot 'config\games\smb1.json'
$profile = Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
$expectedRomSha1 = ([string]$profile.sha1).ToUpperInvariant()
$expectedRomSize = [long]$profile.rom_size

if ($NoRom) {
    if (-not [string]::IsNullOrWhiteSpace($RomPath)) {
        throw 'Do not pass -RomPath when creating a no-ROM release package.'
    }
    $romSha1 = $null
} else {
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
    if (-not (Test-Path -LiteralPath $RomPath -PathType Leaf)) {
        throw "ROM not found: $RomPath"
    }
    $romItem = Get-Item -LiteralPath $RomPath
    $romSha1 = (Get-FileHash -Algorithm SHA1 -LiteralPath $RomPath).Hash.ToUpperInvariant()
    if ($romItem.Length -ne $expectedRomSize -or $romSha1 -ne $expectedRomSha1) {
        throw "The ROM does not match profile '$($profile.profile_id)' (size $expectedRomSize bytes, SHA-1 $expectedRomSha1)."
    }
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
    if ($NoRom) {
        $OutputPath = Join-Path $projectRoot 'build\releases\RetroPvP-0.1.0-alpha.1-win-x64.zip'
    } else {
        $OutputPath = Join-Path $outputDirectory 'RetroPvP-Tester-Package.zip'
    }
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

    $romDirectory = Join-Path $stagePath 'roms'
    New-Item -ItemType Directory -Path $romDirectory -Force | Out-Null
    if ($NoRom) {
        $romNote = "Place your matching SMB1 image here as smb.nes, or choose it when RetroPvP starts.`r`n"
        [System.IO.File]::WriteAllText((Join-Path $romDirectory 'README.txt'), $romNote, (New-Object System.Text.UTF8Encoding($false)))
    } else {
        Copy-Item -LiteralPath $RomPath -Destination (Join-Path $romDirectory 'smb.nes')
    }
    New-Item -ItemType Directory -Path (Join-Path $stagePath 'config\games') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $stagePath 'tools') -Force | Out-Null
    Copy-Item -LiteralPath $profilePath -Destination (Join-Path $stagePath 'config\games\smb1.json')
    Copy-Item -LiteralPath (Join-Path $projectRoot 'Start-RetroPvP.cmd') -Destination $stagePath
    Copy-Item -LiteralPath (Join-Path $projectRoot 'tools\run-retroarch.ps1') -Destination (Join-Path $stagePath 'tools\run-retroarch.ps1') -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE') -Destination $stagePath
    Copy-Item -LiteralPath (Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md') -Destination $stagePath

    $licenseDirectory = Join-Path $stagePath 'licenses'
    New-Item -ItemType Directory -Path $licenseDirectory -Force | Out-Null
    foreach ($license in @(
        @{ Source = (Join-Path $projectRoot '.third_party\MesenCE\LICENSE'); Name = 'MesenCE-LICENSE.txt' },
        @{ Source = (Join-Path $projectRoot 'build\retroarch-src\COPYING'); Name = 'RetroArch-COPYING.txt' },
        @{ Source = (Join-Path $runtimeSource 'assets\COPYING'); Name = 'RetroArch-assets-COPYING.txt' }
    )) {
        if (-not (Test-Path -LiteralPath $license.Source -PathType Leaf)) {
            throw "A required distribution license file is missing: $($license.Source)"
        }
        Copy-Item -LiteralPath $license.Source -Destination (Join-Path $licenseDirectory $license.Name)
    }

    if ($NoRom) {
        $readmeName = 'README.txt'
        $quickStart = @'
RetroPvP 0.1.0-alpha.1 — Windows x64

Extract this folder and start Start-RetroPvP.cmd. On first launch, select your own matching SMB1 ROM, or place it in roms\smb.nes before starting. No ROM is included.

The supported ROM fingerprint is listed in config\games\smb1.json. Project and upstream license texts are in LICENSE, THIRD_PARTY_NOTICES.md, and licenses\.
'@
    } else {
        $readmeName = 'README-Tester.txt'
        $quickStart = @'
RetroPvP — tester package

Windows x64. Extract this folder to a writable location and start Start-RetroPvP.cmd.

For an online session, choose RetroPvP > Host via Relay > Copy Host Address. Send the copied address to the other player. They choose RetroPvP > Join and paste it into Host address.

P1/P2 controls, emulator hotkeys, volume and relay region can be changed from the RetroPvP menu. Settings, saves and logs are kept in retroarch-userdata beside this launcher.
'@
    }
    [System.IO.File]::WriteAllText((Join-Path $stagePath $readmeName), $quickStart + "`r`n", (New-Object System.Text.UTF8Encoding($false)))

    if (-not $NoRom) {
        $stagedRomSha1 = (Get-FileHash -Algorithm SHA1 -LiteralPath (Join-Path $stagePath 'roms\smb.nes')).Hash.ToUpperInvariant()
        if ($stagedRomSha1 -ne $expectedRomSha1) {
            throw 'The staged ROM fingerprint changed during packaging.'
        }
    }

    New-Item -ItemType Directory -Path (Split-Path $OutputPath -Parent) -Force | Out-Null
    Compress-Archive -Path (Join-Path $stagePath '*') -DestinationPath $temporaryArchive -CompressionLevel Optimal

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::OpenRead($temporaryArchive)
    try {
        $entryNames = @($archive.Entries | ForEach-Object { $_.FullName.Replace('\', '/') })
        $requiredEntries = @(
            'Start-RetroPvP.cmd', 'tools/run-retroarch.ps1', 'runtime/retroarch.exe',
            'runtime/cores/mesen2_libretro.dll', 'config/games/smb1.json',
            'roms/README.txt', 'licenses/MesenCE-LICENSE.txt', 'licenses/RetroArch-COPYING.txt',
            'licenses/RetroArch-assets-COPYING.txt', $readmeName
        )
        if (-not $NoRom) {
            $requiredEntries += 'roms/smb.nes'
        }
        foreach ($requiredEntry in $requiredEntries) {
            if ($requiredEntry -notin $entryNames) {
                throw "Package verification failed; missing archive entry: $requiredEntry"
            }
        }
        if ($NoRom -and @($entryNames | Where-Object { $_ -match '(?i)\.(nes|unf|unif|sav|srm|state)$' }).Count -gt 0) {
            throw 'No-ROM package verification failed; ROM or save data is present in the archive.'
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
    if ($NoRom) {
        Write-Output 'ROM included: no'
    } else {
        Write-Output "ROM SHA-1 verified: $romSha1"
    }
    Write-Output 'Archive contents verified.'
} finally {
    if (Test-Path -LiteralPath $stagePath) {
        Remove-Item -LiteralPath $stagePath -Recurse -Force
    }
    if (Test-Path -LiteralPath $temporaryArchive) {
        Remove-Item -LiteralPath $temporaryArchive -Force
    }
}
