/*
 * NetUtil.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * Classroom Guard: Windows network helpers for the firewall backend - resolve
 * host names, read the configured DNS servers and the computer's own subnets
 * into an IpRanges::AddressSet. Windows only (NetUtil.cpp).
 */

#pragma once

#include <string>

#include "IpRanges.h"

namespace NetUtil
{

std::string toUtf8(const std::wstring& text);
std::wstring trim(const std::wstring& text);

// "https://*.Example.org:8080/path" -> "example.org"
std::wstring hostFromEntry(std::wstring entry);

// Adds (or, with `subtract`, removes) what an entry stands for. The entry may be
// an address, a CIDR network, an address range or a host name; a host name is
// resolved now (together with its "www." variant), so the result is a snapshot.
// Returns false if nothing could be made of the entry.
bool applyEntry(IpRanges::AddressSet& set, const std::wstring& entry, bool subtract);

// The DNS servers configured on the active network adapters.
void applyDnsServers(IpRanges::AddressSet& set, bool subtract);

// The networks the computer is directly connected to (on-link prefixes).
void applyLocalSubnets(IpRanges::AddressSet& set, bool subtract);

} // namespace NetUtil
