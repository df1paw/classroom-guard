/*
 * ClassroomGuardConfigurationPage.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 */

#include "ClassroomGuardConfigurationPage.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimeEdit>
#include <QUrl>
#include <QVBoxLayout>

#include "ClassroomGuardConfig.h"
#include "GuardLog.h"

namespace Key = ClassroomGuardConfig::Key;

ClassroomGuardConfigurationPage::ClassroomGuardConfigurationPage(QWidget* parent) :
	ConfigurationPage(parent)
{
	setWindowTitle(tr("Classroom Guard"));
	setWindowIcon(QIcon(QStringLiteral(":/classroom-guard/network-offline.png")));

	// The page is long: put everything into a scroll area.
	auto* outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	auto* scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	outer->addWidget(scroll);

	auto* content = new QWidget;
	scroll->setWidget(content);
	auto* page = new QVBoxLayout(content);

	auto* title = new QLabel(tr("<b>Classroom Guard</b> - block or restrict internet access, USB drives, "
	                            "webcams, microphones and audio on the student computers"));
	title->setWordWrap(true);
	page->addWidget(title);

	// --- Control (this computer) ---------------------------------------------------
	{
		auto* group = new QGroupBox(tr("Control"));
		auto* box = new QVBoxLayout(group);

		addCheck(box, tr("Simulation mode: only write to the log, change nothing "
		                 "(applies to the student computers and to the buttons below)"),
		         Key::Simulation, false);

		m_state = new QLabel(tr("Current state: not changed from here"));
		box->addWidget(m_state);

		addText(box, tr("Allowed websites (one per line; used by \"Restrict\"):"),
		        tr("e.g. docs.veyon.io"), Key::AllowedWebsites, 70);

		auto* buttons = new QHBoxLayout;
		auto* restrictButton = new QPushButton(tr("Restrict internet access"));
		auto* blockButton = new QPushButton(tr("Block internet access"));
		auto* restoreButton = new QPushButton(tr("Restore internet access"));
		buttons->addWidget(restrictButton);
		buttons->addWidget(blockButton);
		buttons->addWidget(restoreButton);
		buttons->addStretch();
		box->addLayout(buttons);

		auto* hint = new QLabel(tr("These buttons act on <b>this</b> computer (administrator rights needed) "
		                           "and are meant for testing. On the student computers use the Classroom "
		                           "Guard buttons in the Veyon Master."));
		hint->setWordWrap(true);
		box->addWidget(hint);

		connect(restrictButton, &QPushButton::clicked, this,
		        [this]() { runLocal(GuardActions::InternetMode::Restrict, tr("restricted")); });
		connect(blockButton, &QPushButton::clicked, this,
		        [this]() { runLocal(GuardActions::InternetMode::Block, tr("blocked")); });
		connect(restoreButton, &QPushButton::clicked, this,
		        [this]() { runLocal(GuardActions::InternetMode::Allow, tr("open")); });
		page->addWidget(group);
	}

	// --- General settings -----------------------------------------------------------------
	{
		auto* group = new QGroupBox(tr("General settings"));
		auto* box = new QVBoxLayout(group);

		box->addWidget(new QLabel(tr("Methods used by \"Block / Restrict internet access\" "
		                             "(several can be combined):")));
		m_useFirewall = addCheck(box, tr("Windows Firewall (outbound block rules)"), Key::UseFirewall, true);
		m_useProxy = addCheck(box, tr("Proxy redirection (browsers that use the system proxy)"),
		                      Key::UseProxy, false);
		m_proxyAddress = addLine(box, tr("Proxy address:"), tr("host:port, e.g. 0.0.0.0:8080"),
		                         Key::ProxyAddress);
		m_useRouting = addCheck(box, tr("Routing table (remove the default routes)"), Key::UseRouting, false);

		addSpin(box, tr("Release automatically after:"), tr(" minutes (0 = never)"),
		        Key::AutoReleaseMinutes, 600);
		addCheck(box, tr("Unblock on service start (a block never survives a restart of the Veyon Server)"),
		         Key::UnblockOnServiceStart, true);
		page->addWidget(group);
	}

	// --- Firewall settings ------------------------------------------------------------------
	{
		auto* group = new QGroupBox(tr("Firewall settings"));
		auto* box = new QVBoxLayout(group);

		addCheck(box, tr("Block only traffic to non-local networks (the local network, the domain "
		                 "controller and the teacher computer stay reachable)"),
		         Key::OnlyNonLocal, true);
		m_blockAllPorts = addCheck(box, tr("Block all outbound traffic (all TCP and UDP ports)"),
		                           Key::BlockAllPorts, false);
		m_blockWeb = addCheck(box, tr("HTTP/HTTPS (80, 443, 8080, 8443, 3128, QUIC)"), Key::BlockWeb, true);
		m_blockFtp = addCheck(box, tr("FTP (21)"), Key::BlockFtp, true);
		m_blockDns = addCheck(box, tr("DNS toward other servers (53, 853)"), Key::BlockDns, true);
		m_customPorts = addLine(box, tr("Custom ports:"), tr("e.g. 8080 9000-9100"), Key::CustomPorts);

		box->addWidget(new QLabel(tr("<b>Exceptions</b> - stay reachable although everything else is blocked:")));
		addLine(box, tr("Ports:"), tr("e.g. 80 443"), Key::ExceptionPorts);
		addText(box, tr("Networks (one per line):"), tr("e.g. 192.168.1.0/24 or 10.0.0.1-10.0.0.9"),
		        Key::ExceptionNetworks, 55);
		addText(box, tr("Hosts (one per line):"), tr("e.g. docs.veyon.io (resolved when the block starts)"),
		        Key::ExceptionHosts, 55);
		addCheck(box, tr("Always let the DNS servers of this computer through"), Key::ExemptDnsServers, false);

		m_blockServers = addCheck(box, tr("Block traffic to these servers completely, "
		                                  "even if they are local (e.g. an internal proxy):"),
		                          Key::BlockServers, false);
		m_serverAddresses = addText(box, tr("Server addresses (one per line):"),
		                            tr("e.g. 192.168.1.253"), Key::ServerAddresses, 55);

		m_usePresetRule = addCheck(box, tr("Switch on a preconfigured firewall rule while blocked"),
		                           Key::UsePresetRule, false);
		m_presetRuleName = addLine(box, tr("Rule name:"), tr("name of an existing, disabled rule"),
		                           Key::PresetRuleName);
		page->addWidget(group);
	}

	// --- School day and notices -------------------------------------------------------------------
	{
		auto* group = new QGroupBox(tr("School day and notices"));
		auto* box = new QVBoxLayout(group);

		m_showNotice = addCheck(box, tr("Show a notice on the student computers when a restriction is switched on"),
		                        Key::ShowStudentNotice, true);
		m_noticeSeconds = addSpin(box, tr("Show the notice for:"), tr(" seconds"), Key::NoticeSeconds, 60, 8);

		m_endOfDay = addCheck(box, tr("Lift all restrictions at the end of the school day"),
		                      Key::EndOfDayEnabled, false);
		m_endOfDayTime = addTime(box, tr("End of the school day:"), Key::EndOfDayTime, QStringLiteral("15:30"));
		auto* hint = new QLabel(tr("At this time every day internet, USB drives, webcams, microphones and audio "
		                           "are released on the student computers. Restrictions switched on later the "
		                           "same day stay."));
		hint->setWordWrap(true);
		box->addWidget(hint);
		page->addWidget(group);
	}

	// --- Log ---------------------------------------------------------------------------------------
	{
		auto* group = new QGroupBox(tr("Log"));
		auto* box = new QVBoxLayout(group);
		m_log = new QPlainTextEdit;
		m_log->setReadOnly(true);
		m_log->setMinimumHeight(120);
		box->addWidget(m_log);

		auto* row = new QHBoxLayout;
		auto* refresh = new QPushButton(tr("Refresh"));
		auto* openFolder = new QPushButton(tr("Open log folder"));
		row->addWidget(refresh);
		row->addWidget(openFolder);
		row->addStretch();
		box->addLayout(row);
		box->addWidget(new QLabel(tr("Log file of this computer: %1").arg(GuardLog::path())));

		connect(refresh, &QPushButton::clicked, this, [this]() { refreshLog(); });
		connect(openFolder, &QPushButton::clicked, this, []()
		{
			QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(GuardLog::path()).absolutePath()));
		});
		page->addWidget(group);
	}

	auto* copyright = new QLabel(QStringLiteral("(c) by <a href=\"https://www.andreas-weller.de/\">Andreas Weller</a>"));
	copyright->setTextFormat(Qt::RichText);
	copyright->setTextInteractionFlags(Qt::TextBrowserInteraction);
	copyright->setOpenExternalLinks(true);
	page->addWidget(copyright);
	page->addStretch();

	resetWidgets();
}

