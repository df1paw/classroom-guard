/*
 * GuardNotice.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * Small notice window shown on the student's desktop when the teacher switches a
 * restriction on. It runs in the Veyon Worker process (the user's session), not in the
 * Veyon Service, which has no access to the user's desktop.
 */

#pragma once

#include <QString>

namespace GuardNotice
{

// Shows the notice in the lower right corner of the primary screen and closes it
// after the given number of seconds (or on click). A notice that is still visible
// is replaced.
void show(const QString& text, int seconds);

} // namespace GuardNotice
