/*
 * ProxyRedirect.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * "Proxy redirection" method for the internet block: the Windows (WinINET)
 * proxy settings are pointed at a proxy that does not answer, so every program
 * that honours the system proxy (Edge, Chrome, Firefox with "use system proxy
 * settings", ...) loses internet access. Hosts in the bypass list keep going
 * out directly, which is how a list of allowed websites is realised.
 *
 * Programs that ignore the system proxy are NOT affected: combine this method
 * with "firewall" or "routing" when a hard block is needed.
 *
 * The settings are written under
 *   HKLM\Software\Microsoft\Windows\CurrentVersion\Internet Settings
 * and into every user profile that is loaded at that moment
 *   HKU\<SID>\Software\Microsoft\Windows\CurrentVersion\Internet Settings
 * (ProxyEnable, ProxyServer, ProxyOverride). The previous values are saved
 * first and put back by disable(), so an existing school proxy is not lost.
 * The DNS cache is flushed afterwards.
 *
 * Windows only (ProxyRedirect.cpp); the header has no Windows dependency.
 */

#pragma once

#include <string>
#include <vector>

namespace ProxyRedirect
{

// Redirects the system proxy to `server` ("host:port"). `bypass` lists hosts
// that must not use the proxy (wildcards like "*.example.org" are allowed).
// Idempotent: calling it again only updates the values, the original settings
// stay saved. Returns false if a registry location could not be written.
bool enable(const std::wstring& server, const std::vector<std::wstring>& bypass);

// Restores the saved settings. Profiles that are not loaded right now (user
// logged off) keep their saved state and are restored by a later call.
// Cheap when nothing was redirected. Returns false on unexpected failures.
bool disable();

} // namespace ProxyRedirect
