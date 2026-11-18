/*
 * ClassroomGuardRuntime.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 * Based on the InternetGuard plugin by prof. Ing. Raffaele Mele (GPL-2.0-or-later).
 *
 * Classroom Guard: what happens on the student computer (Veyon Server): carrying out commands,
 * remembered state, auto-release timers, end of the school day, mute watchdog, notices.
 */

#include "ClassroomGuardPlugin.h"

#include "ClassroomGuardConfig.h"
#include "GuardActions.h"
#include "GuardLog.h"
#include "GuardNotice.h"
#include "FeatureWorkerManagerShim.h"
#include "VeyonServerInterface.h"

#include <QDateTime>
#include <QTime>

#ifdef Q_OS_WIN
#include "DeviceControl.h"
#endif

// Remembers what is switched on. Not in simulation mode: nothing was changed then.
void ClassroomGuardPlugin::updateState(Commands command)
{
	const bool real = !ClassroomGuardConfig::load().simulation;
	switch (command)
	{
	case BlockInternetCommand:      m_internetMode = real ? 1 : 0; break;
	case RestrictInternetCommand:   m_internetMode = real ? 2 : 0; break;
	case AllowInternetCommand:      m_internetMode = 0; break;
	case BlockUsbCommand:           m_usbBlocked = real; break;
	case AllowUsbCommand:           m_usbBlocked = false; break;
	case BlockWebcamCommand:        m_webcamBlocked = real; break;
	case AllowWebcamCommand:        m_webcamBlocked = false; break;
	case MuteMicrophoneCommand:     m_microphoneMuted = real; break;
	case UnmuteMicrophoneCommand:   m_microphoneMuted = false; break;
	case MuteAudioCommand:          m_audioMuted = real; break;
	case UnmuteAudioCommand:        m_audioMuted = false; break;
	case NoticeEndOfDayCommand:     break;
	}
}

void ClassroomGuardPlugin::resetState()
{
	m_internetMode = 0;
	m_usbBlocked = false;
	m_webcamBlocked = false;
	m_microphoneMuted = false;
	m_audioMuted = false;
}

bool ClassroomGuardPlugin::isFeatureActive(VeyonServerInterface& server, Feature::Uid featureUid) const
{
	Q_UNUSED(server)

	for (const FeatureGroup& group : m_groups)
	{
		if (featureUid != group.toggle.uid())
			continue;
		switch (group.blockCommand)
		{
		case BlockInternetCommand:    return m_internetMode == 1;
		case RestrictInternetCommand: return m_internetMode == 2;
		case BlockUsbCommand:         return m_usbBlocked;
		case BlockWebcamCommand:      return m_webcamBlocked;
		case MuteMicrophoneCommand:   return m_microphoneMuted;
		case MuteAudioCommand:        return m_audioMuted;
		default:                      return false;
		}
	}
	return false;
}

void ClassroomGuardPlugin::enforceMute()
{
#ifdef Q_OS_WIN
	if (!m_microphoneMuted && !m_audioMuted)
		return;
	const int again = DeviceControl::reapplyMute(m_microphoneMuted, m_audioMuted);
	const qint64 now = QDateTime::currentSecsSinceEpoch();
	if (again > 0 && now - m_lastMuteLog >= 30)
	{
		m_lastMuteLog = now;
		GuardLog::write(QStringLiteral("mute was lifted on the computer: muted again (%1 endpoint(s))").arg(again));
	}
#endif
}

void ClassroomGuardPlugin::checkEndOfDay()
{
#ifdef Q_OS_WIN
	using namespace ClassroomGuardConfig;
	if (!getBool(Key::EndOfDayEnabled, false))
		return;
	const QTime end = QTime::fromString(getString(Key::EndOfDayTime, QStringLiteral("15:30")).trimmed(),
	                                    QStringLiteral("HH:mm"));
	const QDate today = QDate::currentDate();
	if (!end.isValid() || m_endOfDayDone == today || QTime::currentTime() < end)
		return;

	m_endOfDayDone = today;
	GuardLog::write(QStringLiteral("end of the school day (%1): releasing all restrictions")
	                    .arg(end.toString(QStringLiteral("HH:mm"))));
	GuardActions::releaseAll();
	resetState();
	for (int slot = 0; slot < 5; ++slot)
		stopTimer(slot);

	if (m_server && getBool(Key::ShowStudentNotice, true) && !load().simulation)
	{
		FeatureMessage message(noticeFeatureUid(), NoticeEndOfDayCommand);
		message.addArgument(Argument::NoticeSeconds, qBound(1, getInt(Key::NoticeSeconds, 8), 60));
		m_server->featureWorkerManager().sendMessageToUnmanagedSessionWorker(message);
	}
#endif
}