QCheckBox* ClassroomGuardConfigurationPage::addCheck(QLayout* layout, const QString& text,
                                                     const char* key, bool defaultValue)
{
	auto* box = new QCheckBox(text);
	layout->addWidget(box);
	m_loaders.push_back([box, key, defaultValue]()
	{
		const QSignalBlocker blocker(box);
		box->setChecked(ClassroomGuardConfig::getBool(key, defaultValue));
	});
	connect(box, &QCheckBox::toggled, this, [this, key](bool on)
	{
		ClassroomGuardConfig::setBool(key, on);
		changed();
	});
	return box;
}

QLineEdit* ClassroomGuardConfigurationPage::addLine(QLayout* layout, const QString& label,
                                                    const QString& placeholder, const char* key)
{
	auto* row = new QHBoxLayout;
	row->addWidget(new QLabel(label));
	auto* edit = new QLineEdit;
	edit->setPlaceholderText(placeholder);
	row->addWidget(edit, 1);
	static_cast<QBoxLayout*>(layout)->addLayout(row);
	m_loaders.push_back([edit, key]()
	{
		const QSignalBlocker blocker(edit);
		edit->setText(ClassroomGuardConfig::getString(key));
	});
	connect(edit, &QLineEdit::textChanged, this, [this, key](const QString& text)
	{
		ClassroomGuardConfig::setString(key, text);
		changed();
	});
	return edit;
}

