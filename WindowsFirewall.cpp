/*
 * WindowsFirewall.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 * Based on the InternetGuard plugin by prof. Ing. Raffaele Mele (GPL-2.0-or-later).
 *
 * Windows Firewall backend via the INetFwPolicy2 COM API.
 * See WindowsFirewall.h for the rationale.
 */

#include <QtGlobal>

#include "WindowsFirewall.h"
#include "GuardSettings.h"
#include "IpRanges.h"
#include "NetUtil.h"

#include <windows.h>
#include <netfw.h>
#include <oleauto.h>

#include <string>
#include <vector>

namespace
{

// ---------------------------------------------------------------------------
// COM plumbing
// ---------------------------------------------------------------------------

// GUIDs of the firewall COM objects, defined locally so no uuid import
// library is needed and both MinGW toolchains (g++ 7.3 and 13.x) link
// identically. These are stable, documented Windows identifiers.
const CLSID kClsidNetFwPolicy2 =
	{ 0xE2B3C97F, 0x6AE1, 0x41AC, { 0x81, 0x7A, 0xF6, 0xF9, 0x21, 0x66, 0xD7, 0xDD } };
const IID kIidINetFwPolicy2 =
	{ 0x98325047, 0xC671, 0x4174, { 0x8D, 0x81, 0xDE, 0xFC, 0xD3, 0xF0, 0x31, 0x86 } };
const CLSID kClsidNetFwRule =
	{ 0x2C5BC43E, 0x3369, 0x4C33, { 0xAB, 0x0C, 0xBE, 0x94, 0x69, 0x67, 0x7A, 0xF4 } };
const IID kIidINetFwRule =
	{ 0xAF230D27, 0xBABA, 0x4E42, { 0xAC, 0xED, 0xF5, 0x24, 0xF2, 0x2C, 0xFC, 0xE2 } };

// Minimal RAII wrapper for a COM interface pointer.
template<typename T>
class ComPtr
{
public:
	ComPtr() = default;
	~ComPtr() { if (m_ptr) m_ptr->Release(); }
	ComPtr(const ComPtr&) = delete;
	ComPtr& operator=(const ComPtr&) = delete;

	T* operator->() const { return m_ptr; }
	T* get() const { return m_ptr; }
	explicit operator bool() const { return m_ptr != nullptr; }
	// For out-parameters of creation functions; the pointer must be empty.
	void** out() { return reinterpret_cast<void**>(&m_ptr); }
	T** typedOut() { return &m_ptr; }

private:
	T* m_ptr = nullptr;
};

// RAII BSTR (the string type COM methods take/return).
class Bstr
{
public:
	explicit Bstr(const wchar_t* s) : m_bstr(SysAllocString(s)) {}
	~Bstr() { SysFreeString(m_bstr); }
	Bstr(const Bstr&) = delete;
	Bstr& operator=(const Bstr&) = delete;
	operator BSTR() const { return m_bstr; }

private:
	BSTR m_bstr;
};

// Per-call COM initialization. The plugin runs both on the Veyon Server
// worker thread (no COM yet -> we init/uninit) and on the Master GUI thread
// (Qt already initialized OLE/STA -> S_FALSE) - and if the thread happens to
// be MTA already, RPC_E_CHANGED_MODE just means "use the existing apartment"
// (the firewall objects are in-proc, apartment-agnostic).
class ComInit
{
public:
	ComInit()
	{
		const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		m_mustUninit = SUCCEEDED(hr);
		m_usable = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
		if (!m_usable)
			qWarning("[ClassroomGuard] CoInitializeEx failed (hr=0x%08lx)",
			         static_cast<unsigned long>(hr));
	}
	~ComInit() { if (m_mustUninit) CoUninitialize(); }
	ComInit(const ComInit&) = delete;
	ComInit& operator=(const ComInit&) = delete;

