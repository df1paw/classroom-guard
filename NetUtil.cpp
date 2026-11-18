/*
 * NetUtil.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 */

#include <QtGlobal>

#include "NetUtil.h"

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x0601
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <cwctype>
#include <vector>

namespace
{

void ensureWinsock()
{
	static bool done = false;
	if (!done)
	{
		WSADATA data;
		WSAStartup(MAKEWORD(2, 2), &data);   // never cleaned up: process lifetime
		done = true;
	}
}

IpRanges::U128 toU128(const unsigned char* bytes)
{
	IpRanges::U128 v = 0;
	for (int i = 0; i < 16; ++i)
		v = (v << 8) | bytes[i];
	return v;
}

void applySockaddr(IpRanges::AddressSet& set, const SOCKADDR* sa, int prefix, bool subtract)
{
	if (sa->sa_family == AF_INET)
	{
		const auto* in = reinterpret_cast<const SOCKADDR_IN*>(sa);
		const std::uint32_t addr = ntohl(in->sin_addr.s_addr);
		std::uint32_t lo, hi;
		if (IpRanges::AddressSet::cidrBounds(addr, prefix < 0 ? 32 : prefix, lo, hi))
			subtract ? set.v4.subtract(lo, hi) : set.v4.add(lo, hi);
	}
	else if (sa->sa_family == AF_INET6)
	{
		const auto* in6 = reinterpret_cast<const SOCKADDR_IN6*>(sa);
		IpRanges::U128 lo, hi;
		if (IpRanges::AddressSet::cidrBounds(toU128(in6->sin6_addr.s6_addr),
		                                     prefix < 0 ? 128 : prefix, lo, hi))
			subtract ? set.v6.subtract(lo, hi) : set.v6.add(lo, hi);
	}
}

bool resolveInto(IpRanges::AddressSet& set, const std::wstring& host, bool subtract)
{
	ensureWinsock();
	ADDRINFOW hints = {};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	PADDRINFOW result = nullptr;
	if (GetAddrInfoW(host.c_str(), nullptr, &hints, &result) != 0 || !result)
		return false;
	for (PADDRINFOW p = result; p; p = p->ai_next)
		if (p->ai_addr)
			applySockaddr(set, p->ai_addr, -1, subtract);
	FreeAddrInfoW(result);
	return true;
}

std::vector<unsigned char> adapterBuffer(ULONG flags, bool& ok)
{
	ULONG size = 16 * 1024;
	std::vector<unsigned char> buffer;
	ULONG rc = ERROR_BUFFER_OVERFLOW;
	for (int attempt = 0; attempt < 4 && rc == ERROR_BUFFER_OVERFLOW; ++attempt)
	{
		buffer.assign(size, 0);
		rc = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr,
		                          reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &size);
	}
	ok = rc == NO_ERROR;
	if (!ok)
		qWarning("[ClassroomGuard] cannot read the network adapters (error %lu)",
		         static_cast<unsigned long>(rc));
	return buffer;
}

} // namespace

namespace NetUtil
{

std::string toUtf8(const std::wstring& text)
{
	if (text.empty())
		return std::string();
	const int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
	                                  nullptr, 0, nullptr, nullptr);
	std::string out(static_cast<size_t>(n), '\0');
	WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
	                    &out[0], n, nullptr, nullptr);
	return out;
}

std::wstring trim(const std::wstring& s)
{
	const size_t b = s.find_first_not_of(L" \t\r\n");
	if (b == std::wstring::npos)
		return L"";
	return s.substr(b, s.find_last_not_of(L" \t\r\n") - b + 1);
}

std::wstring hostFromEntry(std::wstring entry)
{
	entry = trim(entry);
	const size_t scheme = entry.find(L"://");
	if (scheme != std::wstring::npos)
		entry.erase(0, scheme + 3);
	const size_t slash = entry.find(L'/');
	if (slash != std::wstring::npos)
		entry.erase(slash);
	// strip ":port" (a single colon; more colons mean an IPv6 literal)
	const size_t colon = entry.find(L':');
	if (colon != std::wstring::npos && entry.find(L':', colon + 1) == std::wstring::npos)
		entry.erase(colon);
	if (entry.compare(0, 2, L"*.") == 0)
		entry.erase(0, 2);
	if (!entry.empty() && entry[0] == L'.')
		entry.erase(0, 1);
	std::transform(entry.begin(), entry.end(), entry.begin(),
	               [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
	return entry;
}

bool applyEntry(IpRanges::AddressSet& set, const std::wstring& entry, bool subtract)
{
	const std::wstring trimmed = trim(entry);
	if (trimmed.empty())
		return false;

	// literal address / network / range
	if (set.apply(toUtf8(trimmed), subtract))
		return true;

	const std::wstring host = hostFromEntry(trimmed);
	if (host.empty())
		return false;

	bool any = resolveInto(set, host, subtract);
	if (host.compare(0, 4, L"www.") != 0)
		any = resolveInto(set, L"www." + host, subtract) || any;
	if (!any)
		qWarning("[ClassroomGuard] cannot resolve host '%ls'", host.c_str());
	return any;
}

void applyDnsServers(IpRanges::AddressSet& set, bool subtract)
{
	bool ok = false;
	std::vector<unsigned char> buffer = adapterBuffer(
		GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_FRIENDLY_NAME, ok);
	if (!ok)
		return;

	for (PIP_ADAPTER_ADDRESSES aa = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
	     aa; aa = aa->Next)
	{
		if (aa->OperStatus != IfOperStatusUp)
			continue;
		for (PIP_ADAPTER_DNS_SERVER_ADDRESS dns = aa->FirstDnsServerAddress; dns; dns = dns->Next)
		{
			if (!dns->Address.lpSockaddr)
				continue;
			if (dns->Address.lpSockaddr->sa_family == AF_INET6)
			{
				// fec0:0:0:ffff::1-3 are Windows' placeholder DNS servers.
				const auto* in6 = reinterpret_cast<const SOCKADDR_IN6*>(dns->Address.lpSockaddr);
				if (in6->sin6_addr.s6_addr[0] == 0xfe && (in6->sin6_addr.s6_addr[1] & 0xc0) == 0xc0)
					continue;
			}
			applySockaddr(set, dns->Address.lpSockaddr, -1, subtract);
		}
	}
}

void applyLocalSubnets(IpRanges::AddressSet& set, bool subtract)
{
	bool ok = false;
	std::vector<unsigned char> buffer = adapterBuffer(
		GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER
		| GAA_FLAG_SKIP_FRIENDLY_NAME, ok);
	if (!ok)
		return;

	for (PIP_ADAPTER_ADDRESSES aa = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
	     aa; aa = aa->Next)
	{
		if (aa->OperStatus != IfOperStatusUp)
			continue;
		for (PIP_ADAPTER_UNICAST_ADDRESS ua = aa->FirstUnicastAddress; ua; ua = ua->Next)
		{
			if (!ua->Address.lpSockaddr)
				continue;
			const int prefix = ua->OnLinkPrefixLength;
			// /0 or an unusable prefix would exempt the whole internet: ignore.
			const int minimum = ua->Address.lpSockaddr->sa_family == AF_INET ? 8 : 16;
			if (prefix < minimum)
				continue;
			applySockaddr(set, ua->Address.lpSockaddr, prefix, subtract);
		}
	}
}

} // namespace NetUtil
