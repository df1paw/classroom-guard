# Classroom Guard user guide

How to install, use and configure Classroom Guard. For building it yourself see
[BUILDING.md](BUILDING.md).

## Installation

Install the plugin on **every student PC** (it carries out the commands) and on
the **teacher PC** (it provides the buttons in Veyon Master). Then restart the
Veyon Service and the Veyon Master.

| Veyon version | Plugin DLL |
|---|---|
| 4.11.x | `classroom-guard-qt6-v411.dll` |
| 4.9.x – 4.10.x | `classroom-guard-qt6.dll` |
| 4.7.5 – 4.8.x | `classroom-guard-qt5.dll` |

**Option A – MSI** (for several computers, scripts and group policy; Veyon 4.11):

```
msiexec /i classroom-guard-4.11.msi /qn
msiexec /i classroom-guard-4.11.msi INSTALLDIR="D:\Veyon\" /qn
msiexec /x classroom-guard-4.11.msi /qn
```

The MSI stops the Veyon Service, copies the DLL into `<Veyon>\plugins` and starts
the service again. It does not remove DLLs of other variants: delete
`internet-guard-*.dll`, `classroom-guard-qt5.dll` and `classroom-guard-qt6.dll`
from that folder first, because all variants share one plugin ID.

**Option B – installer program** (`installer\install-classroom-guard.exe`): detects
Qt 5 or Qt 6 of the installed Veyon, copies the matching DLL and removes DLLs of the
earlier InternetGuard plugin.

**Option C – by hand:** copy the DLL into `<Veyon folder>\plugins\`.

> Try it on one computer first, with **Simulation mode** on, then off.

## What it does

### Controls in the Veyon Master

Each control has a toolbar toggle and, for the selected computers, *Block* /
*Allow* actions in the toolbar menu and the right-click menu.

| Control | What the Server does on the student computer |
|---|---|
| **Block / Allow Internet** | Applies the configured methods (see below). |
| **Restrict Internet** | Like Block, but the configured *allowed websites* stay reachable. |
| **Block / Allow USB Drives** | Removable-storage policy (`Deny_Read`/`Deny_Write`) and disables `USBSTOR` devices through SetupAPI. |
| **Block / Allow Webcams** | Disables all devices of the *Camera* class through SetupAPI. |
| **Mute / Unmute Microphones** | Mutes all active capture endpoints (Core Audio). |
| **Mute / Unmute Audio** | Mutes all active render endpoints (Core Audio). |

### Internet methods (combinable)

- **Windows Firewall** (default): outbound block rules through the firewall COM
  API (`INetFwPolicy2`). The firewall is switched on first.
- **Proxy redirection**: the Windows (WinINET) proxy is pointed at a proxy that
  does not answer. Only programs that use the system proxy are affected; the
  original proxy settings are saved and restored.
- **Routing table**: the default routes are removed; the local subnet stays
  reachable. Exception networks and hosts get their own routes first.

### Firewall behaviour: the local network and the domain stay reachable

Windows Firewall evaluates Block rules before Allow rules, so "block everything
but the LAN" cannot be made of a block plus an allow rule. Classroom Guard
therefore computes the **destination address list** of the block rules: all
addresses **minus** the local networks (10/8, 172.16/12, 192.168/16, link-local,
multicast, IPv6 ULA/link-local, **and the subnets the computer is connected
to**) **minus** your exceptions. Result: the domain controller, internal DNS,
file shares, printers and the teacher computer in the local network keep
working, Veyon included, while destinations on the internet are blocked.
A domain controller or DNS server in a *different, public* address range must be
added to the exception networks or hosts (or "Always let the DNS servers of this
computer through"). The switch *Block only traffic to non-local networks* can be
turned off to block everywhere, as the plugin did up to version 1.3.

Ports: *all ports*, or the groups **HTTP/HTTPS** (TCP 80, 443, 8080, 8443, 3128
and QUIC on UDP 443), **FTP** (21), **DNS toward other servers** (53 and 853,
TCP and UDP) plus **custom ports** (TCP and UDP), minus the **exception ports**.
Further options: block given servers completely (even if local, e.g. an internal
proxy) and switch on a **preconfigured firewall rule** by name while blocked.
Rules are named `CG_*` in the group "Classroom Guard"; they are removed on
"Allow".

Host names in the exceptions and in the allowed websites are resolved when the
block starts (together with their `www.` variant); sites behind CDNs change
their addresses, so Restrict mode is a best-effort filter, not a content filter.

### Restrict mode

*Restrict Internet* blocks like *Block Internet*, but the **allowed websites** stay
reachable. When the teacher starts *Restrict Internet* in the Veyon Master, a dialog
asks for the websites (one per line; the last list is remembered, the list on the
Classroom Guard page is the first suggestion). Entries like `https://docs.veyon.io/page`
are reduced to the host name. Cancelling the dialog sends nothing. The DNS servers of
the computer are let through automatically so the names can be resolved.

