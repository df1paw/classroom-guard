<#
  build-windows.ps1 - builds Classroom Guard for the Veyon that is installed on
  THIS computer and checks that Windows and Qt can load the result.
  (c) by Andreas Weller - https://www.andreas-weller.de/

  What it does
    1. finds the installed Veyon (veyon-core.dll) and reads its version and the
       version of the Qt it ships (Qt6Core.dll);
    2. builds tools\pe_tool.exe and creates the import library of the *installed*
       veyon-core.dll from it (the plugin must be linked against exactly the
       functions that DLL exports);
    3. gets the Veyon sources of the same version (headers only) if needed;
    4. configures and builds the plugin with CMake (Qt 6, MinGW);
    5. CHECKS the result:
         - every function the plugin imports from veyon-core.dll / Qt6*.dll
           must be exported by the installed DLLs of that folder, and
         - the Qt version the plugin was built with must not be newer than the
           Qt of the installed Veyon (Qt refuses plugins built with a newer Qt).
       Only when both hold the script prints PASS.

  Usage (PowerShell, from the repository folder)
    .\tools\build-windows.ps1 -QtDir C:\Qt\6.7.3\mingw_64 -Mingw C:\Qt\Tools\mingw1310_64\bin
    .\tools\build-windows.ps1 -VeyonDir "D:\Veyon" -VeyonSource C:\src\veyon-4.11.4

  Check an existing DLL only (no build), e.g. on the teacher computer:
    .\tools\build-windows.ps1 -CheckOnly -Dll .\dist\classroom-guard-qt6-v411.dll -QtBuiltWith 6.7.3

  Notes
    - Qt for the build: any Qt 6 >= 6.7 for MinGW. The lowest one that is
      installed is used by default, because a plugin built with an older Qt also
      loads in newer Qt hosts (Veyon 4.10/4.11 ship Qt 6.10).
    - Veyon 4.9.x/4.10.x and 4.11.x have different plugin APIs; the script picks
      the right sources by the installed version (both are supported).
#>
param(
    [string]$VeyonDir = "",
    [string]$QtDir = "",
    [string]$Mingw = "",
    [string]$VeyonSource = "",
    [string]$BuildDir = "",
    [string]$CMake = "cmake",
    [switch]$CheckOnly,
    [string]$Dll = "",
    [string]$QtBuiltWith = ""
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $BuildDir) { $BuildDir = Join-Path $root "build-installed-veyon" }

function Fail($text) { Write-Host "ERROR: $text" -ForegroundColor Red; exit 1 }
function Step($text) { Write-Host ""; Write-Host "== $text" -ForegroundColor Cyan }

function Get-FileVer([string]$path) {
    $v = (Get-Item $path).VersionInfo
    $text = $v.ProductVersion
    if (-not $text) { $text = $v.FileVersion }
    $m = [regex]::Match([string]$text, '(\d+)\.(\d+)\.(\d+)')
    if (-not $m.Success) { Fail "cannot read a version number from $path" }
    return [version]("{0}.{1}.{2}" -f $m.Groups[1].Value, $m.Groups[2].Value, $m.Groups[3].Value)
}

# --- 1. the installed Veyon ----------------------------------------------------
Step "Installed Veyon"
$candidates = @()
if ($VeyonDir) { $candidates += $VeyonDir }
foreach ($key in 'HKLM:\SOFTWARE\Veyon Solutions\Veyon', 'HKLM:\SOFTWARE\WOW6432Node\Veyon Solutions\Veyon') {
    if (Test-Path $key) {
        $p = (Get-ItemProperty $key -ErrorAction SilentlyContinue).'(default)'
        if ($p) { $candidates += $p }
    }
}
$candidates += "$env:ProgramFiles\Veyon", "${env:ProgramFiles(x86)}\Veyon"
$VeyonDir = ""
foreach ($c in $candidates) {
    if ($c -and (Test-Path (Join-Path $c "veyon-core.dll"))) { $VeyonDir = $c; break }
}
if (-not $VeyonDir) { Fail "Veyon not found. Pass its folder with -VeyonDir (the folder that contains veyon-core.dll)." }

