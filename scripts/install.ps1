<#
.SYNOPSIS
    Installs NanaBox Lite components inside a Windows guest VM.

.DESCRIPTION
    This script installs the NanaBox Lite driver, CLI tool, and profiles
    on a Windows guest VM running under Hyper-V with GPU-P.

    Run this script as Administrator inside the guest VM.

.PARAMETER InstallDir
    Target installation directory. Defaults to "C:\Program Files\NanaBoxLite".

.PARAMETER BuildConfig
    Build configuration to use (Debug or Release). Defaults to Release.

.EXAMPLE
    .\install.ps1
    Installs using default settings.

.EXAMPLE
    .\install.ps1 -InstallDir "D:\NanaBoxLite" -BuildConfig Debug
    Installs to a custom directory using Debug build.

.NOTES
    Requirements:
    - Run as Administrator
    - Test signing must be enabled for unsigned drivers
    - Build output must be available in the build\ directory
#>

param (
    [string]$InstallDir = "C:\Program Files\NanaBoxLite",
    [ValidateSet("Debug", "Release")]
    [string]$BuildConfig = "Release"
)

$ErrorActionPreference = "Stop"

# Check for administrator privileges
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "This script must be run as Administrator."
    exit 1
}

# Get script directory (where the repo is located)
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoDir = Split-Path -Parent $ScriptDir

# Paths to build output
$Platform = "x64"
$BuildDir = Join-Path $RepoDir "build\$BuildConfig\$Platform"
$DriverDir = Join-Path $RepoDir "driver"

# Source files
$ExePath = Join-Path $BuildDir "nanabox-lite.exe"
$DriverSysPath = Join-Path $BuildDir "nanabox_hvfilter.sys"
$DriverInfPath = Join-Path $DriverDir "nanabox_hvfilter.inf"
$ProfilesDir = Join-Path $RepoDir "profiles"

Write-Host "NanaBox Lite Installer" -ForegroundColor Cyan
Write-Host "======================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Installation Directory: $InstallDir" -ForegroundColor Yellow
Write-Host "Build Configuration: $BuildConfig" -ForegroundColor Yellow
Write-Host ""

# Verify source files exist
Write-Host "Checking source files..." -ForegroundColor White

$filesToCheck = @(
    @{ Path = $ExePath; Description = "CLI executable" },
    @{ Path = $DriverSysPath; Description = "Driver binary" },
    @{ Path = $DriverInfPath; Description = "Driver INF" },
    @{ Path = $ProfilesDir; Description = "Profiles directory" }
)

$missingFiles = @()
foreach ($file in $filesToCheck) {
    if (Test-Path $file.Path) {
        Write-Host "  [OK] $($file.Description): $($file.Path)" -ForegroundColor Green
    } else {
        Write-Host "  [MISSING] $($file.Description): $($file.Path)" -ForegroundColor Red
        $missingFiles += $file.Path
    }
}

if ($missingFiles.Count -gt 0) {
    Write-Host ""
    Write-Error "Missing required files. Please build the solution first."
    exit 1
}

# Step 1: Create installation directory
Write-Host ""
Write-Host "Step 1: Creating installation directory..." -ForegroundColor White

if (-not (Test-Path $InstallDir)) {
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
    Write-Host "  Created: $InstallDir" -ForegroundColor Green
} else {
    Write-Host "  Directory already exists: $InstallDir" -ForegroundColor Yellow
}

# Step 2: Copy CLI executable
Write-Host ""
Write-Host "Step 2: Copying CLI executable..." -ForegroundColor White

$destExe = Join-Path $InstallDir "nanabox-lite.exe"
Copy-Item -Path $ExePath -Destination $destExe -Force
Write-Host "  Copied: $destExe" -ForegroundColor Green

# Step 3: Copy profiles
Write-Host ""
Write-Host "Step 3: Copying profiles..." -ForegroundColor White

$destProfiles = Join-Path $InstallDir "profiles"
if (Test-Path $destProfiles) {
    Remove-Item -Path $destProfiles -Recurse -Force
}
Copy-Item -Path $ProfilesDir -Destination $destProfiles -Recurse -Force
Write-Host "  Copied profiles to: $destProfiles" -ForegroundColor Green

# Step 4: Install driver using pnputil
Write-Host ""
Write-Host "Step 4: Installing driver..." -ForegroundColor White

# Copy driver files to a temp location for installation
$driverInstallDir = Join-Path $env:TEMP "NanaBoxDriver"
if (Test-Path $driverInstallDir) {
    Remove-Item -Path $driverInstallDir -Recurse -Force
}
New-Item -ItemType Directory -Path $driverInstallDir -Force | Out-Null

# Copy driver files
Copy-Item -Path $DriverSysPath -Destination $driverInstallDir -Force
Copy-Item -Path $DriverInfPath -Destination $driverInstallDir -Force

$infInstallPath = Join-Path $driverInstallDir "nanabox_hvfilter.inf"

Write-Host "  Installing driver from: $infInstallPath" -ForegroundColor Yellow

try {
    $result = & pnputil /add-driver "$infInstallPath" /install 2>&1
    Write-Host $result -ForegroundColor Gray

    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "  Warning: Driver installation returned non-zero exit code." -ForegroundColor Yellow
        Write-Host "  This may be expected if the driver is already installed." -ForegroundColor Yellow
    } else {
        Write-Host "  Driver installed successfully." -ForegroundColor Green
    }
} catch {
    Write-Host ""
    Write-Host "  Warning: Failed to install driver: $_" -ForegroundColor Yellow
    Write-Host "  You may need to install the driver manually." -ForegroundColor Yellow
}

# Cleanup temp driver directory
Remove-Item -Path $driverInstallDir -Recurse -Force -ErrorAction SilentlyContinue

# Step 5: Print usage message
Write-Host ""
Write-Host "Installation complete!" -ForegroundColor Green
Write-Host ""
Write-Host "Usage:" -ForegroundColor Cyan

$exeWithPath = "`"$InstallDir\nanabox-lite.exe`""
$profileWithPath = "`"$InstallDir\profiles\roblox-lite.json`""

Write-Host "$exeWithPath --profile $profileWithPath" -ForegroundColor Yellow
Write-Host ""
Write-Host "Notes:" -ForegroundColor Cyan
Write-Host "  - Run the CLI as Administrator" -ForegroundColor White
Write-Host "  - Ensure test signing is enabled for unsigned drivers:" -ForegroundColor White
Write-Host "    bcdedit /set testsigning on" -ForegroundColor Yellow
Write-Host "  - Reboot after enabling test signing" -ForegroundColor White
