/*
 * FeatureWorkerManagerShim.h
 * (c) by Andreas Weller - https://www.andreas-weller.de/
 *
 * Veyon's FeatureWorkerManager.h includes CryptoCore.h and with it QtCrypto (QCA), which
 * is not part of a plugin build environment. The plugin only needs one non-virtual
 * method of that class, so it is declared here as a minimal stand-in. The symbol is
 * resolved from veyon-core.dll like any other import (tools/build-windows.ps1 checks that
 * it exists in the installed Veyon). Do not include Veyon's own FeatureWorkerManager.h
 * in the same translation unit.
 */

#pragma once

class FeatureMessage;

class FeatureWorkerManager
{
public:
	// Sends the message to the Veyon Worker in the session of the logged-in user.
	void sendMessageToUnmanagedSessionWorker(const FeatureMessage& message);
};
