cmake_minimum_required(VERSION 3.20)
project(Contestprogramm VERSION 0.1.1 LANGUAGES CXX)
# macOS: one Objective-C++ file (app/AppActivation.mm) asks AppKit to
# bring the running instance to the front; see if(APPLE) below.
if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# macOS: Homebrew Qt6 is not on the default search path.
if(APPLE)
    execute_process(COMMAND brew --prefix OUTPUT_VARIABLE HOMEBREW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(HOMEBREW_PREFIX)
        list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}/opt/qt")
    endif()
endif()

# Network is needed from Phase 1 step 5 on: RigctldClient (CAT, QTcpSocket)
# and On4kstClient (ON4KST chat/spots, QTcpSocket + QTcpServer in tests).
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql Network Test)

# Phase 2 terrain line-of-sight (core/terrain/SrtmTileLoader) needs real
# gzip decompression for the SRTM1 tiles it downloads -- Qt's own
# qUncompress() expects Qt's own zlib-plus-4-byte-length-prefix wrapper,
# not a genuine gzip stream, so there is no pure-Qt way to do this. zlib
# itself is present on essentially every macOS/Linux/Windows toolchain
# already (verified via `find_package(ZLIB)` succeeding against the
# Xcode Command Line Tools SDK on this machine, 2026-09-12) -- not a new
# runtime dependency to install, just one to actually link.
find_package(ZLIB REQUIRED)

set(CONTESTPROGRAMM_CORE_SOURCES
    src/core/Maidenhead.h
    src/core/Maidenhead.cpp
    src/core/SpotCandidate.h
    src/core/SpotParser.h
    src/core/SpotParser.cpp
    src/core/GeoFilter.h
    src/core/GeoFilter.cpp
    src/core/RigctldClient.h
    src/core/RigctldClient.cpp
    src/core/RotctldClient.h
    src/core/RotctldClient.cpp
    src/core/RotorModels.h
    src/core/RotctldProcess.h
    src/core/RotctldProcess.cpp
    src/core/HamlibInstaller.h
    src/core/HamlibInstaller.cpp
    src/core/BeamHeading.h
    src/core/BeamHeading.cpp
    src/core/ColorTheme.h
    src/core/ColorTheme.cpp
    src/core/RotorDialStyle.h
    src/core/On4kstClient.h
    src/core/On4kstClient.cpp
    src/core/CheckPartialIndex.h
    src/core/CheckPartialIndex.cpp
    src/core/EsmPlanner.h
    src/core/EsmPlanner.cpp
    src/core/BandmapModel.h
    src/core/BandmapModel.cpp
    src/core/SkedList.h
    src/core/SkedList.cpp
    src/core/OnlineScoreboard.h
    src/core/OnlineScoreboard.cpp
    src/core/DxClusterClient.h
    src/core/DxClusterClient.cpp
    src/core/CallsignLocatorLookup.h
    src/core/CallsignLocatorLookup.cpp
    src/core/ClockCheck.cpp
    src/core/BandUtils.h
    src/core/BandUtils.cpp
    src/core/Transverter.cpp
    src/core/BroadcastPublisher.h
    src/core/BroadcastPublisher.cpp
    src/core/ChatImportanceScorer.h
    src/core/ChatImportanceScorer.cpp
    src/core/ChatVisibilityPolicy.h
    src/core/ChatVisibilityPolicy.cpp
    src/core/WeatherClient.h
    src/core/WeatherClient.cpp
    src/core/RecentPropagationTracker.h
    src/core/RecentPropagationTracker.cpp
    src/core/CountryBorders.h
    src/core/CountryBorders.cpp
    src/core/Cities.h
    src/core/Cities.cpp
    src/core/terrain/SrtmTileLoader.h
    src/core/terrain/SrtmTileLoader.cpp
    src/core/terrain/PathProfile.h
    src/core/terrain/PathProfile.cpp
    src/core/terrain/LineOfSight.h
    src/core/terrain/LineOfSight.cpp
    src/core/terrain/HorizonProfile.h
    src/core/terrain/HorizonProfile.cpp
    src/core/terrain/TerrainDataManager.h
    src/core/terrain/TerrainDataManager.cpp
    src/core/assistant/NextTargetSuggester.h
    src/core/assistant/NextTargetSuggester.cpp
    src/core/assistant/MessageDrafter.h
    src/core/assistant/MessageDrafter.cpp
    src/app/ContestSettings.h
    src/app/ContestSettings.cpp
    src/app/AppController.h
    src/app/AppController.cpp
    src/app/AppActivation.h
    src/app/AppActivation.cpp
    src/app/AppLanguage.h
    src/app/AppLanguage.cpp
    src/app/SingleInstanceGuard.h
    src/app/SingleInstanceGuard.cpp
    src/app/SelfUpdater.h
    src/app/SelfUpdater.cpp
    src/data/QsoRecord.h
    src/data/ContestDatabase.h
    src/data/ContestDatabase.cpp
    src/data/DupeChecker.h
    src/data/DupeChecker.cpp
    src/data/ContestDefinition.h
    src/data/ContestDefinition.cpp
    src/data/CabrilloExporter.h
    src/data/CabrilloExporter.cpp
    src/data/AdifExporter.h
    src/data/AdifExporter.cpp
    src/data/EdiExporter.h
    src/data/EdiExporter.cpp
    src/data/ContestScoring.h
    src/data/ContestScoring.cpp
    src/data/ContestSchedule.h
    src/data/ContestSchedule.cpp
    src/data/LogCheck.h
    src/data/LogCheck.cpp
    src/data/ReadinessCheck.cpp
    src/data/DupeRescore.h
    src/data/DupeRescore.cpp
    src/data/LogBackup.h
    src/data/LogBackup.cpp
    src/data/BackupRestore.h
    src/data/BackupRestore.cpp
    src/data/ContestStatistics.h
    src/data/ContestStatistics.cpp
    src/data/LogFileReader.h
    src/data/LogFileReader.cpp
    src/data/MultiplierTracker.h
    src/data/MultiplierTracker.cpp
    src/models/LogTableModel.h
    src/models/LogTableModel.cpp
    src/models/ChatFeedModel.h
    src/models/ChatFeedModel.cpp
)

