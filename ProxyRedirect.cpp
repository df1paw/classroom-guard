/*
 * ProxyRedirect.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * System proxy redirection. See ProxyRedirect.h.
 */

#include <QtGlobal>

#include "ProxyRedirect.h"
#include "GuardSettings.h"

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x0601
#endif
#include <windows.h>

#include <cwchar>

namespace
{

const wchar_t kSettingsPath[] =
	L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";
const wchar_t kLocalMachine[] = L"HKLM";    // name used for the saved state of HKLM
const REGSAM kView = KEY_WOW64_64KEY;

// ---------------------------------------------------------------------------
// Targets: HKLM plus every loaded user hive
// ---------------------------------------------------------------------------

struct Target
{
	std::wstring name;      // "HKLM" or the SID string (also the state sub-key name)
	HKEY root;              // HKEY_LOCAL_MACHINE or HKEY_USERS
	std::wstring path;      // path of the "Internet Settings" key below root
};

// Real user profiles only: S-1-5-21-... (local/domain accounts) and S-1-12-1-...
// (Azure AD accounts), without the "<SID>_Classes" companion hives.
bool isUserHive(const std::wstring& name)
{
	if (name.size() > 8 && name.compare(name.size() - 8, 8, L"_Classes") == 0)
		return false;
	return name.compare(0, 9, L"S-1-5-21-") == 0 || name.compare(0, 9, L"S-1-12-1-") == 0;
}

std::vector<Target> loadedTargets()
{
	std::vector<Target> targets;
	targets.push_back({ kLocalMachine, HKEY_LOCAL_MACHINE, kSettingsPath });

	for (DWORD i = 0;; ++i)
	{
		wchar_t name[256];
		DWORD len = sizeof(name) / sizeof(name[0]);
		const LONG rc = RegEnumKeyExW(HKEY_USERS, i, name, &len, nullptr, nullptr, nullptr, nullptr);
		if (rc == ERROR_NO_MORE_ITEMS)
			break;
		if (rc != ERROR_SUCCESS)
			continue;
		if (isUserHive(name))
			targets.push_back({ name, HKEY_USERS, std::wstring(name) + L"\\" + kSettingsPath });
	}
	return targets;
}

// Resolves a saved-state name back to a target if its hive is loaded now.
bool resolveTarget(const std::wstring& name, Target& target)
{
	if (name == kLocalMachine)
	{
		target = { name, HKEY_LOCAL_MACHINE, kSettingsPath };
		return true;
	}

	HKEY hive;
	if (RegOpenKeyExW(HKEY_USERS, name.c_str(), 0, KEY_READ | kView, &hive) != ERROR_SUCCESS)
		return false;       // profile not loaded (user logged off)
	RegCloseKey(hive);
	target = { name, HKEY_USERS, name + L"\\" + kSettingsPath };
	return true;
}

// ---------------------------------------------------------------------------
// Saved state: HKLM\SOFTWARE\ClassroomGuard\State\Proxy\<target>
//   HasEnable/HasServer/HasOverride  REG_DWORD (value existed before)
//   Enable                           REG_DWORD
//   Server/Override                  REG_SZ
// ---------------------------------------------------------------------------

std::wstring stateRoot()
{
	return GuardSettings::stateKeyPath(L"Proxy");
}

std::wstring stateKey(const std::wstring& targetName)
{
	return stateRoot() + L"\\" + targetName;
}

bool hasSavedState(const std::wstring& targetName)
{
	HKEY key;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, stateKey(targetName).c_str(), 0,
	                  KEY_QUERY_VALUE | kView, &key) != ERROR_SUCCESS)
		return false;
	RegCloseKey(key);
	return true;
}

bool readDword(HKEY key, const wchar_t* name, DWORD& value)
{
	DWORD type = 0, bytes = sizeof(value);
	return RegQueryValueExW(key, name, nullptr, &type,
	                        reinterpret_cast<LPBYTE>(&value), &bytes) == ERROR_SUCCESS
	    && type == REG_DWORD;
}

