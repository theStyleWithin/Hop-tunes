cmake_minimum_required(VERSION 3.22)
project(hop-tunes VERSION 1.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HOPTUNES_BUILD_APP   "Build the desktop application" ON)
option(HOPTUNES_BUILD_TESTS "Build unit tests" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Concurrent Gui)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB  REQUIRED IMPORTED_TARGET taglib)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)

# ---- core library: no GUI, no GStreamer (unit-testable) ---------------------
add_library(hoptunes_core STATIC
    src/core/Paths.h          src/core/Paths.cpp
    src/core/ProfileStore.h src/core/ProfileStore.cpp
    src/library/Track.h
    src/core/Sql.h
    src/library/TrackIndex.h  src/library/TrackIndex.cpp
    src/library/ProfileDb.h   src/library/ProfileDb.cpp
    src/library/Smart.h       src/library/Smart.cpp
    src/library/TagReader.h   src/library/TagReader.cpp
    src/library/CoverArt.h    src/library/CoverArt.cpp
    src/library/CollectionModels.h src/library/CollectionModels.cpp
    src/library/TrackListModel.h src/library/TrackListModel.cpp
    src/library/Scanner.h     src/library/Scanner.cpp
    src/library/Video.h
    src/library/VideoIndex.h  src/library/VideoIndex.cpp
    src/library/VideoListModel.h src/library/VideoListModel.cpp
    src/library/LibraryWatcher.h src/library/LibraryWatcher.cpp
)
target_include_directories(hoptunes_core PUBLIC src)
target_link_libraries(hoptunes_core PUBLIC Qt6::Core Qt6::Concurrent PkgConfig::TAGLIB PkgConfig::SQLITE3)
target_compile_options(hoptunes_core PRIVATE -Wall -Wextra)

# ---- playback library: GStreamer, still no GUI (unit-testable) -----------------
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0)
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)
# Video (spec §5b): GstDiscoverer + a frame-grab pipeline for the library side (probing,
# thumbnails), all parts of gst-plugins-base already required for audio. libmpv is the
# actual video *playback* engine — a new dependency, music keeps GStreamer.
pkg_check_modules(GST_PBUTILS REQUIRED IMPORTED_TARGET gstreamer-pbutils-1.0)
pkg_check_modules(GST_APP     REQUIRED IMPORTED_TARGET gstreamer-app-1.0)
pkg_check_modules(GST_VIDEO   REQUIRED IMPORTED_TARGET gstreamer-video-1.0)
# libmpv is the primary video *playback* engine (spec §5b) — not REQUIRED, unlike
# everything else above: it's not yet packaged for the Flatpak build (see that manifest's
# comment), and a build with no libmpv.pc should still produce a fully working Hop-tunes
# rather than fail to configure. MpvCore.cpp/MpvVideoItem.cpp each carry a stub
# implementation (#else branch, no mpv/* headers) for exactly this case — when
# HOPTUNES_HAVE_MPV is off, App::videoEngine() falls back to GstVideoPlayer (GStreamer's
# own qml6glsink/glsinkbin, needing no new dependency beyond gstreamer-plugins-base
# above) if that's available, else "none". Library scanning/probing (VideoScanner etc.)
# needs only gstreamer-plugins-base regardless, so Videos always scans and lists.
pkg_check_modules(MPV IMPORTED_TARGET mpv)
add_library(hoptunes_playback STATIC
    src/playback/Player.h     src/playback/Player.cpp
    src/playback/MpvCore.h    src/playback/MpvCore.cpp
    src/playback/GstVideoPlayer.h src/playback/GstVideoPlayer.cpp
    src/platform/Mpris.h      src/platform/Mpris.cpp
    src/library/VideoProbe.h     src/library/VideoProbe.cpp
    src/library/VideoThumbnail.h src/library/VideoThumbnail.cpp
    src/library/VideoScanner.h   src/library/VideoScanner.cpp
)
target_link_libraries(hoptunes_playback PUBLIC hoptunes_core Qt6::Gui PkgConfig::GST PkgConfig::GIO
    PkgConfig::GST_PBUTILS PkgConfig::GST_APP PkgConfig::GST_VIDEO)
if(MPV_FOUND)
    target_link_libraries(hoptunes_playback PUBLIC PkgConfig::MPV)
    target_compile_definitions(hoptunes_playback PUBLIC HOPTUNES_HAVE_MPV)
else()
    message(STATUS "libmpv not found: video playback disabled (library scanning still works)")
endif()
target_compile_options(hoptunes_playback PRIVATE -Wall -Wextra)

