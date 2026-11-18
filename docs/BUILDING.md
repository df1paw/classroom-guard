# Building Classroom Guard

## Quick start (Veyon 4.11, Windows)

1. Install Veyon 4.11.x, Git, Python and CMake. Get Qt 6.7 and MinGW without a Qt account:
   ```powershell
   pip install aqtinstall
   aqt install-qt   windows desktop 6.7.3 win64_mingw -O C:\Qt
   aqt install-tool windows desktop tools_mingw1310 -O C:\Qt
   aqt install-tool windows desktop tools_cmake -O C:\Qt
   ```
2. Run the helper from the repository root:
   ```powershell
   powershell -ExecutionPolicy Bypass -File tools\build-windows.ps1 -CMake C:\Qt\Tools\CMake_64\bin\cmake.exe
   ```
   It builds against the **installed** Veyon, checks that every `veyon-core.dll` symbol
   the plugin imports exists there and that the Qt version rule holds, and writes
   `dist\classroom-guard-qt6-v411.dll` (PASS/FAIL at the end).
3. Optional MSI: `installer\msi\build-msi.ps1 -Dll dist\classroom-guard-qt6-v411.dll` (WiX 3).

The sections below describe the manual builds for every Veyon version.

## Supported Veyon versions

| Veyon            | Qt        | Build configuration     | Output DLL                  | Status                          |
|------------------|-----------|-------------------------|-----------------------------|---------------------------------|
| 4.7.5 – 4.8.x    | Qt 5      | default (`WITH_QT6=OFF`)| `classroom-guard-qt5.dll`    | ✅ built and verified            |
| 4.9.0 – 4.10.x   | Qt 6.7    | `-DWITH_QT6=ON`         | `classroom-guard-qt6.dll`    | ✅ verified on 4.9.0 and 4.10.4 |
| 4.11.x           | Qt 6.10 (build with 6.7) | `-DWITH_QT6=ON -DVEYON_TARGET_VERSION=4.11.4` | `classroom-guard-qt6-v411.dll` | ✅ built and tested on 4.11.4 |

At *source* level the plugin interfaces (`PluginInterface`,
`FeatureProviderInterface`, `Feature`, `FeatureMessage`) are unchanged from
4.7.5 to 4.10.x, so a single source builds every variant with no `#if` branches.

> **Important (binary ABI).** What the plugin is *compiled against* matters more
> than the source. The binary ABI is tied to the Veyon **core** version, so:
> - the **Qt 5** build targets the 4.7.5 core → use it on Veyon 4.7.5–4.8.x;
> - the **Qt 6** build targets the 4.9.0 core and is built with **Qt 6.7** → one
>   file works on Veyon 4.9.0–4.10.x (the interfaces this plugin uses are
>   binary-compatible across that range, and Qt 6.7 — the lowest Qt in it, from
>   4.9.0 — loads on the Qt 6.10 of 4.10.x).
>
> Getting this wrong makes Veyon either ignore the plugin or **crash on load**.
> Use the official Windows (MinGW) Veyon packages. Full analysis in
> [`VeyonCompat.h`](VeyonCompat.h) and the sections below.

## Software requirements

**To use the plugin (installation)**

- Windows with Veyon installed (Master and Server).
- No additional requirement: the installer is self-contained.

**To build the plugin**

| Dependency                  | Qt 5 build                                          | Qt 6 build                                          |
|-----------------------------|-----------------------------------------------------|-----------------------------------------------------|
| CMake                       | ≥ 3.16                                              | ≥ 3.16                                              |
| Qt (Core, Widgets, Network) | Qt 5.12 at `C:/Qt/5.12.12/mingw73_64`              | Qt **6.7.x** MinGW (build with 6.7 to cover 4.9.0–4.10.x — see the sections below) |
| MinGW toolchain             | `C:/Qt/Tools/mingw730_64` (g++ 7.3, Qt 5.12 ABI)   | MinGW **13.1.0** (or any recent MinGW — the plugin needs only base libstdc++ symbols; see the sections below) |
| Veyon sources               | a **4.7.5** checkout's `core/src` headers          | a **4.9.0** checkout's `core/src` headers (see the sections below) |
| Veyon import library        | `libveyon-core.dll.a` (in repo root)                | `libveyon-core-qt6.dll.a` (from 4.9.0, in repo root) |
| C++ standard                | C++14                                               | C++14                                               |

