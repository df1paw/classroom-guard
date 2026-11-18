/*
 * GuardLog.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 */

#include "GuardLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>

namespace
{

QMutex g_mutex;
const qint64 kMaxBytes = 512 * 1024;

QString directory()
{
#ifdef Q_OS_WIN
	QString base = qEnvironmentVariable("ProgramData");
	if (base.isEmpty())
		base = QStringLiteral("C:/ProgramData");
	return QDir::fromNativeSeparators(base) + QStringLiteral("/ClassroomGuard");
#else
	return QDir::tempPath() + QStringLiteral("/ClassroomGuard");
#endif
}

} // namespace

namespace GuardLog
{

QString path()
{
	return QDir::toNativeSeparators(directory() + QStringLiteral("/classroom-guard.log"));
}

void write(const QString& text)
{
	QMutexLocker lock(&g_mutex);

	QDir().mkpath(directory());
	const QString file = path();

	if (QFileInfo(file).size() > kMaxBytes)
	{
		QFile::remove(file + QStringLiteral(".old"));
		QFile::rename(file, file + QStringLiteral(".old"));
	}

	QFile f(file);
	if (!f.open(QIODevice::Append | QIODevice::Text))
		return;
	const QString line = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
		+ QStringLiteral("  ") + text + QLatin1Char('\n');
	f.write(line.toUtf8());
}

QString tail(int lines)
{
	QMutexLocker lock(&g_mutex);

	QFile f(path());
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
		return QString();

	const qint64 size = f.size();
	if (size > 64 * 1024)
		f.seek(size - 64 * 1024);
	const QString all = QString::fromUtf8(f.readAll());
	QStringList parts = all.split(QLatin1Char('\n'));
	if (size > 64 * 1024 && !parts.isEmpty())
		parts.removeFirst();            // the first line may be cut off
	while (!parts.isEmpty() && parts.last().isEmpty())
		parts.removeLast();
	while (parts.size() > lines)
		parts.removeFirst();
	return parts.join(QLatin1Char('\n'));
}

} // namespace GuardLog
