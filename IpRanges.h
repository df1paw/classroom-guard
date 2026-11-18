/*
 * IpRanges.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * Classroom Guard: address and port set arithmetic for the firewall backend.
 *
 * Windows Firewall evaluates Block rules before Allow rules, so "block the
 * internet but keep the LAN" cannot be built from a block plus an allow rule.
 * Instead the block rule is given the *complement* of everything that must stay
 * reachable as its remote address list. These helpers compute that complement.
 *
 * Pure C++14, no Windows or Qt dependency: it is unit-tested on any platform
 * (tools/ranges_test.cpp).
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace IpRanges
{

using U128 = unsigned __int128;

// Sorted set of disjoint, non-adjacent closed intervals over an unsigned type.
template<typename T>
class RangeSet
{
public:
	explicit RangeSet(T maxValue) : m_max(maxValue) {}

	bool empty() const { return m_ranges.empty(); }
	const std::vector<std::pair<T, T>>& ranges() const { return m_ranges; }
	T maxValue() const { return m_max; }

	void add(T lo, T hi)
	{
		if (lo > hi)
			std::swap(lo, hi);
		std::vector<std::pair<T, T>> out;
		bool placed = false;
		for (const auto& r : m_ranges)
		{
			// strictly before (and not adjacent to) the new range
			if (r.second < lo && lo - r.second > 1)
				out.push_back(r);
			// strictly after (and not adjacent to)
			else if (r.first > hi && r.first - hi > 1)
			{
				if (!placed) { out.emplace_back(lo, hi); placed = true; }
				out.push_back(r);
			}
			else // overlapping or adjacent: merge
			{
				lo = std::min(lo, r.first);
				hi = std::max(hi, r.second);
			}
		}
		if (!placed)
			out.emplace_back(lo, hi);
		std::sort(out.begin(), out.end());
		m_ranges.swap(out);
	}

	void subtract(T lo, T hi)
	{
		if (lo > hi)
			std::swap(lo, hi);
		std::vector<std::pair<T, T>> out;
		for (const auto& r : m_ranges)
		{
			if (r.second < lo || r.first > hi)
			{
				out.push_back(r);
				continue;
			}
			if (r.first < lo)
				out.emplace_back(r.first, lo - 1);
			if (r.second > hi)
				out.emplace_back(hi + 1, r.second);
		}
		m_ranges.swap(out);
	}

	void subtractAll(const RangeSet& other)
	{
		for (const auto& r : other.m_ranges)
			subtract(r.first, r.second);
	}

	// All values 0..max that are not in the set.
	RangeSet complement() const
	{
		RangeSet result(m_max);
		T next = 0;
		bool open = true;               // false once the range reached max
		for (const auto& r : m_ranges)
		{
			if (r.first > next)
				result.m_ranges.emplace_back(next, r.first - 1);
			if (r.second == m_max) { open = false; break; }
			next = r.second + 1;
		}
		if (open)
			result.m_ranges.emplace_back(next, m_max);
		return result;
	}

private:
	T m_max;
	std::vector<std::pair<T, T>> m_ranges;
};

inline bool isDigit(char c) { return c >= '0' && c <= '9'; }

inline bool parseIpv4(const std::string& text, std::uint32_t& out)
{
	std::uint32_t value = 0;
	int parts = 0;
	size_t i = 0;
	while (i < text.size())
	{
		if (!isDigit(text[i]))
			return false;
		unsigned long part = 0;
		int digits = 0;
		while (i < text.size() && isDigit(text[i]))
		{
			part = part * 10 + static_cast<unsigned long>(text[i] - '0');
			if (++digits > 3 || part > 255)
				return false;
			++i;
		}
		value = (value << 8) | static_cast<std::uint32_t>(part);
		++parts;
		if (i < text.size())
		{
			if (text[i] != '.' || parts == 4)
				return false;
			++i;
			if (i == text.size())
				return false;
		}
	}
	if (parts != 4)
		return false;
	out = value;
	return true;
}

inline int hexValue(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

// IPv6 text form including "::" compression and a trailing dotted IPv4 part.
inline bool parseIpv6(const std::string& textIn, U128& out)
{
	std::string text = textIn;
	const size_t scope = text.find('%');
	if (scope != std::string::npos)
		text.erase(scope);
	if (text.empty())
		return false;

	std::vector<std::uint16_t> head, tail;
	std::vector<std::uint16_t>* current = &head;
	bool compressed = false;
	size_t i = 0;
	if (text.compare(0, 2, "::") == 0)
	{
		compressed = true;
		current = &tail;
		i = 2;
	}
	else if (text[0] == ':')
		return false;

	while (i < text.size())
	{
		// dotted IPv4 tail
		const size_t dot = text.find('.', i);
		const size_t colon = text.find(':', i);
		if (dot != std::string::npos && (colon == std::string::npos || dot < colon))
		{
			std::uint32_t v4;
			if (!parseIpv4(text.substr(i), v4))
				return false;
			current->push_back(static_cast<std::uint16_t>(v4 >> 16));
			current->push_back(static_cast<std::uint16_t>(v4 & 0xffff));
			i = text.size();
			break;
		}

		unsigned long group = 0;
		int digits = 0;
		while (i < text.size() && hexValue(text[i]) >= 0)
		{
			group = group * 16 + static_cast<unsigned long>(hexValue(text[i]));
			if (++digits > 4)
				return false;
			++i;
		}
		if (digits == 0)
			return false;
		current->push_back(static_cast<std::uint16_t>(group));

		if (i < text.size())
		{
			if (text[i] != ':')
				return false;
			++i;
			if (i < text.size() && text[i] == ':')
			{
				if (compressed)
					return false;
				compressed = true;
				current = &tail;
				++i;
			}
			else if (i == text.size())
				return false;
		}
	}

	const size_t total = head.size() + tail.size();
	if (compressed ? total > 7 : total != 8)
		return false;

	std::vector<std::uint16_t> all(head);
	all.insert(all.end(), 8 - total, 0);
	all.insert(all.end(), tail.begin(), tail.end());

	U128 value = 0;
	for (auto g : all)
		value = (value << 16) | g;
	out = value;
	return true;
}

inline std::string formatIpv4(std::uint32_t v)
{
	return std::to_string((v >> 24) & 255) + "." + std::to_string((v >> 16) & 255) + "."
		+ std::to_string((v >> 8) & 255) + "." + std::to_string(v & 255);
}

inline std::string formatIpv6(U128 v)
{
	static const char* hex = "0123456789abcdef";
	std::string s;
	for (int group = 0; group < 8; ++group)
	{
		const unsigned g = static_cast<unsigned>((v >> (16 * (7 - group))) & 0xffff);
		if (group)
			s += ':';
		bool started = false;
		for (int shift = 12; shift >= 0; shift -= 4)
		{
			const unsigned nib = (g >> shift) & 0xf;
			if (nib || started || shift == 0)
			{
				s += hex[nib];
				started = true;
			}
		}
	}
	return s;
}

// IPv4 + IPv6 address set.
struct AddressSet
{
	RangeSet<std::uint32_t> v4{ 0xffffffffu };
	RangeSet<U128> v6{ ~static_cast<U128>(0) };

	bool empty() const { return v4.empty() && v6.empty(); }

	void addAddress(std::uint32_t a) { v4.add(a, a); }
	void addAddress(U128 a) { v6.add(a, a); }
	void subtractAddress(std::uint32_t a) { v4.subtract(a, a); }
	void subtractAddress(U128 a) { v6.subtract(a, a); }

	static bool cidrBounds(std::uint32_t base, int length, std::uint32_t& lo, std::uint32_t& hi)
	{
		if (length < 0 || length > 32)
			return false;
		const std::uint32_t host = length == 32 ? 0u : (0xffffffffu >> length);
		lo = base & ~host;
		hi = base | host;
		return true;
	}

	static bool cidrBounds(U128 base, int length, U128& lo, U128& hi)
	{
		if (length < 0 || length > 128)
			return false;
		const U128 host = length == 128 ? static_cast<U128>(0)
		                                : (~static_cast<U128>(0) >> length);
		lo = base & ~host;
		hi = base | host;
		return true;
	}

	// "a.b.c.d", "a.b.c.d/len", "a.b.c.d-e.f.g.h", the same for IPv6.
	// Returns false (and changes nothing) if the text is not an address,
	// network or range. `subtract` removes instead of adds.
	bool apply(const std::string& entry, bool subtract)
	{
		std::string text;
		for (char c : entry)
			if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
				text += c;
		if (text.empty())
			return false;

		const size_t dash = text.find('-');
		if (dash != std::string::npos)
		{
			std::uint32_t a4, b4;
			if (parseIpv4(text.substr(0, dash), a4) && parseIpv4(text.substr(dash + 1), b4))
			{
				subtract ? v4.subtract(a4, b4) : v4.add(a4, b4);
				return true;
			}
			U128 a6, b6;
			if (parseIpv6(text.substr(0, dash), a6) && parseIpv6(text.substr(dash + 1), b6))
			{
				subtract ? v6.subtract(a6, b6) : v6.add(a6, b6);
				return true;
			}
			return false;
		}

		std::string addr = text;
		int length = -1;
		const size_t slash = text.find('/');
		if (slash != std::string::npos)
		{
			addr = text.substr(0, slash);
			const std::string len = text.substr(slash + 1);
			if (len.empty() || len.size() > 3)
				return false;
			length = 0;
			for (char c : len)
			{
				if (!isDigit(c))
					return false;
				length = length * 10 + (c - '0');
			}
		}

		std::uint32_t a4;
		if (parseIpv4(addr, a4))
		{
			std::uint32_t lo, hi;
			if (!cidrBounds(a4, length < 0 ? 32 : length, lo, hi))
				return false;
			subtract ? v4.subtract(lo, hi) : v4.add(lo, hi);
			return true;
		}
		U128 a6;
		if (parseIpv6(addr, a6))
		{
			U128 lo, hi;
			if (!cidrBounds(a6, length < 0 ? 128 : length, lo, hi))
				return false;
			subtract ? v6.subtract(lo, hi) : v6.add(lo, hi);
			return true;
		}
		return false;
	}

	bool add(const std::string& entry) { return apply(entry, false); }
	bool remove(const std::string& entry) { return apply(entry, true); }
};

// Windows Firewall "RemoteAddresses" syntax: comma-separated addresses and
// "first-last" ranges, IPv4 and IPv6 mixed.
inline std::string toFirewallList(const AddressSet& set)
{
	std::string out;
	auto append = [&out](const std::string& item)
	{
		if (!out.empty())
			out += ',';
		out += item;
	};
	for (const auto& r : set.v4.ranges())
		append(r.first == r.second ? formatIpv4(r.first)
		                           : formatIpv4(r.first) + "-" + formatIpv4(r.second));
	for (const auto& r : set.v6.ranges())
		append(r.first == r.second ? formatIpv6(r.first)
		                           : formatIpv6(r.first) + "-" + formatIpv6(r.second));
	return out;
}

// --- ports ------------------------------------------------------------------

class PortSet
{
public:
	PortSet() : m_bits(65536, false) {}

	bool empty() const { return std::find(m_bits.begin(), m_bits.end(), true) == m_bits.end(); }
	bool contains(int port) const { return port >= 0 && port < 65536 && m_bits[static_cast<size_t>(port)]; }

	void add(int lo, int hi) { set(lo, hi, true); }
	void subtract(int lo, int hi) { set(lo, hi, false); }

	void addAll(const PortSet& other)
	{
		for (int p = 1; p < 65536; ++p)
			if (other.m_bits[static_cast<size_t>(p)])
				m_bits[static_cast<size_t>(p)] = true;
	}
	void subtractAll(const PortSet& other)
	{
		for (int p = 1; p < 65536; ++p)
			if (other.m_bits[static_cast<size_t>(p)])
				m_bits[static_cast<size_t>(p)] = false;
	}

	static PortSet all()
	{
		PortSet s;
		s.add(1, 65535);
		return s;
	}

	// "80 443,8000-8100;21". Returns the number of rejected tokens.
	int parse(const std::string& text)
	{
		int rejected = 0;
		std::string token;
		auto flush = [&]()
		{
			if (token.empty())
				return;
			int lo = 0, hi = 0;
			if (parseToken(token, lo, hi))
				add(lo, hi);
			else
				++rejected;
			token.clear();
		};
		for (char c : text)
		{
			if (c == ' ' || c == ',' || c == ';' || c == '\t' || c == '\r' || c == '\n')
				flush();
			else
				token += c;
		}
		flush();
		return rejected;
	}

	// Windows Firewall port list: "80,443,1000-2000".
	std::string toFirewallList() const
	{
		std::string out;
		int p = 1;
		while (p < 65536)
		{
			if (!m_bits[static_cast<size_t>(p)])
			{
				++p;
				continue;
			}
			int q = p;
			while (q + 1 < 65536 && m_bits[static_cast<size_t>(q + 1)])
				++q;
			if (!out.empty())
				out += ',';
			out += p == q ? std::to_string(p) : std::to_string(p) + "-" + std::to_string(q);
			p = q + 1;
		}
		return out;
	}

private:
	void set(int lo, int hi, bool value)
	{
		if (lo > hi)
			std::swap(lo, hi);
		lo = std::max(lo, 1);
		hi = std::min(hi, 65535);
		for (int p = lo; p <= hi; ++p)
			m_bits[static_cast<size_t>(p)] = value;
	}

	static bool parseNumber(const std::string& s, int& value)
	{
		if (s.empty() || s.size() > 5)
			return false;
		long v = 0;
		for (char c : s)
		{
			if (!isDigit(c))
				return false;
			v = v * 10 + (c - '0');
		}
		if (v < 1 || v > 65535)
			return false;
		value = static_cast<int>(v);
		return true;
	}

	static bool parseToken(const std::string& token, int& lo, int& hi)
	{
		const size_t dash = token.find('-');
		if (dash == std::string::npos)
			return parseNumber(token, lo) && ((hi = lo), true);
		return parseNumber(token.substr(0, dash), lo) && parseNumber(token.substr(dash + 1), hi);
	}

	std::vector<bool> m_bits;
};

// Everything that is "local" and must not be blocked when only destinations
// outside the local networks are to be blocked.
inline void addLocalNetworks(AddressSet& set)
{
	static const char* const nets[] = {
		"127.0.0.0/8",      // loopback
		"10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16",   // private (RFC 1918)
		"169.254.0.0/16",   // link local
		"224.0.0.0/4",      // multicast
		"255.255.255.255",  // broadcast
		"::1",              // loopback
		"fe80::/10",        // link local
		"fc00::/7",         // unique local
		"ff00::/8",         // multicast
	};
	for (const char* n : nets)
		set.add(n);
}

} // namespace IpRanges