### Settings page, simulation, log, timer

The **Classroom Guard** page of the Veyon Configurator offers: the methods, the
proxy address, auto-release, "unblock on service start", all firewall settings
above, the allowed websites, three buttons that apply Restrict / Block / Restore
to **this** computer for testing, and the log. Settings are part of the normal
Veyon configuration (key group `ClassroomGuard`), so they are exported to the
student computers like every other Veyon setting.

- **Simulation mode**: nothing is changed; the log says what would be done.
- **Log**: `%ProgramData%\ClassroomGuard\classroom-guard.log` on each computer
  (also shown on the settings page for the local computer).
- **Release automatically after N minutes**: a block or mute lifts itself after
  the set time (0 = never), so nothing stays blocked after the lesson.
- **Notice on the student computers**: when the teacher switches a restriction on, a
  small window appears in the lower right corner of the student's screen (for example
  "Internet access has been blocked by your teacher."). It closes by itself after the
  configured number of seconds (default 8) or on a click. It is shown by the Veyon
  Worker in the student's session. Switch it off on the settings page. Nothing is shown
  in simulation mode or when a restriction is lifted.
- **End of the school day**: at the configured time (for example 15:30) every day, the
  Veyon Server on each student computer lifts all restrictions: internet, USB drives,
  webcams, microphones and audio. Restrictions switched on later the same day stay. The
  time is the clock of the student computer.
- **Unblock on service start**: when the Veyon Server starts, blocks that
  survive a reboot (firewall rules, proxy, routes, disabled USB storage and
  webcams) are released.


## Language

The plugin shows German texts when the Veyon user interface language is German, and
English otherwise: Master buttons, the "Restrict Internet" dialog, the settings page,
the notice on the student computers and the installer (follows the Windows language).
The log file is always English.

## Default configuration

These values apply on a computer where nothing has been configured yet (registry key group
`ClassroomGuard` of the Veyon configuration). Every value can be changed on the
**Classroom Guard** page of the Veyon Configurator.

| Setting (registry value)                         | Default            | Meaning |
|--------------------------------------------------|--------------------|---------|
| **General** | | |
| Simulation mode (`Simulation`)                    | off                | Really change things; on = only write to the log |
| Release automatically after (`AutoReleaseMinutes`)| 0 (never)          | Minutes until a block or mute lifts itself |
| Unblock on service start (`UnblockOnServiceStart`)| on                 | Release leftovers when the Veyon Server starts |
| **Internet methods** | | |
| Windows Firewall (`UseFirewall`)                  | on                 | Outbound block rules (`CG_*`) |
| Proxy redirection (`UseProxy`)                    | off                | System proxy for browsers |
| Proxy address (`ProxyAddress`)                    | `0.0.0.0:8080`     | Only used with proxy redirection |
| Routing table (`UseRouting`)                      | off                | Removes the default routes |
| **Firewall behaviour** | | |
| Only non-local traffic (`OnlyNonLocal`)           | on                 | LAN, domain controller, teacher PC stay reachable |
| Block all ports (`BlockAllPorts`)                 | off                | Off = only the groups below |
| HTTP/HTTPS + QUIC (`BlockWeb`)                    | on                 | 80, 443, 8080, 8443, 3128, UDP 443 |
| FTP (`BlockFtp`)                                  | on                 | 21 |
| DNS to other servers (`BlockDns`)                 | on                 | 53, 853 |
| Custom ports (`CustomPorts`)                      | empty              | e.g. `8080 9000-9100` |
| **Exceptions** | | |
| Ports / networks / hosts (`ExceptionPorts`, `ExceptionNetworks`, `ExceptionHosts`) | empty | Stay reachable |
| Let DNS servers through (`ExemptDnsServers`)      | off                | Restrict mode does this anyway |
| Block servers completely (`BlockServers`, `ServerAddresses`) | off, empty | Even local ones (e.g. internal proxy) |
| Preset firewall rule (`UsePresetRule`, `PresetRuleName`) | off, empty | Switch on an existing rule while blocked |
| **Restrict mode** | | |
| Allowed websites (`AllowedWebsites`)              | empty              | Entered in the Master each time; the last list is remembered per user |
| **Notices and school day** | | |
| Notice on student computers (`ShowStudentNotice`) | on                 | Not shown in simulation mode |
| Notice duration (`NoticeSeconds`)                 | 8 seconds          | 1-60 |
| End of the school day (`EndOfDayEnabled`)         | off                | Lifts all restrictions every day |
| End of the school day, time (`EndOfDayTime`)      | 15:30              | Clock of the student computer, `HH:mm` |

### Switched on and off by default