	bool usable() const { return m_usable; }

private:
	bool m_mustUninit = false;
	bool m_usable = false;
};

// ---------------------------------------------------------------------------
// Rule names
// ---------------------------------------------------------------------------

// Rules created by Classroom Guard. Names are prefixed "CG_" for reliable
// identification and cleanup.
const wchar_t* kRuleNames[] = {
	L"CG_BlockAll",         // every protocol and port
	L"CG_BlockAll_TCP",     // every TCP port except the exception ports
	L"CG_BlockAll_UDP",
	L"CG_Block_TCP",        // the selected TCP ports (web, FTP, DNS, custom)
	L"CG_Block_UDP",
	L"CG_BlockServers",     // servers that are blocked completely
};

// Rules of the earlier InternetGuard versions (<= 1.3) that are cleaned up when
// found, e.g. after an upgrade while a block was active.
const wchar_t* kLegacyRuleNames[] = {
	L"VeyonIG_AllowLAN",
	L"VeyonIG_BlockHTTP", L"VeyonIG_BlockHTTPS",
	L"VeyonIG_BlockDNS_TCP", L"VeyonIG_BlockDNS_UDP",
	L"VeyonIG_BlockQUIC",
	L"VeyonIG_BlockDoT_TCP", L"VeyonIG_BlockDoT_UDP",
	L"VeyonIG_BlockProxy",
};

const wchar_t* kRuleDescription =
	L"Added by the Classroom Guard plugin for Veyon. Removed automatically when internet access is restored.";
const wchar_t* kRuleGroup = L"Classroom Guard";

// Registry state (relative to HKLM): the preconfigured rule that was switched on.
const wchar_t kStateValuePreset[] = L"PresetRule";

std::wstring widen(const std::string& text)
{
	return std::wstring(text.begin(), text.end());   // only ASCII (addresses, ports)
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Opens the firewall policy and its rule collection.
bool openFirewall(ComPtr<INetFwPolicy2>& policy, ComPtr<INetFwRules>& rules)
{
	HRESULT hr = CoCreateInstance(kClsidNetFwPolicy2, nullptr, CLSCTX_INPROC_SERVER,
	                              kIidINetFwPolicy2, policy.out());
	if (FAILED(hr))
	{
		qWarning("[ClassroomGuard] cannot access the Windows Firewall service (hr=0x%08lx)",
		         static_cast<unsigned long>(hr));
		return false;
	}

	hr = policy->get_Rules(rules.typedOut());
	if (FAILED(hr))
	{
		qWarning("[ClassroomGuard] cannot access the firewall rule collection (hr=0x%08lx)",
		         static_cast<unsigned long>(hr));
		return false;
	}

	return true;
}

// Turning the firewall on is essential: block rules have no effect while a
// profile is disabled, which is the typical cause of "one client was not
// blocked". Safe to run unconditionally.
void ensureFirewallEnabled(INetFwPolicy2* policy)
{
	static const NET_FW_PROFILE_TYPE2 profiles[] = {
		NET_FW_PROFILE2_DOMAIN, NET_FW_PROFILE2_PRIVATE, NET_FW_PROFILE2_PUBLIC
	};
	for (const auto profile : profiles)
	{
		const HRESULT hr = policy->put_FirewallEnabled(profile, VARIANT_TRUE);
		if (FAILED(hr))
			qWarning("[ClassroomGuard] cannot enable firewall profile %d (hr=0x%08lx)",
			         static_cast<int>(profile), static_cast<unsigned long>(hr));
	}
}

// Removes every rule with the given name. Rule names are not unique, so loop
// until no match remains (Item() is the reliable existence probe: Remove()
// reports success even when nothing matched). Missing rules are not an error.
bool removeRulesByName(INetFwRules* rules, const wchar_t* name)
{
	const Bstr bstrName(name);
	for (int guard = 0; guard < 64; ++guard)
	{
		ComPtr<INetFwRule> existing;
		if (FAILED(rules->Item(bstrName, existing.typedOut())))
			return true;    // no (more) rule with this name

		const HRESULT hr = rules->Remove(bstrName);
		if (FAILED(hr))
		{
			qWarning("[ClassroomGuard] cannot remove firewall rule '%ls' (hr=0x%08lx)",
			         name, static_cast<unsigned long>(hr));
			return false;
		}
	}
	return true;
}

// protocol: 6 = TCP, 17 = UDP, 256 = any. `ports` and `addresses` may be empty
// (= no restriction). The protocol is set before the ports, or put_RemotePorts fails.
bool addBlockRule(INetFwRules* rules, const wchar_t* name, long protocol,
                  const std::wstring& ports, const std::wstring& addresses)
{
	ComPtr<INetFwRule> rule;
	HRESULT hr = CoCreateInstance(kClsidNetFwRule, nullptr, CLSCTX_INPROC_SERVER,
	                              kIidINetFwRule, rule.out());
	if (SUCCEEDED(hr)) hr = rule->put_Name(Bstr(name));
	if (SUCCEEDED(hr)) hr = rule->put_Description(Bstr(kRuleDescription));
	if (SUCCEEDED(hr)) hr = rule->put_Grouping(Bstr(kRuleGroup));
	if (SUCCEEDED(hr)) hr = rule->put_Protocol(protocol);
	if (SUCCEEDED(hr) && !ports.empty()) hr = rule->put_RemotePorts(Bstr(ports.c_str()));
	if (SUCCEEDED(hr) && !addresses.empty()) hr = rule->put_RemoteAddresses(Bstr(addresses.c_str()));
	if (SUCCEEDED(hr)) hr = rule->put_Direction(NET_FW_RULE_DIR_OUT);
	if (SUCCEEDED(hr)) hr = rule->put_Action(NET_FW_ACTION_BLOCK);
	if (SUCCEEDED(hr)) hr = rule->put_Profiles(NET_FW_PROFILE2_ALL);
	if (SUCCEEDED(hr)) hr = rule->put_Enabled(VARIANT_TRUE);
	if (SUCCEEDED(hr)) hr = rules->Add(rule.get());

	if (FAILED(hr))
	{
		qWarning("[ClassroomGuard] cannot add firewall rule '%ls' (hr=0x%08lx)",
		         name, static_cast<unsigned long>(hr));
		return false;
	}
	return true;
}

// Switches an existing rule (made by the administrator) on or off.
bool setExistingRuleEnabled(INetFwRules* rules, const std::wstring& name, bool enabled)
{
	ComPtr<INetFwRule> rule;
	const Bstr bstrName(name.c_str());
	HRESULT hr = rules->Item(bstrName, rule.typedOut());
	if (FAILED(hr))
	{
		qWarning("[ClassroomGuard] firewall rule '%ls' not found", name.c_str());
		return false;
	}
	hr = rule->put_Enabled(enabled ? VARIANT_TRUE : VARIANT_FALSE);
	if (FAILED(hr))
	{
		qWarning("[ClassroomGuard] cannot %s firewall rule '%ls' (hr=0x%08lx)",
		         enabled ? "enable" : "disable", name.c_str(), static_cast<unsigned long>(hr));
		return false;
	}
	return true;
}

// Removes all Classroom Guard rules (current and legacy) from an open collection.
bool removeAllRules(INetFwRules* rules)
{
	bool ok = true;
	for (const auto* name : kRuleNames)
		ok = removeRulesByName(rules, name) && ok;
	for (const auto* legacyName : kLegacyRuleNames)
		ok = removeRulesByName(rules, legacyName) && ok;
	return ok;
}

// Switches the preconfigured rule off again, if block() had switched it on.
bool undoPresetRule(INetFwRules* rules)
{
	const std::wstring keyPath = GuardSettings::stateKeyPath(L"Firewall");
	std::wstring name;
	if (!GuardSettings::getString(keyPath.c_str(), kStateValuePreset, name) || name.empty())
		return true;
	const bool ok = setExistingRuleEnabled(rules, name, false);
	if (ok)
		GuardSettings::deleteKeyTree(keyPath.c_str());
	return ok;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

namespace WindowsFirewall
{

bool block(const GuardConfig& cfg, bool restrictMode)
{
	using namespace IpRanges;

	const ComInit com;
	if (!com.usable())
		return false;

	ComPtr<INetFwPolicy2> policy;
	ComPtr<INetFwRules> rules;
	if (!openFirewall(policy, rules))
		return false;

	removeAllRules(rules.get());        // idempotence: clear leftovers first
	undoPresetRule(rules.get());
	ensureFirewallEnabled(policy.get());

	bool ok = true;

	// --- which destinations are blocked ---------------------------------------
	// Start with everything outside the local networks (or the whole address
	// space), then take away every exception. The result is the remote address
	// list of the block rules; Windows Firewall cannot do "allow before block".
	AddressSet destinations;
	destinations.v4.add(0, 0xffffffffu);
	destinations.v6.add(0, ~static_cast<U128>(0));

	if (cfg.onlyNonLocal)
	{
		AddressSet local;
		addLocalNetworks(local);
		destinations.v4.subtractAll(local.v4);
		destinations.v6.subtractAll(local.v6);
		NetUtil::applyLocalSubnets(destinations, true);
	}

	for (const auto& net : cfg.exceptionNetworks)
		if (!net.empty() && !NetUtil::applyEntry(destinations, net, true))
			qWarning("[ClassroomGuard] ignoring invalid exception network '%ls'", net.c_str());
	for (const auto& host : cfg.exceptionHosts)
		if (!host.empty() && !NetUtil::applyEntry(destinations, host, true))
			qWarning("[ClassroomGuard] ignoring unusable exception host '%ls'", host.c_str());
	if (restrictMode)
	{
		for (const auto& site : cfg.allowedWebsites)
			if (!site.empty() && !NetUtil::applyEntry(destinations, site, true))
				qWarning("[ClassroomGuard] ignoring unusable allowed website '%ls'", site.c_str());
	}
	// Name resolution must keep working in Restrict mode, otherwise the allowed
	// websites cannot be opened.
	if (cfg.exemptDnsServers || restrictMode)
		NetUtil::applyDnsServers(destinations, true);

	const std::wstring addresses = widen(toFirewallList(destinations));

	// --- which ports are blocked ------------------------------------------------
	PortSet exceptions;
	const int badExceptions = exceptions.parse(NetUtil::toUtf8(cfg.exceptionPorts));
	if (badExceptions)
		qWarning("[ClassroomGuard] %d invalid entries in the exception ports were ignored", badExceptions);

	if (addresses.empty())
	{
		qWarning("[ClassroomGuard] the exceptions cover every address - nothing left to block");
	}
	else if (cfg.blockAllPorts)
	{
		if (exceptions.empty())
		{
			ok = addBlockRule(rules.get(), L"CG_BlockAll", 256, L"", addresses) && ok;
		}
		else
		{
			PortSet ports = PortSet::all();
			ports.subtractAll(exceptions);
			const std::wstring list = widen(ports.toFirewallList());
			ok = addBlockRule(rules.get(), L"CG_BlockAll_TCP", NET_FW_IP_PROTOCOL_TCP, list, addresses) && ok;
			ok = addBlockRule(rules.get(), L"CG_BlockAll_UDP", NET_FW_IP_PROTOCOL_UDP, list, addresses) && ok;
		}
	}
	else
	{
		PortSet tcp, udp, custom;
		if (cfg.blockWeb)
		{
			tcp.parse("80 443 8080 8443 3128");
			udp.parse("443");               // QUIC / HTTP/3
		}
		if (cfg.blockFtp)
			tcp.parse("21");
		if (cfg.blockDns)
		{
			tcp.parse("53 853");
			udp.parse("53 853");            // DNS, DNS-over-TLS/QUIC
		}
		const int badCustom = custom.parse(NetUtil::toUtf8(cfg.customPorts));
		if (badCustom)
			qWarning("[ClassroomGuard] %d invalid entries in the custom ports were ignored", badCustom);
		tcp.addAll(custom);
		udp.addAll(custom);
		tcp.subtractAll(exceptions);
		udp.subtractAll(exceptions);

		if (!tcp.empty())
			ok = addBlockRule(rules.get(), L"CG_Block_TCP", NET_FW_IP_PROTOCOL_TCP,
			                  widen(tcp.toFirewallList()), addresses) && ok;
		if (!udp.empty())
			ok = addBlockRule(rules.get(), L"CG_Block_UDP", NET_FW_IP_PROTOCOL_UDP,
			                  widen(udp.toFirewallList()), addresses) && ok;
		if (tcp.empty() && udp.empty())
			qWarning("[ClassroomGuard] no port is selected - the firewall blocks nothing");
	}

	// --- servers that are blocked completely, local or not -----------------------
	if (cfg.blockServers && !cfg.serverAddresses.empty())
	{
		AddressSet servers;
		for (const auto& entry : cfg.serverAddresses)
			if (!entry.empty() && !NetUtil::applyEntry(servers, entry, false))
				qWarning("[ClassroomGuard] ignoring unusable server '%ls'", entry.c_str());
		if (!servers.empty())
			ok = addBlockRule(rules.get(), L"CG_BlockServers", 256, L"",
			                  widen(toFirewallList(servers))) && ok;
	}

	// --- preconfigured rule of the administrator ---------------------------------------
	if (cfg.usePresetRule && !cfg.presetRuleName.empty())
	{
		if (setExistingRuleEnabled(rules.get(), cfg.presetRuleName, true))
			GuardSettings::setString(GuardSettings::stateKeyPath(L"Firewall").c_str(),
			                         kStateValuePreset, cfg.presetRuleName);
		else
			ok = false;
	}

	return ok;
}

bool allowInternet()
{
	const ComInit com;
	if (!com.usable())
		return false;

	ComPtr<INetFwPolicy2> policy;
	ComPtr<INetFwRules> rules;
	if (!openFirewall(policy, rules))
		return false;

	const bool ok = removeAllRules(rules.get());
	return undoPresetRule(rules.get()) && ok;
}

} // namespace WindowsFirewall
