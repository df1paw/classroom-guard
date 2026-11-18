/*
 * GuardActions.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * Classroom Guard: the actions the plugin carries out on a computer - used by
 * the Veyon Server (commands from the teacher), by the auto-release timers and
 * by the test buttons on the configuration page.
 *
 * Every action reads the current settings, honours simulation mode (only the
 * log is written) and writes what it did (or would do) to GuardLog. The
 * Windows backends are only available on Windows; elsewhere the actions just
 * log that they are not supported.
 */

#pragma once

#include <QStringList>

namespace GuardActions
{

enum class InternetMode
{
	Block,      // block as configured
	Restrict,   // block, but keep the allowed websites reachable
	Allow       // undo everything
};

// allowedWebsites, if not empty, replaces the configured list of allowed
// websites for this call (sent by the Veyon Master for "Restrict internet").
void internet(InternetMode mode, const QStringList& allowedWebsites = QStringList());
void usb(bool blocked);
void webcams(bool blocked);
void microphones(bool muted);
void audio(bool muted);

// Releases everything that survives a reboot (firewall rules, proxy, routes,
// disabled USB storage and webcams). Muted audio is not touched.
void releasePersistentBlocks();

// Lifts every restriction this plugin can set (internet, USB, webcams, microphones,
// audio). Used at the end of the school day. Respects the simulation mode.
void releaseAll();

} // namespace GuardActions