| On | Off |
|----|-----|
| Windows Firewall method | Proxy redirection |
| Only non-local traffic is blocked | Routing table method |
| Block HTTP/HTTPS + QUIC (80, 443, 8080, 8443, 3128, UDP 443) | Block all ports |
| Block FTP (21) | Simulation mode |
| Block DNS toward other servers (53, 853) | Always let the DNS servers through (Restrict mode always does) |
| Unblock on service start | Block servers completely |
| Notice on the student computers (8 seconds) | Preset firewall rule |
| | Auto-release (0 minutes = never) |
| | End of the school day (time 15:30 preset) |

### Address ranges that stay reachable by default

With "Only non-local traffic" (default on) the firewall blocks everything *except*
these destinations, so they stay reachable in "Block Internet" and "Restrict Internet":

| Range | Meaning |
|-------|---------|
| `127.0.0.0/8`, `::1` | Loopback (this computer) |
| `10.0.0.0/8`, `172.16.0.0/12`, `192.168.0.0/16` | Private networks (RFC 1918) |
| `169.254.0.0/16`, `fe80::/10` | Link-local |
| `fc00::/7` | IPv6 unique local addresses |
| `224.0.0.0/4`, `ff00::/8` | Multicast |
| `255.255.255.255` | Broadcast |
| Subnets of the computer's network adapters | Added when the block starts, even if they are not private (for example a school network with public addresses) |

Switch "Only non-local traffic" off to block all destinations. Exceptions you enter
(networks, hosts, ports) are added to the reachable ones. In Restrict mode the allowed
websites (resolved to addresses when the restriction starts) and the DNS servers of the
computer are reachable as well. A domain controller or DNS server outside these ranges
must be entered as an exception network or host.

If all three internet methods are switched off, the firewall is used anyway (a block
without a method would do nothing). Mute (microphone/audio) and the USB and webcam
blocks have no settings of their own; the log is always written to
`%ProgramData%\ClassroomGuard\classroom-guard.log`.

## Configuration

All run-time settings are on the **Classroom Guard** page of the Veyon
Configurator (above); save the configuration and export it to the student
computers as usual. Build-time behaviour is configured with the CMake
variables in [BUILDING.md](BUILDING.md): to target another Veyon version set `VEYON_SOURCE_DIR`,
`VEYON_CORE_LIBRARY` and possibly `WITH_QT6`, without touching the source.

For the command line test tool (`tools/guard_cli.cpp`) the same settings can be
given in the registry, `HKLM\SOFTWARE\ClassroomGuard` (value names in
`GuardSettings.h`).


## Known limitations

- **Cancelling "Restrict Internet" from the toolbar toggle.** Veyon marks the toggle as active even though the dialog was cancelled and nothing was sent. Click it again to reset it. The menu action does not have this problem.
- **Testing status.** The Veyon 4.11 build has been run on Windows with Veyon 4.11.4. The
  address/port logic is unit tested. Variants for other Veyon versions are compiled and
  checked against the matching headers. Try the plugin on one computer first, with
  *Simulation mode* on, then off.
- **Qt 5 / Veyon 4.7.5 build.** The configuration page and the settings use
  Veyon's `ConfigurationPage` and `Configuration::Object` API; they were checked
  against the 4.9.0 headers only.
- **USB and webcam blocks survive a reboot** (devices stay disabled), so the
  Server releases them when it starts or unloads. **Muted audio** is not released when the Server starts.
- **Mute is enforced, not locked.** Windows lets every user change the volume, so the Server mutes
  microphones and audio again every half second while a mute is active. A student can unmute
  briefly, a short sound may get through, then it is muted again. Switching to another audio device
  works the same way, because every active device is muted. The mute is released by "Unmute", the
  auto-release time or the end of the school day.
- **Routing**: DHCP renewal or a VPN can bring the default route back.
- **Restrict mode** is address-based (see above); it is not a content filter.
- **Group policy**: if a policy forbids local firewall rules, the firewall method has no effect.

- **Windows only.** Blocking uses the Windows Firewall API (`INetFwPolicy2`),
  so the student computers must run Windows. On other platforms the plugin
  compiles but blocking is a no-op that logs a warning.
- **ABI / Qt-version compatibility.** The DLL must match the installed Veyon's
  core and Qt (see [BUILDING.md](BUILDING.md)). Use g++ 7.3 (Qt 5.12 toolchain) for the Qt5 build; for the
  Qt6 build use **Qt 6.7 + MinGW 13.1.0** against the 4.9.0 core — that one file
  works on Veyon 4.9.x and 4.10.x. A newer Qt makes Veyon silently ignore the
  plugin, and the wrong core version makes the Server crash on load (see [BUILDING.md](BUILDING.md)).
- **Permissions.** The Veyon Server runs as a service (system account) and has
  the privileges to modify the firewall; no extra action is required.
- The installer needs write access to the Veyon folder (usually under
  `C:\Program Files`): if missing, it offers to restart as administrator.