$core = Join-Path $VeyonDir "veyon-core.dll"
$qtCore = Join-Path $VeyonDir "Qt6Core.dll"
if (-not (Test-Path $qtCore)) { Fail "Qt6Core.dll not found in $VeyonDir - this script supports Veyon 4.9 and newer (Qt 6) only." }
$veyonVer = Get-FileVer $core
$hostQt = Get-FileVer $qtCore
Write-Host "Veyon folder : $VeyonDir"
Write-Host "Veyon version: $veyonVer"
Write-Host "Qt of Veyon  : $hostQt"
if ($veyonVer -lt [version]"4.9.0") { Fail "Veyon $veyonVer uses Qt 5; build that variant with the CMake instructions in README.md (section 5.1)." }

function Test-QtRule([version]$builtWith, [version]$hostQtVersion) {
    # Qt loads a plugin only if it has the same major version and a minor version
    # that is not newer than the application's.
    return ($builtWith.Major -eq $hostQtVersion.Major) -and ($builtWith.Minor -le $hostQtVersion.Minor)
}

# --- pe_tool ------------------------------------------------------------------
function Find-Mingw {
    if ($Mingw -and (Test-Path (Join-Path $Mingw "g++.exe"))) { return $Mingw }
    $list = @("C:\msys64\mingw64\bin") + (Get-ChildItem "C:\Qt\Tools" -Directory -Filter "mingw*_64" -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | ForEach-Object { Join-Path $_.FullName "bin" })
    foreach ($c in $list) { if (Test-Path (Join-Path $c "g++.exe")) { return $c } }
    return ""
}
$Mingw = Find-Mingw
if (-not $Mingw) { Fail "MinGW not found. Pass the bin folder of the Qt MinGW toolchain with -Mingw." }
$env:PATH = "$Mingw;$env:PATH"

New-Item -ItemType Directory -Force $BuildDir | Out-Null
$peTool = Join-Path $BuildDir "pe_tool.exe"
if (-not (Test-Path $peTool)) {
    Step "Building pe_tool"
    & (Join-Path $Mingw "g++.exe") -std=c++14 -O1 (Join-Path $root "tools\pe_tool.cpp") -o $peTool -static
    if ($LASTEXITCODE -ne 0) { Fail "pe_tool did not compile" }
}

function Invoke-Check([string]$dll, [string]$builtWithText) {
    Step "Checking $dll against $VeyonDir"
    & $peTool check $dll $VeyonDir
    $symbolsOk = ($LASTEXITCODE -eq 0)

    $qtOk = $true
    if ($builtWithText) {
        $builtWith = [version]$builtWithText
        $qtOk = Test-QtRule $builtWith $hostQt
        Write-Host ("Qt rule: plugin built with Qt {0}, Veyon ships Qt {1} -> {2}" -f $builtWith, $hostQt, $(if ($qtOk) { "ok" } else { "REFUSED by Qt" }))
    } else {
        Write-Host "Qt rule: not checked (pass -QtBuiltWith x.y.z)" -ForegroundColor Yellow
    }

    Write-Host ""
    if ($symbolsOk -and $qtOk) {
        Write-Host "PASS: all imports resolve in the installed Veyon and the Qt version fits." -ForegroundColor Green
        Write-Host "      (This is a static check; it does not run Veyon. Test on one computer first.)"
        return $true
    }
    Write-Host "FAIL: this DLL will not load in the installed Veyon." -ForegroundColor Red
    return $false
}

if ($CheckOnly) {
    if (-not $Dll) { Fail "-CheckOnly needs -Dll <file>" }
    if (Invoke-Check (Resolve-Path $Dll).Path $QtBuiltWith) { exit 0 } else { exit 1 }
}

# --- 2. import library of the installed core -----------------------------------------
Step "Import library of the installed veyon-core.dll"
$def = Join-Path $BuildDir "veyon-core-installed.def"
$lib = Join-Path $BuildDir "libveyon-core-installed.dll.a"
& $peTool def $core $def
if ($LASTEXITCODE -ne 0) { Fail "could not read the exports of $core" }
& (Join-Path $Mingw "dlltool.exe") -d $def -l $lib -D "veyon-core.dll"
if ($LASTEXITCODE -ne 0) { Fail "dlltool failed" }

