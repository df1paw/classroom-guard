<div align="center">

# Classroom Guard

**Den Klassenraum vom Veyon Master aus im Griff:**
Internetzugang sperren oder einschränken, USB-Laufwerke und Webcams sperren, Mikrofone und Ton stummschalten.

[![Lizenz: GPL v2+](https://img.shields.io/badge/license-GPL--2.0--or--later-blue.svg)](LICENSE)
![Plattform: Windows](https://img.shields.io/badge/platform-Windows-0078D6.svg)
![Veyon 4.7.5 – 4.11](https://img.shields.io/badge/Veyon-4.7.5%20%E2%80%93%204.11-green.svg)

</div>

Classroom Guard ist ein Plugin für [Veyon](https://veyon.io/) (kein offizielles
Veyon-Produkt). Es fügt der Symbolleiste des **Veyon Master** Schaltflächen hinzu. Ein Klick
führt die Aktion auf allen ausgewählten Schüler-PCs aus: Der **Veyon Server** dort setzt sie
um. Ein zusätzlicher Server, Agent oder eine Konsole ist nicht nötig.

## Funktionen

| | |
|---|---|
| 🌐 **Internet sperren** | Windows-Firewall, Proxy-Umleitung und Routing-Tabelle, einzeln oder kombiniert |
| 🎯 **Internet einschränken** | Nur die genannten Websites bleiben erreichbar. Der Master fragt sie jedes Mal ab |
| 🏫 **Schulnetz bleibt erreichbar** | Nur Ziele außerhalb des lokalen Netzes werden gesperrt. Domäne, Dateifreigaben, Drucker und Veyon funktionieren weiter |
| 💾 **USB-Laufwerke** | Wechseldatenträger sperren und freigeben |
| 📷 **Webcams** | Alle Kameras deaktivieren und aktivieren |
| 🎤 **Mikrofone und Ton** | Aufnahme- und Wiedergabegeräte stummschalten und wieder einschalten |
| ⏱ **Automatische Freigabe** | Jede Sperre hebt sich nach N Minuten selbst auf. Ein einstellbares Schulende hebt zu einer festen Uhrzeit alles auf |
| 💬 **Hinweis für Schüler** | Ein kleines Popup zeigt, was gesperrt wurde, damit niemand den PC für defekt hält |
| 🇩🇪 **Deutsch und Englisch** | Die Oberfläche folgt der Veyon-Sprache |
| 🧪 **Simulationsmodus** | Das Protokoll zeigt, was passieren würde, es wird nichts geändert |

Ausnahmen für Ports, Netze und Hosts, eine Protokolldatei, eine Einstellungsseite im
Veyon Configurator und ein MSI für die Verteilung sind enthalten.

## Schnellstart

1. Laden Sie die DLL oder das MSI für Ihre Veyon-Version von der Seite
   [Releases](../../releases) herunter.

   | Veyon | Plugin |
   |---|---|
   | 4.11.x | `classroom-guard-qt6-v411.dll` (oder das MSI) |
   | 4.9.x – 4.10.x | `classroom-guard-qt6.dll` |
   | 4.7.5 – 4.8.x | `classroom-guard-qt5.dll` |

2. Kopieren Sie die Datei nach `C:\Program Files\Veyon\plugins\` auf **jedem Schüler-PC** und
   auf dem **Lehrer-PC**, oder führen Sie das MSI aus:
   ```
   msiexec /i classroom-guard-4.11.msi /qn
   ```
3. Starten Sie den Veyon-Dienst und den Veyon Master neu.
4. Wählen Sie im Master Computer aus und klicken Sie auf die neuen Schaltflächen. Probieren
   Sie zuerst den *Simulationsmodus* (Veyon Configurator → Classroom Guard).

Mehr im **[Benutzerhandbuch](docs/USER-GUIDE.md)** (englisch).

## So funktioniert es

```
Veyon Master (Lehrkraft)                  Veyon Server (jeder Schüler-PC)
┌────────────────────────┐   Feature-     ┌─────────────────────────────┐
│ Symbolleisten-Buttons  │   Nachricht    │ Classroom-Guard-Plugin      │
│ Dialog "Einschränken"  │ ─────────────► │  ├ Windows-Firewall (COM)   │
│ (erlaubte Websites)    │                │  ├ Proxy / Routing          │
└────────────────────────┘                │  ├ USB / Webcam (SetupAPI)  │
                                          │  └ Audio (Core Audio)       │
                                          └─────────────────────────────┘
```

Die Windows-Firewall wertet Sperrregeln vor Erlaubnisregeln aus. Classroom Guard berechnet
deshalb die Zielliste seiner Sperrregeln als *alle Adressen minus die lokalen Netze minus Ihre
Ausnahmen*. Details im [Benutzerhandbuch](docs/USER-GUIDE.md) und in
[ARCHITECTURE.md](ARCHITECTURE.md) (beide englisch).

## Dokumentation

| | |
|---|---|
| [Benutzerhandbuch](docs/USER-GUIDE.md) | Installation, Bedienung, Firewall-Verhalten, Einstellungen, Grenzen (englisch) |
| [Selbst bauen](docs/BUILDING.md) | Build für jede Veyon-Version, Prüfung, Installer, MSI (englisch) |
| [Architektur](ARCHITECTURE.md) | Entwurfsentscheidungen und Analyse der Veyon-Kompatibilität (englisch) |
| [Änderungsprotokoll](CHANGELOG.md) | Was sich in welcher Version geändert hat (englisch) |

## Selbst bauen

Für Veyon 4.11 unter Windows baut ein Skript gegen Ihr installiertes Veyon und prüft das
Ergebnis:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-windows.ps1
```

Voraussetzungen und die anderen Veyon-Versionen stehen in [docs/BUILDING.md](docs/BUILDING.md).

## Gut zu wissen

- Nur Windows. Die Schüler-PCs müssen unter Windows laufen.
- Die DLL muss zur Veyon-Version passen (die Plugin-Schnittstelle von 4.11 unterscheidet sich
  von 4.9/4.10). Nutzen Sie die Tabelle oben.
- Der Einschränken-Modus filtert nach Adresse, nicht nach Inhalt. Seiten hinter CDNs können
  ihre Adressen ändern.
- Getestet unter Windows mit Veyon 4.11.4. Probieren Sie es zuerst an einem Rechner aus.

## Mitwirken

Issues und Pull Requests sind willkommen. Bitte geben Sie in Fehlerberichten die
Veyon-Version, die Windows-Version und das Protokoll
(`%ProgramData%\ClassroomGuard\classroom-guard.log`) an.

## Lizenz

GPL-2.0-or-later, siehe [LICENSE](LICENSE). Die GPL gilt, weil das Plugin die Header von
Veyon verwendet und gegen `veyon-core` linkt. Das Repository enthält Importbibliotheken, die
aus [Veyon](https://github.com/veyon/veyon) abgeleitet sind (© Veyon Community,
GPL-2.0-or-later).

© [Andreas Weller](https://www.andreas-weller.de/). Basiert auf dem Plugin InternetGuard von
prof. Ing. Raffaele Mele.
