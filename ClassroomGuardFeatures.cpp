/*
 * ClassroomGuardFeatures.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 * Based on the InternetGuard plugin by prof. Ing. Raffaele Mele (GPL-2.0-or-later).
 *
 * Classroom Guard: the Veyon features this plugin offers (toolbar toggles, Block/Allow
 * actions, hidden notice feature) and their UIDs. UIDs are part of the protocol and must not change.
 */

#include "ClassroomGuardPlugin.h"

#include "ClassroomGuardRestrictDialog.h"

// Toolbar/menu icons (see resources.qrc). The internet icon is shared by the
// internet features; the other resources have their own.
static const char* ICON_INTERNET   = ":/classroom-guard/network-offline.png";
static const char* ICON_USB        = ":/classroom-guard/usb-blocked.png";
static const char* ICON_WEBCAM     = ":/classroom-guard/webcam-blocked.png";
static const char* ICON_MICROPHONE = ":/classroom-guard/microphone-muted.png";
static const char* ICON_AUDIO      = ":/classroom-guard/audio-muted.png";

// Feature UUIDs. They only need to be unique and constant. The internet
// toggle/block/allow UUIDs are those of the earlier InternetGuard versions.
static const char* UID_TOGGLE     = "a4b3c2d1-e5f6-7890-abcd-ef1234567890";
static const char* UID_BLOCK_NET  = "b1b2c3d4-1111-2222-3333-444455556666";
static const char* UID_ALLOW_NET  = "c1c2c3d4-1111-2222-3333-444455556666";

static const char* UID_RES_TOGGLE = "b6ea7cce-f355-4ae6-96ca-ce4a740eeb85";
static const char* UID_RES_BLOCK  = "2cb1f592-ee8f-49fc-bdb2-c148b8009cae";
static const char* UID_RES_ALLOW  = "f5062759-dcb6-4ecc-baad-26ceedb09b3b";

static const char* UID_USB_TOGGLE = "568e4387-f0db-48db-9d2b-79c811a0055a";
static const char* UID_USB_BLOCK  = "ca05a47f-8d0a-4357-ae24-815b4f9ef701";
static const char* UID_USB_ALLOW  = "bfc296d2-b717-4ad2-88ef-c4aef3a07ed8";

static const char* UID_CAM_TOGGLE = "9f471bdb-5b08-4959-8bf1-2ca0beefae93";
static const char* UID_CAM_BLOCK  = "0a30195b-4822-4a8e-85c2-029b25d39d96";
static const char* UID_CAM_ALLOW  = "e4e6f0e6-5e89-42d4-9306-4c406b0fecf6";

static const char* UID_MIC_TOGGLE = "2d66cddf-02f9-446b-ac36-c36a7dc1958e";
static const char* UID_MIC_BLOCK  = "cfd65f37-6895-4019-a608-0cf6eb2d6977";
static const char* UID_MIC_ALLOW  = "7b1f0c52-3a6e-4d0b-9f59-5e0c8a1d2b64";

static const char* UID_AUD_TOGGLE = "d3a8e9f4-6c21-4b77-8a35-1f9e47c0b852";
static const char* UID_AUD_BLOCK  = "41c7a0d9-85be-4f13-b6e2-93a5d7c1e048";
static const char* UID_AUD_ALLOW  = "f06b92e3-1d4a-4c85-a7f0-2e8b6d93c517";

// Hidden feature that carries the notices to the Worker on the student computer.
// Veyon treats a feature as active while a Worker runs for its UID, and the Master shows
// that as a pressed toolbar button and an icon over the computer. The notice therefore
// must not use the UID of a visible feature.
static const char* UID_NOTICE     = "5c2f8a47-9d3e-4b61-a0f7-3e1d6b8c4a92";

ClassroomGuardPlugin::FeatureGroup ClassroomGuardPlugin::makeGroup(
	const char* name,
	const char* uidToggle, const char* uidBlock, const char* uidAllow,
	const QString& blockLabel, const QString& allowLabel,
	const QString& toggleDescription,
	const QString& blockDescription, const QString& allowDescription,
	const char* iconUrl,
	Commands blockCommand, Commands allowCommand)
{
	const QString baseName = QString::fromLatin1(name);

	// The toolbar button of the group. NOT a Mode feature: Veyon allows one Mode at a time and stops
	// all other features whenever one starts (also Demo, Lock screen...), which would lift
	// the restrictions of this plugin. The button opens a menu with the explicit Block and
	// Allow actions below; the toggle itself is only a carrier for the menu, the icon and the
	// state (see isFeatureActive).
	Feature toggle(
		baseName,
		Feature::Flag::Action | Feature::Flag::AllComponents,
		Feature::Uid(QLatin1String(uidToggle)),
		Feature::Uid(),
		blockLabel,
		{},   // no "active" name: the right-click menu then always offers both entries (Block / Allow)
		toggleDescription,
		QLatin1String(iconUrl));

	// Explicit per-selection actions: toolbar dropdown and right-click menu. They
	// act on exactly the selected computers and make independent control reliable.
	Feature block(
		baseName + QStringLiteral("Block"),
		Feature::Flag::Action | Feature::Flag::AllComponents,
		Feature::Uid(QLatin1String(uidBlock)),
		toggle.uid(),
		blockLabel,
		{},
		blockDescription,
		QLatin1String(iconUrl));

	Feature allow(
		baseName + QStringLiteral("Allow"),
		Feature::Flag::Action | Feature::Flag::AllComponents,
		Feature::Uid(QLatin1String(uidAllow)),
		toggle.uid(),
		allowLabel,
		{},
		allowDescription,
		QLatin1String(iconUrl));

	return FeatureGroup{ toggle, block, allow, blockCommand, allowCommand };
}

