/*
 * WindowsFirewall.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 * Based on the InternetGuard plugin by prof. Ing. Raffaele Mele (GPL-2.0-or-later).
 *
 * Windows Firewall backend for the Classroom Guard plugin.
 *
 * Talks to the firewall through the native COM API (INetFwPolicy2 /
 * INetFwRule, "HNetCfg" objects) instead of spawning netsh processes:
 * no external tool on PATH, no locale-dependent output parsing, no
 * process timeouts, and precise per-call HRESULT error reporting.
 * The rules created are the same persistent Windows Firewall rules
 * netsh would create (visible in "Windows Defender Firewall with
 * Advanced Security").
 *
 * Compiled on Windows only (see CMakeLists.txt); the header itself has
 * no Windows dependency so it can be included unconditionally.
 */

#pragma once

#include "GuardConfig.h"

namespace WindowsFirewall
{

// Enables the firewall on all profiles, then (re-)adds the outbound block rules
// described by `cfg`: ports (all, or web/FTP/DNS/custom minus the exception
// ports), destinations (everything outside the local networks, or everywhere,
// minus exception networks and hosts; in Restrict mode also minus the allowed
// websites and the DNS servers), the completely blocked servers, and the
// preconfigured rule. Idempotent: existing Classroom Guard rules are removed
// first. Returns false if any rule could not be applied (details are logged
// via qWarning).
bool block(const GuardConfig& cfg, bool restrictMode);

// Removes every Classroom Guard firewall rule (including rules of older plugin
// versions) and switches off the preconfigured rule again. Idempotent and cheap
// when nothing is installed. Returns false only on unexpected COM failures.
bool allowInternet();

} // namespace WindowsFirewall
