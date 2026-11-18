/*
 * DeviceControl.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * Blocks / releases USB storage, webcams, microphones and audio output on the
 * computer the Veyon Server runs on. Windows only (DeviceControl.cpp); the
 * header itself has no Windows dependency.
 *
 *  - USB storage and webcams: the matching devices are disabled through the
 *    Setup API, which is what "Disable device" in the Device Manager does.
 *    The change is persistent (it survives a reboot) until it is reverted.
 *    USB storage is additionally covered by the "Removable Storage Access"
 *    policy (Deny_Read / Deny_Write) so that sticks plugged in *after* the
 *    block are refused too.
 *  - Microphones and audio output: every active capture / render endpoint is
 *    muted or unmuted through the Core Audio API (IAudioEndpointVolume). The
 *    devices stay enabled.
 *
 * All functions are idempotent and return false only if something that should
 * have worked failed (details are logged via qWarning). Run them with
 * administrator rights (the Veyon Server runs as SYSTEM).
 */

#pragma once

namespace DeviceControl
{

// true = block (disable disks of USB sticks / drives, deny removable storage),
// false = release.
bool setUsbStorageBlocked(bool blocked);

// true = disable all devices of the "Camera" device class, false = enable them.
bool setWebcamsBlocked(bool blocked);

// true = mute every active capture endpoint, false = unmute.
bool setMicrophonesMuted(bool muted);

// true = mute every active render (speaker / headphone / HDMI) endpoint, false = unmute.
bool setAudioMuted(bool muted);

// Mutes again every selected endpoint that is not muted (the user may have unmuted it
// in the volume mixer or with a keyboard key). Returns how many endpoints had to be
// muted again. Cheap enough to call every second.
int reapplyMute(bool microphones, bool audio);

} // namespace DeviceControl
