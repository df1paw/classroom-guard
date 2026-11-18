# Architecture notes

Technical/architecture summary for contributors. User-facing documentation
lives in `README.md`.

## What this is

Classroom Guard (fork of the InternetGuard plugin, version 2.0) is a [Veyon](https://veyon.io/) plugin that adds classroom controls to the Veyon Master UI: block or restrict internet access, block USB storage and webcams, mute microphones and audio. The Veyon Server on each student machine carries out the commands. A configuration page in the Veyon Configurator holds the settings.

## Version compatibility (important)

At *source* level the Veyon plugin interfaces used here — `PluginInterface`, `FeatureProviderInterface` (`controlFeature`, `handleFeatureMessage`, `sendFeatureMessage`), `Feature` (9-arg constructor + `Flag` enum), `FeatureMessage` — compile unchanged across every Veyon release from 4.7.5 through 4.10.x, with **no per-version `#if` branches**. The **binary ABI**, however, is tied to the Veyon *core* version the plugin is compiled against (see the **Binary ABI** lesson below — getting it wrong crashes the Veyon Server on load). Together with the Qt branch this gives two builds:

- Veyon 4.7.5 – 4.8.x → Qt 5, **core 4.7.5** (default, `WITH_QT6=OFF`) → output: `classroom-guard-qt5.dll`
- Veyon 4.11.x → **Qt 6.7+, core 4.11.4** (`WITH_QT6=ON -DVEYON_TARGET_VERSION=4.11.4`) → output: `classroom-guard-qt6-v411.dll` (API differs from 4.9/4.10; separate binary; Veyon 4.11 itself ships Qt 6.10, which loads Qt 6.7-built plugins)
- Veyon 4.9.0 – 4.10.x → **Qt 6.7, core 4.9.0** (`WITH_QT6=ON`) → output: `classroom-guard-qt6.dll` (one binary covers the whole 4.9–4.10 range)

All version handling is centralized in `VeyonCompat.h` (single include point for the Veyon API + `VEYON_TARGET_VERSION_*` macros and `VEYON_VERSION_AT_LEAST()`).

### API change in Veyon 4.10.0: `FeatureMessage::command()`

In Veyon ≤4.9.x, `FeatureMessage::command()` returned `int`.  
In Veyon ≥4.10.0, it returns `FeatureMessage::Command` (a class enum), with a
template overload `command<YourEnum>()` to decode into a user-defined enum type.

`VeyonCompat.h` provides the `VEYON_DECODE_COMMAND(msg, EnumType)` macro to
abstract this difference; `ClassroomGuardPlugin.cpp` uses it exclusively so the
same source compiles against both API versions.

## Build

**Prerequisites**

| Dependency | Qt 5 build | Qt 6 build |
|---|---|---|
| CMake | ≥ 3.16 | ≥ 3.16 |
| Qt (Core, Widgets, Network) | Qt 5.12 at `C:/Qt/5.12.12/mingw73_64` | Qt **6.7.x** MinGW (e.g. `C:/Qt-aqt/6.7.2/mingw_64`) |
| MinGW toolchain | `C:/Qt/Tools/mingw730_64` (g++ 7.3) | MinGW **13.1.0** (`tools_mingw1310`) |
| Veyon source tree | a 4.7.5 checkout's `core/src` | a **4.9.0** checkout's `core/src` |
| Veyon import library | `libveyon-core.dll.a` (in repo root) | `libveyon-core-qt6.dll.a` (from 4.9.0, in repo root) |
| C++ standard | C++14 | C++14 |

> Qt Svg is **not** a dependency: the toolbar icon is a PNG (see "Toolbar icon"
> below), so `QIcon` needs no SVG icon-engine plugin.

**ABI note (Qt 5):** build with the **same compiler/Qt that the installed Veyon uses**. The official Windows Veyon 4.7.5–4.8.x builds use MinGW g++ 7.3 + Qt 5.12; using a different MinGW (e.g. MSYS2 GCC) can produce a DLL that fails to load.

**ABI note (Qt 6 / GCC 16):** GCC 16 emits both a local vtable and `__imp__ZTV` references for `dllimport` base classes (`FeatureProviderInterface`, `PluginInterface`). Both definitions are identical (same header), so the linker flag `-Wl,--allow-multiple-definition` is added for WIN32 builds to silently resolve the conflict. This is harmless and expected with this toolchain combination.

**Configure and build (Qt 5 / Veyon 4.7.5–4.8.x)**

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

**Configure and build (Qt 6 / Veyon 4.9.x–4.10.x — one binary, verified on Veyon 4.9.0 and 4.10.4)**

> ⚠️ **Two things must match: the Qt minor *and* the Veyon core version.**
> - *Version gate.* Qt's plugin loader rejects — *silently*, no error, no toolbar
>   button — a plugin built with a Qt **newer** than the host (plugin minor ≤ host
>   minor, same major). The lowest Qt in the range is **6.7.2** (Veyon 4.9.0;
>   later 4.9.x moved to 6.8, 4.10.x to 6.10), so the plugin is built with Qt
>   **6.7** and loads on every host up to 4.10.x. An earlier attempt with MSYS2's
>   rolling Qt 6.11 produced `qt_version_tag_6_11`, refused even by 4.10.x. Check
>   with `strings classroom-guard-qt6.dll | grep qt_version_tag`.
> - *Core ABI.* Build against the headers **and** import library of the Veyon
>   **core** the plugin loads into, or the Server **crashes on load**. The
>   interfaces this plugin uses are binary-compatible across 4.9.0–4.10.x, so the
>   **4.9.0** core (the floor) is the single target; the resulting DLL loads on
>   4.9.0, 4.9.8 and 4.10.4. Read the host's Qt from the `ProductVersion` of
>   `Qt6Core.dll` in the Veyon folder.

Get a Qt 6.7 MinGW toolchain non-interactively with
[`aqtinstall`](https://github.com/miurahr/aqtinstall) and check out the 4.9.0 headers:

```powershell
py -m pip install aqtinstall
py -m aqt install-qt   windows desktop 6.7.2 win64_mingw --outputdir C:\Qt-aqt
py -m aqt install-tool windows desktop tools_mingw1310    --outputdir C:\Qt-aqt
git clone --depth 1 --branch v4.9.0 https://github.com/veyon/veyon.git ..\veyon-src-490
```

Then configure and build with **MinGW Makefiles** (not Ninja — keeps the toolchain explicit):

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

Output: `build-qt6/classroom-guard-qt6.dll` (`qt_version_tag_6_7`). `VEYON_TARGET_VERSION`
defaults to 4.9.0 when `WITH_QT6=ON`, so `VEYON_DECODE_COMMAND` uses the `<4.10`
`static_cast<Commands>(msg.command())` path — correct, since 4.9.0's `FeatureMessage`
exposes `command()` as `qint32`.

The repo ships `libveyon-core-qt6.dll.a` (generated from Veyon **4.9.0**'s
`veyon-core.dll` via `gendef` + `dlltool`) and `libveyon-core-qt6.def` (its export
list), selected automatically when `WITH_QT6=ON`. To target a different core,
regenerate the import lib from that Veyon's `veyon-core.dll` and point
`VEYON_SOURCE_DIR` at the matching source checkout.

For the CMake flags (`WITH_QT6`, `VEYON_TARGET_VERSION`, `VEYON_SOURCE_DIR`, `VEYON_CORE_LIBRARY`), see `README.md` §5–§6.

## Architecture

The plugin is a single shared library (`classroom-guard-qt5.dll` or `classroom-guard-qt6.dll`):

- **`ClassroomGuardPlugin.h/.cpp`** — plugin class (`PluginInterface`, `FeatureProviderInterface`, `ConfigurationPagePluginInterface`). Master side (`controlFeature`) sends a `FeatureMessage` with a command code; Server side (`handleFeatureMessage`) calls `execute(command)`, which calls `GuardActions` and arms/stops the auto-release timer of that resource. Six feature groups, each a `Mode` toggle plus `Block`/`Allow` `Action` sub-features (`parentUid` = toggle): Internet, Restrict Internet, USB, Webcam, Microphone, Audio. The toggle gives a one-click global block/allow; the sub-features act on exactly the selected computers. Commands are dispatched by command code, independent of the feature UID that carried them. **Wire protocol:** commands 0/1 (internet) are unchanged since InternetGuard 1.0, new commands are appended, the only message arguments are `AllowedWebsites` (Restrict, sent by the Master) and `NoticeSeconds` (Service to Worker). All other settings are read on the Server, never sent.
  - **Timers**: five single-shot `QTimer`s (internet, USB, webcam, microphone, audio) created in the constructor; start/stop are queued calls so they run in the plugin's thread. When a timer fires, the release command of its resource is executed.
  - **Startup/unload**: `onStartup()` (queued from the constructor, so the Veyon configuration exists) releases persistent blocks in `veyon-server.exe` when "Unblock on service start" is on. The destructor must not read the configuration (it may be gone); it undoes through the saved state only (firewall, proxy, routes; in the Server also USB and webcams).
- **`GuardActions.h/.cpp`** — the actions on one computer: read settings, honour **simulation mode**, call the Windows backends (`Q_OS_WIN` only), write the **log** (`GuardLog`). Used by the Server, the timers and the test buttons of the configuration page.
- **`ClassroomGuardConfig.h/.cpp`** — settings in the Veyon configuration, key group `ClassroomGuard`, via the generic `VeyonCore::config().value()/setValue()` only (no property macros, so no dependency on Veyon's property ABI). Lists are one text with one entry per line. `load()` builds the plain `GuardConfig` struct.
- **`ClassroomGuardConfigurationPage.h/.cpp`** — the Configurator page (code only, no `.ui` file): widgets write straight into the configuration (`MainWindow` reacts to `configurationChanged`), `resetWidgets()` loads them under `QSignalBlocker`.
- **`ClassroomGuardPlugin.cpp` / `ClassroomGuardFeatures.cpp` / `ClassroomGuardRuntime.cpp`** — one plugin class in three files: plugin interface and message dispatch / feature list and UIDs / execution on the student computer (state, timers, end of day, mute watchdog, notices).
- **`IpRanges.h`** — dependency-free address/port set arithmetic (`RangeSet`, `AddressSet`, `PortSet`), unit-tested by `tools/ranges_test.cpp`.
- **`NetUtil.h/.cpp`** — host name resolution (with `www.` variant), configured DNS servers and on-link subnets (`GetAdaptersAddresses`) into an `AddressSet`.
- **`WindowsFirewall.h/.cpp`** — firewall backend through the **`INetFwPolicy2` / `INetFwRule` COM API** (no `netsh`: no PATH lookup in a SYSTEM process, no locale-dependent output, per-call `HRESULT` logging). `block(cfg, restrict)` removes leftovers, enables all profiles (block rules do nothing while a profile is off), builds the rules, optionally switches on the administrator's preconfigured rule (its name is saved under `State\Firewall` and switched off again by `allowInternet()`). `put_Protocol` must be called **before** `put_RemotePorts`. Rule names are not unique, so removal probes with `Item()` and loops.
- **`ProxyRedirect`**, **`RoutingBlock`**, **`DeviceControl`**, **`GuardSettings`** — see the file headers. The state needed to undo a block is kept in `HKLM\SOFTWARE\ClassroomGuard\State\...`.
- **`VeyonCompat.h`** — single point of contact with the Veyon API + version macros + `VEYON_DECODE_COMMAND`.
- **`resources.qrc`** — PNG icons (`:/classroom-guard/...`), see "Toolbar icon".
- **`installer/`** — standalone Win32 installer, embeds both DLLs; installs the matching Qt variant and deletes the old `internet-guard-qt5/6.dll` (same plugin UID, they would clash).
- **`tools/`** — `guard_cli.cpp` (all backends from the command line, settings from the registry), `fw_test.cpp`, `ranges_test.cpp`, `qtshim/QtGlobal`.

### Firewall rule design (version 2.0)

Windows Firewall evaluates **Block rules before Allow rules**, so "block the internet but keep the LAN" cannot be built from a block plus an allow rule (plugin ≤ 1.1 tried with `VeyonIG_AllowLAN`; it never worked). Version 2.0 gives the block rules a **`RemoteAddresses` list that is the complement of what must stay reachable**:

`destinations = (all addresses − local networks − own on-link subnets) − exception networks − exception hosts [− allowed websites − DNS servers in Restrict mode]`

computed with `IpRanges` (IPv4 and IPv6), written as `a-b,c-d,…`. With "block only non-local" turned off the local networks stay in. An internal proxy is no longer an escape route because *Block servers* blocks given addresses completely, local or not.

| Rule | Protocol / ports | Remote addresses |
|---|---|---|
| `CG_BlockAll` | any (no exception ports) | destinations |
| `CG_BlockAll_TCP` / `_UDP` | all ports except the exception ports | destinations |
| `CG_Block_TCP` | web 80,443,8080,8443,3128 · FTP 21 · DNS 53,853 · custom, minus exception ports | destinations |
| `CG_Block_UDP` | QUIC 443 · DNS 53,853 · custom, minus exception ports | destinations |
| `CG_BlockServers` | any | the listed servers |

All rules are outbound, all profiles, group "Classroom Guard", prefix `CG_`. `allowInternet()` also deletes the `VeyonIG_*` rules of InternetGuard ≤ 1.3. Host names are resolved when the block starts (snapshot); a roaming CDN breaks Restrict mode by design.

### Plugin identity

The plugin's UUID `a4b3c2d1-e5f6-7890-abcd-ef1234567890` is the identity of the **toggle** feature and must stay consistent across `uid()` and the toggle's `Feature::Uid` (the metadata IID `io.veyon.Veyon.Plugins.ClassroomGuard` is a separate string). It is deliberately the UUID of the earlier InternetGuard plugin. Every other feature has its own constant UUID; only uniqueness matters for those.

### Qt plugin metadata export

Qt 5 exports `qt_plugin_query_metadata`; Qt 6 exports `qt_plugin_query_metadata_v2`. Both are generated automatically by `moc` / `AUTOMOC` — no manual action needed.

### Toolbar icon (PNG, not SVG)

Veyon renders a feature's icon with `QIcon(feature.iconUrl())`. Two constraints, both learned the hard way:

1. **No SVG at runtime.** Veyon's Windows deployment does not ship Qt's SVG icon-engine plugin (`iconengines/qsvgicon.dll`), so `QIcon(":/…svg")` yields an *empty* icon — the toolbar button appears but blank (same background as its neighbours). Every built-in Veyon plugin uses PNG icons; so does this one (`:/classroom-guard/network-offline.png`). This also removes any dependency on Qt Svg.
2. **`network-offline.svg` is kept only as the editable source.** Regenerate the PNG after editing it. Note that Qt's SVG module is "SVG Tiny" and does **not** parse `rgba(r,g,b,a)` fill notation — it renders such a shape as opaque black (this is what originally hid the globe). Use `fill="#rrggbb" fill-opacity="0.x"` instead. Rasterize with any Qt build that has the Svg module (a ~20-line `QSvgRenderer` → `QPainter` on a transparent `QImage` → `img.save()` program at 128×128 is enough).

## Lessons learned (read before changing build/icon/version code)

- **Binary ABI ≠ source compatibility — this one crashes the Server.** The plugin must be compiled against the Veyon **core** version it loads into (interface headers + `veyon-core` import library). The source compiles fine against any 4.7.x–4.10.x headers, but a Qt6 DLL built against **4.7.5** headers **crashed Veyon 4.10.x at load** (vtable / class-layout mismatch) even though its Qt matched (both 6.10.3). Fix: build the Qt6 plugin against the **4.9.0** core (the floor of the Qt6 range). The interfaces this plugin uses are binary-compatible across 4.9.0–4.10.x — verified by `diff`-ing `core/src` (PluginInterface/FeatureProviderInterface headers identical; the `FeatureMessage` data layout — `m_command`, a 4-byte field — is unchanged; the 3 imported core symbols exist in every release) and by loading the very same DLL on 4.9.0, 4.9.8 and 4.10.4.
- **Qt 6 plugin version gate.** Qt rejects — silently, no button — a plugin built with a Qt **newer** than the host's minor. The lowest Qt across 4.9.0–4.10.x is **6.7.2** (Veyon 4.9.0; later 4.9.x use 6.8, 4.10.x uses 6.10), so build the Qt6 plugin with **Qt 6.7** to cover the whole range. Verify with `strings … | grep qt_version_tag` (must read `…_6_7`).
- **Icon must be PNG**, not SVG, and the source SVG must avoid `rgba()`. See "Toolbar icon".
- **`VEYON_TARGET_VERSION` tracks the core:** 4.7.5 for the Qt5 build, 4.9.0 for the Qt6 build (auto-selected by `WITH_QT6` in CMake); it drives the `VEYON_DECODE_COMMAND` branch.
- **ABI (toolchain):** Qt5 ⇒ g++ 7.3 + Qt 5.12. Qt6 ⇒ Qt 6.7 + any recent MinGW (we use 13.1.0): the plugin imports only base C++ ABI symbols from libstdc++ (`operator new`/`delete`, `__cxxabiv1` type_info vtables), present in every Veyon's runtime, so the MinGW version need not match Qt 6.7's bundled 11.2.0.
- **Do not commit `*_instructions*.txt` or any chatbot-instruction file** (already covered by `.gitignore`).
- **Per-IP / per-domain blocking is intentionally NOT attempted (don't re-add it).** An "AI-sites only" feature (block ChatGPT/Claude/… while leaving the rest of the internet up) was prototyped via the `hosts` file (+ blocking public DoH/DoT resolvers to defeat browser "Secure DNS") and then **removed** because it cannot be made reliable or safe from the client with `netsh`/`hosts`:
  1. The `hosts` file and DoH-blocking only affect **new** name resolutions. Browsers keep an **internal DNS cache and persistent HTTP/2-3 connections**; once a site is open it keeps working (real requests included) regardless — `ipconfig /flushdns` does not clear the browser's own cache.
  2. Cutting an already-open connection requires blocking the destination **IP**, but the major AI sites sit on **shared CDNs (Cloudflare/Google)**, so per-IP firewall rules cause massive collateral damage.
  3. Windows Firewall cannot filter by hostname/SNI, so it cannot target a single domain at the network layer.
  Reliable per-site filtering belongs at the **network level** (DNS filtering on the router/firewall, or a filtering proxy), outside this plugin. The **full** internet block stays reliable because it filters by *port* and therefore also kills existing connections.


## Version 2.0 (Classroom Guard) — notes

- Registry root is `HKLM\SOFTWARE\ClassroomGuard` (state only; settings are in the Veyon configuration). Log: `%ProgramData%\ClassroomGuard\classroom-guard.log`.
- Persistent blocks are released on load/unload only in `veyon-server.exe`; Master, Configurator and CLI never touch local devices (they only undo internet blocks on unload).
- Not verified: the Qt 5 / Veyon 4.7.5 build of the configuration page (only 4.9.0 headers were available), audio mute from session 0, enabling non-present USB devices, a live run in Veyon.

---

(c) by [Andreas Weller](https://www.andreas-weller.de/)

## Verification tooling

- `tools/pe_tool.cpp`: PE import/export parser (`exports`, `imports`, `def`, `check`) used to verify that
  every symbol the plugin imports from `veyon-core.dll` exists in the installed Veyon.
- `tools/build-windows.ps1`: builds against the installed Veyon and applies the Qt load rule
  (same major, plugin minor <= host minor), printing PASS/FAIL.
- Configuration access uses `VeyonCore::config()` directly (no QtCrypto dependency).

## Student notice, school day, language

- **Notice:** the Veyon Service runs in session 0 and cannot show windows on the student's desktop. After a restriction
  is switched on, the Service forwards a `FeatureMessage` to the Veyon Worker of the student's session
  (`FeatureWorkerManager::sendMessageToUnmanagedSessionWorker`, the same way the text message feature works, with one
  fixed, hidden feature UID so a single Worker serves all notices. Veyon counts a feature as active while a Worker runs for its UID
  (`FeatureManager::activeFeatures`), so the UID of a visible feature would show as a pressed button and an icon over the computer). The Worker's `handleFeatureMessage(VeyonWorkerInterface&, ...)`
  shows `GuardNotice`. The Worker also loads the plugin, so its destructor must not undo blocks
  (`isVeyonWorkerProcess()`).
- **End of the school day:** a 30 second timer in the Service (`checkEndOfDay`) releases everything once per day at the set time.
- **Language:** `GuardTranslator` holds the German texts in a compiled-in table (no .qm files). It is installed when
  `QLocale()` is German, which Veyon sets from its UI language before plugins load. Every `tr()` source string needs an entry.
