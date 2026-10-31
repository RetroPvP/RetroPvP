param(
    [string] $RomPath
)

$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$portableRuntime = Join-Path $projectRoot 'runtime'
$portableCore = Join-Path $portableRuntime 'cores\mesen2_libretro.dll'
$portableRom = Join-Path $projectRoot 'roms\smb.nes'
$portableMode = Test-Path -LiteralPath $portableRuntime -PathType Container

if ($portableMode) {
    foreach ($requiredFile in @((Join-Path $portableRuntime 'retroarch.exe'), $portableCore, $portableRom)) {
        if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
            throw "Portable package file is missing: $requiredFile"
        }
    }
    $retroArchRuntime = $portableRuntime
    $retroArch = Join-Path $retroArchRuntime 'retroarch.exe'
    $core = $portableCore
    if ([string]::IsNullOrWhiteSpace($RomPath)) {
        $RomPath = $portableRom
    }
} else {
    if (-not [string]::IsNullOrWhiteSpace($env:RETROPVP_RETROARCH_DIR)) {
        $retroArchRuntime = $env:RETROPVP_RETROARCH_DIR
    } else {
        $runtimeConfig = Join-Path $projectRoot 'build\retroarch-runtime-path.txt'
        if (Test-Path -LiteralPath $runtimeConfig -PathType Leaf) {
            $retroArchRuntime = (Get-Content -LiteralPath $runtimeConfig -Raw).Trim()
        } else {
            $retroArchRuntime = Join-Path $projectRoot 'build\runtime\RetroArch-Win64'
        }
    }
    $retroArch = Join-Path $projectRoot 'build\retropvp-frontend\retroarch.exe'
    $core = Join-Path $projectRoot 'build\mesen2_libretro.dll'
    if ([string]::IsNullOrWhiteSpace($RomPath)) {
        $preferredRom = Join-Path $projectRoot 'roms\smb.nes'
        if (Test-Path -LiteralPath $preferredRom) {
            $RomPath = $preferredRom
        } else {
            Add-Type -AssemblyName System.Windows.Forms
            $dialog = New-Object System.Windows.Forms.OpenFileDialog
            $dialog.Title = 'Select the supported SMB1 ROM'
            $dialog.Filter = 'NES ROMs (*.nes;*.unf;*.unif)|*.nes;*.unf;*.unif|All files (*.*)|*.*'
            if ($dialog.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
                return
            }
            $RomPath = $dialog.FileName
        }
    }
}

