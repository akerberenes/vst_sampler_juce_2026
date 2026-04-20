# backup.ps1 — Rolling backup of source + VST artefact
# Keeps 2 backups: backup_1.zip (latest) and backup_2.zip (previous).
# Run from the project root after a successful build + test.

param(
    [string]$ProjectRoot = $PSScriptRoot
)

$backupDir = Join-Path $ProjectRoot "backup"
$backup1   = Join-Path $backupDir "backup_1.zip"
$backup2   = Join-Path $backupDir "backup_2.zip"

# Ensure the backup folder exists.
if (-not (Test-Path $backupDir)) {
    New-Item -ItemType Directory -Path $backupDir | Out-Null
}

# Rotate: backup_1 -> backup_2 (discard old backup_2).
if (Test-Path $backup1) {
    if (Test-Path $backup2) { Remove-Item $backup2 -Force }
    Move-Item $backup1 $backup2 -Force
}

# Collect source files and VST artefact into a temp staging folder.
$staging = Join-Path $env:TEMP "vst_backup_staging_$(Get-Random)"
New-Item -ItemType Directory -Path $staging | Out-Null

try {
    # Copy source tree (exclude build, backup, .vs, node_modules).
    $excludeDirs = @("build", "backup", ".vs", "node_modules")
    $items = Get-ChildItem -Path $ProjectRoot -Force |
        Where-Object { -not ($excludeDirs -contains $_.Name) }

    foreach ($item in $items) {
        $dest = Join-Path $staging $item.Name
        if ($item.PSIsContainer) {
            Copy-Item $item.FullName $dest -Recurse -Force
        } else {
            Copy-Item $item.FullName $dest -Force
        }
    }

    # Copy the built VST3 if it exists.
    $vstSource = Join-Path $ProjectRoot "build\SamplerWithFreeze_artefacts\Release\VST3"
    if (Test-Path $vstSource) {
        $vstDest = Join-Path $staging "VST3_artefact"
        Copy-Item $vstSource $vstDest -Recurse -Force
    }

    # Compress to backup_1.zip.
    Compress-Archive -Path "$staging\*" -DestinationPath $backup1 -Force

    Write-Host "Backup created: $backup1"
    if (Test-Path $backup2) {
        Write-Host "Previous backup: $backup2"
    }
} finally {
    # Clean up staging folder.
    Remove-Item $staging -Recurse -Force -ErrorAction SilentlyContinue
}
