/*
 * ClassroomGuardPlugin.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 * Based on the InternetGuard plugin by prof. Ing. Raffaele Mele (GPL-2.0-or-later).
 *
 * Classroom Guard - third-party Veyon plugin, not part of the official Veyon
 * distribution. Blocks or restricts internet access and blocks USB storage,
 * webcams, microphones and audio on student computers.
 */

#pragma once

#include <atomic>
#include <vector>

#include <QDate>
#include <QTimer>

#include "ConfigurationPagePluginInterface.h"
#include "VeyonCompat.h"

class ClassroomGuardPlugin : public QObject, public FeatureProviderInterface, public PluginInterface,
                             public ConfigurationPagePluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.ClassroomGuard")
	Q_INTERFACES(PluginInterface FeatureProviderInterface ConfigurationPagePluginInterface)
public:
	explicit ClassroomGuardPlugin(QObject* parent = nullptr);
	~ClassroomGuardPlugin();

	// The UID is the plugin identity; it is the one of the earlier InternetGuard
	// versions on purpose, so Veyon keeps recognising the plugin.
	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("a4b3c2d1-e5f6-7890-abcd-ef1234567890") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber(2, 0);
	}

	QString name() const override
	{
		return QStringLiteral("ClassroomGuard");
	}

	QString description() const override
	{
		return tr("Classroom Guard: block or restrict internet access, USB storage, webcams, "
		          "microphones and audio on student computers");
	}

	QString vendor() const override
	{
		return QStringLiteral("Andreas Weller");
	}

	QString copyright() const override
	{
		return QStringLiteral("(c) by Andreas Weller, https://www.andreas-weller.de/");
	}

	const FeatureList& featureList() const override
	{
		return m_features;
	}

	bool controlFeature(Feature::Uid featureUid, Operation operation,
	                    const QVariantMap& arguments,
	                    const ComputerControlInterfaceList& computerControlInterfaces) override;

	bool handleFeatureMessage(VeyonServerInterface& server,
	                          const MessageContext& messageContext,
	                          const FeatureMessage& message) override;

	// Service side: tells the Master which restrictions are on (pressed state, icon and
	// tooltip of the computer in the Master).
	bool isFeatureActive(VeyonServerInterface& server, Feature::Uid featureUid) const override;

	// Worker side (runs in the student's session): shows the notice.
	bool handleFeatureMessage(VeyonWorkerInterface& worker, const FeatureMessage& message) override;

	ConfigurationPage* createConfigurationPage() override;

private Q_SLOTS:
	void onStartup();

private:
	// Wire protocol Master -> Server. The values are part of the protocol: the
	// first two are unchanged since InternetGuard 1.0, new commands are only ever
	// appended, so plugins of different versions on teacher and student PCs still
	// understand each other (an old Server ignores commands it does not know).
	enum Commands
	{
		BlockInternetCommand,
		AllowInternetCommand,
		BlockUsbCommand,
		AllowUsbCommand,
		BlockWebcamCommand,
		AllowWebcamCommand,
		MuteMicrophoneCommand,
		UnmuteMicrophoneCommand,
		MuteAudioCommand,
		UnmuteAudioCommand,
		RestrictInternetCommand,
		NoticeEndOfDayCommand   // only sent from the Service to its Worker: "all restrictions lifted"
	};

	// Message arguments (Veyon keys them by number).
	enum class Argument
	{
		AllowedWebsites = 1,  // QString, one site per line: used by RestrictInternetCommand
		NoticeSeconds = 2     // int: how long the notice stays on the student screen
	};

	// One controllable resource: a one-click toggle (Mode) plus explicit
	// per-selection Block / Allow actions.
	struct FeatureGroup
	{
		Feature toggle;
		Feature block;
		Feature allow;
		Commands blockCommand;
		Commands allowCommand;
	};

	static FeatureGroup makeGroup(const char* name,
	                              const char* uidToggle, const char* uidBlock, const char* uidAllow,
	                              const QString& blockLabel, const QString& allowLabel,
	                              const QString& toggleDescription,
	                              const QString& blockDescription, const QString& allowDescription,
	                              const char* iconUrl,
	                              Commands blockCommand, Commands allowCommand);

	static std::vector<FeatureGroup> makeGroups();
	static FeatureList makeFeatureList(const std::vector<FeatureGroup>& groups);
	static Feature::Uid noticeFeatureUid();

	bool isOwnFeature(Feature::Uid featureUid) const;

	// Carries out a command on this computer (server side) and arms or stops the
	// auto-release timer of the affected resource.
	void execute(Commands command, const QStringList& allowedWebsites = QStringList());

	// Auto-release timers, one per resource: 0 internet (block and restrict),
	// 1 USB, 2 webcam, 3 microphone, 4 audio.
	static int timerSlot(Commands command);
	static Commands releaseCommand(int slot);
	void armTimer(int slot);
	void stopTimer(int slot);

	const std::vector<FeatureGroup> m_groups;
	const FeatureList m_features;
	std::vector<QTimer*> m_timers;

	// End of the school day (Service only): checked every 30 seconds.
	void checkEndOfDay();
	void notifyStudent(Commands command);
	QTimer* m_endOfDayTimer = nullptr;

	// Muted audio is enforced: students can unmute in the volume mixer, so the Service mutes
	// again every half second while a mute is active (Service only).
	void enforceMute();
	QTimer* m_muteWatchdog = nullptr;
	qint64 m_lastMuteLog = 0;

	// What is switched on on this computer (Service only). Reported to the Master through
	// isFeatureActive() and used by the mute watchdog. Written by the message handler,
	// read from other threads: atomics.
	void updateState(Commands command);
	void resetState();
	std::atomic<int> m_internetMode{0};     // 0 open, 1 blocked, 2 restricted
	std::atomic<bool> m_usbBlocked{false};
	std::atomic<bool> m_webcamBlocked{false};
	std::atomic<bool> m_microphoneMuted{false};
	std::atomic<bool> m_audioMuted{false};
	QDate m_endOfDayDone;
	VeyonServerInterface* m_server = nullptr;   // known after the first message from the Master
};