QPlainTextEdit* ClassroomGuardConfigurationPage::addText(QLayout* layout, const QString& label,
                                                         const QString& placeholder, const char* key,
                                                         int height)
{
	auto* box = static_cast<QBoxLayout*>(layout);
	box->addWidget(new QLabel(label));
	auto* edit = new QPlainTextEdit;
	edit->setPlaceholderText(placeholder);
	edit->setFixedHeight(height);
	box->addWidget(edit);
	m_loaders.push_back([edit, key]()
	{
		const QSignalBlocker blocker(edit);
		edit->setPlainText(ClassroomGuardConfig::getString(key));
	});
	connect(edit, &QPlainTextEdit::textChanged, this, [this, edit, key]()
	{
		ClassroomGuardConfig::setString(key, edit->toPlainText());
		changed();
	});
	return edit;
}

QTimeEdit* ClassroomGuardConfigurationPage::addTime(QLayout* layout, const QString& label,
                                                    const char* key, const QString& defaultValue)
{
	auto* row = new QHBoxLayout;
	row->addWidget(new QLabel(label));
	auto* edit = new QTimeEdit;
	edit->setDisplayFormat(QStringLiteral("HH:mm"));
	row->addWidget(edit);
	row->addStretch();
	static_cast<QBoxLayout*>(layout)->addLayout(row);
	m_loaders.push_back([edit, key, defaultValue]()
	{
		const QSignalBlocker blocker(edit);
		QTime t = QTime::fromString(ClassroomGuardConfig::getString(key, defaultValue), QStringLiteral("HH:mm"));
		edit->setTime(t.isValid() ? t : QTime::fromString(defaultValue, QStringLiteral("HH:mm")));
	});
	connect(edit, &QTimeEdit::timeChanged, this, [this, key](const QTime& t)
	{
		ClassroomGuardConfig::setString(key, t.toString(QStringLiteral("HH:mm")));
		changed();
	});
	return edit;
}

QSpinBox* ClassroomGuardConfigurationPage::addSpin(QLayout* layout, const QString& label,
                                                   const QString& suffix, const char* key, int maximum,
                                                   int defaultValue)
{
	auto* row = new QHBoxLayout;
	row->addWidget(new QLabel(label));
	auto* spin = new QSpinBox;
	spin->setRange(0, maximum);
	spin->setSuffix(suffix);
	row->addWidget(spin);
	row->addStretch();
	static_cast<QBoxLayout*>(layout)->addLayout(row);
	m_loaders.push_back([spin, key, defaultValue]()
	{
		const QSignalBlocker blocker(spin);
		spin->setValue(ClassroomGuardConfig::getInt(key, defaultValue));
	});
	connect(spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this, key](int v)
	{
		ClassroomGuardConfig::setInt(key, v);
		changed();
	});
	return spin;
}

void ClassroomGuardConfigurationPage::changed()
{
	updateEnabled();
	Q_EMIT widgetsChanged();
}

void ClassroomGuardConfigurationPage::updateEnabled()
{
	if (!m_proxyAddress)
		return;
	m_proxyAddress->setEnabled(m_useProxy->isChecked());

	const bool ports = !m_blockAllPorts->isChecked();
	m_blockWeb->setEnabled(ports);
	m_blockFtp->setEnabled(ports);
	m_blockDns->setEnabled(ports);
	m_customPorts->setEnabled(ports);

	m_serverAddresses->setEnabled(m_blockServers->isChecked());
	m_presetRuleName->setEnabled(m_usePresetRule->isChecked());
	m_noticeSeconds->setEnabled(m_showNotice->isChecked());
	m_endOfDayTime->setEnabled(m_endOfDay->isChecked());
}

void ClassroomGuardConfigurationPage::resetWidgets()
{
	for (const auto& load : m_loaders)
		load();
	updateEnabled();
}

void ClassroomGuardConfigurationPage::connectWidgetsToProperties()
{
	// The widgets write their values themselves (see add*), nothing to connect here.
	updateEnabled();
}

void ClassroomGuardConfigurationPage::applyConfiguration()
{
	// The settings are part of the Veyon configuration the Configurator saves.
}

void ClassroomGuardConfigurationPage::showEvent(QShowEvent* event)
{
	ConfigurationPage::showEvent(event);
	refreshLog();
}

void ClassroomGuardConfigurationPage::refreshLog()
{
	if (!m_log)
		return;
	m_log->setPlainText(GuardLog::tail(200));
	m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void ClassroomGuardConfigurationPage::runLocal(GuardActions::InternetMode mode, const QString& stateText)
{
	GuardActions::internet(mode);
	const bool simulation = ClassroomGuardConfig::getBool(Key::Simulation, false);
	m_state->setText(simulation ? tr("Current state: unchanged (simulation mode)")
	                            : tr("Current state: %1").arg(stateText));
	refreshLog();
}