set(CONTESTPROGRAMM_UI_SOURCES
    src/ui/StyleKit.h
    src/ui/StyleKit.cpp
    src/ui/PanelHeaderBar.h
    src/ui/PanelHeaderBar.cpp
    src/ui/PanelContainerWidget.h
    src/ui/PanelContainerWidget.cpp
    src/ui/PanelLayoutManager.h
    src/ui/PanelLayoutManager.cpp
    src/ui/ProfileRail.h
    src/ui/ProfileRail.cpp
    src/ui/LayoutProfileManager.h
    src/ui/LayoutProfileManager.cpp
    src/ui/LogCheckWindow.h
    src/ui/LogCheckWindow.cpp
    src/ui/ReadinessWindow.cpp
    src/ui/ShortcutsWindow.cpp
    src/ui/AboutDialog.cpp
    src/ui/UpdateDialog.h
    src/ui/UpdateDialog.cpp
    src/ui/TransverterDialog.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/UnifiedLogWidget.h
    src/ui/UnifiedLogWidget.cpp
    src/ui/RateMeterWidget.h
    src/ui/RateMeterWidget.cpp
    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.cpp
    src/ui/ContestRulesEditor.h
    src/ui/ContestRulesEditor.cpp
    src/ui/ContestPickerDialog.h
    src/ui/ContestPickerDialog.cpp
    src/ui/MultiplierWindow.h
    src/ui/MultiplierWindow.cpp
    src/ui/RotorWidget.h
    src/ui/RotorWidget.cpp
    src/ui/CwMacroPanel.h
    src/ui/CwMacroPanel.cpp
    src/ui/EdiExportDialog.h
    src/ui/EdiExportDialog.cpp
    src/ui/CheckPartialWidget.h
    src/ui/CheckPartialWidget.cpp
    src/ui/EsmTemplatesDialog.h
    src/ui/EsmTemplatesDialog.cpp
    src/ui/BackupRestoreDialog.h
    src/ui/BackupRestoreDialog.cpp
    src/ui/BandmapWidget.h
    src/ui/BandmapWidget.cpp
    src/ui/SkedPanel.h
    src/ui/SkedPanel.cpp
    src/ui/ScoreboardDialog.h
    src/ui/ScoreboardDialog.cpp
    src/ui/StatisticsWindow.h
    src/ui/StatisticsWindow.cpp
    src/ui/MapWidget.h
    src/ui/MapWidget.cpp
    src/ui/UtcClockWidget.h
    src/ui/UtcClockWidget.cpp
    src/ui/SuggestionPanel.h
    src/ui/SuggestionPanel.cpp
)