if (-not (Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "ROM not found: $RomPath"
}
$rom = (Resolve-Path -LiteralPath $RomPath).Path
$userData = Join-Path $projectRoot 'retroarch-userdata'
$config = Join-Path $userData 'retroarch.cfg'
$localPathsConfig = Join-Path $userData 'retropvp-local.cfg'
$log = Join-Path $userData 'retroarch.log'

if (-not (Test-Path -LiteralPath $retroArchRuntime -PathType Container)) {
    throw "RetroArch runtime files are missing: $retroArchRuntime"
}
if (-not (Test-Path -LiteralPath $retroArch)) {
    if ($portableMode) {
        throw "The bundled RetroPvP frontend is missing: $retroArch"
    }
    & (Join-Path $projectRoot 'tools\build-retropvp-frontend.ps1')
}
if (-not (Test-Path -LiteralPath $retroArch)) {
    throw "RetroPvP frontend was not created at $retroArch."
}
if (-not (Test-Path -LiteralPath $core)) {
    throw "The custom core is missing: $core"
}

$userDirectories = @(
    'saves', 'states', 'screenshots', 'system', 'playlists\builtin', 'cache',
    'cheats', 'core-options', 'database', 'downloads', 'config\remaps',
    'config\record', 'logs', 'recordings', 'shaders', 'overlays', 'overlays\keyboards',
    'thumbnails', 'filters\video', 'filters\audio'
)
foreach ($directory in $userDirectories) {
    New-Item -ItemType Directory -Force -Path (Join-Path $userData $directory) | Out-Null
}

$runtimeDirectories = @{
    assets = Join-Path $retroArchRuntime 'assets'
    autoconfig = Join-Path $retroArchRuntime 'autoconfig'
}
$portableDirectories = @{
    saves = Join-Path $userData 'saves'
    states = Join-Path $userData 'states'
    screenshots = Join-Path $userData 'screenshots'
    system = Join-Path $userData 'system'
    playlists = Join-Path $userData 'playlists'
    cache = Join-Path $userData 'cache'
    assets = $runtimeDirectories.assets
    cheats = Join-Path $userData 'cheats'
    coreOptions = Join-Path $userData 'core-options'
    database = Join-Path $userData 'database'
    downloads = Join-Path $userData 'downloads'
    remaps = Join-Path $userData 'config\remaps'
    recordConfig = Join-Path $userData 'config\record'
    logs = Join-Path $userData 'logs'
    recordings = Join-Path $userData 'recordings'
    shaders = Join-Path $userData 'shaders'
    overlays = Join-Path $userData 'overlays'
    thumbnails = Join-Path $userData 'thumbnails'
    core = Join-Path $retroArchRuntime 'cores'
    autoconfig = $runtimeDirectories.autoconfig
    videoFilters = Join-Path $userData 'filters\video'
    audioFilters = Join-Path $userData 'filters\audio'
    roms = Join-Path $projectRoot 'roms'
}

$localPathLines = @(
    'video_driver = "gl"'
    'menu_driver = "ozone"'
    'input_toggle_fast_forward = "nul"'
    'input_hold_fast_forward = "nul"'
    "assets_directory = `"$($portableDirectories.assets)`""
    "savefile_directory = `"$($portableDirectories.saves)`""
    "savestate_directory = `"$($portableDirectories.states)`""
    "screenshot_directory = `"$($portableDirectories.screenshots)`""
    "core_options_path = `"$($portableDirectories.coreOptions)`""
    "system_directory = `"$($portableDirectories.system)`""
    "cache_directory = `"$($portableDirectories.cache)`""
    "cheat_database_path = `"$($portableDirectories.cheats)`""
    "playlist_directory = `"$($portableDirectories.playlists)`""
    "content_history_path = `"$(Join-Path $portableDirectories.playlists 'builtin\content_history.lpl')`""
    "content_favorites_path = `"$(Join-Path $portableDirectories.playlists 'builtin\content_favorites.lpl')`""
    "content_music_history_path = `"$(Join-Path $portableDirectories.playlists 'builtin\content_music_history.lpl')`""
    "content_image_history_path = `"$(Join-Path $portableDirectories.playlists 'builtin\content_image_history.lpl')`""
    "content_video_history_path = `"$(Join-Path $portableDirectories.playlists 'builtin\content_video_history.lpl')`""
    "content_database_path = `"$($portableDirectories.database)`""
    "core_assets_directory = `"$($portableDirectories.downloads)`""
    "input_remapping_directory = `"$($portableDirectories.remaps)`""
    "libretro_directory = `"$($portableDirectories.core)`""
    "joypad_autoconfig_dir = `"$($portableDirectories.autoconfig)`""
    "video_filter_dir = `"$($portableDirectories.videoFilters)`""
    "audio_filter_dir = `"$($portableDirectories.audioFilters)`""
    "shader_directory = `"$($portableDirectories.shaders)`""
    "overlay_directory = `"$($portableDirectories.overlays)`""
    "osk_overlay_directory = `"$(Join-Path $portableDirectories.overlays 'keyboards')`""
    "recording_config_directory = `"$($portableDirectories.recordConfig)`""
    "recording_output_directory = `"$($portableDirectories.recordings)`""
    "thumbnails_directory = `"$($portableDirectories.thumbnails)`""
    "rgui_browser_directory = `"$($portableDirectories.roms)`""
    "rgui_config_directory = `"$(Join-Path $userData 'config')`""
    "runtime_log_directory = `"$($portableDirectories.logs)`""
)
$encoding = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($localPathsConfig, ($localPathLines -join "`r`n") + "`r`n", $encoding)

if (-not (Test-Path -LiteralPath $config) -or (Get-Item -LiteralPath $config).Length -eq 0) {
    $configLines = @(
        'config_save_on_exit = true'
        'netplay_mitm_server = "madrid"'
        'input_player1_up = "w"'
        'input_player1_down = "s"'
        'input_player1_left = "a"'
        'input_player1_right = "d"'
        'input_player1_a = "j"'
        'input_player1_b = "k"'
        'input_player1_start = "enter"'
        'input_player1_x = "nul"'
        'input_player1_y = "nul"'
        'input_frame_advance = "nul"'
        'input_player2_a = "comma"'
        'input_player2_b = "period"'
        'input_player2_select = "keypad0"'
        'input_player2_select_mbtn = "2"'
        'input_player2_mouse_index = "0"'
        'input_player2_l = "keypad7"'
        'input_player2_r = "keypad9"'
        'input_player2_x = "keypad5"'
        'input_player2_up = "keypad8"'
        'input_player2_down = "keypad2"'
        'input_player2_left = "keypad4"'
        'input_player2_right = "keypad6"'
    )
    [System.IO.File]::WriteAllText($config, ($configLines -join "`r`n") + "`r`n", $encoding)
}

$arguments = @(
    '--verbose',
    '--config', "`"$config`"",
    '--appendconfig', "`"$localPathsConfig`"",
    '--log-file', "`"$log`"",
    '--libretro', "`"$core`"",
    "`"$rom`""
)
$retroArchProcess = Start-Process -FilePath $retroArch -ArgumentList $arguments -WorkingDirectory $retroArchRuntime -PassThru
Start-Sleep -Seconds 2
$retroArchProcess.Refresh()
if ($retroArchProcess.HasExited) {
    $recentLog = Get-Content -LiteralPath $log -Tail 25 -ErrorAction SilentlyContinue | Out-String
    throw "RetroPvP exited during startup (code $($retroArchProcess.ExitCode)). Log: $log`n$recentLog"
}
