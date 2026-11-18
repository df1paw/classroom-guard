/*
 * GuardSettings.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * Registry-backed settings (see GuardSettings.h). Windows only.
 */

#include <QtGlobal>

#include "GuardSettings.h"

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x0601
#endif
#include <windows.h>

#include <cwchar>
#include <cwctype>

namespace
{

const wchar_t kRoot[] = L"SOFTWARE\\ClassroomGuard";

// The Veyon Server is a 64-bit process on 64-bit Windows; asking for the 64-bit
// view explicitly keeps the plugin independent of WOW64 redirection.
const REGSAM kView = KEY_WOW64_64KEY;

bool openKey(const wchar_t* path, REGSAM access, HKEY& key)
{
	return RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, access | kView, &key) == ERROR_SUCCESS;
}

bool queryRaw(const wchar_t* path, const wchar_t* name, DWORD& type, std::vector<wchar_t>& data)
{
	HKEY key;
	if (!openKey(path, KEY_QUERY_VALUE, key))
		return false;

	DWORD bytes = 0;
	LONG rc = RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes);
	if (rc == ERROR_SUCCESS)
	{
		// +2 wide chars: guarantees termination even for badly written values.
		data.assign(bytes / sizeof(wchar_t) + 2, L'\0');
		rc = RegQueryValueExW(key, name, nullptr, &type,
		                      reinterpret_cast<LPBYTE>(data.data()), &bytes);
	}
	RegCloseKey(key);
	return rc == ERROR_SUCCESS;
}

std::wstring regString(const wchar_t* name, const std::wstring& def)
{
	DWORD type = 0;
	std::vector<wchar_t> data;
	if (!queryRaw(kRoot, name, type, data) || (type != REG_SZ && type != REG_EXPAND_SZ))
		return def;
	return std::wstring(data.data());
}

DWORD regDword(const wchar_t* name, DWORD def)
{
	HKEY key;
	if (!openKey(kRoot, KEY_QUERY_VALUE, key))
		return def;
	DWORD type = 0, value = 0, bytes = sizeof(value);
	const LONG rc = RegQueryValueExW(key, name, nullptr, &type,
	                                 reinterpret_cast<LPBYTE>(&value), &bytes);
	RegCloseKey(key);
	return (rc == ERROR_SUCCESS && type == REG_DWORD) ? value : def;
}

std::wstring lower(std::wstring s)
{
	for (auto& c : s)
		c = static_cast<wchar_t>(std::towlower(c));
	return s;
}

} // namespace

namespace GuardSettings
{

const wchar_t* rootKeyPath()
{
	return kRoot;
}

std::wstring stateKeyPath(const wchar_t* name)
{
	return std::wstring(kRoot) + L"\\State\\" + name;
}

std::vector<std::wstring> getMultiString(const wchar_t* keyPath, const wchar_t* valueName)
{
	std::vector<std::wstring> result;
	DWORD type = 0;
	std::vector<wchar_t> data;
	if (!queryRaw(keyPath, valueName, type, data) || type != REG_MULTI_SZ)
		return result;

	for (const wchar_t* p = data.data(); *p; p += std::wcslen(p) + 1)
		result.emplace_back(p);
	return result;
}

bool setMultiString(const wchar_t* keyPath, const wchar_t* valueName,
                    const std::vector<std::wstring>& values)
{
	HKEY key;
	if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, nullptr, 0,
	                    KEY_SET_VALUE | kView, nullptr, &key, nullptr) != ERROR_SUCCESS)
	{
		qWarning("[ClassroomGuard] cannot create registry key HKLM\\%ls", keyPath);
		return false;
	}

	std::vector<wchar_t> buffer;
	for (const auto& v : values)
	{
		buffer.insert(buffer.end(), v.begin(), v.end());
		buffer.push_back(L'\0');
	}
	buffer.push_back(L'\0');    // list terminator (also makes an empty list valid)

	const LONG rc = RegSetValueExW(key, valueName, 0, REG_MULTI_SZ,
	                               reinterpret_cast<const BYTE*>(buffer.data()),
	                               static_cast<DWORD>(buffer.size() * sizeof(wchar_t)));
	RegCloseKey(key);
	if (rc != ERROR_SUCCESS)
		qWarning("[ClassroomGuard] cannot write registry value '%ls' (error %ld)", valueName, rc);
	return rc == ERROR_SUCCESS;
}

