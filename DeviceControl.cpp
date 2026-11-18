/*
 * DeviceControl.cpp
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * USB storage / webcam / microphone / audio blocking. See DeviceControl.h.
 */

#include <QtGlobal>

#include "DeviceControl.h"

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

#include <cwchar>

namespace
{

// ---------------------------------------------------------------------------
// Stable, documented Windows identifiers, defined locally (like the firewall
// backend does) so no uuid import library is needed.
// ---------------------------------------------------------------------------

// Device setup class "DiskDrive" - the disks of USB sticks / drives (USBSTOR\...).
const GUID kClassDiskDrive =
	{ 0x4d36e967, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };
// Device setup class "Camera".
const GUID kClassCamera =
	{ 0xca3e7ab9, 0xb4c3, 0x4ae6, { 0x82, 0x51, 0x57, 0x9e, 0xf9, 0x33, 0x89, 0x0f } };

const CLSID kClsidMMDeviceEnumerator =
	{ 0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
const IID kIidIMMDeviceEnumerator =
	{ 0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };
const IID kIidIAudioEndpointVolume =
	{ 0x5CDF2C82, 0x841E, 0x4546, { 0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A } };

// Policy key behind "Removable Storage Access: Removable Disks: Deny read/write access".
const wchar_t kRemovableDisksPolicy[] =
	L"SOFTWARE\\Policies\\Microsoft\\Windows\\RemovableStorageDevices\\"
	L"{53f5630d-b6bf-11d0-94f2-00a0c91efb8b}";

// ---------------------------------------------------------------------------
// COM plumbing (same pattern as WindowsFirewall.cpp)
// ---------------------------------------------------------------------------

template<typename T>
class ComPtr
{
public:
	ComPtr() = default;
	~ComPtr() { if (m_ptr) m_ptr->Release(); }
	ComPtr(const ComPtr&) = delete;
	ComPtr& operator=(const ComPtr&) = delete;

	T* operator->() const { return m_ptr; }
	T* get() const { return m_ptr; }
	explicit operator bool() const { return m_ptr != nullptr; }
	void** out() { return reinterpret_cast<void**>(&m_ptr); }
	T** typedOut() { return &m_ptr; }

private:
	T* m_ptr = nullptr;
};

// The plugin runs on the Veyon Server worker thread (no COM yet) and on the
// Master GUI thread (Qt already initialized COM); RPC_E_CHANGED_MODE just means
// "use the apartment that is already there".
class ComInit
{
public:
	ComInit()
	{
		const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		m_mustUninit = SUCCEEDED(hr);
		m_usable = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
		if (!m_usable)
			qWarning("[ClassroomGuard] CoInitializeEx failed (hr=0x%08lx)",
			         static_cast<unsigned long>(hr));
	}
	~ComInit() { if (m_mustUninit) CoUninitialize(); }
	ComInit(const ComInit&) = delete;
	ComInit& operator=(const ComInit&) = delete;

	bool usable() const { return m_usable; }

private:
	bool m_mustUninit = false;
	bool m_usable = false;
};

// ---------------------------------------------------------------------------
// Setup API: enable / disable the devices of one class
// ---------------------------------------------------------------------------

#ifndef CONFIGFLAG_DISABLED
#  define CONFIGFLAG_DISABLED 0x00000001
#endif

// 1 = the device is flagged "disabled" (Device Manager: "Disable device"),
// 0 = not flagged, -1 = cannot tell. Works for connected and absent devices.
// Used to leave devices alone that already are in the wanted state: switching
// a running device on again would restart it for nothing (a webcam in use
// would blink).
int disabledFlag(HDEVINFO set, SP_DEVINFO_DATA& dev)
{
	DWORD flags = 0;
	if (!SetupDiGetDeviceRegistryPropertyW(set, &dev, SPDRP_CONFIGFLAGS, nullptr,
	                                       reinterpret_cast<PBYTE>(&flags), sizeof(flags), nullptr))
		return -1;
	return (flags & CONFIGFLAG_DISABLED) ? 1 : 0;
}

// Enables or disables every device of `deviceClass` whose instance id starts
// with `idPrefix` (nullptr = all devices of the class).
//
// `presentOnly`: true  = only devices that are plugged in / running now. A
//                        failure counts as an error.
//                false = also devices that are known but currently not
//                        connected. Needed when releasing, otherwise a stick
//                        that was disabled and then unplugged would come back
//                        disabled when it is plugged in again. Failures are
//                        ignored here (a phantom device cannot always be
//                        changed, and that is harmless).
bool changeClassDevices(const GUID& deviceClass, const wchar_t* idPrefix,
                        bool enable, bool presentOnly, const char* what)
{
	const HDEVINFO set = SetupDiGetClassDevsW(&deviceClass, nullptr, nullptr,
	                                          presentOnly ? DIGCF_PRESENT : 0);
	if (set == INVALID_HANDLE_VALUE)
	{
		qWarning("[ClassroomGuard] cannot list %s devices (error %lu)",
		         what, static_cast<unsigned long>(GetLastError()));
		return false;
	}

	const size_t prefixLen = idPrefix ? std::wcslen(idPrefix) : 0;
	bool ok = true;

	SP_DEVINFO_DATA dev;
	for (DWORD index = 0;; ++index)
	{
		dev.cbSize = sizeof(dev);
		if (!SetupDiEnumDeviceInfo(set, index, &dev))
			break;      // ERROR_NO_MORE_ITEMS

		wchar_t id[MAX_DEVICE_ID_LEN] = {};
		if (CM_Get_Device_IDW(dev.DevInst, id, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS)
			continue;
		if (prefixLen && _wcsnicmp(id, idPrefix, prefixLen) != 0)
			continue;

		if (disabledFlag(set, dev) == (enable ? 0 : 1))
			continue;   // already in the wanted state

		SP_PROPCHANGE_PARAMS params = {};
		params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
		params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
		params.StateChange = enable ? DICS_ENABLE : DICS_DISABLE;
		params.Scope = DICS_FLAG_GLOBAL;
		params.HwProfile = 0;

		const bool changed =
			SetupDiSetClassInstallParamsW(set, &dev, &params.ClassInstallHeader, sizeof(params))
			&& SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, set, &dev);

		if (!changed && presentOnly)
		{
			qWarning("[ClassroomGuard] cannot %s %s device '%ls' (error %lu)",
			         enable ? "enable" : "disable", what, id,
			         static_cast<unsigned long>(GetLastError()));
			ok = false;
		}
	}

	SetupDiDestroyDeviceInfoList(set);
	return ok;
}

bool setClassDevicesEnabled(const GUID& deviceClass, const wchar_t* idPrefix,
                            bool enable, const char* what)
{
	if (!enable)
		return changeClassDevices(deviceClass, idPrefix, false, true, what);

	// Releasing: present devices first (errors count), then the known-but-absent
	// ones so they do not stay disabled the next time they are connected.
	const bool ok = changeClassDevices(deviceClass, idPrefix, true, true, what);
	changeClassDevices(deviceClass, idPrefix, true, false, what);
	return ok;
}

// ---------------------------------------------------------------------------
// Removable storage policy
// ---------------------------------------------------------------------------

bool setRemovableStoragePolicy(bool deny)
{
	if (deny)
	{
		HKEY key;
		LONG rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, kRemovableDisksPolicy, 0, nullptr, 0,
		                          KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &key, nullptr);
		if (rc != ERROR_SUCCESS)
		{
			qWarning("[ClassroomGuard] cannot create the removable storage policy key (error %ld)", rc);
			return false;
		}
		const DWORD one = 1;
		rc = RegSetValueExW(key, L"Deny_Write", 0, REG_DWORD,
		                    reinterpret_cast<const BYTE*>(&one), sizeof(one));
		const LONG rc2 = RegSetValueExW(key, L"Deny_Read", 0, REG_DWORD,
		                                reinterpret_cast<const BYTE*>(&one), sizeof(one));
		RegCloseKey(key);
		if (rc != ERROR_SUCCESS || rc2 != ERROR_SUCCESS)
		{
			qWarning("[ClassroomGuard] cannot write the removable storage policy values");
			return false;
		}
		return true;
	}

	HKEY key;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kRemovableDisksPolicy, 0,
	                  KEY_SET_VALUE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
		return true;    // nothing set, nothing to release

	const LONG rc1 = RegDeleteValueW(key, L"Deny_Read");
	const LONG rc2 = RegDeleteValueW(key, L"Deny_Write");
	RegCloseKey(key);
	const bool ok = (rc1 == ERROR_SUCCESS || rc1 == ERROR_FILE_NOT_FOUND)
	             && (rc2 == ERROR_SUCCESS || rc2 == ERROR_FILE_NOT_FOUND);
	if (!ok)
		qWarning("[ClassroomGuard] cannot remove the removable storage policy values");
	// Remove the (now empty) key; fails harmlessly if other values live there.
	RegDeleteKeyExW(HKEY_LOCAL_MACHINE, kRemovableDisksPolicy, KEY_WOW64_64KEY, 0);
	return ok;
}

// ---------------------------------------------------------------------------
// Core Audio: mute / unmute all active endpoints of one direction
// ---------------------------------------------------------------------------

// changed (optional, only used when muting): counts the endpoints that were not muted
// yet and had to be muted now. Endpoints that are muted already are left alone.
bool setEndpointsMuted(EDataFlow flow, bool mute, const char* what, int* changed = nullptr)
{
	const ComInit com;
	if (!com.usable())
		return false;

	ComPtr<IMMDeviceEnumerator> enumerator;
	HRESULT hr = CoCreateInstance(kClsidMMDeviceEnumerator, nullptr, CLSCTX_INPROC_SERVER,
	                              kIidIMMDeviceEnumerator, enumerator.out());
	if (FAILED(hr))
	{
		qWarning("[ClassroomGuard] cannot create the audio device enumerator (hr=0x%08lx)",
		         static_cast<unsigned long>(hr));
		return false;
	}

	ComPtr<IMMDeviceCollection> devices;
	hr = enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, devices.typedOut());
	if (FAILED(hr))
	{
		qWarning("[ClassroomGuard] cannot list %s endpoints (hr=0x%08lx)",
		         what, static_cast<unsigned long>(hr));
		return false;
	}

