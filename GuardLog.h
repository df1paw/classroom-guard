/*
 * GuardLog.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * Classroom Guard: small log file of everything the plugin does (and, in
 * simulation mode, would do). Written on the computer that carries out the
 * command: %ProgramData%\ClassroomGuard\classroom-guard.log
 */

#pragma once

#include <QString>

namespace GuardLog
{

QString path();

// Appends one line "2026-11-17 09:30:00  text". Never throws; the file is
// rotated (classroom-guard.log.old) when it grows beyond about 512 kB.
void write(const QString& text);

// The last `lines` lines of the log (empty string if there is no log yet).
QString tail(int lines = 200);

} // namespace GuardLog
