/*
 * ClassroomGuardPlugin.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 * Based on the InternetGuard plugin by prof. Ing. Raffaele Mele (GPL-2.0-or-later).
 *
 * Classroom Guard - third-party Veyon plugin, not part of the official Veyon
 * distribution.
 */

#include "ClassroomGuardPlugin.h"

#include "ClassroomGuardConfig.h"
#include "ClassroomGuardConfigurationPage.h"
#include "ClassroomGuardRestrictDialog.h"
#include "GuardNotice.h"
#include "GuardTranslator.h"

#include <QDateTime>
#include <QTime>

#include "FeatureWorkerManagerShim.h"
#include "VeyonServerInterface.h"
#include "GuardActions.h"
#include "GuardLog.h"

#ifdef Q_OS_WIN
#include "DeviceControl.h"
#include "ProxyRedirect.h"
#include "RoutingBlock.h"
#include "WindowsFirewall.h"
#include <windows.h>
#endif

// -------------------------------------------------------------------------

#ifdef Q_OS_WIN
// True inside the Veyon Server process (the one that carries out the commands on
// a student computer). Only there the plugin may undo blocks on load/unload: the
// Master (teacher), the Configurator and the command line load the plugin as
// well and must not touch the local devices or audio.
static bool isVeyonServerProcess()
{
	wchar_t path[MAX_PATH] = {};
	const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
	if (n == 0 || n >= MAX_PATH)
		return false;

	const wchar_t* name = path;
	for (const wchar_t* p = path; *p; ++p)
		if (*p == L'\\' || *p == L'/')
			name = p + 1;
	return _wcsicmp(name, L"veyon-server.exe") == 0;
}

// The Worker runs in the student's session and only shows notices. It must never
// undo blocks when it exits.
static bool isVeyonWorkerProcess()
{
	wchar_t path[MAX_PATH] = {};
	const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
	if (n == 0 || n >= MAX_PATH)
		return false;

	const wchar_t* name = path;
	for (const wchar_t* p = path; *p; ++p)
		if (*p == L'\\' || *p == L'/')
			name = p + 1;
	return _wcsicmp(name, L"veyon-worker.exe") == 0;
}
#endif

ClassroomGuardPlugin::ClassroomGuardPlugin(QObject* parent) :
	QObject(parent),
	m_groups((GuardTranslator::installIfGerman(), makeGroups())),
	m_features(makeFeatureList(m_groups))
{
	// One single-shot timer per resource. Created here, so they live in the
	// plugin's thread; they are started and stopped through queued calls.
	for (int slot = 0; slot < 5; ++slot)
	{
		auto* timer = new QTimer(this);
		timer->setSingleShot(true);
		connect(timer, &QTimer::timeout, this, [this, slot]()
		{
			GuardLog::write(QStringLiteral("auto-release time is up"));
			execute(releaseCommand(slot));
		});
		m_timers.push_back(timer);
	}

	// A block that survives a reboot must not outlive a crashed or restarted
	// Server. Done from the event loop: the Veyon configuration is surely there by then.
	QMetaObject::invokeMethod(this, "onStartup", Qt::QueuedConnection);
}

void ClassroomGuardPlugin::onStartup()
{
#ifdef Q_OS_WIN
	if (isVeyonServerProcess()
	    && ClassroomGuardConfig::getBool(ClassroomGuardConfig::Key::UnblockOnServiceStart, true))
	{
		GuardLog::write(QStringLiteral("Veyon Server started: releasing blocks left over from before"));
		GuardActions::releasePersistentBlocks();
	}

	// End of the school day: only the Service releases the restrictions.
	if (isVeyonServerProcess())
	{
		m_endOfDayTimer = new QTimer(this);
		connect(m_endOfDayTimer, &QTimer::timeout, this, [this]() { checkEndOfDay(); });
		m_endOfDayTimer->start(30 * 1000);
		checkEndOfDay();

		m_muteWatchdog = new QTimer(this);
		connect(m_muteWatchdog, &QTimer::timeout, this, [this]() { enforceMute(); });
		m_muteWatchdog->start(500);
		GuardLog::write(QStringLiteral("Classroom Guard started in the Veyon Server (mute watchdog and end-of-day check running)"));
	}
#endif
}

bool ClassroomGuardPlugin::handleFeatureMessage(VeyonWorkerInterface& worker, const FeatureMessage& message)
{
	Q_UNUSED(worker)

	if (!isOwnFeature(message.featureUid()))
		return false;

	QString text;
	switch (VEYON_DECODE_COMMAND(message, Commands))
	{
	case BlockInternetCommand:    text = tr("Internet access has been blocked by your teacher."); break;
	case RestrictInternetCommand: text = tr("Internet access is restricted to the websites your teacher has allowed."); break;
	case BlockUsbCommand:         text = tr("USB drives have been blocked by your teacher."); break;
	case BlockWebcamCommand:      text = tr("Webcams have been blocked by your teacher."); break;
	case MuteMicrophoneCommand:   text = tr("The microphone has been muted by your teacher."); break;
	case MuteAudioCommand:        text = tr("The audio output has been muted by your teacher."); break;
	case NoticeEndOfDayCommand:   text = tr("All restrictions have been lifted (end of the school day)."); break;
	default: return true;
	}

	GuardNotice::show(text, qBound(1, message.argument(Argument::NoticeSeconds).toInt(), 60));
	return true;
}

