/*
 * ClassroomGuardRestrictDialog.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 */

#include "ClassroomGuardRestrictDialog.h"
#include "ClassroomGuardConfig.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace
{
const QString kSettingsOrg = QStringLiteral("ClassroomGuard");
const QString kSettingsApp = QStringLiteral("Master");
const QString kLastList = QStringLiteral("LastAllowedWebsites");
}

ClassroomGuardRestrictDialog::ClassroomGuardRestrictDialog(const QString& initialText, int computerCount,
                                                           QWidget* parent) :
	QDialog(parent),
	m_edit(new QPlainTextEdit(initialText)),
	m_error(new QLabel)
{
	setWindowTitle(tr("Restrict Internet"));
	setMinimumWidth(420);

	auto* layout = new QVBoxLayout(this);
	layout->addWidget(new QLabel(
	    tr("Websites that stay reachable on the %n selected computer(s), one per line.\n"
	       "Everything else is blocked. Pages of the local network stay reachable.", nullptr, computerCount)));

	m_edit->setPlaceholderText(tr("docs.veyon.io\nwww.wikipedia.org"));
	m_edit->setMinimumHeight(140);
	layout->addWidget(m_edit);

	m_error->setStyleSheet(QStringLiteral("color: #b00020;"));
	m_error->hide();
	layout->addWidget(m_error);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	buttons->button(QDialogButtonBox::Ok)->setText(tr("Restrict"));
	connect(buttons, &QDialogButtonBox::accepted, this, &ClassroomGuardRestrictDialog::validateAndAccept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);
}

QStringList ClassroomGuardRestrictDialog::websites() const
{
	return ClassroomGuardConfig::normalizeSites(m_edit->toPlainText());
}

void ClassroomGuardRestrictDialog::validateAndAccept()
{
	if (websites().isEmpty())
	{
		m_error->setText(tr("Enter at least one website."));
		m_error->show();
		return;
	}
	accept();
}

bool ClassroomGuardRestrictDialog::ask(int computerCount, QStringList* websites)
{
	QSettings settings(kSettingsOrg, kSettingsApp);
	QString initial = settings.value(kLastList).toString();
	if (initial.trimmed().isEmpty())
		initial = ClassroomGuardConfig::getString(ClassroomGuardConfig::Key::AllowedWebsites);

	ClassroomGuardRestrictDialog dialog(initial, computerCount, QApplication::activeWindow());
	if (dialog.exec() != QDialog::Accepted)
		return false;

	*websites = dialog.websites();
	settings.setValue(kLastList, websites->join(QLatin1Char('\n')));
	return true;
}