# ---- desktop app --------------------------------------------------------------
if(HOPTUNES_BUILD_APP)
    # OpenGL: QQuickFramebufferObject (MpvVideoItem's base, spec §5b) needs Qt6::OpenGL
    # since Qt 6 split QOpenGLFramebufferObject etc. out of Gui.
    find_package(Qt6 6.4 REQUIRED COMPONENTS Gui Quick QuickControls2 OpenGL)

    add_executable(hop-tunes
        src/main.cpp
        src/App.h                    src/App.cpp
        src/ui/MpvVideoItem.h        src/ui/MpvVideoItem.cpp
        src/ui/qml.qrc
        assets/assets.qrc
    )
    target_compile_definitions(hop-tunes PRIVATE HOPTUNES_VERSION="${PROJECT_VERSION}")
    target_link_libraries(hop-tunes PRIVATE hoptunes_playback Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::OpenGL)
    target_compile_options(hop-tunes PRIVATE -Wall -Wextra)

    include(GNUInstallDirs)
    install(TARGETS hop-tunes RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    set(HOPTUNES_APP_ID io.github.thestylewithin.Hop-tunes)
    install(FILES packaging/${HOPTUNES_APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES packaging/${HOPTUNES_APP_ID}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    install(FILES packaging/${HOPTUNES_APP_ID}.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES docs/hop-tunes.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
    install(FILES THIRD_PARTY_NOTICES.md README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

# ---- tests --------------------------------------------------------------------
if(HOPTUNES_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
    add_executable(test_library tests/test_library.cpp)
    target_link_libraries(test_library PRIVATE hoptunes_core Qt6::Test)
    add_test(NAME library COMMAND test_library)

    add_executable(test_profiles tests/test_profiles.cpp)
    target_link_libraries(test_profiles PRIVATE hoptunes_core Qt6::Test)
    add_test(NAME profiles COMMAND test_profiles)

    add_executable(test_m4 tests/test_m4.cpp)
    target_link_libraries(test_m4 PRIVATE hoptunes_core Qt6::Test)
    add_test(NAME playlists-search-dj COMMAND test_m4)

    add_executable(test_player tests/test_player.cpp)
    target_link_libraries(test_player PRIVATE hoptunes_playback Qt6::Test)
    # Run the playback tests against both GStreamer engines.
    add_test(NAME player COMMAND test_player)
    set_tests_properties(player PROPERTIES ENVIRONMENT "HOPTUNES_AUDIO_SINK=fakesink sync=true")
    add_test(NAME player-playbin COMMAND test_player)
    set_tests_properties(player-playbin PROPERTIES ENVIRONMENT "HOPTUNES_AUDIO_SINK=fakesink sync=true;HOPTUNES_PLAYBIN=playbin")
    add_test(NAME player-playbin3 COMMAND test_player)
    set_tests_properties(player-playbin3 PROPERTIES ENVIRONMENT "HOPTUNES_AUDIO_SINK=fakesink sync=true;HOPTUNES_PLAYBIN=playbin3")

    # MPRIS needs a D-Bus session bus; the test starts its own private one (skips if unavailable).
    add_executable(test_mpris tests/test_mpris.cpp)
    target_link_libraries(test_mpris PRIVATE hoptunes_playback Qt6::Test)
    add_test(NAME mpris COMMAND test_mpris)
    set_tests_properties(mpris PROPERTIES ENVIRONMENT "HOPTUNES_AUDIO_SINK=fakesink sync=true")

    # Video (spec §5b): probing, thumbnailing and the headless (vo=null/ao=null) mpv core —
    # no Qt Quick needed, same shape as test_player/test_mpris.
    add_executable(test_video tests/test_video.cpp)
    target_link_libraries(test_video PRIVATE hoptunes_playback Qt6::Test)
    add_test(NAME video COMMAND test_video)
    set_tests_properties(video PROPERTIES TIMEOUT 120)

    # Profile flows driven through the real QML UI, offscreen.
    if(HOPTUNES_BUILD_APP)
        add_executable(test_ui tests/test_ui.cpp src/App.cpp src/ui/MpvVideoItem.cpp src/ui/qml.qrc assets/assets.qrc)
        target_link_libraries(test_ui PRIVATE hoptunes_playback Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::OpenGL Qt6::Test)
        target_compile_definitions(test_ui PRIVATE HOPTUNES_VERSION="${PROJECT_VERSION}")

        # Damaged files, odd names, broken databases and playlists must never crash or block startup.
        add_executable(test_robust tests/test_robust.cpp src/App.cpp)
        target_link_libraries(test_robust PRIVATE hoptunes_playback Qt6::Gui Qt6::Quick Qt6::Test)
        add_test(NAME robustness COMMAND test_robust)
        set_tests_properties(robustness PROPERTIES TIMEOUT 180)
        add_test(NAME ui COMMAND test_ui)
        set_tests_properties(ui PROPERTIES TIMEOUT 120)
    endif()
endif()
