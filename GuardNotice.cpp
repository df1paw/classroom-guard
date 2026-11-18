/*
 * GuardNotice.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 */

#include "GuardNotice.h"

#include <QApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace
{

class NoticeWidget : public QWidget
{
public:
	NoticeWidget(const QString& text, int seconds) :
		QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
		                     | Qt::WindowDoesNotAcceptFocus)
	{
		setAttribute(Qt::WA_DeleteOnClose);
		setAttribute(Qt::WA_ShowWithoutActivating);
		setStyleSheet(QStringLiteral(
		    "NoticeWidget { background: #ffffff; border: 2px solid #198cb3; border-radius: 8px; }"
		    "QLabel#title { color: #198cb3; font-weight: bold; }"
		    "QLabel#text { color: #202020; }"));
		setObjectName(QStringLiteral("NoticeWidget"));
		setAttribute(Qt::WA_StyledBackground, true);

		auto* icon = new QLabel;
		icon->setPixmap(QPixmap(QStringLiteral(":/classroom-guard/network-offline.png"))
		                    .scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		icon->setAlignment(Qt::AlignTop);

		auto* title = new QLabel(QCoreApplication::translate("ClassroomGuardPlugin", "Classroom Guard"));
		title->setObjectName(QStringLiteral("title"));
		auto* body = new QLabel(text);
		body->setObjectName(QStringLiteral("text"));
		body->setWordWrap(true);
		body->setMinimumWidth(260);
		body->setMaximumWidth(340);

		auto* column = new QVBoxLayout;
		column->addWidget(title);
		column->addWidget(body);

		auto* row = new QHBoxLayout(this);
		row->setContentsMargins(14, 12, 14, 12);
		row->setSpacing(12);
		row->addWidget(icon);
		row->addLayout(column);

		adjustSize();
		if (const QScreen* screen = QGuiApplication::primaryScreen())
		{
			const QRect area = screen->availableGeometry();
			move(area.right() - width() - 24, area.bottom() - height() - 24);
		}

		QTimer::singleShot(qMax(1, seconds) * 1000, this, &QWidget::close);
	}

protected:
	void mousePressEvent(QMouseEvent*) override { close(); }
};

QPointer<NoticeWidget> current;

} // namespace

namespace GuardNotice
{

void show(const QString& text, int seconds)
{
	if (!QApplication::instance())
		return;
	if (current)
		current->close();
	current = new NoticeWidget(text, seconds);
	current->show();
	current->raise();
}

} // namespace GuardNotice
