// guard_cli.exe - manual test tool for the Classroom Guard backends (run as Administrator).
// (c) by Andreas Weller - https://www.andreas-weller.de/
//
// Usage: guard_cli <internet|restrict|usb|webcam|mic|audio> <block|allow>
//
// Settings come from HKLM\SOFTWARE\ClassroomGuard (value names: see
// GuardSettings.h). With the DWORD "Simulation" = 1 nothing is changed; the
// action is only printed.
#include "../DeviceControl.h"
#include "../GuardSettings.h"
#include "../ProxyRedirect.h"
#include "../RoutingBlock.h"
#include "../WindowsFirewall.h"

#include <cstdio>
#include <cstring>

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::puts("usage: guard_cli <internet|restrict|usb|webcam|mic|audio> <block|allow>");
		return 2;
	}
	const bool block = std::strcmp(argv[2], "block") == 0;
	const char* what = argv[1];
	const GuardConfig cfg = GuardSettings::loadFromRegistry();

	if (cfg.simulation)
	{
		std::printf("[simulation] would %s %s\n", block ? "block" : "allow", what);
		return 0;
	}

	bool ok = false;
	if (!std::strcmp(what, "internet") || !std::strcmp(what, "restrict"))
	{
		if (block)
		{
			const bool restrictMode = !std::strcmp(what, "restrict");
			std::vector<std::wstring> hosts = cfg.exceptionHosts;
			if (restrictMode)
				hosts.insert(hosts.end(), cfg.allowedWebsites.begin(), cfg.allowedWebsites.end());
			ok = true;
			if (cfg.useFirewall) ok = WindowsFirewall::block(cfg, restrictMode) && ok;
			if (cfg.useProxy)    ok = ProxyRedirect::enable(cfg.proxyAddress, hosts) && ok;
			if (cfg.useRouting)  ok = RoutingBlock::block(hosts, cfg.exceptionNetworks) && ok;
		}
		else
		{
			ok = WindowsFirewall::allowInternet();
			ok = ProxyRedirect::disable() && ok;
			ok = RoutingBlock::unblock() && ok;
		}
	}
	else if (!std::strcmp(what, "usb"))    ok = DeviceControl::setUsbStorageBlocked(block);
	else if (!std::strcmp(what, "webcam")) ok = DeviceControl::setWebcamsBlocked(block);
	else if (!std::strcmp(what, "mic"))    ok = DeviceControl::setMicrophonesMuted(block);
	else if (!std::strcmp(what, "audio"))  ok = DeviceControl::setAudioMuted(block);
	else
	{
		std::puts("unknown target");
		return 2;
	}
	std::printf("%s\n", ok ? "OK" : "FAILED");
	return ok ? 0 : 1;
}
