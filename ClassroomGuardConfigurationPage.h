/*
 * ClassroomGuardConfigurationPage.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * The "Classroom Guard" page of the Veyon Configurator: general settings,
 * firewall settings, allowed websites, local test buttons and the log.
 */

#pragma once

#include <functional>
#include <vector>

#include "ConfigurationPage.h"
#include "GuardActions.h"

class QCheckBox;
class QLabel;
class QLayout;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTimeEdit;
class QWidget;

class ClassroomGuardConfigurationPage : public ConfigurationPage
{
	Q_OBJECT
public:
	explicit ClassroomGuardConfigurationPage(QWidget* parent = nullptr);

	void resetWidgets() override;
	void connectWidgetsToProperties() override;
	void applyConfiguration() override;

protected:
	void showEvent(QShowEvent* event) override;

private:
	QCheckBox* addCheck(QLayout* layout, const QString& text, const char* key, bool defaultValue);
	QLineEdit* addLine(QLayout* layout, const QString& label, const QString& placeholder, const char* key);
	QPlainTextEdit* addText(QLayout* layout, const QString& label, const QString& placeholder,
	                        const char* key, int height);
	QSpinBox* addSpin(QLayout* layout, const QString& label, const QString& suffix, const char* key,
	                  int maximum, int defaultValue = 0);

	QTimeEdit* addTime(QLayout* layout, const QString& label, const char* key, const QString& defaultValue);

	void changed();
	void updateEnabled();
	void refreshLog();
	void runLocal(GuardActions::InternetMode mode, const QString& stateText);

	std::vector<std::function<void()>> m_loaders;

	QCheckBox* m_useFirewall = nullptr;
	QCheckBox* m_useProxy = nullptr;
	QCheckBox* m_useRouting = nullptr;
	QLineEdit* m_proxyAddress = nullptr;

	QCheckBox* m_blockAllPorts = nullptr;
	QCheckBox* m_blockWeb = nullptr;
	QCheckBox* m_blockFtp = nullptr;
	QCheckBox* m_blockDns = nullptr;
	QLineEdit* m_customPorts = nullptr;

	QCheckBox* m_blockServers = nullptr;
	QPlainTextEdit* m_serverAddresses = nullptr;
	QCheckBox* m_usePresetRule = nullptr;
	QLineEdit* m_presetRuleName = nullptr;

	QCheckBox* m_showNotice = nullptr;
	QSpinBox* m_noticeSeconds = nullptr;
	QCheckBox* m_endOfDay = nullptr;
	QTimeEdit* m_endOfDayTime = nullptr;

	QLabel* m_state = nullptr;
	QPlainTextEdit* m_log = nullptr;
};
