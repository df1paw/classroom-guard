/*
 * GuardConfig.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * Classroom Guard: all settings the backends need, as one plain struct (no Qt,
 * no Windows types). The plugin fills it from the Veyon configuration
 * (ClassroomGuardConfig.h); the command line test tool fills it from the
 * registry (GuardSettings::loadFromRegistry()).
 */

#pragma once

#include <string>
#include <vector>

struct GuardConfig
{
	// --- general -----------------------------------------------------------
	// Only write what would be done to the log; change nothing.
	bool simulation = false;
	// Block/mute commands release themselves after this many minutes (0 = never).
	int autoReleaseMinutes = 0;
	// When the Veyon Server starts, undo blocks that survive a reboot.
	bool unblockOnServiceStart = true;

	// --- internet: which methods are applied --------------------------------
	bool useFirewall = true;
	bool useProxy = false;
	bool useRouting = false;
	std::wstring proxyAddress = L"0.0.0.0:8080";

	// --- internet: firewall backend ------------------------------------------
	// true: only destinations outside the local networks are blocked (LAN,
	// domain controller, teacher PC stay reachable). false: block everywhere.
	bool onlyNonLocal = true;
	// true: block every port; false: only the groups below.
	bool blockAllPorts = false;
	bool blockWeb = true;       // HTTP/HTTPS (80, 443, 8080, 8443, 3128) and QUIC (UDP 443)
	bool blockFtp = true;       // 21
	bool blockDns = true;       // 53, 853 (DNS, DNS-over-TLS) - toward non-local servers
	std::wstring customPorts;   // e.g. "8080 9000-9100", TCP and UDP

	// Exceptions: stay reachable although everything else is blocked.
	std::wstring exceptionPorts;                    // never blocked
	std::vector<std::wstring> exceptionNetworks;    // CIDR or address ranges
	std::vector<std::wstring> exceptionHosts;       // names or addresses
	// Always let the configured DNS servers through (Restrict mode does this anyway).
	bool exemptDnsServers = false;

	// Block these servers completely (internal proxy, DNS, ...), even if local.
	bool blockServers = false;
	std::vector<std::wstring> serverAddresses;

	// Switch on an existing, preconfigured firewall rule while blocked.
	bool usePresetRule = false;
	std::wstring presetRuleName;

	// --- restrict mode ----------------------------------------------------------
	// Websites that stay reachable in "Restrict internet access".
	std::vector<std::wstring> allowedWebsites;
};
