/*
 * RoutingBlock.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * Internet block through the routing table (IP Helper API). See RoutingBlock.h.
 */

#include <QtGlobal>

#include "RoutingBlock.h"
#include "GuardSettings.h"

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x0601
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <set>
#include <sstream>

namespace
{

const wchar_t kSavedDefaults[] = L"SavedDefaults";
const wchar_t kAddedRoutes[]   = L"AddedRoutes";

std::wstring stateKey()
{
	return GuardSettings::stateKeyPath(L"Routing");
}

// ---------------------------------------------------------------------------
// Winsock (name resolution)
// ---------------------------------------------------------------------------

class WsaInit
{
public:
	WsaInit() { WSADATA data; m_ok = WSAStartup(MAKEWORD(2, 2), &data) == 0; }
	~WsaInit() { if (m_ok) WSACleanup(); }
	WsaInit(const WsaInit&) = delete;
	WsaInit& operator=(const WsaInit&) = delete;

private:
	bool m_ok = false;
};

// ---------------------------------------------------------------------------
// Addresses and routes <-> text
// ---------------------------------------------------------------------------

bool addressToString(const SOCKADDR_INET& a, std::wstring& out)
{
	wchar_t buffer[64] = {};
	const void* src = a.si_family == AF_INET6
		? static_cast<const void*>(&a.Ipv6.sin6_addr)
		: static_cast<const void*>(&a.Ipv4.sin_addr);
	if (a.si_family != AF_INET && a.si_family != AF_INET6)
		return false;
	if (!InetNtopW(a.si_family, const_cast<void*>(src), buffer, 64))
		return false;
	out = buffer;
	return true;
}

bool stringToAddress(const std::wstring& text, SOCKADDR_INET& a)
{
	std::memset(&a, 0, sizeof(a));
	if (InetPtonW(AF_INET, text.c_str(), &a.Ipv4.sin_addr) == 1)
	{
		a.si_family = AF_INET;
		return true;
	}
	std::memset(&a, 0, sizeof(a));
	if (InetPtonW(AF_INET6, text.c_str(), &a.Ipv6.sin6_addr) == 1)
	{
		a.si_family = AF_INET6;
		return true;
	}
	return false;
}

// "family|luid|ifindex|prefix|prefixlen|nexthop|metric"
std::wstring serializeRoute(const MIB_IPFORWARD_ROW2& row)
{
	std::wstring prefix, nextHop;
	addressToString(row.DestinationPrefix.Prefix, prefix);
	addressToString(row.NextHop, nextHop);

	std::wostringstream s;
	s << static_cast<unsigned>(row.DestinationPrefix.Prefix.si_family) << L'|'
	  << static_cast<unsigned long long>(row.InterfaceLuid.Value) << L'|'
	  << static_cast<unsigned long>(row.InterfaceIndex) << L'|'
	  << prefix << L'|'
	  << static_cast<unsigned>(row.DestinationPrefix.PrefixLength) << L'|'
	  << nextHop << L'|'
	  << static_cast<unsigned long>(row.Metric);
	return s.str();
}

bool parseRoute(const std::wstring& line, MIB_IPFORWARD_ROW2& row)
{
	std::vector<std::wstring> f;
	std::wstring part;
	std::wstringstream s(line);
	while (std::getline(s, part, L'|'))
		f.push_back(part);
	if (f.size() != 7)
		return false;

	InitializeIpForwardEntry(&row);
	row.InterfaceLuid.Value = std::wcstoull(f[1].c_str(), nullptr, 10);
	row.InterfaceIndex = static_cast<NET_IFINDEX>(std::wcstoul(f[2].c_str(), nullptr, 10));
	if (!stringToAddress(f[3], row.DestinationPrefix.Prefix))
		return false;
	row.DestinationPrefix.PrefixLength = static_cast<UINT8>(std::wcstoul(f[4].c_str(), nullptr, 10));
	if (!f[5].empty() && !stringToAddress(f[5], row.NextHop))
		return false;
	if (f[5].empty())   // on-link route: unspecified next hop of the same family
	{
		std::memset(&row.NextHop, 0, sizeof(row.NextHop));
		row.NextHop.si_family = row.DestinationPrefix.Prefix.si_family;
	}
	row.Metric = std::wcstoul(f[6].c_str(), nullptr, 10);
	row.Protocol = RouteProtocolNetMgmt;
	row.Origin = NlroManual;
	row.ValidLifetime = 0xffffffff;
	row.PreferredLifetime = 0xffffffff;
	return true;
}

bool isDefaultRoute(const MIB_IPFORWARD_ROW2& row)
{
	if (row.DestinationPrefix.PrefixLength != 0)
		return false;
	if (row.DestinationPrefix.Prefix.si_family == AF_INET)
		return row.DestinationPrefix.Prefix.Ipv4.sin_addr.s_addr == 0;
	if (row.DestinationPrefix.Prefix.si_family == AF_INET6)
		return IN6_IS_ADDR_UNSPECIFIED(&row.DestinationPrefix.Prefix.Ipv6.sin6_addr) != 0;
	return false;
}

bool isHardFailure(DWORD error)
{
	// "already there" / "already gone" / "interface vanished" are not worth failing for.
	return error != NO_ERROR && error != ERROR_OBJECT_ALREADY_EXISTS
	    && error != ERROR_NOT_FOUND && error != ERROR_FILE_NOT_FOUND
	    && error != ERROR_INVALID_PARAMETER && error != ERROR_INVALID_NAME;
}

// ---------------------------------------------------------------------------
// Route table access
// ---------------------------------------------------------------------------

std::vector<MIB_IPFORWARD_ROW2> readDefaultRoutes(bool& ok)
{
	std::vector<MIB_IPFORWARD_ROW2> routes;
	PMIB_IPFORWARD_TABLE2 table = nullptr;
	const DWORD rc = GetIpForwardTable2(AF_UNSPEC, &table);
	if (rc != NO_ERROR)
	{
		qWarning("[ClassroomGuard] cannot read the routing table (error %lu)",
		         static_cast<unsigned long>(rc));
		ok = false;
		return routes;
	}
	for (ULONG i = 0; i < table->NumEntries; ++i)
		if (isDefaultRoute(table->Table[i]))
			routes.push_back(table->Table[i]);
	FreeMibTable(table);
	ok = true;
	return routes;
}

DWORD addRoute(const MIB_IPFORWARD_ROW2& row)
{
	return CreateIpForwardEntry2(&row);
}

DWORD removeRoute(const MIB_IPFORWARD_ROW2& row)
{
	return DeleteIpForwardEntry2(&row);
}

// Route with `prefix`/`length` that leaves through the same interface and next
// hop as the default route `via`.
MIB_IPFORWARD_ROW2 routeVia(const MIB_IPFORWARD_ROW2& via, const SOCKADDR_INET& prefix, UINT8 length)
{
	MIB_IPFORWARD_ROW2 row;
	InitializeIpForwardEntry(&row);
	row.InterfaceLuid = via.InterfaceLuid;
	row.InterfaceIndex = via.InterfaceIndex;
	row.DestinationPrefix.Prefix = prefix;
	row.DestinationPrefix.PrefixLength = length;
	row.NextHop = via.NextHop;
	row.Metric = via.Metric;
	row.Protocol = RouteProtocolNetMgmt;
	row.Origin = NlroManual;
	row.ValidLifetime = 0xffffffff;
	row.PreferredLifetime = 0xffffffff;
	return row;
}

// ---------------------------------------------------------------------------
// Exceptions: allowed hosts, DNS servers, exempt networks
// ---------------------------------------------------------------------------

struct Destination
{
	SOCKADDR_INET address;
	UINT8 prefixLength;
};

std::wstring trim(const std::wstring& s)
{
	const size_t b = s.find_first_not_of(L" \t\r\n");
	if (b == std::wstring::npos)
		return L"";
	return s.substr(b, s.find_last_not_of(L" \t\r\n") - b + 1);
}

// "https://*.Example.org/path" -> "example.org"
std::wstring hostFromEntry(std::wstring entry)
{
	entry = trim(entry);
	const size_t scheme = entry.find(L"://");
	if (scheme != std::wstring::npos)
		entry.erase(0, scheme + 3);
	const size_t slash = entry.find(L'/');
	if (slash != std::wstring::npos)
		entry.erase(slash);
	if (entry.compare(0, 2, L"*.") == 0)
		entry.erase(0, 2);
	if (!entry.empty() && entry[0] == L'.')
		entry.erase(0, 1);
	std::transform(entry.begin(), entry.end(), entry.begin(),
	               [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
	return entry;
}

void addHostAddresses(const std::wstring& host, std::vector<Destination>& out)
{
	if (host.empty())
		return;

	ADDRINFOW hints = {};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	PADDRINFOW result = nullptr;
	if (GetAddrInfoW(host.c_str(), nullptr, &hints, &result) != 0 || !result)
	{
		qWarning("[ClassroomGuard] cannot resolve allowed host '%ls'", host.c_str());
		return;
	}
	for (PADDRINFOW ai = result; ai; ai = ai->ai_next)
	{
		Destination d;
		std::memset(&d.address, 0, sizeof(d.address));
		if (ai->ai_family == AF_INET && ai->ai_addrlen >= sizeof(SOCKADDR_IN))
		{
			std::memcpy(&d.address.Ipv4, ai->ai_addr, sizeof(SOCKADDR_IN));
			d.address.Ipv4.sin_port = 0;
			d.prefixLength = 32;
		}
		else if (ai->ai_family == AF_INET6 && ai->ai_addrlen >= sizeof(SOCKADDR_IN6))
		{
			std::memcpy(&d.address.Ipv6, ai->ai_addr, sizeof(SOCKADDR_IN6));
			d.address.Ipv6.sin6_port = 0;
			d.address.Ipv6.sin6_scope_id = 0;
			d.prefixLength = 128;
		}
		else
			continue;
		out.push_back(d);
	}
	FreeAddrInfoW(result);
}

void addDnsServers(std::vector<Destination>& out)
{
	ULONG size = 16 * 1024;
	std::vector<unsigned char> buffer;
	ULONG rc = ERROR_BUFFER_OVERFLOW;
	for (int attempt = 0; attempt < 4 && rc == ERROR_BUFFER_OVERFLOW; ++attempt)
	{
		buffer.assign(size, 0);
		rc = GetAdaptersAddresses(AF_UNSPEC,
		                          GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST
		                          | GAA_FLAG_SKIP_FRIENDLY_NAME,
		                          nullptr,
		                          reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
	}
	if (rc != NO_ERROR)
	{
		qWarning("[ClassroomGuard] cannot read the configured DNS servers (error %lu) - "
		         "allowed hosts will probably not be reachable", static_cast<unsigned long>(rc));
		return;
	}

	for (PIP_ADAPTER_ADDRESSES aa = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
	     aa; aa = aa->Next)
	{
		if (aa->OperStatus != IfOperStatusUp)
			continue;
		for (PIP_ADAPTER_DNS_SERVER_ADDRESS dns = aa->FirstDnsServerAddress; dns; dns = dns->Next)
		{
			if (!dns->Address.lpSockaddr)
				continue;
			Destination d;
			std::memset(&d.address, 0, sizeof(d.address));
			const ADDRESS_FAMILY family = dns->Address.lpSockaddr->sa_family;
			if (family == AF_INET && dns->Address.iSockaddrLength >= (INT)sizeof(SOCKADDR_IN))
			{
				std::memcpy(&d.address.Ipv4, dns->Address.lpSockaddr, sizeof(SOCKADDR_IN));
				d.address.Ipv4.sin_port = 0;
				d.prefixLength = 32;
			}
			else if (family == AF_INET6 && dns->Address.iSockaddrLength >= (INT)sizeof(SOCKADDR_IN6))
			{
				std::memcpy(&d.address.Ipv6, dns->Address.lpSockaddr, sizeof(SOCKADDR_IN6));
				d.address.Ipv6.sin6_port = 0;
				d.address.Ipv6.sin6_scope_id = 0;
				// fec0:0:0:ffff::1-3 are Windows' placeholder DNS servers, not real hosts.
				const unsigned char* b = d.address.Ipv6.sin6_addr.s6_addr;
				if (b[0] == 0xfe && (b[1] & 0xc0) == 0xc0)
					continue;
				d.prefixLength = 128;
			}
			else
				continue;
			out.push_back(d);
		}
	}
}

bool parseCidr(const std::wstring& entry, Destination& d)
{
	const std::wstring text = trim(entry);
	const size_t slash = text.find(L'/');
	if (slash == std::wstring::npos)
		return false;
	if (!stringToAddress(text.substr(0, slash), d.address))
		return false;
	const unsigned long length = std::wcstoul(text.c_str() + slash + 1, nullptr, 10);
	const unsigned long maxLength = d.address.si_family == AF_INET6 ? 128 : 32;
	if (length > maxLength)
		return false;
	d.prefixLength = static_cast<UINT8>(length);
	return true;
}

} // namespace

namespace RoutingBlock
{

bool unblock()
{
	const std::wstring key = stateKey();
	const std::vector<std::wstring> saved = GuardSettings::getMultiString(key.c_str(), kSavedDefaults);
	const std::vector<std::wstring> added = GuardSettings::getMultiString(key.c_str(), kAddedRoutes);
	if (saved.empty() && added.empty())
		return true;    // not blocked

	bool ok = true;

	for (const std::wstring& line : added)
	{
		MIB_IPFORWARD_ROW2 row;
		if (!parseRoute(line, row))
			continue;
		const DWORD rc = removeRoute(row);
		if (isHardFailure(rc))
		{
			qWarning("[ClassroomGuard] cannot remove route %ls (error %lu)",
			         line.c_str(), static_cast<unsigned long>(rc));
			ok = false;
		}
	}

	for (const std::wstring& line : saved)
	{
		MIB_IPFORWARD_ROW2 row;
		if (!parseRoute(line, row))
			continue;
		const DWORD rc = addRoute(row);
		if (isHardFailure(rc))
		{
			qWarning("[ClassroomGuard] cannot restore default route %ls (error %lu)",
			         line.c_str(), static_cast<unsigned long>(rc));
			ok = false;
		}
	}

	if (ok)
		GuardSettings::deleteKeyTree(key.c_str());
	return ok;
}

bool block(const std::vector<std::wstring>& allowedHosts,
           const std::vector<std::wstring>& exemptNetworks)
{
	const std::wstring key = stateKey();

	// A block that is already active: put the original routes back first so the
	// state we save below is the real one, not our own leftovers.
	if (!GuardSettings::getMultiString(key.c_str(), kSavedDefaults).empty()
	    || !GuardSettings::getMultiString(key.c_str(), kAddedRoutes).empty())
		unblock();

	bool readOk = false;
	std::vector<MIB_IPFORWARD_ROW2> defaults = readDefaultRoutes(readOk);
	if (!readOk)
		return false;
	if (defaults.empty())
	{
		qWarning("[ClassroomGuard] no default route found - there is no way out to the internet already");
		return true;
	}

	// 1. Save what we are going to remove. If that fails, change nothing.
	std::vector<std::wstring> savedLines;
	for (const auto& row : defaults)
		savedLines.push_back(serializeRoute(row));
	if (!GuardSettings::setMultiString(key.c_str(), kSavedDefaults, savedLines))
		return false;

	// 2. Exceptions, routed through the best (lowest metric) default of each family.
	const WsaInit wsa;
	std::vector<Destination> destinations;

	const bool wantsExceptions = !allowedHosts.empty();
	if (wantsExceptions)
	{
		addDnsServers(destinations);
		for (const std::wstring& entry : allowedHosts)
		{
			const std::wstring host = hostFromEntry(entry);
			if (host.empty())
				continue;
			addHostAddresses(host, destinations);
			if (host.compare(0, 4, L"www.") != 0)
				addHostAddresses(L"www." + host, destinations);
		}
	}
	for (const std::wstring& entry : exemptNetworks)
	{
		Destination d;
		if (parseCidr(entry, d))
			destinations.push_back(d);
		else if (!trim(entry).empty())
			qWarning("[ClassroomGuard] ignoring invalid exempt network '%ls' (use CIDR, e.g. 10.0.0.0/8)",
			         entry.c_str());
	}

	bool ok = true;
	std::set<std::wstring> addedSet;
	std::vector<std::wstring> addedLines;
	for (const Destination& d : destinations)
	{
		const MIB_IPFORWARD_ROW2* via = nullptr;
		for (const auto& row : defaults)
			if (row.DestinationPrefix.Prefix.si_family == d.address.si_family
			    && (!via || row.Metric < via->Metric))
				via = &row;
		if (!via)
			continue;       // no default route of this family: nothing to preserve

		const MIB_IPFORWARD_ROW2 route = routeVia(*via, d.address, d.prefixLength);
		const std::wstring line = serializeRoute(route);
		if (!addedSet.insert(line).second)
			continue;
		const DWORD rc = addRoute(route);
		if (isHardFailure(rc))
		{
			qWarning("[ClassroomGuard] cannot add route %ls (error %lu)",
			         line.c_str(), static_cast<unsigned long>(rc));
			ok = false;
			continue;
		}
		if (rc == NO_ERROR)     // only routes we created are ours to remove later
			addedLines.push_back(line);
	}
	if (!addedLines.empty())
		ok = GuardSettings::setMultiString(key.c_str(), kAddedRoutes, addedLines) && ok;

	// 3. Remove the default routes.
	for (const auto& row : defaults)
	{
		const DWORD rc = removeRoute(row);
		if (isHardFailure(rc))
		{
			qWarning("[ClassroomGuard] cannot remove default route (error %lu)",
			         static_cast<unsigned long>(rc));
			ok = false;
		}
	}
	return ok;
}

} // namespace RoutingBlock
