/*
 * ClassroomGuardRestrictDialog.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * Shown in the Veyon Master when the teacher starts "Restrict Internet":
 * asks for the websites that stay reachable on the selected computers.
 */

#pragma once

#include <QDialog>
#include <QStringList>

class QPlainTextEdit;
class QLabel;

class ClassroomGuardRestrictDialog : public QDialog
{
	Q_OBJECT
public:
	explicit ClassroomGuardRestrictDialog(const QString& initialText, int computerCount,
	                                      QWidget* parent = nullptr);

	// The cleaned list entered by the user (ClassroomGuardConfig::normalizeSites).
	QStringList websites() const;

	// Shows the dialog; returns false if the user cancelled. The list is
	// remembered per user for the next time.
	static bool ask(int computerCount, QStringList* websites);

private slots:
	void validateAndAccept();

private:
	QPlainTextEdit* m_edit;
	QLabel* m_error;
};