bool readString(HKEY key, const wchar_t* name, std::wstring& value)
{
	DWORD type = 0, bytes = 0;
	if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS
	    || (type != REG_SZ && type != REG_EXPAND_SZ))
		return false;
	std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 2, L'\0');
	if (RegQueryValueExW(key, name, nullptr, &type,
	                     reinterpret_cast<LPBYTE>(buffer.data()), &bytes) != ERROR_SUCCESS)
		return false;
	value = buffer.data();
	return true;
}

bool writeDword(HKEY key, const wchar_t* name, DWORD value)
{
	return RegSetValueExW(key, name, 0, REG_DWORD,
	                      reinterpret_cast<const BYTE*>(&value), sizeof(value)) == ERROR_SUCCESS;
}

bool writeString(HKEY key, const wchar_t* name, const std::wstring& value)
{
	return RegSetValueExW(key, name, 0, REG_SZ,
	                      reinterpret_cast<const BYTE*>(value.c_str()),
	                      static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

// Saves ProxyEnable / ProxyServer / ProxyOverride of `settings` (an open
// "Internet Settings" key) into the state key of the target.
bool saveState(const std::wstring& targetName, HKEY settings)
{
	HKEY state;
	if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, stateKey(targetName).c_str(), 0, nullptr, 0,
	                    KEY_SET_VALUE | kView, nullptr, &state, nullptr) != ERROR_SUCCESS)
		return false;

	bool ok = true;
	DWORD enable = 0;
	const bool hasEnable = readDword(settings, L"ProxyEnable", enable);
	ok = writeDword(state, L"HasEnable", hasEnable ? 1 : 0) && ok;
	if (hasEnable)
		ok = writeDword(state, L"Enable", enable) && ok;

	std::wstring server, override_;
	const bool hasServer = readString(settings, L"ProxyServer", server);
	ok = writeDword(state, L"HasServer", hasServer ? 1 : 0) && ok;
	if (hasServer)
		ok = writeString(state, L"Server", server) && ok;

	const bool hasOverride = readString(settings, L"ProxyOverride", override_);
	ok = writeDword(state, L"HasOverride", hasOverride ? 1 : 0) && ok;
	if (hasOverride)
		ok = writeString(state, L"Override", override_) && ok;

	RegCloseKey(state);
	return ok;
}

bool restoreState(const std::wstring& targetName, HKEY settings)
{
	HKEY state;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, stateKey(targetName).c_str(), 0,
	                  KEY_QUERY_VALUE | kView, &state) != ERROR_SUCCESS)
		return true;

	bool ok = true;
	DWORD has = 0, enable = 0;
	std::wstring text;

	if (readDword(state, L"HasEnable", has) && has && readDword(state, L"Enable", enable))
		ok = writeDword(settings, L"ProxyEnable", enable) && ok;
	else
		ok = writeDword(settings, L"ProxyEnable", 0) && ok;   // was not set: proxy off

	has = 0;
	if (readDword(state, L"HasServer", has) && has && readString(state, L"Server", text))
		ok = writeString(settings, L"ProxyServer", text) && ok;
	else
		RegDeleteValueW(settings, L"ProxyServer");

	has = 0;
	text.clear();
	if (readDword(state, L"HasOverride", has) && has && readString(state, L"Override", text))
		ok = writeString(settings, L"ProxyOverride", text) && ok;
	else
		RegDeleteValueW(settings, L"ProxyOverride");

	RegCloseKey(state);
	return ok;
}

// ---------------------------------------------------------------------------
// DNS cache
// ---------------------------------------------------------------------------

// Runs "%SystemRoot%\System32\ipconfig.exe /flushdns" by absolute path (the
// Server runs as SYSTEM: never rely on PATH). Best effort, 5 s timeout.
void flushDnsCache()
{
	wchar_t system32[MAX_PATH] = {};
	const UINT n = GetSystemDirectoryW(system32, MAX_PATH);
	if (n == 0 || n >= MAX_PATH)
		return;

	const std::wstring exe = std::wstring(system32) + L"\\ipconfig.exe";
	const std::wstring commandLine = L"\"" + exe + L"\" /flushdns";
	// CreateProcessW may modify the command line buffer, so hand it a copy.
	std::vector<wchar_t> cmd(commandLine.begin(), commandLine.end());
	cmd.push_back(L'\0');

	STARTUPINFOW si = {};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION pi = {};
	if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE,
	                    CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
		return;
	WaitForSingleObject(pi.hProcess, 5000);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
}

