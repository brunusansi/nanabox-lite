param(
    [string]$InstallDir = "C:\Program Files\NanaBoxLite"
)

Write-Host "NanaBox Lite Installer" -ForegroundColor Cyan
Write-Host ""

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
Write-Host "Source directory: $scriptRoot"
Write-Host "Installation directory: $InstallDir"
Write-Host ""

# Caminhos de origem (dentro do bundle extraído)
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
    Write-Host "Erro: arquivos necessários não encontrados:" -ForegroundColor Red
    $missing | ForEach-Object { Write-Host " - $_" -ForegroundColor Red }
    throw "Missing required files. Did you extract the entire artifact (nanabox-lite-bundle)?"
}

Write-Host "Todos os arquivos de origem foram encontrados." -ForegroundColor Green
Write-Host ""

# Cria pasta de instalação
if (-not (Test-Path $InstallDir)) {
    Write-Host "Criando pasta: $InstallDir"
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
}

# Copia CLI e perfis
Write-Host "Copiando binário e perfis..."
Copy-Item $cliPath -Destination (Join-Path $InstallDir "nanabox-lite.exe") -Force
if (Test-Path $profilesDir) {
    $destProfiles = Join-Path $InstallDir "profiles"
    Copy-Item $profilesDir -Destination $destProfiles -Recurse -Force
}

# Instala driver via pnputil usando o INF
Write-Host "Instalando driver (nanabox_hvfilter) via pnputil..."
$pnputil = "$env:SystemRoot\System32\pnputil.exe"

& $pnputil /add-driver $driverInf /install
if ($LASTEXITCODE -ne 0) {
    Write-Host "Falha ao instalar o driver via pnputil (código $LASTEXITCODE)." -ForegroundColor Red
    throw "Driver installation failed."
}

Write-Host ""
Write-Host "Instalação concluída." -ForegroundColor Green
Write-Host "Binário: $InstallDir\nanabox-lite.exe"
Write-Host "Perfis:  $InstallDir\profiles"
Write-Host ""
Write-Host "Você pode aplicar um perfil com, por exemplo:" -ForegroundColor Yellow
Write-Host "`"$InstallDir\nanabox-lite.exe`" --profile `"$InstallDir\profiles\roblox-lite.json`""
