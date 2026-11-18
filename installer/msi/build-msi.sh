#!/bin/sh
# Build the MSI with wixl (msitools) on Linux. Usage: ./build-msi.sh path/to/classroom-guard-qt6-v411.dll [version]
set -e
wixl -a x64 -D Version="${2:-2.0.0}" -D DllPath="$1" -o classroom-guard-4.11.msi classroom-guard.wxs
