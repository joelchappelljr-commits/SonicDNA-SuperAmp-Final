$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$BuildDir = Join-Path $PSScriptRoot "build-windows"
$DistDir = Join-Path $PSScriptRoot "distribution"
$InstallerOut = Join-Path $PSScriptRoot "installer-output"

if (Test-Path $BuildDir) { Remove-Item $BuildDir -Recurse -Force }
if (Test-Path $DistDir) { Remove-Item $DistDir -Recurse -Force }
if (Test-Path $InstallerOut) { Remove-Item $InstallerOut -Recurse -Force }

cmake -S . -B $BuildDir -G "Visual Studio 17 2022" -A x64
cmake --build $BuildDir --config Release --target SDNASuperAmp_Standalone SDNASuperAmp_VST3 --parallel 2

$Standalone = Join-Path $BuildDir "SDNASuperAmp_artefacts\Release\Standalone\SonicDNA SuperAmp.exe"
$VST3 = Join-Path $BuildDir "SDNASuperAmp_artefacts\Release\VST3\SonicDNA SuperAmp.vst3"

if (-not (Test-Path $Standalone -PathType Leaf)) { throw "Standalone build missing: $Standalone" }
if (-not (Test-Path $VST3 -PathType Container)) { throw "VST3 bundle missing: $VST3" }

New-Item -ItemType Directory -Force (Join-Path $DistDir "Standalone") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $DistDir "VST3") | Out-Null
Copy-Item $Standalone (Join-Path $DistDir "Standalone\SonicDNA SuperAmp.exe")
Copy-Item $VST3 (Join-Path $DistDir "VST3") -Recurse

$ISCC = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $ISCC)) {
    Write-Host ""
    Write-Host "Standalone and VST3 built successfully."
    Write-Host "Inno Setup 6 was not found, so the installer was not created."
    Write-Host "Install Inno Setup 6 and rerun this script."
    exit 0
}

New-Item -ItemType Directory -Force $InstallerOut | Out-Null
& $ISCC "Installer\Windows\SDNASuperAmp.iss"

Write-Host ""
Write-Host "BUILD COMPLETE"
Write-Host "Standalone: $Standalone"
Write-Host "VST3:       $VST3"
Write-Host "Installer:  $InstallerOut\SonicDNA-SuperAmp-Windows-x64-ASIO-Setup.exe"
