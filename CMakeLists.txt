cmake_minimum_required(VERSION 3.22)

# Allow CMake 4.x to configure subprojects that declare an older minimum.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Single source of truth for the app version. A release tag must match this
# exactly, or the in-app self-updater will never find the published artifact.
project(FlowtoysUpdater VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

if(MSVC)
    # Static CRT: no vc_redist / DLL bundling required by the installer.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(ThirdParty/JUCE)

set(HIDAPI_BUILD_HIDTEST OFF CACHE BOOL "" FORCE)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # The libusb backend matches what shipped historically (the vendored
    # signal11 hid.c was its libusb backend). hidraw would need udev rules
    # re-verified on real props before switching.
    set(HIDAPI_WITH_LIBUSB ON CACHE BOOL "" FORCE)
    set(HIDAPI_WITH_HIDRAW OFF CACHE BOOL "" FORCE)
endif()
add_subdirectory(ThirdParty/hidapi)

# PRODUCT_NAME and BUNDLE_ID must not change: they drive the .app name the
# self-updater relaunches, ProjectInfo::projectName, and the identity that
# installed 1.x clients in the field already have.
juce_add_gui_app(FlowtoysUpdater
    PRODUCT_NAME "FlowtoysUpdater"
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "flowtoys"
    BUNDLE_ID com.benkuper.flowtoysupdater
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    # update.json / firmwares.php / downloads are plain HTTP; ATS must allow
    # them until the server side moves to HTTPS (issue #12).
    PLIST_TO_MERGE [[
        <plist version="1.0">
        <dict>
            <key>NSAppTransportSecurity</key>
            <dict>
                <key>NSAllowsArbitraryLoads</key>
                <true/>
            </dict>
            <key>NSHighResolutionCapable</key>
            <true/>
        </dict>
        </plist>]])

# Replaces the previously committed Projucer-generated BinaryData.cpp.
# Symbol names (Capsule_png, Club_png, icon_png) match existing usage.
juce_add_binary_data(FlowtoysUpdaterData SOURCES
    Resources/Capsule.png
    Resources/Club.png
    Resources/icon.png)

target_sources(FlowtoysUpdater PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/AppUpdater.cpp
    Source/FirmwareManager.cpp
    Source/Prop.cpp
    Source/PropManager.cpp
    Source/PropUI.cpp
    Source/Screen.cpp
    Source/ScreenManager.cpp
    Source/PropChooserScreen.cpp
    Source/PropConnectScreen.cpp
    Source/FirmwareChooserScreen.cpp
    Source/UploadScreen.cpp
    Source/EndScreen.cpp)

# Keeps the existing `#include "hidapi.h"` working unchanged.
target_include_directories(FlowtoysUpdater PRIVATE ThirdParty/hidapi/hidapi)

target_compile_definitions(FlowtoysUpdater PRIVATE
    JUCE_WEB_BROWSER=0
    # FirmwareChooserScreen uses synchronous FileChooser / AlertWindow.
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:FlowtoysUpdater,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:FlowtoysUpdater,JUCE_VERSION>")

target_link_libraries(FlowtoysUpdater PRIVATE
    FlowtoysUpdaterData
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    hidapi::hidapi
    juce::juce_recommended_config_flags)

juce_generate_juce_header(FlowtoysUpdater)

option(FLOWTOYS_BUILD_TESTS "Build the unit tests" ON)
if(FLOWTOYS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