bool deleteValue(const wchar_t* keyPath, const wchar_t* valueName)
{
	HKEY key;
	if (!openKey(keyPath, KEY_SET_VALUE, key))
		return true;    // no key, nothing to delete
	const LONG rc = RegDeleteValueW(key, valueName);
	RegCloseKey(key);
	return rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND;
}

bool deleteKeyTree(const wchar_t* keyPath)
{
	const LONG rc = RegDeleteTreeW(HKEY_LOCAL_MACHINE, keyPath);
	return rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND;
}

bool getString(const wchar_t* keyPath, const wchar_t* valueName, std::wstring& out)
{
	DWORD type = 0;
	std::vector<wchar_t> data;
	if (!queryRaw(keyPath, valueName, type, data) || (type != REG_SZ && type != REG_EXPAND_SZ))
		return false;
	out.assign(data.data());
	return true;
}

bool setString(const wchar_t* keyPath, const wchar_t* valueName, const std::wstring& value)
{
	HKEY key;
	if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, nullptr, 0,
	                    KEY_SET_VALUE | kView, nullptr, &key, nullptr) != ERROR_SUCCESS)
	{
		qWarning("[ClassroomGuard] cannot create registry key HKLM\\%ls", keyPath);
		return false;
	}
	const LONG rc = RegSetValueExW(key, valueName, 0, REG_SZ,
	                               reinterpret_cast<const BYTE*>(value.c_str()),
	                               static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
	RegCloseKey(key);
	return rc == ERROR_SUCCESS;
}

GuardConfig loadFromRegistry()
{
	GuardConfig cfg;

	// "firewall;proxy;routing" - any separator, any case. An empty or invalid
	// list falls back to the firewall.
	const std::wstring methods = lower(regString(L"InternetMethods", L"firewall"));
	cfg.useFirewall = methods.find(L"firewall") != std::wstring::npos;
	cfg.useProxy    = methods.find(L"proxy")    != std::wstring::npos;
	cfg.useRouting  = methods.find(L"routing")  != std::wstring::npos;
	if (!cfg.useFirewall && !cfg.useProxy && !cfg.useRouting)
		cfg.useFirewall = true;

	cfg.proxyAddress = regString(L"ProxyAddress", cfg.proxyAddress);
	if (cfg.proxyAddress.empty())
		cfg.proxyAddress = L"0.0.0.0:8080";

	auto flag = [](const wchar_t* name, bool def) { return regDword(name, def ? 1 : 0) != 0; };
	cfg.simulation            = flag(L"Simulation", cfg.simulation);
	cfg.autoReleaseMinutes    = static_cast<int>(regDword(L"AutoReleaseMinutes", 0));
	cfg.unblockOnServiceStart = flag(L"UnblockOnServiceStart", cfg.unblockOnServiceStart);
	cfg.onlyNonLocal          = flag(L"OnlyNonLocal", cfg.onlyNonLocal);
	cfg.blockAllPorts         = flag(L"BlockAllPorts", cfg.blockAllPorts);
	cfg.blockWeb              = flag(L"BlockWeb", cfg.blockWeb);
	cfg.blockFtp              = flag(L"BlockFtp", cfg.blockFtp);
	cfg.blockDns              = flag(L"BlockDns", cfg.blockDns);
	cfg.exemptDnsServers      = flag(L"ExemptDnsServers", cfg.exemptDnsServers);
	cfg.blockServers          = flag(L"BlockServers", cfg.blockServers);
	cfg.usePresetRule         = flag(L"UsePresetRule", cfg.usePresetRule);

	cfg.customPorts     = regString(L"CustomPorts", L"");
	cfg.exceptionPorts  = regString(L"ExceptionPorts", L"");
	cfg.presetRuleName  = regString(L"PresetRuleName", L"");
	cfg.exceptionNetworks = getMultiString(kRoot, L"ExceptionNetworks");
	cfg.exceptionHosts    = getMultiString(kRoot, L"ExceptionHosts");
	cfg.serverAddresses   = getMultiString(kRoot, L"ServerAddresses");
	cfg.allowedWebsites   = getMultiString(kRoot, L"AllowedWebsites");
	return cfg;
}

} // namespace GuardSettings