std::vector<ClassroomGuardPlugin::FeatureGroup> ClassroomGuardPlugin::makeGroups()
{
	std::vector<FeatureGroup> groups;

	groups.push_back(makeGroup(
		"ClassroomGuardInternet", UID_TOGGLE, UID_BLOCK_NET, UID_ALLOW_NET,
		tr("Block Internet"), tr("Allow Internet"),
		tr("Block or allow internet access on the selected computers"),
		tr("Block internet access on the selected computers"),
		tr("Restore internet access on the selected computers"),
		ICON_INTERNET, BlockInternetCommand, AllowInternetCommand));

	groups.push_back(makeGroup(
		"ClassroomGuardRestrict", UID_RES_TOGGLE, UID_RES_BLOCK, UID_RES_ALLOW,
		tr("Restrict Internet"), tr("Lift Internet Restriction"),
		tr("Restrict internet access to the allowed websites on the selected computers"),
		tr("Restrict internet access to the allowed websites on the selected computers"),
		tr("Lift the internet restriction on the selected computers"),
		ICON_INTERNET, RestrictInternetCommand, AllowInternetCommand));

	groups.push_back(makeGroup(
		"ClassroomGuardUsb", UID_USB_TOGGLE, UID_USB_BLOCK, UID_USB_ALLOW,
		tr("Block USB Drives"), tr("Allow USB Drives"),
		tr("Block or allow USB storage devices on the selected computers"),
		tr("Block USB storage devices on the selected computers"),
		tr("Allow USB storage devices on the selected computers"),
		ICON_USB, BlockUsbCommand, AllowUsbCommand));

	groups.push_back(makeGroup(
		"ClassroomGuardWebcam", UID_CAM_TOGGLE, UID_CAM_BLOCK, UID_CAM_ALLOW,
		tr("Block Webcams"), tr("Allow Webcams"),
		tr("Block or allow webcams on the selected computers"),
		tr("Block webcams on the selected computers"),
		tr("Allow webcams on the selected computers"),
		ICON_WEBCAM, BlockWebcamCommand, AllowWebcamCommand));

	groups.push_back(makeGroup(
		"ClassroomGuardMicrophone", UID_MIC_TOGGLE, UID_MIC_BLOCK, UID_MIC_ALLOW,
		tr("Mute Microphones"), tr("Unmute Microphones"),
		tr("Mute or unmute the microphones of the selected computers"),
		tr("Mute the microphones of the selected computers"),
		tr("Unmute the microphones of the selected computers"),
		ICON_MICROPHONE, MuteMicrophoneCommand, UnmuteMicrophoneCommand));

	groups.push_back(makeGroup(
		"ClassroomGuardAudio", UID_AUD_TOGGLE, UID_AUD_BLOCK, UID_AUD_ALLOW,
		tr("Mute Audio"), tr("Unmute Audio"),
		tr("Mute or unmute the audio output of the selected computers"),
		tr("Mute the audio output of the selected computers"),
		tr("Unmute the audio output of the selected computers"),
		ICON_AUDIO, MuteAudioCommand, UnmuteAudioCommand));

	return groups;
}

Feature::Uid ClassroomGuardPlugin::noticeFeatureUid()
{
	return Feature::Uid(QLatin1String(UID_NOTICE));
}

FeatureList ClassroomGuardPlugin::makeFeatureList(const std::vector<FeatureGroup>& groups)
{
	FeatureList features;
	for (const FeatureGroup& group : groups)
	{
		features.append(group.toggle);
		features.append(group.block);
		features.append(group.allow);
	}

	// No Master flag, Meta, no name and no icon: invisible in the Master.
	features.append(Feature(
		QStringLiteral("ClassroomGuardNotice"),
		Feature::Flag::Meta | Feature::Flag::Service | Feature::Flag::Worker,
		noticeFeatureUid(),
		Feature::Uid(),
		QString(),
		QString(),
		QString(),
		QString()));
	return features;
}
