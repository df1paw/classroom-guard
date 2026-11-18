// Unit test for IpRanges.h - build/run on any platform:
//   g++ -std=c++14 -I. tools/ranges_test.cpp -o ranges_test && ./ranges_test
#include "../IpRanges.h"
#include <cstdio>
#include <cstdlib>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #cond); ++failures; } } while (0)
#define CHECK_EQ(a, b) do { std::string x = (a), y = (b); if (x != y) { std::printf("FAIL line %d: '%s' != '%s'\n", __LINE__, x.c_str(), y.c_str()); ++failures; } } while (0)

using namespace IpRanges;

int main()
{
	// parsing
	std::uint32_t v4; U128 v6;
	CHECK(parseIpv4("192.168.1.10", v4) && v4 == 0xC0A8010Au);
	CHECK(!parseIpv4("256.1.1.1", v4)); CHECK(!parseIpv4("1.2.3", v4)); CHECK(!parseIpv4("1.2.3.4.5", v4));
	CHECK(!parseIpv4("1..3.4", v4)); CHECK(!parseIpv4("a.b.c.d", v4)); CHECK(!parseIpv4("1.2.3.4.", v4));
	CHECK(parseIpv6("::1", v6) && v6 == 1);
	CHECK(parseIpv6("::", v6) && v6 == 0);
	CHECK(parseIpv6("fe80::1%12", v6));
	CHECK(parseIpv6("2001:db8::ff00:42:8329", v6));
	CHECK(parseIpv6("::ffff:1.2.3.4", v6) && v6 == ((static_cast<U128>(0xffff) << 32) | 0x01020304u));
	CHECK(!parseIpv6("1::2::3", v6)); CHECK(!parseIpv6("12345::", v6)); CHECK(!parseIpv6(":1", v6));
	CHECK(!parseIpv6("1:2:3:4:5:6:7", v6)); CHECK(parseIpv6("1:2:3:4:5:6:7:8", v6));
	CHECK_EQ(formatIpv6(1), "0:0:0:0:0:0:0:1");

	// range set basics
	RangeSet<std::uint32_t> r(0xffffffffu);
	r.add(10, 20); r.add(30, 40); r.add(21, 29);
	CHECK(r.ranges().size() == 1 && r.ranges()[0].first == 10 && r.ranges()[0].second == 40);
	r.subtract(15, 35);
	CHECK(r.ranges().size() == 2 && r.ranges()[0].second == 14 && r.ranges()[1].first == 36);
	RangeSet<std::uint32_t> c = r.complement();
	CHECK(c.ranges().size() == 3);
	CHECK(c.ranges()[0].first == 0 && c.ranges()[0].second == 9);
	CHECK(c.ranges()[2].second == 0xffffffffu);
	RangeSet<std::uint32_t> full(0xffffffffu); full.add(0, 0xffffffffu);
	CHECK(full.complement().empty());
	RangeSet<std::uint32_t> none(0xffffffffu);
	CHECK(none.complement().ranges().size() == 1);

	// complement of the local networks: everything else
	AddressSet local;
	addLocalNetworks(local);
	AddressSet external;
	external.v4 = local.v4.complement();
	external.v6 = local.v6.complement();
	// spot checks through the firewall list
	std::string list = toFirewallList(external);
	CHECK(list.find("0.0.0.0-9.255.255.255") == 0);                 // before 10/8
	CHECK(list.find("11.0.0.0-126.255.255.255") != std::string::npos);
	CHECK(list.find("172.15.255.255") != std::string::npos);        // just below 172.16/12
	CHECK(list.find("172.32.0.0-") != std::string::npos);           // just above
	CHECK(list.find("192.168.") == std::string::npos);
	CHECK(list.find("239.255.255.255") == std::string::npos);       // multicast excluded
	CHECK(list.find("240.0.0.0-255.255.255.254") != std::string::npos);

	// exceptions: network, host, range
	AddressSet ext2 = external;
	CHECK(ext2.remove("8.8.8.0/24"));
	CHECK(ext2.remove("93.184.216.34"));
	CHECK(ext2.remove("1.1.1.1-1.1.1.5"));
	CHECK(!ext2.remove("not-an-address"));
	std::string l2 = toFirewallList(ext2);
	CHECK(l2.find("8.8.8.") == std::string::npos || l2.find("8.8.8.255") == std::string::npos);
	CHECK(l2.find("93.184.216.33") != std::string::npos);
	CHECK(l2.find("93.184.216.35") != std::string::npos);
	{   // 8.8.8.8 must not be inside any range
		bool inside = false;
		for (auto& rg : ext2.v4.ranges())
			if (rg.first <= 0x08080808u && 0x08080808u <= rg.second) inside = true;
		CHECK(!inside);
		inside = false;
		for (auto& rg : ext2.v4.ranges())
			if (rg.first <= 0x01010103u && 0x01010103u <= rg.second) inside = true;
		CHECK(!inside);
		inside = false;
		for (auto& rg : ext2.v4.ranges())
			if (rg.first <= 0x01010106u && 0x01010106u <= rg.second) inside = true;
		CHECK(inside);
	}
	AddressSet ipv6;
	CHECK(ipv6.add("2001:db8::/32"));
	CHECK_EQ(toFirewallList(ipv6), "2001:db8:0:0:0:0:0:0-2001:db8:ffff:ffff:ffff:ffff:ffff:ffff");

	// ports
	PortSet p;
	CHECK(p.parse("80 443,8000-8002;21") == 0);
	CHECK_EQ(p.toFirewallList(), "21,80,443,8000-8002");
	PortSet bad;
	CHECK(bad.parse("0 99999 abc 22") == 3);
	CHECK_EQ(bad.toFirewallList(), "22");
	PortSet all = PortSet::all();
	PortSet ex; ex.parse("80 443");
	all.subtractAll(ex);
	CHECK_EQ(all.toFirewallList(), "1-79,81-442,444-65535");
	CHECK(PortSet().empty());

	// "everything minus local" built the way the firewall does it
	AddressSet world;
	world.v4.add(0, 0xffffffffu);
	world.v6.add(0, ~static_cast<U128>(0));
	world.v4.subtractAll(local.v4);
	world.v6.subtractAll(local.v6);
	CHECK(toFirewallList(world) == list);

	std::printf(failures ? "%d FAILURE(S)\n" : "all tests passed\n", failures);
	return failures ? 1 : 0;
}
