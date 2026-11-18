/*
 * GuardTranslator.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * German texts for every tr() string of the plugin. The table is compiled in, so
 * the plugin needs no .qm files and no Qt tooling at build time. The translator is
 * installed when Veyon's user interface language is German (Veyon sets the default
 * locale from its "Language" setting); every other language shows the English texts.
 */

#pragma once

namespace GuardTranslator
{

// Installs the German translator once per process, if the UI language is German.
void installIfGerman();

// True if the UI language is German (also used by the notice popup).
bool isGerman();

} // namespace GuardTranslator
