# Creates Desktop shortcuts for Heritage Jarvis in BOTH desktop locations
# (OneDrive Desktop and local Desktop, since Windows merges them)
$WshShell = New-Object -ComObject WScript.Shell

# Get both possible desktop paths
$Desktops = @(
    $WshShell.SpecialFolders("Desktop"),
    [Environment]::GetFolderPath("Desktop")
) | Select-Object -Unique

$IconPath = Join-Path ([Environment]::GetFolderPath("Desktop")) "jarvis.ico"
if (-not (Test-Path $IconPath)) {
    $IconPath = Join-Path $WshShell.SpecialFolders("Desktop") "jarvis.ico"
}

foreach ($Desktop in $Desktops) {
    $ShortcutPath = Join-Path $Desktop "Heritage Jarvis.lnk"

    # Remove any stale shortcut
    if (Test-Path $ShortcutPath) { Remove-Item $ShortcutPath -Force }

    # Also remove old shortcut names
    $OldNames = @("Jarvis-AI.lnk", "Heritage_Jarvis.lnk", "HeritageJarvis.lnk")
    foreach ($Old in $OldNames) {
        $OldPath = Join-Path $Desktop $Old
        if (Test-Path $OldPath) { Remove-Item $OldPath -Force }
    }

    $Shortcut = $WshShell.CreateShortcut($ShortcutPath)
    $Shortcut.TargetPath = "C:\HeritageJarvis_UE5\launch_hj.bat"
    $Shortcut.WorkingDirectory = "C:\HeritageJarvis_UE5"
    $Shortcut.Description = "Launch Heritage Jarvis"
    $Shortcut.WindowStyle = 1
    $Shortcut.Arguments = ""

    if (Test-Path $IconPath) {
        $Shortcut.IconLocation = "$IconPath,0"
    }

    $Shortcut.Save()
    Write-Host "Shortcut created: $ShortcutPath"
}