> Qt Svg is **not** required: the toolbar icon is a PNG, so `QIcon` needs no SVG
> icon-engine plugin (Veyon's Windows build does not ship one anyway).

## Build instructions

### Veyon 4.7.5 – 4.8.x (Qt 5)

From PowerShell, in the repository root:

```powershell
$env:PATH = "C:\Qt\Tools\mingw730_64\bin;$env:PATH"

cmake -S . -B build-qt5 -G "MinGW Makefiles" `
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw730_64/bin/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="C:/Qt/Tools/mingw730_64/bin/mingw32-make.exe" `
  -DVEYON_TARGET_VERSION=4.7.5 `
  -DVEYON_SOURCE_DIR="C:/path/to/veyon-4.8-src"

cmake --build build-qt5
```

Output: `build-qt5/classroom-guard-qt5.dll`.

> The g++ 7.3 shipped with Qt 5.12.12 is used explicitly: it is the only one
> ABI-compatible with the Qt 5.12 libraries and the official `veyon-core.dll`.
> Newer MinGW compilers (e.g. MSYS2 ones) produce a DLL that may fail to load
> correctly in Veyon.

### Veyon 4.9.x – 4.10.x (Qt 6) — one binary, verified on Veyon 4.9.0 and 4.10.4

A single Qt 6 DLL covers the whole 4.9.0–4.10.x range. Two rules make it work:

> ⚠️ **Build with Qt 6.7 and against the 4.9.0 core.**
> - *Version-gate:* Qt's plugin loader silently rejects a plugin built with a Qt
>   *minor* newer than the host's (plugin minor ≤ host minor, otherwise "no
>   toolbar button" with no error). The lowest Qt in the range is **6.7.2**
>   (Veyon 4.9.0; later 4.9.x use 6.8, 4.10.x uses 6.10), so building with **Qt
>   6.7** loads on the whole range. (Read your Veyon's Qt from the
>   `ProductVersion` of `Qt6Core.dll` in its folder.)
> - *Core ABI:* the plugin must be compiled against headers and an import library
>   matching the Veyon **core** it loads into, or the Server **crashes on load**.
>   The interfaces this plugin uses are binary-compatible across 4.9.0–4.10.x, so
>   the **4.9.0** core (the floor) is the right single target.

Get a Qt 6.7 MinGW toolchain non-interactively with
[`aqtinstall`](https://github.com/miurahr/aqtinstall) (no Qt account needed):

```powershell
py -m pip install aqtinstall
py -m aqt install-qt   windows desktop 6.7.2 win64_mingw --outputdir C:\Qt-aqt
py -m aqt install-tool windows desktop tools_mingw1310    --outputdir C:\Qt-aqt
```

Check out the Veyon **4.9.0** sources for the headers:

```powershell
git clone --depth 1 --branch v4.9.0 https://github.com/veyon/veyon.git ..\veyon-src-490
```

Then configure and build. `VEYON_TARGET_VERSION` defaults to 4.9.0 when
`WITH_QT6=ON`, and the repo already ships the matching `libveyon-core-qt6.dll.a`:

```powershell
$mingw = "C:\Qt-aqt\Tools\mingw1310_64\bin"
$qt6   = "C:\Qt-aqt\6.7.2\mingw_64"
$env:PATH = "$mingw;$qt6\bin;$env:PATH"

cmake -S . -B build-qt6 -G "MinGW Makefiles" `
  -DWITH_QT6=ON `
  -DCMAKE_CXX_COMPILER="$mingw/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="$mingw/mingw32-make.exe" `
  -DCMAKE_PREFIX_PATH="$qt6" `
  -DVEYON_SOURCE_DIR="..\veyon-src-490"

cmake --build build-qt6
```

Output: `build-qt6/classroom-guard-qt6.dll` — its Qt tag is `qt_version_tag_6_8`
(`strings build-qt6\classroom-guard-qt6.dll | Select-String qt_version_tag`).

> **Targeting a different Veyon core?** Regenerate the import library from that
> Veyon's installed `veyon-core.dll` and point `VEYON_SOURCE_DIR` at the matching
> source checkout:
> ```powershell
> gendef veyon-core.dll
> dlltool -d veyon-core.def -l libveyon-core-qt6.dll.a -D veyon-core.dll
> ```

### Veyon 4.11.x (Qt 6) — separate binary

Veyon 4.11 changed the plugin API (`FeatureMessage` command types, `ComputerControlInterfaceList`
is a class, C++17), so a DLL built for 4.9/4.10 does **not** load there. Build a dedicated one
(`classroom-guard-qt6-v411.dll`, selected automatically by CMake when `VEYON_TARGET_VERSION >= 4.11.0`).

Veyon's bundled Qt: 4.9.0 → Qt 6.7, 4.9.1+ → 6.8, 4.10.x and 4.11.x → 6.10. Qt loads a plugin built
with the same major and a minor **less than or equal** to the host's, so a plugin built with Qt 6.7
loads in every one of them.

The easiest way to build and verify on Windows is the helper script:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-windows.ps1
```

It builds against the installed Veyon, then checks statically (`tools/pe_tool.cpp`) that every
`veyon-core.dll` import exists in the installed Veyon and that the Qt rule above holds, and prints
PASS/FAIL. `-CheckOnly -Dll <file> -QtBuiltWith 6.7.3` only verifies an existing DLL.

### Building the installer

After building all three plugin variants, run from the repository root:

```powershell
pwsh -File installer\build-installer.ps1 `
    -PluginDllQt5 build-qt5\classroom-guard-qt5.dll `
    -PluginDllQt6 build-qt6\classroom-guard-qt6.dll `
    -PluginDllQt6V411 build-qt6-v411\classroom-guard-qt6-v411.dll
```

Produces a single `installer\install-classroom-guard.exe` that embeds the
Qt 5, Qt 6 and Qt 6 / Veyon 4.11 plugins and picks the right one at install time by detecting the
target Veyon's Qt version. It is a self-contained executable with no Qt dependency.

## Available CMake flags

| Variable              | Default                              | Description                                            |
|-----------------------|--------------------------------------|--------------------------------------------------------|
| `WITH_QT6`            | `OFF`                                | Build with Qt 6 instead of Qt 5.                       |
| `VEYON_TARGET_VERSION`| auto: `4.7.5` (Qt5) / `4.9.0` (Qt6)  | Target Veyon version (compatibility macros).           |
| `VEYON_SOURCE_DIR`    | `../veyon-src`                       | Veyon `core/src` headers — a 4.7.5 checkout for Qt5, a 4.9.0 one for Qt6. |
| `VEYON_CORE_LIBRARY`  | auto (`libveyon-core.dll.a` or `-qt6`)| `veyon-core` import library; auto-selected by `WITH_QT6`. |

`VEYON_TARGET_VERSION` is split by CMake and passed to the code as
`VEYON_TARGET_VERSION_MAJOR/MINOR/PATCH`; `VeyonCompat.h` derives the
`VEYON_VERSION_AT_LEAST(maj,min,patch)` macro from it, ready for any future
adaptations.

## Running the tests

The plugin has no automated unit-test suite (it is a thin adapter between the
Veyon API and the Windows Firewall API). Verification is functional:

1. **Build check (Qt 5)** — inspect the produced DLL:

   ```powershell
   & "C:\Qt\Tools\mingw730_64\bin\objdump.exe" -p build-qt5\classroom-guard-qt5.dll | Select-String "qt_plugin|DLL Name"
   ```

   Expected: exports `qt_plugin_instance` and `qt_plugin_query_metadata`,
   dependencies `Qt5Core.dll`, `Qt5Gui.dll`, `veyon-core.dll`.

2. **Build check (Qt 6)** — inspect the produced DLL:

   ```powershell
   & "C:\Qt-aqt\Tools\mingw1310_64\bin\objdump.exe" -p build-qt6\classroom-guard-qt6.dll | Select-String "qt_plugin|DLL Name"
   ```

   Expected: exports `qt_plugin_instance` and `qt_plugin_query_metadata_v2`
   (Qt 6 uses `_v2`), dependencies `Qt6Core.dll`, `Qt6Gui.dll`, `veyon-core.dll`.
   Also check `qt_version_tag` is ≤ your Veyon's Qt (see the sections below).

3. **Installer check** — a "dry run" install into a temporary folder, with no
   GUI:

   ```powershell
   $t = Join-Path $env:TEMP "ig_test"; New-Item -ItemType Directory -Force $t | Out-Null
   New-Item -ItemType File -Force "$t\Qt6Core.dll" | Out-Null   # simulate a Qt 6 Veyon (use Qt5Core.dll for Qt 5)
   .\installer\install-classroom-guard.exe --elevated "$t"
   # expected: $t\plugins\classroom-guard-qt6.dll (the variant matching the detected Qt)
   ```

4. **Firewall backend check (standalone)** — `tools/fw_test.cpp` exercises the
   real rule creation/removal without Veyon (build command in the file header;
   the exe is static, no Qt needed at runtime). Run it from an **elevated**
   prompt: expected `blockInternet -> true` with no warnings (the `CG_*`
   rules exist between step 2 and 3, then the test removes them itself). From
   a normal prompt it verifies the graceful-failure path instead
   (`blockInternet -> false`, one `0x80070005` warning per rule, no crash).

4b. **Address and port logic (any platform)** — `tools/ranges_test.cpp` tests the
   address/port arithmetic behind the firewall rules:
   `g++ -std=c++14 -I. tools/ranges_test.cpp -o ranges_test && ./ranges_test`
   (expected: `all tests passed`).


5. **Runtime check in Veyon**:
   - Install the plugin (see [USER-GUIDE.md](USER-GUIDE.md)) on a student PC and on the teacher PC.
   - Restart Veyon Master and Veyon Server.
   - The **Block Internet** button must appear in the Master toolbar.
   - When activated, on the student computer `netsh advfirewall firewall show rule name=all`
     must list the `CG_*` rules; Internet browsing must stop while Veyon
     keeps working.
   - When deactivated, the `CG_*` rules must disappear.


## Project structure

```
Classroom Guard/
├─ ClassroomGuardPlugin.h/.cpp             Plugin class: interface, message dispatch (Master + Server)
├─ ClassroomGuardFeatures.cpp              Feature list and UIDs
├─ ClassroomGuardRuntime.cpp               Server side: state, timers, end of day, mute watchdog, notices
├─ ClassroomGuardConfigurationPage.h/.cpp  "Classroom Guard" page of the Veyon Configurator
├─ ClassroomGuardConfig.h/.cpp             Settings in the Veyon configuration (key group "ClassroomGuard")
├─ GuardActions.h/.cpp                     Actions on a computer: simulation, log, calls into the backends
├─ GuardConfig.h                           Plain settings struct shared by plugin and backends
├─ GuardLog.h/.cpp                         Log file (%ProgramData%\ClassroomGuard)
├─ IpRanges.h                              Address and port set arithmetic (no dependencies, unit tested)
├─ WindowsFirewall.h/.cpp                  Firewall backend (INetFwPolicy2 COM API)
├─ ProxyRedirect.h/.cpp                    Proxy redirection backend (WinINET registry values)
├─ RoutingBlock.h/.cpp                     Routing table backend (IP Helper API)
├─ DeviceControl.h/.cpp                    USB storage, webcams, microphones, audio (SetupAPI, Core Audio)
├─ NetUtil.h/.cpp                          Host resolution, DNS servers, local subnets
├─ GuardSettings.h/.cpp                    Registry helpers (state to undo blocks, test tool settings)
├─ VeyonCompat.h                           Single point of contact with the Veyon API + version macros
├─ CMakeLists.txt                          Multi-version build (WITH_QT6, VEYON_* flags)
├─ resources.qrc, *.png                    Icons (PNG)
├─ libveyon-core*.dll.a, *.def             veyon-core import libraries / symbol lists
├─ installer/                              Self-contained Win32 installer (removes the old internet-guard DLLs)
├─ tools/
│  ├─ guard_cli.cpp                        Command line test tool for all backends (Administrator)
│  ├─ fw_test.cpp                          Standalone test of the firewall backend
│  ├─ ranges_test.cpp                      Unit test of IpRanges.h
│  └─ qtshim/QtGlobal                      qWarning→stderr shim so the tools compile without Qt
├─ build-qt5/                              Qt 5 build output (classroom-guard-qt5.dll)
└─ build-qt6/                              Qt 6 build output (classroom-guard-qt6.dll)
```

