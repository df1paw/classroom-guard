# Changelog

## 2.1.0 (unreleased)

- Settings page showed 0 seconds for the notice duration when nothing was configured (the real default is 8). Default configuration is now documented in the user guide.
- Fix: the toolbar buttons are no longer Veyon "Mode" features. Veyon sends Stop to all other
  features whenever a mode starts (toolbar and right-click), which lifted other restrictions
  and cleared the mute enforcement. Restrictions are now only lifted by the explicit Allow
  actions, the auto-release time and the end of the school day.
- The Service reports the real state of each restriction to the Master (isFeatureActive):
  the right-click menu shows the matching entry ("Allow ..." only when active) and the
  computer icon reflects the state.
- Log: commands received and mute watchdog start are written to the log file.
- "Restrict Internet" asks for the allowed websites in the Veyon Master. The list is sent
  with the command. The last list is remembered per user.
- Notice on the student computers when a restriction is switched on (configurable duration).
- Setting "End of the school day": lifts all restrictions every day at a given time.
- German user interface (Master, settings page, notices, installer). English remains the
  project language and the default.
- Muted microphones and audio are enforced: if a student unmutes in the volume mixer, the Service mutes again within half a second while the mute is active.
- Fix: the Notice no longer makes the Internet toggle look active in the Master (hidden feature UID).
- Fix: the Veyon Worker process no longer undoes internet blocks when it unloads.
- Entries such as `https://docs.veyon.io/page` are reduced to the host name.

## 2.0.0

- Renamed from InternetGuard to **Classroom Guard**.
- New: block USB drives and webcams, mute microphones and audio.
- New firewall design: only non-local destinations are blocked, so the local network and
  the domain keep working. Exceptions for ports, networks and hosts, block servers,
  preconfigured firewall rule, DNS-server exemption.
- New: Restrict mode with allowed websites, simulation mode and log, auto-release timer,
  "unblock on service start", settings page in the Veyon Configurator.
- New: variant for Veyon 4.11 (`classroom-guard-qt6-v411.dll`), MSI package,
  `tools/build-windows.ps1` with static verification.
- Configuration is stored in the Veyon configuration (no QtCrypto dependency).
- Documentation and installer texts in English.

## 1.x

InternetGuard by prof. Ing. Raffaele Mele.
