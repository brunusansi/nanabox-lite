param(
    [string]$InstallDir = "C:\Program Files\NanaBoxLite"
)

Write-Host "NanaBox Lite Installer" -ForegroundColor Cyan
Write-Host ""

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
Write-Host "Source directory: $scriptRoot"
Write-Host "Installation directory: $InstallDir"
Write-Host ""

# Source paths (within the extracted bundle)
$cliPath      = Join-Path $scriptRoot "nanabox-lite.exe"
$driverSys    = Join-Path $scriptRoot "nanabox_hvfilter.sys"
$driverInf    = Join-Path $scriptRoot "nanabox_hvfilter.inf"
$profilesDir  = Join-Path $scriptRoot "profiles"

$missing = @()

if (-not (Test-Path $cliPath))     { $missing += $cliPath }
if (-not (Test-Path $driverSys))  { $missing += $driverSys }
if (-not (Test-Path $driverInf))  { $missing += $driverInf }
if (-not (Test-Path $profilesDir)) { $missing += $profilesDir }

if ($missing.Count -gt 0) {
    Write-Host "Error: required files not found:" -ForegroundColor Red
    $missing | ForEach-Object { Write-Host " - $_" -ForegroundColor Red }
    throw "Missing required files. Did you extract the entire artifact (nanabox-lite-bundle)?"
}

Write-Host "All source files found." -ForegroundColor Green
Write-Host ""

# Create installation folder
if (-not (Test-Path $InstallDir)) {
    Write-Host "Creating folder: $InstallDir"
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
}

# Copy CLI and profiles
Write-Host "Copying binary and profiles..."
Copy-Item $cliPath -Destination (Join-Path $InstallDir "nanabox-lite.exe") -Force
if (Test-Path $profilesDir) {
    $destProfiles = Join-Path $InstallDir "profiles"
    Copy-Item $profilesDir -Destination $destProfiles -Recurse -Force
}

# Install driver via pnputil using the INF
Write-Host "Installing driver (nanabox_hvfilter) via pnputil..."
$pnputil = "$env:SystemRoot\System32\pnputil.exe"

& $pnputil /add-driver $driverInf /install
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to install driver via pnputil (code $LASTEXITCODE)." -ForegroundColor Red
    throw "Driver installation failed."
}

Write-Host ""
Write-Host "Installation completed." -ForegroundColor Green
Write-Host "Binary: $InstallDir\nanabox-lite.exe"
Write-Host "Profiles: $InstallDir\profiles"
Write-Host ""
Write-Host "You can apply a profile with, for example:" -ForegroundColor Yellow
Write-Host "`"$InstallDir\nanabox-lite.exe`" --profile `"$InstallDir\profiles\roblox-lite.json`""