std::wstring join(const std::vector<std::wstring>& items, wchar_t separator)
{
	std::wstring result;
	for (const auto& item : items)
	{
		if (item.empty())
			continue;
		if (!result.empty())
			result += separator;
		result += item;
	}
	return result;
}

} // namespace

namespace ProxyRedirect
{

bool enable(const std::wstring& server, const std::vector<std::wstring>& bypass)
{
	const std::wstring override_ = join(bypass, L';');
	bool ok = true;

	for (const Target& target : loadedTargets())
	{
		HKEY settings;
		const LONG rc = RegCreateKeyExW(target.root, target.path.c_str(), 0, nullptr, 0,
		                                KEY_QUERY_VALUE | KEY_SET_VALUE | kView, nullptr,
		                                &settings, nullptr);
		if (rc != ERROR_SUCCESS)
		{
			qWarning("[ClassroomGuard] cannot open the proxy settings of '%ls' (error %ld)",
			         target.name.c_str(), rc);
			ok = false;
			continue;
		}

		// Keep the ORIGINAL settings: only save when nothing is saved yet.
		if (!hasSavedState(target.name) && !saveState(target.name, settings))
		{
			qWarning("[ClassroomGuard] cannot save the proxy settings of '%ls'; leaving them untouched",
			         target.name.c_str());
			RegCloseKey(settings);
			ok = false;
			continue;
		}

		const bool written = writeDword(settings, L"ProxyEnable", 1)
		                  && writeString(settings, L"ProxyServer", server)
		                  && writeString(settings, L"ProxyOverride", override_);
		RegCloseKey(settings);
		if (!written)
		{
			qWarning("[ClassroomGuard] cannot write the proxy settings of '%ls'", target.name.c_str());
			ok = false;
		}
	}

	flushDnsCache();
	return ok;
}

bool disable()
{
	// Everything that was redirected has a sub-key below State\Proxy.
	HKEY root;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, stateRoot().c_str(), 0,
	                  KEY_ENUMERATE_SUB_KEYS | kView, &root) != ERROR_SUCCESS)
		return true;    // nothing was redirected

	std::vector<std::wstring> names;
	for (DWORD i = 0;; ++i)
	{
		wchar_t name[256];
		DWORD len = sizeof(name) / sizeof(name[0]);
		const LONG rc = RegEnumKeyExW(root, i, name, &len, nullptr, nullptr, nullptr, nullptr);
		if (rc == ERROR_NO_MORE_ITEMS)
			break;
		if (rc == ERROR_SUCCESS)
			names.emplace_back(name);
	}
	RegCloseKey(root);

	bool ok = true;
	bool anyLeft = false;
	for (const std::wstring& name : names)
	{
		Target target;
		if (!resolveTarget(name, target))
		{
			anyLeft = true;     // profile not loaded: keep the state for a later call
			continue;
		}

		HKEY settings;
		if (RegCreateKeyExW(target.root, target.path.c_str(), 0, nullptr, 0,
		                    KEY_QUERY_VALUE | KEY_SET_VALUE | kView, nullptr,
		                    &settings, nullptr) != ERROR_SUCCESS)
		{
			qWarning("[ClassroomGuard] cannot open the proxy settings of '%ls'", name.c_str());
			ok = false;
			anyLeft = true;
			continue;
		}

		if (restoreState(name, settings))
			GuardSettings::deleteKeyTree(stateKey(name).c_str());
		else
		{
			qWarning("[ClassroomGuard] cannot restore the proxy settings of '%ls'", name.c_str());
			ok = false;
			anyLeft = true;
		}
		RegCloseKey(settings);
	}

	if (!anyLeft)
		GuardSettings::deleteKeyTree(stateRoot().c_str());

	flushDnsCache();
	return ok;
}

} // namespace ProxyRedirect
