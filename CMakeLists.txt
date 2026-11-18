#
# Classroom Guard - Veyon plugin: block / restrict internet, USB, webcam, microphone, audio
# (c) by Andreas Weller - https://www.andreas-weller.de/
# Based on the InternetGuard plugin by prof. Ing. Raffaele Mele (GPL-2.0-or-later).
#
# Multi-version build. The plugin's binary ABI is tied to the Veyon *core*
# version it is compiled against, so there are two builds:
#   Qt5  (WITH_QT6=OFF) -> Veyon 4.7.5-4.8.x   (core+Qt5 headers from 4.7.5)
#   Qt6  (WITH_QT6=ON)  -> Veyon 4.9.0-4.10.x  (core headers from 4.9.0, Qt 6.7)
#                          -DVEYON_TARGET_VERSION=4.9.0
#   Qt6  (WITH_QT6=ON)  -> Veyon 4.11.x        (core headers from 4.11.x, Qt 6.7, C++17)
#                          the default of the Qt6 build; its API differs from 4.9/4.10
#                          (FeatureMessage, ComputerControlInterfaceList), so it is a
#                          separate DLL. Veyon 4.11 ships Qt 6.10; a plugin built with
#                          Qt 6.7 loads there, Qt only refuses plugins NEWER than the host.
# A single Qt6 binary covers 4.9-4.10: the plugin's interface ABI is unchanged
# across that range, and building with Qt 6.7 (the lowest Qt in the range, shipped
# by 4.9.0) satisfies the plugin version-gate on every host up to 4.10.x (Qt 6.10).
# See VeyonCompat.h for the full compatibility analysis.
#
# Output names:
#   WITH_QT6=OFF  ->  classroom-guard-qt5.dll
#   WITH_QT6=ON   ->  classroom-guard-qt6.dll       (target Veyon 4.9.0 - 4.10.x)
#                     classroom-guard-qt6-v411.dll   (target Veyon 4.11.x)
#
# Configurable cache variables:
#   WITH_QT6             OFF      Build against Qt6 instead of Qt5
#   VEYON_TARGET_VERSION auto     4.7.5 (Qt5) or 4.9.0 (Qt6); drives compat macros
#   VEYON_SOURCE_DIR     ../veyon-src   Veyon source tree (core/src headers):
#                                       a 4.7.5 checkout for Qt5, a 4.9.0 checkout for Qt6
#   VEYON_CORE_LIBRARY   auto-selected  libveyon-core.dll.a (Qt5) or
#                                       libveyon-core-qt6.dll.a (Qt6, from 4.9.0)
#
cmake_minimum_required(VERSION 3.16)
project(ClassroomGuard LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Options / paths
# ---------------------------------------------------------------------------
option(WITH_QT6 "Build against Qt 6 (target Veyon built with WITH_QT6=ON)" OFF)

# The binary ABI follows the Veyon *core* version, so the default target tracks
# the Qt branch: Qt5 -> 4.7.5 (covers 4.7.5-4.8.x), Qt6 -> 4.9.0 (covers
# 4.9.0-4.10.x). Override only to target a specific point release.
if(WITH_QT6)
    set(_default_target_version "4.11.4")
else()
    set(_default_target_version "4.7.5")
endif()
set(VEYON_TARGET_VERSION "${_default_target_version}" CACHE STRING
    "Target Veyon version (4.7.5 for the Qt5 build, 4.9.0 for the Qt6 build)")

set(VEYON_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../veyon-src" CACHE PATH
    "Path to the Veyon source tree providing core/src headers")

if(WITH_QT6 AND VEYON_TARGET_VERSION VERSION_GREATER_EQUAL 4.11.0)
    # generated from the installed veyon-core.dll by tools/build-windows.ps1
    set(_default_veyon_lib "${CMAKE_CURRENT_SOURCE_DIR}/libveyon-core-qt6-v411.dll.a")
elseif(WITH_QT6)
    set(_default_veyon_lib "${CMAKE_CURRENT_SOURCE_DIR}/libveyon-core-qt6.dll.a")
else()
    set(_default_veyon_lib "${CMAKE_CURRENT_SOURCE_DIR}/libveyon-core.dll.a")
endif()
set(VEYON_CORE_LIBRARY "${_default_veyon_lib}" CACHE FILEPATH
    "Path to the veyon-core import library to link against")

# Veyon core is compiled as C++14; match it so the ABI/headers agree.
# (Veyon 4.11 headers need C++17, and so does Qt 6)
if(WITH_QT6)
    set(CMAKE_CXX_STANDARD 17)
else()
    set(CMAKE_CXX_STANDARD 14)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ---------------------------------------------------------------------------
# Decode VEYON_TARGET_VERSION into integer components for VeyonCompat.h
# ---------------------------------------------------------------------------
string(REPLACE "." ";" _veyon_ver_parts "${VEYON_TARGET_VERSION}")
list(LENGTH _veyon_ver_parts _veyon_ver_len)
list(GET _veyon_ver_parts 0 VEYON_VER_MAJOR)
if(_veyon_ver_len GREATER 1)
    list(GET _veyon_ver_parts 1 VEYON_VER_MINOR)
else()
    set(VEYON_VER_MINOR 0)
endif()
if(_veyon_ver_len GREATER 2)
    list(GET _veyon_ver_parts 2 VEYON_VER_PATCH)
else()
    set(VEYON_VER_PATCH 0)
endif()

# ---------------------------------------------------------------------------
# Qt
# ---------------------------------------------------------------------------
# Convenience: if Qt5 is not on the prefix path, fall back to the toolchain
# bundled with the assumed Qt 5.12.12 MinGW install. Override by passing
# -DCMAKE_PREFIX_PATH=... on the command line.
if(NOT WITH_QT6 AND NOT DEFINED Qt5_DIR
   AND NOT DEFINED CMAKE_PREFIX_PATH
   AND EXISTS "C:/Qt/5.12.12/mingw73_64")
    set(CMAKE_PREFIX_PATH "C:/Qt/5.12.12/mingw73_64")
endif()

# The toolbar icon is a PNG (see resources.qrc), so Qt Svg is not needed.
if(WITH_QT6)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
    set(VEYON_QT_LIBS Qt6::Core Qt6::Widgets Qt6::Network)
    set(VEYON_QT_MAJOR 6)
else()
    find_package(Qt5 5.12 REQUIRED COMPONENTS Core Widgets Network)
    set(VEYON_QT_LIBS Qt5::Core Qt5::Widgets Qt5::Network)
    set(VEYON_QT_MAJOR 5)
endif()

# ---------------------------------------------------------------------------
# Target
# ---------------------------------------------------------------------------
add_library(classroom-guard SHARED
    ClassroomGuardPlugin.cpp
    ClassroomGuardFeatures.cpp
    ClassroomGuardRuntime.cpp
    ClassroomGuardPlugin.h
    ClassroomGuardConfig.cpp
    ClassroomGuardConfig.h
    ClassroomGuardConfigurationPage.cpp
    ClassroomGuardRestrictDialog.cpp
    FeatureWorkerManagerShim.h
    GuardNotice.cpp
    GuardNotice.h
    GuardTranslator.cpp
    GuardTranslator.h
    ClassroomGuardRestrictDialog.h
    ClassroomGuardConfigurationPage.h
    GuardActions.cpp
    GuardActions.h
    GuardConfig.h
    GuardLog.cpp
    GuardLog.h
    IpRanges.h
    VeyonCompat.h
    resources.qrc
)

# The firewall backend talks to the Windows Firewall COM API (INetFwPolicy2),
# so it is compiled and linked (ole32/oleaut32) on Windows only; elsewhere the
# plugin builds as a no-op that just logs a warning.
if(WIN32)
    target_sources(classroom-guard PRIVATE
        WindowsFirewall.cpp
        WindowsFirewall.h
        NetUtil.cpp
        NetUtil.h
        GuardSettings.cpp
        GuardSettings.h
        DeviceControl.cpp
        DeviceControl.h
        ProxyRedirect.cpp
        ProxyRedirect.h
        RoutingBlock.cpp
        RoutingBlock.h
    )
    target_link_libraries(classroom-guard PRIVATE
        ole32 oleaut32 setupapi advapi32 iphlpapi ws2_32)
endif()

target_include_directories(classroom-guard PRIVATE
    "${VEYON_SOURCE_DIR}/core/src"
)

target_compile_definitions(classroom-guard PRIVATE
    QT_DEPRECATED_WARNINGS
    VEYON_TARGET_VERSION_MAJOR=${VEYON_VER_MAJOR}
    VEYON_TARGET_VERSION_MINOR=${VEYON_VER_MINOR}
    VEYON_TARGET_VERSION_PATCH=${VEYON_VER_PATCH}
)

target_link_libraries(classroom-guard PRIVATE
    ${VEYON_QT_LIBS}
    "${VEYON_CORE_LIBRARY}"
)

# GCC 16+ emits both a local vtable and __imp__ZTV references for dllimport base
# classes (FeatureProviderInterface, PluginInterface). The vtable definitions are
# identical (same header), so picking the local copy is safe.
if(WIN32)
    target_link_options(classroom-guard PRIVATE -Wl,--allow-multiple-definition)
endif()

if(WITH_QT6 AND VEYON_TARGET_VERSION VERSION_GREATER_EQUAL 4.11.0)
    set(_output_name "classroom-guard-qt6-v411")
elseif(WITH_QT6)
    set(_output_name "classroom-guard-qt6")
else()
    set(_output_name "classroom-guard-qt5")
endif()
set_target_properties(classroom-guard PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${_output_name}"
)

message(STATUS "ClassroomGuard: target Veyon ${VEYON_TARGET_VERSION} (Qt${VEYON_QT_MAJOR})")
