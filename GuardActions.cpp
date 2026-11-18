/*
 * GuardActions.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 */

#include "GuardActions.h"

#include <QString>

#include "ClassroomGuardConfig.h"
#include "GuardLog.h"

#ifdef Q_OS_WIN
#include "DeviceControl.h"
#include "ProxyRedirect.h"
#include "RoutingBlock.h"
#include "WindowsFirewall.h"
#endif

namespace
{

QString describeInternet(const GuardConfig& c, bool restrictMode)
{
	QStringList methods;
	if (c.useFirewall) methods << QStringLiteral("firewall");
	if (c.useProxy)    methods << QStringLiteral("proxy (%1)").arg(QString::fromStdWString(c.proxyAddress));
	if (c.useRouting)  methods << QStringLiteral("routing");

	QString ports;
	if (c.blockAllPorts)
		ports = QStringLiteral("all ports");
	else
	{
		QStringList p;
		if (c.blockWeb) p << QStringLiteral("web");
		if (c.blockFtp) p << QStringLiteral("ftp");
		if (c.blockDns) p << QStringLiteral("dns");
		if (!c.customPorts.empty()) p << QString::fromStdWString(c.customPorts).trimmed();
		ports = p.isEmpty() ? QStringLiteral("no ports") : p.join(QStringLiteral("+"));
	}

	QString text = QStringLiteral("methods=%1; ports=%2; scope=%3; exceptions: %4 networks, %5 hosts")
		.arg(methods.join(QStringLiteral(",")), ports,
		     c.onlyNonLocal ? QStringLiteral("non-local networks only") : QStringLiteral("everywhere"))
		.arg(c.exceptionNetworks.size())
		.arg(c.exceptionHosts.size());
	if (!c.exceptionPorts.empty())
		text += QStringLiteral(", ports %1").arg(QString::fromStdWString(c.exceptionPorts).trimmed());
	if (restrictMode)
		text += QStringLiteral("; allowed websites: %1").arg(c.allowedWebsites.size());
	if (c.blockServers)
		text += QStringLiteral("; blocked servers: %1").arg(c.serverAddresses.size());
	if (c.usePresetRule)
		text += QStringLiteral("; preset rule '%1'").arg(QString::fromStdWString(c.presetRuleName));
	return text;
}

// Logs the outcome of a real action.
void report(const QString& what, bool ok)
{
	GuardLog::write(ok ? what + QStringLiteral(": done")
	                   : what + QStringLiteral(": FAILED or incomplete (see the Veyon log)"));
}

#ifdef Q_OS_WIN
using HostList = std::vector<std::wstring>;
#endif

} // namespace

namespace GuardActions
{

void internet(InternetMode mode, const QStringList& allowedWebsites)
{
	GuardConfig cfg = ClassroomGuardConfig::load();
	if (mode == InternetMode::Restrict && !allowedWebsites.isEmpty())
	{
		cfg.allowedWebsites.clear();
		for (const QString& site : allowedWebsites)
			cfg.allowedWebsites.push_back(site.toStdWString());
	}

	if (mode == InternetMode::Allow)
	{
		if (cfg.simulation)
		{
			GuardLog::write(QStringLiteral("[simulation] would restore internet access"));
			return;
		}
#ifdef Q_OS_WIN
		// Undo every method whatever is configured now: the setting may have
		// been changed while a block was active.
		bool ok = WindowsFirewall::allowInternet();
		ok = ProxyRedirect::disable() && ok;
		ok = RoutingBlock::unblock() && ok;
		report(QStringLiteral("restore internet access"), ok);
#else
		GuardLog::write(QStringLiteral("internet control is only implemented on Windows"));
#endif
		return;
	}

	const bool restrictMode = mode == InternetMode::Restrict;
	const QString what = restrictMode ? QStringLiteral("restrict internet access")
	                                  : QStringLiteral("block internet access");

	if (cfg.simulation)
	{
		GuardLog::write(QStringLiteral("[simulation] would %1 - %2").arg(what, describeInternet(cfg, restrictMode)));
		return;
	}

#ifdef Q_OS_WIN
	// Hosts that proxy and routing keep reachable: the exception hosts, in
	// Restrict mode also the allowed websites.
	HostList hosts = cfg.exceptionHosts;
	if (restrictMode)
		hosts.insert(hosts.end(), cfg.allowedWebsites.begin(), cfg.allowedWebsites.end());

	bool ok = true;
	if (cfg.useFirewall)
		ok = WindowsFirewall::block(cfg, restrictMode) && ok;
	if (cfg.useProxy)
		ok = ProxyRedirect::enable(cfg.proxyAddress, hosts) && ok;
	if (cfg.useRouting)
		ok = RoutingBlock::block(hosts, cfg.exceptionNetworks) && ok;
	report(what + QStringLiteral(" (") + describeInternet(cfg, restrictMode) + QStringLiteral(")"), ok);
#else
	GuardLog::write(QStringLiteral("internet control is only implemented on Windows"));
#endif
}

#ifdef Q_OS_WIN
#define CG_DEVICE_ACTION(NAME, TEXT_ON, TEXT_OFF, CALL)                                      \
void NAME(bool on)                                                                           \
{                                                                                            \
	const QString what = on ? QStringLiteral(TEXT_ON) : QStringLiteral(TEXT_OFF);            \
	if (ClassroomGuardConfig::load().simulation)                                             \
	{                                                                                        \
		GuardLog::write(QStringLiteral("[simulation] would ") + what);                       \
		return;                                                                              \
	}                                                                                        \
	report(what, CALL(on));                                                                  \
}
#else
#define CG_DEVICE_ACTION(NAME, TEXT_ON, TEXT_OFF, CALL)                                      \
void NAME(bool on)                                                                           \
{                                                                                            \
	Q_UNUSED(on)                                                                             \
	GuardLog::write(QStringLiteral("device control is only implemented on Windows"));        \
}
#endif

CG_DEVICE_ACTION(usb,         "block USB storage",     "allow USB storage",   DeviceControl::setUsbStorageBlocked)
CG_DEVICE_ACTION(webcams,     "block webcams",         "allow webcams",       DeviceControl::setWebcamsBlocked)
CG_DEVICE_ACTION(microphones, "mute microphones",      "unmute microphones",  DeviceControl::setMicrophonesMuted)
CG_DEVICE_ACTION(audio,       "mute audio output",     "unmute audio output", DeviceControl::setAudioMuted)

void releasePersistentBlocks()
{
	internet(InternetMode::Allow);
	usb(false);
	webcams(false);
}

void releaseAll()
{
	internet(InternetMode::Allow);
	usb(false);
	webcams(false);
	microphones(false);
	audio(false);
}

} // namespace GuardActions