add_library(ContestprogrammCore STATIC ${CONTESTPROGRAMM_CORE_SOURCES} ${CONTESTPROGRAMM_UI_SOURCES})
if(APPLE)
    target_sources(ContestprogrammCore PRIVATE src/app/AppActivation.mm)
    target_link_libraries(ContestprogrammCore PUBLIC "-framework AppKit")
endif()
target_include_directories(ContestprogrammCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
# Version, commit and build date for Hilfe › Über -- generated on
# every build, see cmake/BuildInfo.cmake.
set(CONTESTPROGRAMM_BUILDINFO_DIR "${CMAKE_BINARY_DIR}/generated")
add_custom_target(contestprogramm_buildinfo
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CONTESTPROGRAMM_BUILDINFO_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT="${CONTESTPROGRAMM_BUILDINFO_DIR}/BuildInfo.h"
        -DSOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        -DVERSION="${PROJECT_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildInfo.cmake"
    BYPRODUCTS "${CONTESTPROGRAMM_BUILDINFO_DIR}/BuildInfo.h"
    COMMENT "BuildInfo.h (Version, Commit, Datum)")
add_dependencies(ContestprogrammCore contestprogramm_buildinfo)
target_include_directories(ContestprogrammCore PUBLIC "${CONTESTPROGRAMM_BUILDINFO_DIR}")
target_link_libraries(ContestprogrammCore PUBLIC Qt6::Core Qt6::Widgets Qt6::Sql Qt6::Network ZLIB::ZLIB)

# Dev-build fallback so the app finds resources/contest_definitions/*.json
# without a packaging step; see ui/MainWindow.cpp findContestDefinitionsDir().
target_compile_definitions(ContestprogrammCore PUBLIC
    CONTESTPROGRAMM_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

# macOS: build a real .app bundle (not a bare binary) so the program
# has a Dock icon and can be pinned to the Dock like any other app.
# The bundle icon is embedded here; MainWindow/AppController's resource
# lookup already walks applicationDirPath(), which CMake's
# $<TARGET_FILE_DIR:...> resolves to Contents/MacOS inside the bundle,
# so no path changes were needed there.
set(CONTESTPROGRAMM_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/mac/AppIcon.icns")

add_executable(Contestprogramm src/main.cpp)
if(APPLE)
    target_sources(Contestprogramm PRIVATE "${CONTESTPROGRAMM_ICON}")
    set_source_files_properties("${CONTESTPROGRAMM_ICON}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    set_target_properties(Contestprogramm PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
        MACOSX_BUNDLE_BUNDLE_NAME "Contestprogramm"
        MACOSX_BUNDLE_GUI_IDENTIFIER "at.oe5sos.contestprogramm"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "OE5SOS"
    )
endif()
target_link_libraries(Contestprogramm PRIVATE ContestprogrammCore)

# Also copy resources next to the built binary, so a redistributed
# build (not just a dev build run from the source tree) finds them too.
# $<TARGET_FILE_DIR:...> is Contents/MacOS inside the .app bundle on
# macOS.
#
# This used to be a plain add_custom_command(TARGET ... POST_BUILD),
# which only reruns when Contestprogramm's OWN build inputs (its C++
# sources) change -- discovered 2026-09-12 when editing only
# resources/geo/cities_110m.dat left the bundled copy silently stale:
# loadCities()/loadCountryBorders() both check the bundled path first
# and only fall back to CONTESTPROGRAMM_SOURCE_DIR if nothing exists
# there at all, so a *stale-but-present* bundled file wins over the
# fresher source-tree one with no error or warning either way. Listing
# every file under resources/ as an explicit DEPENDS makes this step
# itself out-of-date whenever any of them changes, independent of
# whether the executable target needs relinking.
file(GLOB_RECURSE CONTESTPROGRAMM_RESOURCE_FILES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/resources/*")
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/resources_copied.stamp"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources"
            "$<TARGET_FILE_DIR:Contestprogramm>/resources"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/resources_copied.stamp"
    DEPENDS ${CONTESTPROGRAMM_RESOURCE_FILES}
    COMMENT "Copying resources/ next to the built binary"
)
add_custom_target(copy_resources ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/resources_copied.stamp")
add_dependencies(Contestprogramm copy_resources)

enable_testing()
add_subdirectory(tests)
