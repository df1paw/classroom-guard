# Build the MSI with WiX Toolset 3.x on Windows (candle/light must be in PATH).
# Usage: powershell -ExecutionPolicy Bypass -File build-msi.ps1 -Dll ..\..\dist\classroom-guard-qt6-v411.dll [-Version 2.0.0]
param([Parameter(Mandatory)][string]$Dll, [string]$Version = "2.0.0")
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot
candle.exe -arch x64 "-dVersion=$Version" "-dDllPath=$(Resolve-Path $Dll)" classroom-guard.wxs
light.exe -o classroom-guard-4.11.msi classroom-guard.wixobj
