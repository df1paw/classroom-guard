/*
 * RoutingBlock.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * "Routing" method for the internet block: the default routes (0.0.0.0/0 and
 * ::/0) are removed from the routing table. The computer keeps talking to its
 * own subnet (on-link routes stay) but has no way out to the internet.
 *
 * Optional exceptions are installed *before* the default routes are removed:
 *  - host routes for allowed hosts (their addresses are resolved first) and for
 *    the configured DNS servers, sent through the original gateway;
 *  - routes for "exempt networks" (e.g. the teacher's subnet when Veyon has to
 *    cross a router), sent through the original gateway.
 *
 * The removed default routes and the added exception routes are saved in the
 * registry (HKLM\SOFTWARE\ClassroomGuard\State\Routing), so unblock() puts
 * everything back even after the Veyon Server was restarted.
 *
 * Limitations: a DHCP renewal, a new network connection or a VPN may add a
 * default route again; the next "block" command removes it again. Allowed hosts
 * are matched by IP address: CDN-hosted sites rotate addresses.
 *
 * Windows only (RoutingBlock.cpp); the header has no Windows dependency.
 */

#pragma once

#include <string>
#include <vector>

namespace RoutingBlock
{

// Removes the default routes and installs the exceptions described above.
// `allowedHosts`: host names or IP addresses ("*." / "http://" prefixes are
// stripped). `exemptNetworks`: CIDR notation, e.g. "10.20.0.0/16".
// Idempotent: a block that is already active is first undone, then re-applied.
// Returns false if the original routes could not be saved or a route change failed.
bool block(const std::vector<std::wstring>& allowedHosts,
           const std::vector<std::wstring>& exemptNetworks);

// Removes the exception routes and restores the saved default routes.
// Cheap when nothing is blocked. Returns false if a route could not be restored
// (the saved state is then kept so a later call can retry).
bool unblock();

} // namespace RoutingBlock
