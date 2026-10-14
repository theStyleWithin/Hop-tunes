cmake_minimum_required(VERSION 3.22)
project(hop-tunes VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HOPTUNES_BUILD_APP   "Build the desktop application" ON)
option(HOPTUNES_BUILD_TESTS "Build unit tests" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Concurrent)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB  REQUIRED IMPORTED_TARGET taglib)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)

# ---- core library: no GUI, no GStreamer (unit-testable) ---------------------
add_library(hoptunes_core STATIC
    src/core/Paths.h          src/core/Paths.cpp
    src/library/Track.h
    src/library/TrackIndex.h  src/library/TrackIndex.cpp
    src/library/TagReader.h   src/library/TagReader.cpp
    src/library/Scanner.h     src/library/Scanner.cpp
)
target_include_directories(hoptunes_core PUBLIC src)
target_link_libraries(hoptunes_core PUBLIC Qt6::Core Qt6::Concurrent PkgConfig::TAGLIB PkgConfig::SQLITE3)
target_compile_options(hoptunes_core PRIVATE -Wall -Wextra)

# ---- playback library: GStreamer, still no GUI (unit-testable) -----------------
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0)
add_library(hoptunes_playback STATIC
    src/playback/Player.h     src/playback/Player.cpp
)
target_link_libraries(hoptunes_playback PUBLIC hoptunes_core PkgConfig::GST)
target_compile_options(hoptunes_playback PRIVATE -Wall -Wextra)

# ---- desktop app --------------------------------------------------------------
if(HOPTUNES_BUILD_APP)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Gui Quick QuickControls2)

    add_executable(hop-tunes
        src/main.cpp
        src/App.h                    src/App.cpp
        src/library/TrackListModel.h src/library/TrackListModel.cpp
        src/ui/qml.qrc
    )
    target_compile_definitions(hop-tunes PRIVATE HOPTUNES_VERSION="${PROJECT_VERSION}")
    target_link_libraries(hop-tunes PRIVATE hoptunes_playback Qt6::Gui Qt6::Quick Qt6::QuickControls2)
    target_compile_options(hop-tunes PRIVATE -Wall -Wextra)

    include(GNUInstallDirs)
    install(TARGETS hop-tunes RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES packaging/hop-tunes.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif()

# ---- tests --------------------------------------------------------------------
if(HOPTUNES_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    add_executable(test_library tests/test_library.cpp)
    target_link_libraries(test_library PRIVATE hoptunes_core Qt6::Test)
    add_test(NAME library COMMAND test_library)

    add_executable(test_player tests/test_player.cpp)
    target_link_libraries(test_player PRIVATE hoptunes_playback Qt6::Test)
    add_test(NAME player COMMAND test_player)
    set_tests_properties(player PROPERTIES ENVIRONMENT "HOPTUNES_AUDIO_SINK=fakesink")
endif()