# --- 3. Veyon headers of the same version ------------------------------------------------
Step "Veyon headers"
$ver3 = "{0}.{1}.{2}" -f $veyonVer.Major, $veyonVer.Minor, $veyonVer.Build
if (-not $VeyonSource) {
    $VeyonSource = Join-Path (Split-Path $root -Parent) "veyon-src-$ver3"
    if (-not (Test-Path (Join-Path $VeyonSource "core\src\Feature.h"))) {
        Write-Host "Getting Veyon v$ver3 (headers) with git ..."
        & git clone --depth 1 --branch "v$ver3" https://github.com/veyon/veyon.git $VeyonSource
        if ($LASTEXITCODE -ne 0) { Fail "git clone failed. Download Veyon v$ver3 yourself and pass its folder with -VeyonSource." }
    }
}
if (-not (Test-Path (Join-Path $VeyonSource "core\src\Feature.h"))) { Fail "$VeyonSource does not contain core\src\Feature.h" }
Write-Host "Headers from : $VeyonSource"

# --- 4. Qt for the build ----------------------------------------------------------------------
function Find-QtDir {
    if ($QtDir) { return $QtDir }
    $found = Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^6\.\d+\.\d+$' } |
        Where-Object { Test-Path (Join-Path $_.FullName "mingw_64\lib\cmake\Qt6") } |
        Sort-Object { [version]$_.Name }
    foreach ($f in $found) {
        if ([version]$f.Name -ge [version]"6.7.0" -and (Test-QtRule ([version]$f.Name) $hostQt)) {
            return (Join-Path $f.FullName "mingw_64")
        }
    }
    return ""
}
$QtDir = Find-QtDir
if (-not $QtDir) { Fail "No suitable Qt 6 (MinGW, 6.7 or newer, not newer than $hostQt) found. Pass it with -QtDir, e.g. C:\Qt\6.7.3\mingw_64." }
$qtVersionFile = Join-Path $QtDir "lib\cmake\Qt6\Qt6ConfigVersionImpl.cmake"
$builtWithText = ""
if (Test-Path $qtVersionFile) {
    $m = [regex]::Match((Get-Content $qtVersionFile -Raw), 'PACKAGE_VERSION\s+"(\d+\.\d+\.\d+)')
    if ($m.Success) { $builtWithText = $m.Groups[1].Value }
}
if (-not $builtWithText) {
    $m = [regex]::Match($QtDir, '(\d+\.\d+\.\d+)')
    if ($m.Success) { $builtWithText = $m.Groups[1].Value }
}
Write-Host "Qt for build : $QtDir (Qt $builtWithText)"

# --- 5. configure and build ----------------------------------------------------------------------
Step "Configure and build"
$make = Join-Path $Mingw "mingw32-make.exe"
$cmakeArgs = @(
    "-S", $root, "-B", $BuildDir,
    "-G", "MinGW Makefiles",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DWITH_QT6=ON",
    "-DVEYON_TARGET_VERSION=$ver3",
    "-DVEYON_SOURCE_DIR=$VeyonSource",
    "-DVEYON_CORE_LIBRARY=$lib",
    "-DCMAKE_PREFIX_PATH=$QtDir",
    "-DCMAKE_CXX_COMPILER=$(Join-Path $Mingw 'g++.exe')",
    "-DCMAKE_C_COMPILER=$(Join-Path $Mingw 'gcc.exe')"
)
if (Test-Path $make) { $cmakeArgs += "-DCMAKE_MAKE_PROGRAM=$make" }
& $CMake @cmakeArgs
if ($LASTEXITCODE -ne 0) { Fail "CMake configure failed" }
& $CMake --build $BuildDir --config Release
if ($LASTEXITCODE -ne 0) { Fail "build failed" }

$name = if ($veyonVer -ge [version]"4.11.0") { "classroom-guard-qt6-v411.dll" } else { "classroom-guard-qt6.dll" }
$built = Join-Path $BuildDir $name
if (-not (Test-Path $built)) { Fail "expected $built was not produced" }
New-Item -ItemType Directory -Force (Join-Path $root "dist") | Out-Null
$dist = Join-Path $root ("dist\" + $name)
Copy-Item $built $dist -Force
Write-Host "Built: $dist"

# --- 6. check -------------------------------------------------------------------------------------
if (Invoke-Check $dist $builtWithText) {
    Write-Host ""
    Write-Host "Install: copy $name to $VeyonDir\plugins\ (remove any older classroom-guard-*.dll / internet-guard-*.dll there), then restart Veyon."
    exit 0
}
exit 1
