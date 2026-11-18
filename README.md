<div align="center">

# Classroom Guard

**Take control of the classroom from the Veyon Master:**
block or restrict internet access, lock USB drives and webcams, mute microphones and audio.

[![License: GPL v2+](https://img.shields.io/badge/license-GPL--2.0--or--later-blue.svg)](LICENSE)
![Platform: Windows](https://img.shields.io/badge/platform-Windows-0078D6.svg)
![Veyon 4.7.5 – 4.11](https://img.shields.io/badge/Veyon-4.7.5%20%E2%80%93%204.11-green.svg)

</div>

Classroom Guard is a plugin for [Veyon](https://veyon.io/) (not an official Veyon
product). It adds buttons to the **Veyon Master** toolbar. A click runs the action on
all selected student computers: the **Veyon Server** there carries it out. No extra
server, agent or console is needed.

## Features

| | |
|---|---|
| 🌐 **Block internet** | Windows Firewall, proxy redirection and routing table, alone or combined |
| 🎯 **Restrict internet** | Only the websites you name stay reachable. The Master asks for them each time |
| 🏫 **LAN stays alive** | Only non-local destinations are blocked. Domain, file shares, printers and Veyon keep working |
| 💾 **USB drives** | Block and allow removable storage |
| 📷 **Webcams** | Disable and enable all cameras |
| 🎤 **Microphones and audio** | Mute and unmute capture and playback devices |
| ⏱ **Auto-release** | Every block lifts itself after N minutes. A configurable end of the school day lifts everything at a set time |
| 💬 **Notice for students** | A small popup tells the student what was locked, so nobody thinks the PC is broken |
| 🇩🇪 **German and English** | The interface follows the Veyon language |
| 🧪 **Simulation mode** | The log says what would happen, nothing is changed |

Exceptions for ports, networks and hosts, a log file, a settings page in the Veyon
Configurator and a MSI for rollout are included.

## Quick start

1. Download the DLL or MSI for your Veyon version from the
   [Releases](../../releases) page.

   | Veyon | Plugin |
   |---|---|
   | 4.11.x | `classroom-guard-qt6-v411.dll` (or the MSI) |
   | 4.9.x – 4.10.x | `classroom-guard-qt6.dll` |
   | 4.7.5 – 4.8.x | `classroom-guard-qt5.dll` |

2. Copy it into `C:\Program Files\Veyon\plugins\` on **every student PC** and on the
   **teacher PC**, or run the MSI:
   ```
   msiexec /i classroom-guard-4.11.msi /qn
   ```
3. Restart the Veyon Service and the Veyon Master.
4. In the Master select computers and click the new buttons. Try *Simulation mode* first
   (Veyon Configurator → Classroom Guard).

More in the **[user guide](docs/USER-GUIDE.md)**.

## How it works

```
Veyon Master (teacher)                    Veyon Server (each student PC)
┌────────────────────────┐   feature      ┌─────────────────────────────┐
│ toolbar buttons        │   message      │ Classroom Guard plugin      │
│ "Restrict" dialog      │ ─────────────► │  ├ Windows Firewall (COM)   │
│ (allowed websites)     │                │  ├ proxy / routing          │
└────────────────────────┘                │  ├ USB / webcam (SetupAPI)  │
                                          │  └ audio (Core Audio)       │
                                          └─────────────────────────────┘
```

Windows Firewall evaluates block rules before allow rules. Classroom Guard therefore
computes the destination list of its block rules as *all addresses minus the local
networks minus your exceptions*. Details in the [user guide](docs/USER-GUIDE.md) and
[ARCHITECTURE.md](ARCHITECTURE.md).

## Documentation

| | |
|---|---|
| [User guide](docs/USER-GUIDE.md) | Installation, controls, firewall behaviour, settings, limitations |
| [Building](docs/BUILDING.md) | Build for every Veyon version, verification, installer, MSI |
| [Architecture](ARCHITECTURE.md) | Design decisions and the Veyon compatibility analysis |
| [Changelog](CHANGELOG.md) | What changed in each version |

## Build it yourself

For Veyon 4.11 on Windows one script builds against your installed Veyon and checks the
result:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-windows.ps1
```

See [docs/BUILDING.md](docs/BUILDING.md) for the prerequisites and the other Veyon versions.

## Good to know

- Windows only. The student computers must run Windows.
- A DLL must match the Veyon version (the plugin API of 4.11 differs from 4.9/4.10).
  Use the table above.
- Restrict mode filters by address, not by content. Sites behind CDNs can change addresses.
- Tested on Windows with Veyon 4.11.4. Try it on one computer first.

## Contributing

Issues and pull requests are welcome. Please write the Veyon version, Windows version and
the log (`%ProgramData%\ClassroomGuard\classroom-guard.log`) into bug reports.

## License

GPL-2.0-or-later, see [LICENSE](LICENSE). The GPL applies because the plugin uses Veyon's
headers and links against `veyon-core`. The repository includes import libraries derived
from [Veyon](https://github.com/veyon/veyon) (© Veyon Community, GPL-2.0-or-later).

(c) by [Andreas Weller](https://www.andreas-weller.de/). Based on the InternetGuard
plugin by prof. Ing. Raffaele Mele.
