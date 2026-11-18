/*
 * ClassroomGuardConfig.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 */

#include "ClassroomGuardConfig.h"

#include <QRegularExpression>

// VeyonConfiguration.h is deliberately not included: it pulls in QtCrypto (QCA),
// which would make the QCA headers a build requirement of this plugin. Only the
// generic Configuration::Object interface is needed, and VeyonConfiguration is
// derived from it as its one and only base class.
#include "Configuration/Object.h"
#include "VeyonCore.h"

namespace
{

Configuration::Object& veyonConfig()
{
	return reinterpret_cast<Configuration::Object&>(VeyonCore::config());
}

const QString kGroup = QStringLiteral("ClassroomGuard");

QVariant raw(const char* key, const QVariant& def)
{
	return veyonConfig().value(QString::fromLatin1(key), kGroup, def);
}

void store(const char* key, const QVariant& value)
{
	veyonConfig().setValue(QString::fromLatin1(key), value, kGroup);
}

std::vector<std::wstring> wideList(const char* key)
{
	std::vector<std::wstring> out;
	for (const QString& entry : ClassroomGuardConfig::splitList(ClassroomGuardConfig::getString(key)))
		out.push_back(entry.toStdWString());
	return out;
}

} // namespace

namespace ClassroomGuardConfig
{

bool getBool(const char* key, bool defaultValue)
{
	return raw(key, defaultValue).toBool();
}

int getInt(const char* key, int defaultValue)
{
	bool ok = false;
	const int v = raw(key, defaultValue).toInt(&ok);
	return ok ? v : defaultValue;
}

QString getString(const char* key, const QString& defaultValue)
{
	return raw(key, defaultValue).toString();
}

void setBool(const char* key, bool value) { store(key, value); }
void setInt(const char* key, int value) { store(key, value); }
void setString(const char* key, const QString& value) { store(key, value); }

QStringList splitList(const QString& text)
{
	QStringList out;
	QString token;
	for (const QChar c : text)
	{
		if (c == QLatin1Char('\n') || c == QLatin1Char('\r') || c == QLatin1Char(';')
		    || c == QLatin1Char(',') || c == QLatin1Char(' ') || c == QLatin1Char('\t'))
		{
			if (!token.isEmpty())
				out.append(token);
			token.clear();
		}
		else
			token += c;
	}
	if (!token.isEmpty())
		out.append(token);
	return out;
}

QStringList normalizeSites(const QString& text)
{
	QStringList out;
	for (QString entry : splitList(text))
	{
		const int scheme = entry.indexOf(QStringLiteral("://"));
		if (scheme >= 0)
			entry = entry.mid(scheme + 3);
		const int slash = entry.indexOf(QLatin1Char('/'));
		if (slash > 0)
		{
			// Keep "10.0.0.0/24"-style network entries, cut paths off host names.
			const QString head = entry.left(slash);
			static const QRegularExpression address(
			    QStringLiteral("^(?:[0-9]{1,3}(?:\\.[0-9]{1,3}){3}|[0-9a-fA-F:]*:[0-9a-fA-F:]*)$"));
			static const QRegularExpression prefix(QStringLiteral("^[0-9]{1,3}$"));
			if (!(address.match(head).hasMatch() && prefix.match(entry.mid(slash + 1)).hasMatch()))
				entry = head;
		}
		entry = entry.trimmed();
		if (entry.isEmpty() || entry.size() > 253 || out.contains(entry, Qt::CaseInsensitive))
			continue;
		out.append(entry);
		if (out.size() >= 200)
			break;
	}
	return out;
}

GuardConfig load()
{
	GuardConfig d;   // defaults
	GuardConfig c;

	c.simulation            = getBool(Key::Simulation, d.simulation);
	c.autoReleaseMinutes    = qMax(0, getInt(Key::AutoReleaseMinutes, d.autoReleaseMinutes));
	c.unblockOnServiceStart = getBool(Key::UnblockOnServiceStart, d.unblockOnServiceStart);

	c.useFirewall = getBool(Key::UseFirewall, d.useFirewall);
	c.useProxy    = getBool(Key::UseProxy, d.useProxy);
	c.useRouting  = getBool(Key::UseRouting, d.useRouting);
	if (!c.useFirewall && !c.useProxy && !c.useRouting)
		c.useFirewall = true;          // a block without any method would do nothing
	c.proxyAddress = getString(Key::ProxyAddress, QString::fromStdWString(d.proxyAddress)).trimmed().toStdWString();
	if (c.proxyAddress.empty())
		c.proxyAddress = d.proxyAddress;

	c.onlyNonLocal  = getBool(Key::OnlyNonLocal, d.onlyNonLocal);
	c.blockAllPorts = getBool(Key::BlockAllPorts, d.blockAllPorts);
	c.blockWeb      = getBool(Key::BlockWeb, d.blockWeb);
	c.blockFtp      = getBool(Key::BlockFtp, d.blockFtp);
	c.blockDns      = getBool(Key::BlockDns, d.blockDns);
	c.customPorts   = getString(Key::CustomPorts).toStdWString();

	c.exceptionPorts    = getString(Key::ExceptionPorts).toStdWString();
	c.exceptionNetworks = wideList(Key::ExceptionNetworks);
	c.exceptionHosts    = wideList(Key::ExceptionHosts);
	c.exemptDnsServers  = getBool(Key::ExemptDnsServers, d.exemptDnsServers);

	c.blockServers    = getBool(Key::BlockServers, d.blockServers);
	c.serverAddresses = wideList(Key::ServerAddresses);

	c.usePresetRule  = getBool(Key::UsePresetRule, d.usePresetRule);
	c.presetRuleName = getString(Key::PresetRuleName).trimmed().toStdWString();

	for (const QString& site : normalizeSites(getString(Key::AllowedWebsites)))
		c.allowedWebsites.push_back(site.toStdWString());
	return c;
}

} // namespace ClassroomGuardConfig