// Restore access on unload so stale blocks are never left behind. This must not
// read the Veyon configuration (it may already be gone): it undoes through the
// saved state only. Inside the Server everything persistent is released;
// elsewhere (Master, Configurator, CLI) only internet blocks, as in earlier versions.
ClassroomGuardPlugin::~ClassroomGuardPlugin()
{
#ifdef Q_OS_WIN
	if (isVeyonWorkerProcess())
		return;
	WindowsFirewall::allowInternet();
	ProxyRedirect::disable();
	RoutingBlock::unblock();
	if (isVeyonServerProcess())
	{
		DeviceControl::setUsbStorageBlocked(false);
		DeviceControl::setWebcamsBlocked(false);
	}
#endif
}

ConfigurationPage* ClassroomGuardPlugin::createConfigurationPage()
{
	return new ClassroomGuardConfigurationPage();
}

bool ClassroomGuardPlugin::isOwnFeature(Feature::Uid featureUid) const
{
	if (featureUid == noticeFeatureUid())
		return true;
	for (const FeatureGroup& group : m_groups)
	{
		if (featureUid == group.toggle.uid()
		    || featureUid == group.block.uid()
		    || featureUid == group.allow.uid())
			return true;
	}
	return false;
}

bool ClassroomGuardPlugin::controlFeature(Feature::Uid featureUid,
                                          Operation operation,
                                          const QVariantMap& arguments,
                                          const ComputerControlInterfaceList& computerControlInterfaces)
{
	// Restrictions are only lifted by the explicit "Allow" actions, the auto-release time and the
	// end of the school day. Veyon sends Stop to every feature whenever another mode starts
	// (Demo, Lock screen...) and when the Master closes; that must not lift anything.
	if (operation != Operation::Start)
		return false;

	// "Restrict Internet" needs the list of allowed websites. The teacher enters it here in the
	// Master; callers that already pass one (scripts, the CLI) skip the dialog. The key is a
	// plain name in this map, not the message argument.
	const auto send = [&](Feature::Uid uid, Commands command) {
		FeatureMessage message(uid, command);
		if (command == RestrictInternetCommand)
		{
			QString sites = arguments.value(QStringLiteral("AllowedWebsites")).toString();
			if (sites.trimmed().isEmpty())
			{
				QStringList entered;
				if (!ClassroomGuardRestrictDialog::ask(int(computerControlInterfaces.size()), &entered))
					return;   // cancelled: nothing is sent
				sites = entered.join(QLatin1Char('\n'));
			}
			message.addArgument(Argument::AllowedWebsites, sites);
		}
		sendFeatureMessage(message, computerControlInterfaces);
	};

	for (const FeatureGroup& group : m_groups)
	{
		// The group's own feature (command line, scripts): switches between block and allow
		// depending on whether it is on for all given computers.
		if (featureUid == group.toggle.uid())
		{
			bool allOn = !computerControlInterfaces.isEmpty();
			for (const auto& controlInterface : computerControlInterfaces)
				if (!controlInterface->activeFeatures().contains(featureUid))
					allOn = false;
			send(featureUid, allOn ? group.allowCommand : group.blockCommand);
			return true;
		}

		// The explicit actions in the toolbar menu and the right-click menu.
		if (featureUid == group.block.uid())
		{
			send(featureUid, group.blockCommand);
			return true;
		}
		if (featureUid == group.allow.uid())
		{
			send(featureUid, group.allowCommand);
			return true;
		}
	}

	return false;
}

bool ClassroomGuardPlugin::handleFeatureMessage(VeyonServerInterface& server,
                                                const MessageContext& messageContext,
                                                const FeatureMessage& message)
{
	Q_UNUSED(messageContext)

	if (!isOwnFeature(message.featureUid()))
		return false;

	m_server = &server;

	// Commands are dispatched by code, independent of which feature UID carried them.
	const Commands command = VEYON_DECODE_COMMAND(message, Commands);
	GuardLog::write(QStringLiteral("command %1 received (feature %2)")
	                    .arg(int(command)).arg(message.featureUid().toString()));
	switch (command)
	{
	case RestrictInternetCommand:
		execute(command, message.hasArgument(Argument::AllowedWebsites)
		                     ? ClassroomGuardConfig::normalizeSites(
		                           message.argument(Argument::AllowedWebsites).toString())
		                     : QStringList());
		notifyStudent(command);
		return true;
	case NoticeEndOfDayCommand:
		return true;   // only ever sent from the Service to its Worker
	case BlockInternetCommand:
	case AllowInternetCommand:
	case BlockUsbCommand:
	case AllowUsbCommand:
	case BlockWebcamCommand:
	case AllowWebcamCommand:
	case MuteMicrophoneCommand:
	case UnmuteMicrophoneCommand:
	case MuteAudioCommand:
	case UnmuteAudioCommand:
		execute(command);
		notifyStudent(command);
		return true;
	}

	return false;
}
