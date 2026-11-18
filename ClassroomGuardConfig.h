/*
 * ClassroomGuardConfig.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * Classroom Guard settings in the Veyon configuration.
 *
 * Everything lives below the key group "ClassroomGuard" of the normal Veyon
 * configuration (on Windows: HKLM\SOFTWARE\Veyon Solutions\Veyon). It is edited
 * on the "Classroom Guard" page of the Veyon Configurator and travels with the
 * usual Veyon configuration export/import, so every student computer gets the
 * same settings. Only the generic value()/setValue() calls of Veyon's
 * configuration object are used, which keeps the plugin independent of Veyon's
 * property macros.
 *
 * Not to be confused with GuardConfig.h: this file reads/writes the Veyon configuration
 * (Qt types, plugin only); GuardConfig.h is the plain struct the backends receive.
 *
 * Lists (hosts, networks, ...) are stored as one text with one entry per line.
 */

#pragma once

#include <QString>
#include <QStringList>

#include "GuardConfig.h"

namespace ClassroomGuardConfig
{

// keys (the value names inside the "ClassroomGuard" group)
namespace Key
{
constexpr const char* Simulation = "Simulation";
constexpr const char* AutoReleaseMinutes = "AutoReleaseMinutes";
constexpr const char* UnblockOnServiceStart = "UnblockOnServiceStart";
constexpr const char* UseFirewall = "UseFirewall";
constexpr const char* UseProxy = "UseProxy";
constexpr const char* UseRouting = "UseRouting";
constexpr const char* ProxyAddress = "ProxyAddress";
constexpr const char* OnlyNonLocal = "OnlyNonLocal";
constexpr const char* BlockAllPorts = "BlockAllPorts";
constexpr const char* BlockWeb = "BlockWeb";
constexpr const char* BlockFtp = "BlockFtp";
constexpr const char* BlockDns = "BlockDns";
constexpr const char* CustomPorts = "CustomPorts";
constexpr const char* ExceptionPorts = "ExceptionPorts";
constexpr const char* ExceptionNetworks = "ExceptionNetworks";
constexpr const char* ExceptionHosts = "ExceptionHosts";
constexpr const char* ExemptDnsServers = "ExemptDnsServers";
constexpr const char* BlockServers = "BlockServers";
constexpr const char* ServerAddresses = "ServerAddresses";
constexpr const char* UsePresetRule = "UsePresetRule";
constexpr const char* PresetRuleName = "PresetRuleName";
constexpr const char* AllowedWebsites = "AllowedWebsites";
constexpr const char* ShowStudentNotice = "ShowStudentNotice";
constexpr const char* NoticeSeconds = "NoticeSeconds";
constexpr const char* EndOfDayEnabled = "EndOfDayEnabled";
constexpr const char* EndOfDayTime = "EndOfDayTime";   // "HH:mm"
}

bool getBool(const char* key, bool defaultValue);
int getInt(const char* key, int defaultValue);
QString getString(const char* key, const QString& defaultValue = QString());

void setBool(const char* key, bool value);
void setInt(const char* key, int value);
void setString(const char* key, const QString& value);

// Splits a list text at line breaks, semicolons, commas and blanks; drops empty entries.
QStringList splitList(const QString& text);

// Splits a list of websites and cleans every entry: "https://docs.veyon.io/path"
// becomes "docs.veyon.io" (network entries such as "10.0.0.0/24" stay as they are).
// Duplicates are dropped; at most 200 entries of at most 253 characters are kept.
QStringList normalizeSites(const QString& text);

// The current settings, missing values at their defaults (GuardConfig.h).
GuardConfig load();

} // namespace ClassroomGuardConfig