// Tells the student that a restriction was switched on: the message goes to the
// Worker in the student's session, which shows the notice.
void ClassroomGuardPlugin::notifyStudent(Commands command)
{
	using namespace ClassroomGuardConfig;
	if (!m_server || !getBool(Key::ShowStudentNotice, true) || load().simulation)
		return;

	switch (command)
	{
	case BlockInternetCommand:
	case RestrictInternetCommand:
	case BlockUsbCommand:
	case BlockWebcamCommand:
	case MuteMicrophoneCommand:
	case MuteAudioCommand:
		break;
	default:
		return;   // releases need no notice
	}

	// One fixed, hidden feature UID, so a single Worker process serves all notices.
	FeatureMessage message(noticeFeatureUid(), command);
	message.addArgument(Argument::NoticeSeconds, qBound(1, getInt(Key::NoticeSeconds, 8), 60));
	m_server->featureWorkerManager().sendMessageToUnmanagedSessionWorker(message);
}

void ClassroomGuardPlugin::execute(Commands command, const QStringList& allowedWebsites)
{
	using GuardActions::InternetMode;

	switch (command)
	{
	case BlockInternetCommand:      GuardActions::internet(InternetMode::Block);    armTimer(0);  break;
	case RestrictInternetCommand:   GuardActions::internet(InternetMode::Restrict, allowedWebsites); armTimer(0);  break;
	case AllowInternetCommand:      GuardActions::internet(InternetMode::Allow);    stopTimer(0); break;
	case BlockUsbCommand:           GuardActions::usb(true);                        armTimer(1);  break;
	case AllowUsbCommand:           GuardActions::usb(false);                       stopTimer(1); break;
	case BlockWebcamCommand:        GuardActions::webcams(true);                    armTimer(2);  break;
	case AllowWebcamCommand:        GuardActions::webcams(false);                   stopTimer(2); break;
	case MuteMicrophoneCommand:     GuardActions::microphones(true);                armTimer(3);  break;
	case UnmuteMicrophoneCommand:   GuardActions::microphones(false);               stopTimer(3); break;
	case MuteAudioCommand:          GuardActions::audio(true);                      armTimer(4);  break;
	case UnmuteAudioCommand:        GuardActions::audio(false);                     stopTimer(4); break;
	case NoticeEndOfDayCommand:     break;
	}

	updateState(command);
}

int ClassroomGuardPlugin::timerSlot(Commands command)
{
	switch (command)
	{
	case BlockInternetCommand:
	case RestrictInternetCommand:
	case AllowInternetCommand:      return 0;
	case BlockUsbCommand:
	case AllowUsbCommand:           return 1;
	case BlockWebcamCommand:
	case AllowWebcamCommand:        return 2;
	case MuteMicrophoneCommand:
	case UnmuteMicrophoneCommand:   return 3;
	case MuteAudioCommand:
	case UnmuteAudioCommand:        return 4;
	case NoticeEndOfDayCommand:     return 0;
	}
	return 0;
}

ClassroomGuardPlugin::Commands ClassroomGuardPlugin::releaseCommand(int slot)
{
	switch (slot)
	{
	case 1:  return AllowUsbCommand;
	case 2:  return AllowWebcamCommand;
	case 3:  return UnmuteMicrophoneCommand;
	case 4:  return UnmuteAudioCommand;
	default: return AllowInternetCommand;
	}
}

void ClassroomGuardPlugin::armTimer(int slot)
{
	const int minutes = ClassroomGuardConfig::load().autoReleaseMinutes;
	if (minutes <= 0)
	{
		stopTimer(slot);
		return;
	}

	GuardLog::write(QStringLiteral("auto-release in %1 minute(s)").arg(minutes));
	QTimer* timer = m_timers[static_cast<size_t>(slot)];
	QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, minutes * 60 * 1000));
}

void ClassroomGuardPlugin::stopTimer(int slot)
{
	QMetaObject::invokeMethod(m_timers[static_cast<size_t>(slot)], "stop", Qt::QueuedConnection);
}

