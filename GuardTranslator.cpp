/*
 * GuardTranslator.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 */

#include "GuardTranslator.h"

#include <QCoreApplication>
#include <QHash>
#include <QLocale>
#include <QTranslator>

namespace
{

struct Entry { const char* en; const char* de; };

const Entry kGerman[] = {
	{ "Block Internet",
	  "Internet sperren" },
	{ "Allow Internet",
	  "Internet freigeben" },
	{ "Block or allow internet access on the selected computers",
	  "Internetzugang auf den ausgewählten Computern sperren oder freigeben" },
	{ "Block internet access on the selected computers",
	  "Internetzugang auf den ausgewählten Computern sperren" },
	{ "Restore internet access on the selected computers",
	  "Internetzugang auf den ausgewählten Computern wiederherstellen" },
	{ "Restrict Internet",
	  "Internet einschränken" },
	{ "Restrict internet access to the allowed websites on the selected computers",
	  "Internetzugang auf den ausgewählten Computern auf die erlaubten Websites einschränken" },
	{ "Lift the internet restriction on the selected computers",
	  "Internet-Einschränkung auf den ausgewählten Computern aufheben" },
	{ "Lift Internet Restriction",
	  "Einschränkung aufheben" },
	{ "Block USB Drives",
	  "USB-Laufwerke sperren" },
	{ "Allow USB Drives",
	  "USB-Laufwerke freigeben" },
	{ "Block or allow USB storage devices on the selected computers",
	  "USB-Speichergeräte auf den ausgewählten Computern sperren oder freigeben" },
	{ "Block USB storage devices on the selected computers",
	  "USB-Speichergeräte auf den ausgewählten Computern sperren" },
	{ "Allow USB storage devices on the selected computers",
	  "USB-Speichergeräte auf den ausgewählten Computern freigeben" },
	{ "Block Webcams",
	  "Webcams sperren" },
	{ "Allow Webcams",
	  "Webcams freigeben" },
	{ "Block or allow webcams on the selected computers",
	  "Webcams auf den ausgewählten Computern sperren oder freigeben" },
	{ "Block webcams on the selected computers",
	  "Webcams auf den ausgewählten Computern sperren" },
	{ "Allow webcams on the selected computers",
	  "Webcams auf den ausgewählten Computern freigeben" },
	{ "Mute Microphones",
	  "Mikrofone stummschalten" },
	{ "Unmute Microphones",
	  "Mikrofone aktivieren" },
	{ "Mute or unmute the microphones of the selected computers",
	  "Mikrofone der ausgewählten Computer stummschalten oder aktivieren" },
	{ "Mute the microphones of the selected computers",
	  "Mikrofone der ausgewählten Computer stummschalten" },
	{ "Unmute the microphones of the selected computers",
	  "Mikrofone der ausgewählten Computer aktivieren" },
	{ "Mute Audio",
	  "Ton stummschalten" },
	{ "Unmute Audio",
	  "Ton aktivieren" },
	{ "Mute or unmute the audio output of the selected computers",
	  "Tonausgabe der ausgewählten Computer stummschalten oder aktivieren" },
	{ "Mute the audio output of the selected computers",
	  "Tonausgabe der ausgewählten Computer stummschalten" },
	{ "Unmute the audio output of the selected computers",
	  "Tonausgabe der ausgewählten Computer aktivieren" },
	{ "Classroom Guard",
	  "Classroom Guard" },
	{ "<b>Classroom Guard</b> - block or restrict internet access, USB drives, webcams, microphones and audio on the student computers",
	  "<b>Classroom Guard</b> – Internetzugang, USB-Laufwerke, Webcams, Mikrofone und Ton auf den Schülercomputern sperren oder einschränken" },
	{ "Control",
	  "Steuerung" },
	{ "Simulation mode: only write to the log, change nothing (applies to the student computers and to the buttons below)",
	  "Simulationsmodus: nur ins Protokoll schreiben, nichts ändern (gilt für die Schülercomputer und für die Schaltflächen unten)" },
	{ "Current state: not changed from here",
	  "Aktueller Zustand: von hier aus nicht geändert" },
	{ "Allowed websites (one per line; used by \"Restrict\"):",
	  "Erlaubte Websites (eine pro Zeile; gilt für „Internet einschränken“):" },
	{ "e.g. docs.veyon.io",
	  "z. B. docs.veyon.io" },
	{ "Restrict internet access",
	  "Internetzugang einschränken" },
	{ "Block internet access",
	  "Internetzugang sperren" },
	{ "Restore internet access",
	  "Internetzugang wiederherstellen" },
	{ "These buttons act on <b>this</b> computer (administrator rights needed) and are meant for testing. On the student computers use the Classroom Guard buttons in the Veyon Master.",
	  "Diese Schaltflächen wirken auf <b>diesen</b> Computer (Administratorrechte nötig) und dienen zum Testen. Für die Schülercomputer verwenden Sie die Classroom-Guard-Schaltflächen im Veyon Master." },
	{ "restricted",
	  "eingeschränkt" },
	{ "blocked",
	  "gesperrt" },
	{ "open",
	  "offen" },
	{ "General settings",
	  "Allgemeine Einstellungen" },
	{ "Methods used by \"Block / Restrict internet access\" (several can be combined):",
	  "Methoden für „Internetzugang sperren / einschränken“ (mehrere kombinierbar):" },
	{ "Windows Firewall (outbound block rules)",
	  "Windows-Firewall (ausgehende Blockregeln)" },
	{ "Proxy redirection (browsers that use the system proxy)",
	  "Proxy-Umleitung (Browser, die den System-Proxy nutzen)" },
	{ "Proxy address:",
	  "Proxy-Adresse:" },
	{ "host:port, e.g. 0.0.0.0:8080",
	  "Host:Port, z. B. 0.0.0.0:8080" },
	{ "Routing table (remove the default routes)",
	  "Routingtabelle (Standardrouten entfernen)" },
	{ "Release automatically after:",
	  "Automatisch aufheben nach:" },
	{ " minutes (0 = never)",
	  " Minuten (0 = nie)" },
	{ "Unblock on service start (a block never survives a restart of the Veyon Server)",
	  "Beim Dienststart freigeben (eine Sperre überlebt keinen Neustart des Veyon-Servers)" },
	{ "Firewall settings",
	  "Firewall-Einstellungen" },
	{ "Block only traffic to non-local networks (the local network, the domain controller and the teacher computer stay reachable)",
	  "Nur Verkehr in nicht-lokale Netze sperren (lokales Netz, Domänencontroller und Lehrercomputer bleiben erreichbar)" },
	{ "Block all outbound traffic (all TCP and UDP ports)",
	  "Gesamten ausgehenden Verkehr sperren (alle TCP- und UDP-Ports)" },
	{ "HTTP/HTTPS (80, 443, 8080, 8443, 3128, QUIC)",
	  "HTTP/HTTPS (80, 443, 8080, 8443, 3128, QUIC)" },
	{ "FTP (21)",
	  "FTP (21)" },
	{ "DNS toward other servers (53, 853)",
	  "DNS zu anderen Servern (53, 853)" },
	{ "Custom ports:",
	  "Eigene Ports:" },
	{ "e.g. 8080 9000-9100",
	  "z. B. 8080 9000-9100" },
	{ "<b>Exceptions</b> - stay reachable although everything else is blocked:",
	  "<b>Ausnahmen</b> – bleiben erreichbar, obwohl alles andere gesperrt ist:" },
	{ "Ports:",
	  "Ports:" },
	{ "e.g. 80 443",
	  "z. B. 80 443" },
	{ "Networks (one per line):",
	  "Netzwerke (eines pro Zeile):" },
	{ "e.g. 192.168.1.0/24 or 10.0.0.1-10.0.0.9",
	  "z. B. 192.168.1.0/24 oder 10.0.0.1-10.0.0.9" },
	{ "Hosts (one per line):",
	  "Hosts (einer pro Zeile):" },
	{ "e.g. docs.veyon.io (resolved when the block starts)",
	  "z. B. docs.veyon.io (wird beim Start der Sperre aufgelöst)" },
	{ "Always let the DNS servers of this computer through",
	  "DNS-Server dieses Computers immer durchlassen" },
	{ "Block traffic to these servers completely, even if they are local (e.g. an internal proxy):",
	  "Verkehr zu diesen Servern vollständig sperren, auch wenn sie lokal sind (z. B. interner Proxy):" },
	{ "Server addresses (one per line):",
	  "Serveradressen (eine pro Zeile):" },
	{ "e.g. 192.168.1.253",
	  "z. B. 192.168.1.253" },
	{ "Switch on a preconfigured firewall rule while blocked",
	  "Während der Sperre eine vorkonfigurierte Firewall-Regel einschalten" },
	{ "Rule name:",
	  "Regelname:" },
	{ "name of an existing, disabled rule",
	  "Name einer vorhandenen, deaktivierten Regel" },
	{ "Log",
	  "Protokoll" },
	{ "Refresh",
	  "Aktualisieren" },
	{ "Open log folder",
	  "Protokollordner öffnen" },
	{ "Log file of this computer: %1",
	  "Protokolldatei dieses Computers: %1" },
	{ "Current state: unchanged (simulation mode)",
	  "Aktueller Zustand: unverändert (Simulationsmodus)" },
	{ "Current state: %1",
	  "Aktueller Zustand: %1" },
	{ "Websites that stay reachable on the %n selected computer(s), one per line.\nEverything else is blocked. Pages of the local network stay reachable.",
	  "Websites, die auf den %n ausgewählten Computern erreichbar bleiben, eine pro Zeile.\nAlles andere wird gesperrt. Seiten im lokalen Netz bleiben erreichbar." },
	{ "docs.veyon.io\nwww.wikipedia.org",
	  "docs.veyon.io\nwww.wikipedia.org" },
	{ "Restrict",
	  "Einschränken" },
	{ "Enter at least one website.",
	  "Geben Sie mindestens eine Website ein." },
	{ "School day and notices",
	  "Schultag und Hinweise" },
	{ "Show a notice on the student computers when a restriction is switched on",
	  "Hinweis auf den Schülercomputern anzeigen, wenn eine Sperre eingeschaltet wird" },
	{ "Show the notice for:",
	  "Hinweis anzeigen für:" },
	{ " seconds",
	  " Sekunden" },
	{ "Lift all restrictions at the end of the school day",
	  "Alle Sperren zum Schulschluss aufheben" },
	{ "End of the school day:",
	  "Schulschluss:" },
	{ "At this time every day internet, USB drives, webcams, microphones and audio are released on the student computers. Restrictions switched on later the same day stay.",
	  "Zu dieser Uhrzeit werden täglich Internet, USB-Laufwerke, Webcams, Mikrofone und Ton auf den Schülercomputern freigegeben. Sperren, die am selben Tag später eingeschaltet werden, bleiben bestehen." },
	{ "Internet access has been blocked by your teacher.",
	  "Der Internetzugang wurde von der Lehrkraft gesperrt." },
	{ "Internet access is restricted to the websites your teacher has allowed.",
	  "Der Internetzugang ist auf die von der Lehrkraft erlaubten Websites beschränkt." },
	{ "USB drives have been blocked by your teacher.",
	  "USB-Laufwerke wurden von der Lehrkraft gesperrt." },
	{ "Webcams have been blocked by your teacher.",
	  "Webcams wurden von der Lehrkraft gesperrt." },
	{ "The microphone has been muted by your teacher.",
	  "Das Mikrofon wurde von der Lehrkraft stummgeschaltet." },
	{ "The audio output has been muted by your teacher.",
	  "Die Tonausgabe wurde von der Lehrkraft stummgeschaltet." },
	{ "All restrictions have been lifted (end of the school day).",
	  "Alle Sperren wurden aufgehoben (Schulschluss)." },
	{ "Click to close",
	  "Zum Schließen klicken" },
};

class Translator : public QTranslator
{
public:
	explicit Translator(QObject* parent) : QTranslator(parent)
	{
		for (const Entry& e : kGerman)
			m_table.insert(QString::fromUtf8(e.en), QString::fromUtf8(e.de));
	}

	QString translate(const char* context, const char* sourceText, const char* disambiguation,
	                  int n) const override
	{
		Q_UNUSED(context)
		Q_UNUSED(disambiguation)
		Q_UNUSED(n)
		// Returning an empty string makes Qt fall back to the source text.
		return sourceText ? m_table.value(QString::fromUtf8(sourceText)) : QString();
	}

	bool isEmpty() const override { return false; }

private:
	QHash<QString, QString> m_table;
};

} // namespace

namespace GuardTranslator
{

bool isGerman()
{
	return QLocale().language() == QLocale::German;
}

void installIfGerman()
{
	static bool done = false;
	if (done || !QCoreApplication::instance())
		return;
	done = true;
	if (isGerman())
		QCoreApplication::installTranslator(new Translator(QCoreApplication::instance()));
}

} // namespace GuardTranslator
