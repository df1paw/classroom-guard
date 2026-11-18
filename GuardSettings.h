/*
 * GuardSettings.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * Registry helpers of Classroom Guard.
 *
 * The plugin takes its settings from the Veyon configuration (editable on the
 * "Classroom Guard" page of the Veyon Configurator and deployed to all
 * computers with the normal Veyon configuration export). The registry is used
 * for two other things:
 *
 *  - the run-time state needed to undo a block later, below
 *      HKLM\SOFTWARE\ClassroomGuard\State   (do not edit by hand);
 *  - loadFromRegistry(): an optional way to feed the command line test tool
 *    (tools/guard_cli.cpp) with settings, value names as in GuardConfig.h
 *    ("InternetMethods" = firewall;proxy;routing, "ProxyAddress",
 *    "OnlyNonLocal", "BlockAllPorts", "BlockWeb", "BlockFtp", "BlockDns",
 *    "CustomPorts", "ExceptionPorts", "ExceptionNetworks", "ExceptionHosts",
 *    "ExemptDnsServers", "BlockServers", "ServerAddresses", "UsePresetRule",
 *    "PresetRuleName", "AllowedWebsites", "Simulation", "AutoReleaseMinutes",
 *    "UnblockOnServiceStart").
 *
 * The header has no Windows dependency; GuardSettings.cpp is Windows-only.
 */

#pragma once

#include <string>
#include <vector>

#include "GuardConfig.h"

namespace GuardSettings
{

// Reads all settings from the registry (used by the command line test tool; the
// plugin itself reads the Veyon configuration). Missing values keep the
// defaults of GuardConfig.
GuardConfig loadFromRegistry();

bool getString(const wchar_t* keyPath, const wchar_t* valueName, std::wstring& out);
bool setString(const wchar_t* keyPath, const wchar_t* valueName, const std::wstring& value);

// --- low-level helpers shared with the other backends ---------------------

// Root of the plugin's registry settings, below HKEY_LOCAL_MACHINE.
const wchar_t* rootKeyPath();
// State sub-key (relative to HKLM), e.g. "SOFTWARE\\ClassroomGuard\\State\\Routing".
std::wstring stateKeyPath(const wchar_t* name);

std::vector<std::wstring> getMultiString(const wchar_t* keyPath, const wchar_t* valueName);
bool setMultiString(const wchar_t* keyPath, const wchar_t* valueName,
                    const std::vector<std::wstring>& values);
bool deleteValue(const wchar_t* keyPath, const wchar_t* valueName);
// Removes the key and everything below it (no error if it does not exist).
bool deleteKeyTree(const wchar_t* keyPath);

} // namespace GuardSettings