	UINT count = 0;
	devices->GetCount(&count);

	bool ok = true;
	for (UINT i = 0; i < count; ++i)
	{
		ComPtr<IMMDevice> device;
		if (FAILED(devices->Item(i, device.typedOut())))
			continue;

		ComPtr<IAudioEndpointVolume> volume;
		hr = device->Activate(kIidIAudioEndpointVolume, CLSCTX_ALL, nullptr, volume.out());
		if (SUCCEEDED(hr) && mute && changed)
		{
			BOOL muted = FALSE;
			if (SUCCEEDED(volume->GetMute(&muted)) && muted)
				continue;
			++*changed;
		}
		if (SUCCEEDED(hr))
			hr = volume->SetMute(mute ? TRUE : FALSE, nullptr);

		if (FAILED(hr))
		{
			qWarning("[ClassroomGuard] cannot %s %s endpoint %u (hr=0x%08lx)",
			         mute ? "mute" : "unmute", what, i, static_cast<unsigned long>(hr));
			ok = false;
		}
	}
	return ok;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

namespace DeviceControl
{

bool setUsbStorageBlocked(bool blocked)
{
	// USBSTOR\ is the enumerator of USB mass storage (sticks, external drives);
	// internal disks have other prefixes and are never touched.
	bool ok = true;
	if (blocked)
	{
		ok = setRemovableStoragePolicy(true) && ok;     // refuses sticks plugged in later
		ok = setClassDevicesEnabled(kClassDiskDrive, L"USBSTOR\\", false, "USB storage") && ok;
	}
	else
	{
		ok = setClassDevicesEnabled(kClassDiskDrive, L"USBSTOR\\", true, "USB storage") && ok;
		ok = setRemovableStoragePolicy(false) && ok;
	}
	return ok;
}

bool setWebcamsBlocked(bool blocked)
{
	return setClassDevicesEnabled(kClassCamera, nullptr, !blocked, "webcam");
}

bool setMicrophonesMuted(bool muted)
{
	return setEndpointsMuted(eCapture, muted, "microphone");
}

int reapplyMute(bool microphones, bool audio)
{
	int changed = 0;
	if (microphones)
		setEndpointsMuted(eCapture, true, "microphone", &changed);
	if (audio)
		setEndpointsMuted(eRender, true, "audio output", &changed);
	return changed;
}

bool setAudioMuted(bool muted)
{
	return setEndpointsMuted(eRender, muted, "audio output");
}

} // namespace DeviceControl
